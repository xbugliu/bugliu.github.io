---
title: "GPTQ 落地与优化"
date: 2023-06-19T20:41:48+08:00
# draft: true
categories: ["开发"]
---

回顾前一篇[文章][2]，GPTQ给大模型带来了降本的可能，但无法直接落地。经过迁移适配，我们将GPTQ的INT4 Kernel集成进[FasterTransformer][3]（简称FT），优化后可以在2卡A100运行175B的模型，对比fp16相同算力下**性能提升3.8倍，成本降低74%**。

看一下数据：
| 模型版本                            | 模型文件大小 | 初始显存 | 峰值显存 | 显卡(A100)个数 | 算力  | 每秒生成token数 |
| ----------------------------------- | -------- | -------- | -------- | ------ | ----- | ---- |
| FP16                                | 329G     | 49G*8    | 无数据   | 8卡    | 99%   | 23   |
| INT8                                | 329G     | 49G*4    | 50G*4    | 4卡    | 99%   | 27   |
| INT4-开源版本-old-cuda              | 93G      | 52G*2    | 54G*2    | 2卡    | 99%   | 10   |
| INT4-开源版本-fastest-inference-4bit| 93G      | 52G*2    | 54G*2    | 2卡    | 99%   | 12   |
| INT4-优化版本                       | 93G      | 52G*2    | 54G*2    | 2卡    | 99%   | 22   |


测试基于FasterTransformer, 基础模型为bloom 175B, 模型版本:
* FP16 - 默认的推理方式，配置8卡Tensor并行，Linear算子调用的cublas的gemm
* INT8 - Weight Only的量化方式，算子基于cutlass::gemm::kernel::GemmFpAIntB
* [INT4-开源版本-old-cuda][0] GPTQ最初的开源实现，VecQuant4MatMulKernel是INT4的矩阵乘算子
* [INT4-开源版本-fastest-inference-4bit][1] GPTQ-for-LLaMa项目对INT4算子做了优化，主要的改动是2个half变成一个half2
* INT4-优化版本 我们基于fastest-inference-4bit版本做了一些优化

## 一、FT改造
改造FT以支持GPTQ的INT4比较简单，首先了解下int4的权重文件。

### 权重矩阵

![int4权重](/images/posts/gptq/int4_weight.png)

INT4权重说明：

1. 一个Linear层的fp16权重，输出3个权重：int4的权重，scale系数，zero。
2. 三个权重的类型和尺寸。假设原始fp16权重为[K, N], 则: 
  * int4_weight为[K/8, N] (一列的8个int4合并一个int32)
  * scale为[K/128, N] (格式为fp16, 一列的128个权重共用一个scale, 128为量化时配置的groupsize)
  * zero为[K/128, N/8] (一行8个int4合并一个int32, 一列的128个权重共用一个int4的zero)

### 转换脚本

修改[huggingface_bloom_convert.py][4]以支持转换int4权重，流程简单，关键代码：


```python

# 处理int4权重，转换名字，保存成numpy格式
 def _process_quant_file(model_config, model_file, dtype: torch.dtype, tp_size: int, save_dir: Path):
    state_dict = torch.load(model_file, map_location="cpu")
    for name in state_dict:
        param = state_dict[name]
        # Preprocess
        param_name = convert_parameter_name(name)
        if not param_name.endswith(("qzeros", "scales", "qweight")):
            # param = safe_transpose(param) 注意不要转置，因为gqtq已转置
            continue
        param = handle_exceptions(model_config, param_name, param)
        convert_and_save_parameter(param_name, param.detach().cpu().numpy(), tp_size, save_dir)

# qkv权重reorder
def reorder_qkv_weight_or_bias(model_config: PretrainedConfig,
                               name: str,
                               param: torch.nn.Parameter):
    if 'query_key_value' not in name:
        # Nothing to do for the non-eligible parameters.
        return param

    num_heads = model_config.n_head
    head_dim = model_config.hidden_size // model_config.n_head
    if "qzeros" in name:
        head_dim = head_dim // 8 # qzero 一行8个int4合并int32, 所以需要除8

    # (..., 3 x hidden) view as (..., num_heads, 3, head_dim)
    param = param.view(-1, num_heads, 3, head_dim)
    # permute to (..., 3, num_heads, head_dim)
    param = param.permute(0, 2, 1, 3)
    # final shape: weight=(hidden, 3, hidden) or bias=(3, hidden)
    if 'query_key_value.bias' in name:
        return param.reshape(3, num_heads * head_dim)
    return param.reshape(-1, 3, num_heads * head_dim)

```

