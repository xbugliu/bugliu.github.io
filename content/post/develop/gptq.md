---
title: "大模型推理 - GPTQ 量化过程解析"
date: 2023-04-26T20:05:35+08:00
categories: ["开发"]
slug: 2023/04/26/gptq/
---



## 什么是GPTQ

[量化][1]是一种通过实数映射整数，来降低位宽度从而减小权重大小、提高推理速度的技术。随着大模型的兴起，常见的INT8量化已不满足降本的需求。GPTQ是针对GPT模型的量化技术，可以实现INT4、INT3量化且保持良好的精度。

## 开源地址
* 论文： https://arxiv.org/pdf/2210.17323.pdf
* 官方实现： https://github.com/IST-DASLab/gptq
* 三方实现1:  https://github.com/LianjiaTech/BELLE/tree/main/models/gptq 
* 三方实现2: https://github.com/qwopqwop200/GPTQ-for-LLaMa

当前（2023年4月26日）官方实现了核心的量化和算子： [gptq.py](https://github.com/IST-DASLab/gptq/blob/main/gptq.py) [quant_cuda_kernel.cu](https://github.com/IST-DASLab/gptq/blob/main/quant_cuda_kernel.cu) 但没有实现具体比如bloom模型的权重保存和推理过程，三方实现的较为完整，可以直接使用，下面的讲解以BELLE项目的bloom为示例。

## 术语

* scale  实数映射整数的系数
* zero [非对称量化][3]中，实数0映射为整数的值。
* Linear pytorch的[线性层][4]。
* groupsize GPTQ中的分组，每个分组会共用一个scale
* hook 一种注入机制，pytorch支持注入来添加推理的自定义前后处理操作

## 量化流程
量化会针对transformer decoder layer进行量化

首先看一下[GPTQ][2]这个负责量化的类，里面有2个重要的函数：

1. add_batch 添加Linear层的输入输出数据
2. fasterquant进行量化，并返回量化后的scale和zero数据。

### 准备工作
见代码[bloom.py][5]中的get_bloom和get_loader：
1. 加载测试集
2. 加载原始的model

### 量化第0层（layer）
见代码[bloom.py][5]的bloom_sequential方法：
1. 找到Linear层，并创建对应的GPTQ实例。
2. 通过hook的方式，用测试集调用推理，拿到layer0的推理参数：hidden_states、alibi和attention_mask
3. 用上一步获取的参数来调用layer，通过hook的方式，将Linear层的输入输出add_batch到对应的GPTQ实例。
4. 调用GPTQ的fasterquant，拿到scale和zero。

### 量化剩余的层
然后将0层的输出，输入到第1层进行量化，直到量化完所有层。

## 权重保存
1. 遍历替换Linear层为QuantLinear，其中有3个关键的Tensor: qweight, scales, qzeros
2. 将原Linear层里面的weight通过scale变换为QuantLinear里面的qweight
3. 保存模型的state_dict为文件bloom_quant.pt

推理流程：

代码在：[bloom_inference.py][6]

1. 通过模型的config创建BloomForCausalLM的实例model
2. 替换model里面的Linear层为QuantLinear
3. 调用load_state_dict加载bloom_quant.pt
4. 完成加载后，实际推理时QuantLinear会调用cuda kernel算子：VecQuant4MatMulKernel进行量化计算

VecQuant4MatMulKernel的实现：

```cpp
template <typename scalar_t>
__global__ void VecQuant4MatMulKernel(
    const  scalar_t* __restrict__ vec,  // 输入
    const       int* __restrict__ mat,  // 权重
           scalar_t* __restrict__ mul, // 输入
    const  scalar_t* __restrict__ scales, // scales
    const       int* __restrict__ zeros, // zeros
    int batch,
    int vec_height,
    int height,
    int width,
    int zero_width,
    int groupsize
) {
  int b = blockIdx.z;
  int h = BLOCKHEIGHT4 * blockIdx.x;
  int w = BLOCKWIDTH * blockIdx.y + threadIdx.x;

  __shared__ scalar_t blockvec[BLOCKWIDTH];
  blockvec[threadIdx.x] = vec[b * vec_height + blockIdx.x * BLOCKWIDTH + threadIdx.x];
  __syncthreads();

  scalar_t res = 0;
  int i = width * h + w;
  int g_h = h * 8;
  int k = 0;

  int z_w = w / 8; 
  int z_mod = (w % 8) * 4;

  unsigned int tmp;

  while (k < BLOCKWIDTH) {
    tmp = as_unsigned(mat[i]);
	
    int g = (g_h + k) / groupsize;
    scalar_t scale = scales[g * width + w];
    scalar_t zero = scale * scalar_t(((as_unsigned(zeros[g * zero_width + z_w]) >> z_mod) & 0xF) + 1);
	
    res += (scale * scalar_t((tmp >> 0) & 0xF) - zero) * blockvec[k + 0];
    res += (scale * scalar_t((tmp >> 4) & 0xF) - zero) * blockvec[k + 1];
    res += (scale * scalar_t((tmp >> 8) & 0xF) - zero) * blockvec[k + 2];
    res += (scale * scalar_t((tmp >> 12) & 0xF) - zero) * blockvec[k + 3];
    res += (scale * scalar_t((tmp >> 16) & 0xF) - zero) * blockvec[k + 4];
    res += (scale * scalar_t((tmp >> 20) & 0xF) - zero) * blockvec[k + 5];
    res += (scale * scalar_t((tmp >> 24) & 0xF) - zero) * blockvec[k + 6];
    res += (scale * scalar_t((tmp >> 28) & 0xF) - zero) * blockvec[k + 7];
	
    i += width;
    k += 8;
  }

  atomicAdd(&mul[b * width + w], res);
}
```

## 问答

### 0. 量化后精度损失如何

经过评测精度损失可以接受


### 1. 量化了哪些算子
   
量化了Transfromer decoder层的四个算子：
* self_attention.dense
* self_attention.query_key_value
* mlp.dense_4h_to_h
* mlp.dense_h_to_4h

### 2. 能降低多少显存

采用int4量化，groupsize=128，相比于fp16：

* 4个算子的权重，从fp16降低到int4, 降低3/4
* 每个算子会增加scale:  1/128*2
* 每个算子会增加zero:  1/128/*2/8

 7b下decoder层占比：85%，175b下占比：98%

所以：
* 7B共降低：0.75 - 0.0175 = 0.7325 * 12/14 ~ 63%
* 175B共降低：0.7325 * 0.98 ~ 72%

### 3. 算子是否采用了INT4计算
      
  没有，只是权重进行了量化，目前计算用的是fp32.

### 4. 性能有提升吗

 性能基本没有提升，测试参数：bloom 7b模型 单卡 int4量化，max_lenght 1024, do_sample=False, 使用bloom_inference.py对比测试：


| bloom 7b	| fp16	| int4 |
| ------| ----- | --------- |
| 文件大小	| 13.2G | 	5.0G | 
| 显存占用（初始化完成）| 	13.9	| 7.8G | 
| 显存占用（峰值）| 	19.8	| 15.4G | 
| GPU算力	| 70%	| 50% | 
| 速度（token/秒）| 	53	| 35 | 

从结果看，略有下降，后续需优化算子提升性能。

传送门：[GPTQ 落地与优化][7]

[1]: https://huggingface.co/docs/optimum/concept_guides/quantization
[2]: https://github.com/LianjiaTech/BELLE/blob/main/models/gptq/gptq.py
[3]: https://intellabs.github.io/distiller/algo_quantization.html
[4]: https://pytorch.org/docs/stable/generated/torch.nn.Linear.html
[5]: https://github.com/LianjiaTech/BELLE/blob/main/models/gptq/bloom.py
[6]: https://github.com/LianjiaTech/BELLE/blob/main/models/gptq/bloom_inference.py
[7]: /blog/2023/06/19/gptq2


参考：
https://oldpan.me/archives/how-to-quan-1