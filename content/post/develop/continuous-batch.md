---
title: "Continuous batch架构剖析"
date: 2023-09-02T12:24:53+08:00
draft: true
---

## 什么是continuous batch

介绍continuous batch之前，先说下Batch。Batch将多个请求合并一次处理，是提升GPU推理吞吐量(throughput)的一种方法。其原理有三：
1. 并发提升，提升了GPU核的利用率
2. 显卡带宽&算力的复用，比如矩阵乘多batch时权重的带宽复用
3. Kernel Launch次数减少，有效载荷提升

传统的Batch是模型粒度的，将多个请求合并在一起，然后调用模型的推理接口，完成后在解batch，将结果返回给各个请求。这种方式对于大语言模型推理并不适用，因为大模型的推理有2个特点：
1. 生成长度不可知
2. 一个请求的时间可能很长

这2个特点导致，传统的Batch方案会导致极大的算力浪费，系统大部分时间都在处理较长的请求，如图所示：


各个框架都注意到这个问题，23年5月 TGI提出了[Continuous batching][3]的解决方案，后vllm 实现了PagedAttention更加强了Continuous batching的能力。除了Continuous batching，lmdeploy里面的[Persistent Batch][4]，TensorRT里面的Inflight Batch都是相似的思路。

Continuous batching是专门提升大语言模型吞吐量的一种组batch方式，相比于传统的模型粒度的组batch，是在迭代阶段(生成token)组batch，完美的规避了请求长短不一的问题。如图所示：


除了batch，有些同学询问是否可以通过多并发来提升大模型吞吐量，回答是提升有限。前面batch提升吞吐量的三个原理，多并发只对第一个原理有帮助，而在大模型推理领域，原因2带宽也是很关键的。

另外需要注意吞吐量和延迟(latency)是矛盾的，Batch在提升吞吐量的同时，往往伴随着延迟的增加。

## 基于FasterTransfromer的Continuous batch

FasterTransfromer（简称FT）是英伟达开源的针对transformer结构的加速引擎，在单batch场景下有非常优秀的表现，但只支持普通batch, 且有诸多限制，所以早在VLLM以前我们就计划优化FasterTransfromer的batch。正好vllm的成功给了我们启发和借鉴。

有人会说Vllm已足够优秀，基于FasterTransformer新造轮子是否有意义？ 答案是非常有必要，就像简单的一道西红柿炒蛋，食材一样，但锅具不同，厨师各异， 味道天地之别。在大模型GPU推理上，为性能计，C++这口锅显然优于Python（vllm基于python），加上我们针对业务场景优化，性能超越Vllm是水到渠成的。看一下数据：


为了对比的公平性，移除了vllm里面的tokenizer，放入测试脚本中。对比vllm，FT实现有如下优化：

1. 无python的GIL问题，观察vllm, batch>16时cpu占用率100%，并发越高，cuda util越低，GIL的问题非常明显
2. 预分配显存内存 在服务启动时预分配资源，减少推理过程中的动态开销。
3. 更高效的算子，比如qkv拆分+保存cache+bias融合为一个算子，更快的[sample算子][1]
4. 调度层细节优化  比如 一、获取结果用内核信号量代替vllm里面的map查询，二、仅任务变更时才刷新mask,  三、alibi全局计算一次 等等

当然我们是参考vllm实现了这些功能，有后发优势，vllm中依然有一些优秀的feature:  flashattention, tokenizer增量解码等值得借鉴。

## PagedAttention

在PagedAttention以前，kv cache一般基于output_length预分配的，output_lenght往往很大，导致显存占用很大，比如175B output_lenght=1024时显存占用3.9G。但实际生成的output_lenght往往很小，导致显存的巨大浪费。显存的浪费导致，导致无法组很大的batch, 吞吐量上不去。

所以vllm发明了PagedAttention，减少kv cache显存浪费，从而可以配合continuous batch提高batch数，提升吞吐量。核心原理如图：


keycache和valuecache是独立存在的，以keycache说明
1. cache申请为连续的显存
2. 逻辑上上以页来划分，每个页有Block_size个槽位(slot), 每个槽位对应一个Token
3. 会话在生成过程中，动态按页申请显存。每个会话维护一个页表
4. 推理时将页表信息传入MHA算子，即为PagedAttention。

这里可以看出PagedAttention的优势是按页分配无浪费，但引入了一个问题，页是动态申请的，会话进行到一半申请不到页怎么办？这个就是后面调度要解决的了。

## 架构解析


[1]:https://github.com/vllm-project/vllm/issues/249
[2]: https://kipp.ly/transformer-inference-arithmetic/
[3]: https://github.com/huggingface/text-generation-inference/tree/main/router#continuous-batching
[4]: https://github.com/InternLM/lmdeploy/blob/main/docs/zh_cn/turbomind.md#persistent-batch