### 模型加载
修改ParallelGptDecoderLayerWeight<T>::loadModel函数，以加载int4的权重文件，比较简单，不再赘述。

### 替换矩阵乘kernel
封装[vecquant4matmul_cuda][0]算子成GptqGemmRunner，然后替换掉  
* self_attention.dense 
* self_attention.query_key_value 
* mlp.dense_4h_to_h 
* mlp.dense_h_to_4h等处的调用, 示例：

```cpp
// src/fastertransformer/layers/attention_layers/DecoderSelfAttentionLayer.cc

template<typename T>
void DecoderSelfAttentionLayer<T>::forward(TensorMap*                output_tensors,
                                           TensorMap*                input_tensors,
                                           const AttentionWeight<T>* attention_weights)
{
    if (int8_mode_ == 1) {
        // int8 weight only gemm
    }
    else if (int8_mode_ == 10) { // gptq gemm
              gptq_runner_->gemm(attention_weights->query_weight.qweight, // int4的权重 buf 
                              {d_model_, 3 * local_hidden_units_}, // 权重的尺寸
                               attention_weights->query_weight.qscales,  // 量化系数 buf
                               attention_weights->query_weight.qzeros,   // 零点值 buf
                               attention_input, // 输入的matrix buf 
                               {batch_size, d_model_},  // 输入的尺寸
                               qkv_buf_, // 输出buf
                               stream_, cublas_wrapper_);
    }
    else { // fp16
        cublas_wrapper_->Gemm(CUBLAS_OP_N,
                                CUBLAS_OP_N,
                                3 * local_hidden_units_,  // n
                                batch_size,
                                d_model_,  // k
                                attention_weights->query_weight.kernel,
                                3 * local_hidden_units_,  // n
                                attention_input,
                                d_model_,  // k
                                qkv_buf_,
                                3 * local_hidden_units_ /* n */);
    }
}
```

## 二、kernel性能摸底
调通流程不难，难点在于kernel算子性能，未经优化int4的kernel性能不及FP16，不符合预期，所以必须针对kernel进行优化。

### 1. 原始实现-old-cuda
参加代码[VecQuant4MatMulKernel][0]:
* 功能：矩阵乘，假设C=A * B, A=[M, K], B=[K, N], C=[M, N]
* scalar_t为fp16, vec为A[M, K], mat为int4权重[K/8, N], mul为结果C[M, N], scales为[K/128, N], zeros为[K/128, N/8]

计算过程：
* 将mat切分为[BLOCKWIDTH, BLOCKWIDTH]的子矩阵，每个block处理[1, BLOCKWIDTH] * [BLOCKWIDTH, BLOCKWIDTH]的矩阵乘，子结果通过atomicAdd更新到mul中。
* K/BLOCKWIDTH个block的结果相加得到mul[1, BLOCKWIDTH]
* {K/BLOCKWIDTH, N/BLOCKWIDTH}个block处理[1, N]个结果
* {K/BLOCKWIDTH, N/BLOCKWIDTH, M}个block得到[M, N]个结果
* 每个线程处理BLOCKWIDTH个乘加

线程内的流程：
* BLOCKWIDTH个线程协作拷贝vec中BLOCKWIDTH个值到share memory中（变量blockvec）。
* 从mat中获取Int32的权重，通过scale和zero还原成8个fp16，与blockvec中的值进行乘加。
* 将乘加的结果通过atomicAdd保存到mul中。

性能测试：

* 测试环境：A100单卡，权重为bloom 175B 2卡query_key_value的尺寸：14336*21504
* 矩阵乘算子耗时对比，时间单位（微秒）

|  M   | cublas FP16耗时 | int4算子-BLOCKWIDTH-256  | int4算子-BLOCKWIDTH-1024  |
| :--: | :-------------: | :-------------------: | :-------------------: |
|  1   |     372.794     |        419.525        |        271.103        |
|  2   |     373.715     |       1308.533        |        515.462        |
|  4   |     373.525     |       2544.975        |         958.13        |
|  8   |     378.458     |       5040.562        |        1905.114       |
|  16  |     384.483     |      10024.748        |        3815.136       |
|  128 |      395.75     |      79716.668        |       30248.278       |
|  256 |     704.908     |      159450.097       |       61029.231       |
|  320 |     967.704     |       199256.5        |       76277.884       |

结论：默认的实现有一些问题：

1. int4比fp16约快37%，不符合预期，应该更快。因为带宽约降低75%，算力增加了约2倍，但带宽一般是瓶颈。
2. 默认的BLOCKWIDTH配置的太小，计算访存比低，测试下来1024最优。
3. 随着M的增加，耗时线性增加，原因是多个M未做还原复用。

### 2. half2实现fastest-inference-4bit

默认的kernel实现性能不理想，然后发现GPTQ-for-LLaMa项目里面针对int4进行了专项的[优化][5].

优化思想:
* 向量化读取，将读取vec的过程改成half2的格式，加快读取的速度
*  乘加计算也通过half2的类型，减少算力占用

经过测试fastest-inference-4bit约提升18%，随着M的增加，耗时线性增加依旧。


## 三、kernel性能优化

因为性能依然不理想，所以基于fastest-inference-4bit进行优化，经过三轮优化，最终实现：

1. 耗时降低到cublas fp16耗时到31% (m=1)
2. 耗时降低到fastest-inference-4bit的51% (m=1)
3. 初步解决了m增加，耗时线性增加到问题, m=16时耗时降低到fastest-inference-4bit的10%

看一下数据：

* 测试环境：A100单卡，权重为bloom 175B 2卡query_key_value的尺寸：14336*21504
* 矩阵乘算子耗时对比，时间单位（微秒）

|  m   | cublas耗时 | int4-old-cuda-耗时 | int4-fastest-inference-4bit | 优化1 | 优化2 | 优化3  |
| :--: | :----------------: | :-------------------: | :----------------------: | :------: | :------: | :--------: |
|  1   |      372.794       |        271.103        |          228.472          | 178.361  | 147.065  |   117.33   |
|  2   |      373.715       |        515.462        |          430.873          | 330.344  | 215.531  |   140.514  |
|  4   |      373.525       |        958.13         |          807.954          | 614.537  | 298.237  |   169.338  |
|  8   |      378.458       |       1905.114        |          1622.786         | 1202.168 | 386.775  |   266.117  |
|  16  |      384.483       |       3815.136        |          3266.978         | 2411.934 | 762.359  |   348.608  |
|  128 |       395.75       |       30248.278       |          26160.017        | 19724.196 | 6112.127 |  2639.796  |
|  256 |      704.908       |       61029.231       |          52545.124        |  39206.874  | 12224.146 |  5231.433  |
|  320 |      967.704       |       76277.884       |          65818.762        | 48847.196  | 18336.961 |  6698.273  |


讲一下3轮优化的思路：
### 优化1： scale和zero读取优化


优化思想：
* 合并减少读取scale和zero的次数 因为每128个权重共享一个scale和zero。
* 加大每个线程处理的结果数，每个线程处理2个结果。

优化效果：相比fastest-inference-4bit耗时降低22%。

### 优化2：解决m增加耗时增加问题

优化思想：
*  共享权重还原，多个m共享一次权重的读取和还原
*  减少bank冲突，加大deq2的冗余，减少冲突，当m=1，配置TBANK=32。

工程实现注意事项：
1. 因为share memory的限制，最多16行vec共享一次权重读取和还原时。对于m大于16的情况，16的整数倍通过配置blockIdx.z在一次kernel launch中执行，余数单独调用一次kernel launch

优化效果：相比fastest-inference-4bit耗时降低35%, 且m增加耗时增加问题改善。


<!-- ```
template <int BATCH, int TWIDTH, int TBANK, typename scalar_t>
__global__ void VecQuant4MatMulKernel_perf2(
    const  half2* __restrict__ vec,
    const    int* __restrict__ mat,
           scalar_t* __restrict__ mul,
    const  scalar_t* __restrict__ scales,
    const    int* __restrict__ zeros,
    int vec_height,
    int height,
    int width,
    int zero_width
) {
  const int blockwidth2 = TWIDTH / 2;
  int h = TWIDTH / 8 * blockIdx.x;
  int w = TWIDTH * blockIdx.y + threadIdx.x;
  int w2 = w + blockwidth2;

  __shared__ half2 blockvec[BATCH][TWIDTH];
  #pragma unroll
  for (int b=0; b<BATCH; ++b) {
      blockvec[b][threadIdx.x] = vec[b * vec_height + blockIdx.x * blockwidth2 + threadIdx.x];
    }

  __shared__ half2 deq2[256][TBANK];
  int val = threadIdx.x / TBANK;
  int off = threadIdx.x % TBANK;
  for (; val < 256; val += blockwidth2 / TBANK) {
    deq2[val][off] = __halves2half2(
       __int2half_rn(val & 0xF), __int2half_rn(val >> 4)
    );
  }

  __syncthreads();

  int i = width * h + w;
  int g_h = h * 8;
  int k = 0;

  int z_w = w / 8; 
  int z_mod = (w % 8) * 4;

  int z_w2 = w2 / 8; 
  int z_mod2 = (w2 % 8) * 4;

  float res[BATCH] = {0};
  float res2[BATCH] = {0};
  half2 resh2;

  unsigned int tmp;
  unsigned int tmp2;

  int g = g_h / GROUPSIZE;
  const int group_count = TWIDTH / GROUPSIZE;
  #pragma unroll
  for (int gi=0; gi<group_count; ++gi) {
   float scale_f = scales[g * width + w];
    half2 scale = __float2half2_rn(scale_f);
    half2 zero = __float2half2_rn(-(scale_f * (((as_unsigned(zeros[g * zero_width + z_w]) >> z_mod) & 0xF) + 1)));

   float scale_f2 = scales[g * width + w2];
    half2 scale2 = __float2half2_rn(scale_f2);
    half2 zero2 = __float2half2_rn(-(scale_f2 * (((as_unsigned(zeros[g * zero_width + z_w2]) >> z_mod2) & 0xF) + 1)));

    const int gk = GROUPSIZE / 2 * (gi+1);
    while (k < gk) { // blockvec一次取2个值

      tmp = as_unsigned(mat[i]);
      tmp2 = as_unsigned(mat[i + blockwidth2]);

        half2 m0 = __hfma2(deq2[(tmp >>  0) & 0xff][off], scale, zero);
        half2 m1 = __hfma2(deq2[(tmp >>  8) & 0xff][off], scale, zero);
        half2 m2 = __hfma2(deq2[(tmp >>  16) & 0xff][off], scale, zero);
        half2 m3 = __hfma2(deq2[(tmp >>  24) & 0xff][off], scale, zero);

        half2 m2_0 = __hfma2(deq2[(tmp2 >>  0) & 0xff][off], scale2, zero2);
        half2 m2_1 = __hfma2(deq2[(tmp2 >>  8) & 0xff][off], scale2, zero2);
        half2 m2_2 = __hfma2(deq2[(tmp2 >>  16) & 0xff][off], scale2, zero2);
        half2 m2_3 = __hfma2(deq2[(tmp2 >>  24) & 0xff][off], scale2, zero2);
          
          #pragma unroll
          for (int b=0; b<BATCH; ++b) {
              resh2 = {};
              
              half2 v0 = blockvec[b][k + 0];
              half2 v1 = blockvec[b][k + 1];
              half2 v2 = blockvec[b][k + 2];
              half2 v3 = blockvec[b][k + 3];

              resh2 = __hfma2(m0, v0, resh2);
              resh2 = __hfma2(m1, v1, resh2);
              resh2 = __hfma2(m2, v2, resh2);
              resh2 = __hfma2(m3, v3, resh2);
              res[b] += __half2float(resh2.x) + __half2float(resh2.y);

              resh2 = {};
              resh2 = __hfma2(m2_0, v0, resh2);
              resh2 = __hfma2(m2_1, v1, resh2);
              resh2 = __hfma2(m2_2, v2, resh2);
              resh2 = __hfma2(m2_3, v3, resh2);
              res2[b] += __half2float(resh2.x) + __half2float(resh2.y);
          }
          i += width;
          k += 4;
        }

    g = g + 1;    
  }

  for (int b=0; b<BATCH; ++b) {
    atomicAdd(&mul[b * width + w], res[b]);
    atomicAdd(&mul[b * width + w2], res2[b]);
  }
}
``` -->

### 优化3：提高带宽利用率

完成优化2后，用Nsight Compute分析瓶颈在mat读取。

优化思想：
* 合并读取，一个线程连续读取2个连续的int32。
* 向量化写入, 合并写入2个half为一个half2。


最终优化效果：
1. 耗时降低到fastest-inference-4bit的51% (m=1)
2. 初步解决了m增加，耗时线性增加到问题, m=16时耗时降低到fastest-inference-4bit的10%

最终矩阵乘代码：
```
template <int BATCH, int TWIDTH, int TBANK, typename scalar_t>
__global__ void VecQuant4MatMulKernel_perf(
    const  half2* __restrict__ vec,
    const    int* __restrict__ mat,
           half2* __restrict__ mul,
    const  scalar_t* __restrict__ scales,
    const    int* __restrict__ zeros,
    int vec_height,
    int height,
    int width,
    int zero_width
) 
{
  const int blockwidth2 = TWIDTH / 2;
  int h = TWIDTH / 8 * blockIdx.x;
  int w = TWIDTH * blockIdx.y + 2 * threadIdx.x;
  int w2 = w + 1;
  vec = vec + BATCH * blockIdx.z * vec_height;
  mul = mul + BATCH * blockIdx.z * width / 2;

  __shared__ half2 blockvec[BATCH][TWIDTH];
  #pragma unroll
  for (int b=0; b<BATCH; ++b) {
      blockvec[b][threadIdx.x] = vec[b * vec_height + blockIdx.x * blockwidth2 + threadIdx.x];
    }

  __shared__ half2 deq2[256][TBANK];
  int val = threadIdx.x / TBANK;
  int off = threadIdx.x % TBANK;
  for (; val < 256; val += blockwidth2 / TBANK) {
    deq2[val][off] = __halves2half2(
       __int2half_rn(val & 0xF), __int2half_rn(val >> 4)
    );
  }

  __syncthreads();

  int i = width * h + w;
  int g_h = h * 8;
  int k = 0;

  int z_w = w / 8; 
  int z_mod = (w % 8) * 4;

  // int z_w2 = w2 / 8; 
  int z_mod2 = (w2 % 8) * 4;

  // float res[BATCH] = {0};
  // float res2[BATCH] = {0};
  half2 resh[BATCH] = { {} };
  half2 resh2[BATCH] = { {} };

  unsigned int tmp;
  unsigned int tmp2;
  int g = g_h / GROUPSIZE;
  const int group_count = TWIDTH / GROUPSIZE;
  // #pragma unroll
  for (int gi=0; gi<group_count; ++gi) {
    float scale_f = scales[g * width + w];
    half2 scale = __float2half2_rn(scale_f);
    unsigned tmp_zero = as_unsigned(zeros[g * zero_width + z_w]);
    half2 zero = __float2half2_rn(-(scale_f * (((tmp_zero >> z_mod) & 0xF) + 1)));

    float scale_f2 = scales[g * width + w2];
    half2 scale2 = __float2half2_rn(scale_f2);
    half2 zero2 = __float2half2_rn(-(scale_f2 * (((tmp_zero >> z_mod2) & 0xF) + 1)));

    const int gk = GROUPSIZE / 2 * (gi+1);
    while (k < gk) { // blockvec一次取2个值

        tmp = as_unsigned(mat[i]);
        tmp2 = as_unsigned(mat[i + 1]);

        half2 m0 = __hfma2(deq2[(tmp >>  0) & 0xff][off], scale, zero);
        half2 m1 = __hfma2(deq2[(tmp >>  8) & 0xff][off], scale, zero);
        half2 m2 = __hfma2(deq2[(tmp >>  16) & 0xff][off], scale, zero);
        half2 m3 = __hfma2(deq2[(tmp >>  24) & 0xff][off], scale, zero);

        half2 m2_0 = __hfma2(deq2[(tmp2 >>  0) & 0xff][off], scale2, zero2);
        half2 m2_1 = __hfma2(deq2[(tmp2 >>  8) & 0xff][off], scale2, zero2);
        half2 m2_2 = __hfma2(deq2[(tmp2 >>  16) & 0xff][off], scale2, zero2);
        half2 m2_3 = __hfma2(deq2[(tmp2 >>  24) & 0xff][off], scale2, zero2);

          #pragma unroll
          for (int b=0; b<BATCH; ++b) {
              
              half2 v0 = blockvec[b][k + 0];
              half2 v1 = blockvec[b][k + 1];
              half2 v2 = blockvec[b][k + 2];
              half2 v3 = blockvec[b][k + 3];

              resh[b] = __hfma2(m0, v0, resh[b]);
              resh[b] = __hfma2(m1, v1, resh[b]);
              resh[b] = __hfma2(m2, v2, resh[b]);
              resh[b] = __hfma2(m3, v3, resh[b]);

              resh2[b] = __hfma2(m2_0, v0, resh2[b]);
              resh2[b] = __hfma2(m2_1, v1, resh2[b]);
              resh2[b] = __hfma2(m2_2, v2, resh2[b]);
              resh2[b] = __hfma2(m2_3, v3, resh2[b]);

          }
          i += width;
          k += 4;
        }

    g = g + 1;    
  }

  w = blockwidth2 * blockIdx.y + threadIdx.x;

  for (int b=0; b<BATCH; ++b) {
    float resa = __half2float(resh[b].x) + __half2float(resh[b].y);
    float resb = __half2float(resh2[b].x) + __half2float(resh2[b].y);
    
    half2 result = make_half2(resa, resb);
    atomicAdd(&mul[b * width / 2 + w], result);
    // atomicAdd(&mul[b * width + w2], res2[b]);
  }
}

```

### 优化4： 解决M>16后，耗时线性增长的问题

经过3轮优化后，m<16时，int4算子比cublas有明显优势，但当m>16时，int4算子耗时增加明显。

在FastTransformer中，有2个场景会用到m>16:
1. 生成content的阶段，即由embding计算kv cache的时候，m=batch*input_token_len，m很容易超过16，此时慢会影响首字耗时
2. 在生成一个tokenid的过程中，m=batch, 一般很难超过16

因为m>16会影响首字耗时，所以采用下列策略：

1. 回退cublas的方案，因为cublas随着m的增加耗时基本不增加，所以实现了一个反量化的接口，对于m>48的场景先反量化到fp16，然后再调用cublas, 48为实测下，反量化+cublas耗时小于int4算子耗时的拐点。
2. 反量化需要显存，一块卡复用一个buf即可。

### kernel优化的套路

因为我也是第一次做kernel优化，所以记录下：

1. 规划好block和thread的职责，先实现一个版本，常见的优化方法：多级存储，向量化读写
2. 核对kernel结果是否正确
3. 调整grid和block的size, 对比测试找到一个最优的值
4. 使用 Nsight Compute分析kernel的瓶颈, 需要关注的点：
  * Occupancy，理论Occupancy和实际Occupancy
  *  Compute (SM) Throughput 和 Memory Throughput
  * memory workload 里面的bank conflict
  * Source Counters 可以定位warp挂起的原因
5. 解决Nsight Compute分析出的瓶颈

##  四、未完的事

1. 继续优化int4的kernel, 在m>16后，应该有更优雅的实现方法
2. 实现并验证双buf预取方法，增加带宽的利用率


[0]: https://github.com/qwopqwop200/GPTQ-for-LLaMa/blob/old-cuda/quant_cuda_kernel.cu
[1]: https://github.com/qwopqwop200/GPTQ-for-LLaMa/blob/fastest-inference-4bit/quant_cuda_kernel.cu
[2]: /blog/2023/04/26/gptq/
[3]: https://github.com/NVIDIA/FasterTransformer
[4]: https://github.com/NVIDIA/FasterTransformer/blob/main/examples/pytorch/gpt/utils/huggingface_bloom_convert.py
[5]: https://github.com/qwopqwop200/GPTQ-for-LLaMa/issues/227


