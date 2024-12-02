---
title: "Ft Pd Dist"
date: 2024-12-02T22:12:59+08:00
draft: true
---

# 为什么要PD分离

大模型推理过程分成2个阶段：

1. Prefill阶段，处理请求prompt。
2. Decode阶段，生成文本。

在单batch的场景下，这2个阶段顺序执行，互不干扰。但单batch吞吐很低，为提高吞吐，我们支持了[continuous batch](/blog/2023/10/10/ft-debug/), 这导致在Decode阶段会和Prefill阶段同时执行，互相干扰。Prefill是计算密集型操作，抢占了Decode的资源，拖累了生成的速度，造成了2个问题：
1. 速度不稳定。新的请求进来，生成速度会变慢，用户有卡顿感。
2. 吞吐上不去。为了保证Decode速度，QPS不能高，导致吞吐上不去。

如何减少Prefill对Decode的干扰呢，业界实践中有3种方案：
1. 前缀缓存。 通过前缀缓存可以减少Prefill对Decode的干扰，见[前缀缓存优化](/blog/develop/ft-perfix-caching/)
2. 前缀切分。也就是业界说的[Chunked Prefill](https://arxiv.org/abs/2308.16369)通过将一个长Prompt切分成短Prompt，然后和Decode拼在一起处理，减少对Decode的影响。该方案有一个前提：大模型GPU推理在处理小于N个Token的耗时是基本恒定的，而我们的A30多卡70B模型不太满足这个前提，所以我们没有支持该方案。
3. PD分离。将Prefill和Decode分开，放到不同的机器（或显卡）上，实现物理的隔离，一劳永逸的解决干扰问题。


# 行业现状

23年11月份，微软发布了论文[Splitwise](https://arxiv.org/abs/2308.16369)，论证了PD分离的价值：在相同的硬件资源下，可以提升2.x倍的吞吐，但在方案细节上没有过多的介绍。

24年6月，月之暗面提出了[Mooncake](https://arxiv.org/abs/2407.00079)的架构，并表示该架构已经大规模在月之暗面落地，带来了较大的收益，在某些场景下可以提升5倍的吞吐。这无疑鉴定了我们实现PD分离的信心。MoonCake的论文突破性的提出了以kvcache为中心的优化思路，并披露了一些细节，有指路之功。

24年下半年各大厂都在或者已经支持PD分离，都是闭源的，思想类似，但实现应该各个不同。开源界VLLM作为大模型推理最活跃的项目，目前也正在支持PD分离，有贡献者正在合入[实现1](https://github.com/vllm-project/vllm/pull/8498)，[实现2](https://github.com/vllm-project/vllm/pull/9079)

自研PD分离在24年7月初启动，到9月底初步完成一个版本，其中借鉴了Mooncake的实现，也有自己的特色和基于现状的优化。

# 自研PD分离架构Features
* 异构的的Prefill和Decode分离，支持M:N配比。比如Prefill: 2*A100, Decoder: 1*英伟达A30 + 3*华为
* 自动前缀缓存，显存/内存2级存储，多轮对话自动加速。利用了闲置的内存资源：500G内存可以缓存160万token，约为GPU缓存的8倍。(70B八卡A30）
* 智能负载均衡，业务方无感知
* 节点间KVCache逐层传输，低性能损耗
* 一个引擎多个角色，引擎可以根据需求配置Prefill角色、Decode角色，单机模式

# 性能提升

## 测试环境：
* 模型： 70B模型
* 机器： 3台A30八卡机，PD分离采用2P1D的部署方式
* 测试方法： 关闭前缀缓存功能，测试不同长度下，极低、低、中、高四种QPS的情况

## 测试数据：

![性能数据](images/ceshi_data.jpg "性能数据")
<center>图-1</center>

> 注：以上的数据包含2个优化：1. AllReduce&Gemm Overlap的优化，在高QPS下首字耗时减少10%~50%。2. flashinfer pageattention优化，在输入>=1024的高QPS下decode速度增加30%。

## 性能提升总结（测试环境下）：
1. 解码速度的提升：在不同长度、不同QPS下均有提升，QPS越高提升越大，在高QPS下平均提升156%
2. 单机有效吞吐的提升30%：满足SLO的吞吐为有效吞吐，设[SLO](https://help.aliyun.com/zh/asm/user-guide/slo-overview)为
* 首字耗时<=1秒
* 解码速度>=20Token/秒
  
  比如图-1输入1024时的最大单机有效吞吐，PD分离方案是496.74/3=165.33, 未分离方案是381.45/3=127.15。
由此可知所有长度下最大有效吞吐均提升30%。
3. 性能瓶颈在Prefill节点，假如3P1D则有效吞吐可以提升到50%。

## 自动前缀缓存的收益

前缀缓存的第一个优势是可以降低首字耗时，可以简单认为首字耗时降低是和前缀缓存率等比例的，假如前缀缓存率50%，则首字耗时降低50%。

第二个优势是可以提高单机有效吞吐，假如前缀缓存率为50%，则上图的性能1P1D两台服务器即可达到，保持解码速度不变的同时，单机有效吞吐提升为100%。

不同业务前缀缓存率是不同的，比如意图的前缀缓存匹配率>90%。

## Prefill和Decode的配比

Prefill和Decode的配比很重要，不合理的配置会导致算力的浪费，可以用下面的方法初步评估配比，设：
* Prefill、Decode节点单机的处理能力为Max_Token_Prefill、Max_Token_Decode 
* 前缀缓存率为Prefill_Cache_Rate
* 输入输出个数为Max_Input_Token、Max_Output_Token

则理论配比为：
（MAX_Input_Token*Prefill_Cache_Rate/Max_Token_Prefill） / (Max_Output_Token/Max_Token_Decode)。

以上数据可以根据性能压测和线上监控获知。理论配比作为参考，实际要考虑节点带宽，SLO和Decode节点速度换吞吐的特性。

# 架构分析

## 整体架构
![架构图](images/main_arch.jpg "架构图")
<center>图-2</center>

## 模块介绍
* PD-Scheduler
独立部署的服务，无状态，其主要的功能是将一个请求调度到一对Prefill&Decode实例上。

* Client
每个请求会创建一个Client, Client会构造一个生成Task, 然后监听Task的状态，流式返回结果给调用方

* Batch调度器
将多个请求，根据当前的硬件的资源，放在一起组Batch。详见：[continuous batch](/blog/2023/09/02/continuous-batch/)

* KVCache Pool
按Page预分配好的KVCache Pool，存在于内存和显存上，会在引擎启动的时候预分配。任务需要的KVcache Table会由调度层动态的进行按需分配。

* Block-Manger
管理KVcache Pool中的页的使用。Batch调度器调用Block-Manger为Task申请和释放页。

* Block-Syncer
负责KVcache内存 <-> 显存双向的同步。

* Messenger
负责节点间消息通知和kvcache传输

* 前缀树
前缀树是Block-Manager的一部分，如果一个Task要申请的KVcache已经存在于前缀树中，则复用此部分KVcache, 对于Prefill阶段来说这将减少大量的计算。


## 整体流程
![整体流程图](images/main_flow.jpg "整体流程图")
<center>图-3</center>

# 技术详解

PD分离架构是简单的，但实现上有体现在广度上的复杂性，会涉及服务化，负载均衡，传输协议，推理调度策略，大模型推理流程，算子库，前缀机制，cuda编程等，要把这些东西串起来，细节非常多，难以下手。但如果我们把各个模块解耦开，则每个模块的实现又比较简单，接下来对重点模块逐一介绍。

## PD调度的策略
调度需要做到两点：
1. 为请求寻找最优的Prefill&Decode节点 
2. 为请求屏蔽不适宜的节点。

寻找最优节点的流程：
1. 实时获取节点的负载情况，Prefill节点的负载以running和waiting的Token个数表示，Decode节点的负载以running和waiting的请求数表示
2. 根据prompt查询所有节点的前缀缓存率
3. 根据负载和前缀缓存率加权排序，挑选最优的Prefill&Decode节点

屏蔽不适宜节点的条件：
1. 模型版本是否匹配
2. MAX_INPUT_TOKEN和MAX_TOTAL_TOKEN是否满足
3. Prefill&Decode的版本是否匹配

## 自动前缀缓存
前缀缓存通过减少Prefill的计算量，可以显著的减少首字耗时，之前我们的前缀主要是SYSTEM_PROMPT, 是通过手工配置的。而自动前缀缓存有以下优势：
1. 多轮对话加速。多轮对话生成Token可以自动缓存，下一轮请求可以自动匹配前几轮的前缀。
2. 请求间前缀复用。相同前缀复用同一页KVcache, 减少显存的浪费，减少计算量。
3. 减少配置的工作量。

### 前缀树

自动前缀缓存主要是做2件事：1. 前缀树的管理和匹配 2. 模型层适配前缀匹配信息

![Hash_ID](images/hash_id.jpg "Hash_id")
<center>图-4</center>

第2点的细节在[这里](/blog/develop/ft-perfix-caching/)，不再赘述。现在重点讲一下前缀树，其实现思想是参考VLLM，是实现了一种变相的、结合了PageAttention页机制的、基于hash_id的前缀树：
1. 每一个页都有唯一hash_id，每一页的hash_id是由该页及之前的所有Token取Hash得来的。比如图-4中，请求共有37个Token, 申请了3页，Page-0的hash_id由[T1-T16]得到，Page-1的Hash_id由[T1-T32]得到。
2. 如果一个页计算的hash_id相同，则表示其前缀是完全匹配的，其kvcache是可以复用的。
3. 前缀树等价于map[hash_id]，匹配前缀树只需要计算当前页的hash_id，然后去map中find。

原理简单，实现上首先是hash_id的算法选择，需满足：
1. 相同的tokens，必须要有相同的hashid
2. 速度要快，不能影响推理速度
3. 不同节点间，计算出的hashid要一致
4. 支持增量hash，非增量的hash算法时间复杂度是O(N^2)，在长文本场景会影响性能。

目前用的是[xxHash](https://github.com/Cyan4973/xxHash)中的XXH64算法，可以满足以上要求。基于hash_id的前缀树有实现简单的优点，但有hash冲突的理论风险，按XXH64 每100Gi次hash有300个[冲突](https://github.com/Cyan4973/xxHash/wiki/Collision-ratio-comparison)，按前面讲的160万个Token的Kvcache规模，大概会出现0.0003个冲突的hash_id, 暂时可以忽略不计，后续需要监控实际的冲突情况。

### 页的状态

前缀树按页的粒度来缓存前缀。我们定义一个完整页的概念：一个页被定义为最大容纳N（比如16）个Token, 已容纳N个Token的页为完整页，比如下图中的任务有3个页，前2个事完整页，最后一页是非完整页。图-4中Page-0和Page-1都是完整页，Page-2是非完整页。

![页的状态](images/page_state.jpg "页的状态")
<center>图-5</center>

页在Block-Manager中有3中状态：
1. freed。 页的初始状态，表示这一页没有被使用过，或者Task结束时释放的非完整页。
2. cached。被缓存且引用计数>=1的页，表示当前正在被Task使用的完整页。
3. evicted。被缓存且引用计数=0的页，来源是Task释放的完整页。

### 前缀匹配的逻辑

新请求的Token通过hash算法得到`num_token/num_page`个hash_id，然后去cached和evictord中查找有无匹配的hash_id，有则对该页引用计数加1，复用该页。注意前缀匹配必须从第0到k页完全匹配。

### 前缀cache更新的逻辑

Task每新增一个完整页则添加该页到cached中，若cache中已存在该hash_id则复用。Task结束时释放页，引用计数减1，若引用计数为0，则移入evictord中。

### 淘汰的策略

新申请的没有命中cache的页，首先会去freed中申请，如果freed中没有资源则从evicted中申请，evicted移除页的过程称为cache页的淘汰。淘汰的策略：
1. 更早时间的优先淘汰
2. 时间相同则淘汰hash_id对应num_token更多的

evicted的管理可以视为LRU, 有insert, find_by_hash_id, remove_oldest三个操作，最初参考vllm的实现，remove_oldest时进行一次排序操作，耗时较高O(N⋅log(M))。后优化为双端队列+hashmap，时间复杂度降低O(1)。

### 显存/内存两级缓存

两级缓存的作用：
1. 加大前缀缓存池。机器上有1T闲置的内存，利用起来可以存储更多的kvcache
2. 作为PD分离跨节点传输的中转，更简单的支持异构的显卡

显存&内存分别管理自己的前缀树，新请求分别查询内存和显存中缓存前缀页，并差量的将内存上的页拷贝到显存中。因为前缀缓存从内存拷贝到显存是一个耗时操作，只有拷贝的耗时小于计算的耗时才是有收益的。在我们的70B八卡A30中，拷贝一页的耗时为0.5ms，一页16个token计算耗时约7.20ms，所有总是有收益的。 

## 跨节点KVCache传输

PD分离后有2个KVCache传输的场景：
1. 首字阶段，需要将Prefill产生的首字KVCache传输到Decode，用于接下来的生成。传输的方案用的是逐层传输。
2. 生成阶段，需要将新生成Token的KVcache从Decode传输到Context, 用于后续下一轮对话的前缀缓存。传输方案是按页传输。

生成阶段的KVcache传输是异步的，是后续备用的，不会影响当次请求的性能。而首字阶段跨节点传输的KVCache是Decode亟需的，传输的损耗不过不处理好，会影响第二个Token的生成的耗时，如何减少这种损耗呢，逐层传输是最主要的方法。

### 逐层传输

![逐层传输](images/layer_wise.jpg "逐层传输")
<center>图-6</center>

模型是逐层计算的，每层有自己的KVCache, 我们可以每算完一层，则传输一层，设传输一层的耗时为TimeT, 计算一层的耗时为TimeC, 若TimeT<TimeC, 则总的损耗等于最后一层的TimeT。

以70B八卡A30实测数据说明，在1024的输入长度下，TimeC=5.6ms。TimeT可以分成三部分：
1.Prefill节点显存 -> 内存的拷贝的耗时TimeT_1=0.4ms
2.节点间网络传输的耗时TimeT_2=2.5ms
3.Decode节点内存 -> 显存的拷贝的耗时TimeT_3=0.4ms

满足TimeT < TimeC, 延迟是可以掩盖的，通过逐层传输，传输的耗时站首字耗时<1%, 传输不会成为瓶颈。

逐层传输实现上主要有2个点：
1.实时的获取显卡上面异步执行的每层完成的通知。这个基于cuda提供的Event机制来实现。
2.高效的内存 <-> 显存同步算子。因为页和层的关系，待同步的KVCache是非连续的地址，1024个Token的同步假如没有自定义的算子，需要调用5120次CudaMemcpy，大量的kernel launch是极其低效的。


#### 带宽要求

![带宽要求](images/bandwidth.jpg "带宽要求")
<center>图-7</center>

> 注：decoder_ratio指一个prefill节点对应的decode节点个数，目前一般在0.33到1.0之间。

### 内存-显存同步BlockSync

内存到显存KVcache的同步是非连续的小内存操作，调用系统提供的CudaMemcpy会有大量的kernel launch，性能非常差，所以我们实现了基于页的显存拷贝算子。经测试在各个size下，自研的拷贝算子性能均优于CudaMemcpy，平均提升10.3倍，达到单卡5226MBps。

思想非常简单，通过将非连续内存转换成连续内存的方式来减少大量的CudaMemcpy调用，如下面图-8演示了一个内存到显存的一次同步操作：
1. 内存上非连续内存到连续内存 (H2H)
2. 然后将连续内存通过CudaMemcpy拷贝到连续的显存（H2D）
3. 然后将连续的显存拷贝到非连续的显存（通过自定义kernel）（D2D）

![内存到显存拷贝优化](images/h2d.jpg "内存到显存拷贝优化")
<center>图-8</center>


针对逐层传输和按页传输，实现了2个算子：
1.按页拷贝，一次性同步num_pages页的所有层的kvcache。cuda kernel比较简单：
thread.x = num_pages * token_per_page。一个线程拷贝一个token一层的数据。
grid.x = num_layers。1个block拷贝1层所有token的数据。

2.按层拷贝。每次同步部分层的数据，每一层要拷贝的页不一定相同，采用的策略是分组的方案，将每一层要拷贝的页按每个layer_group<=16分组，比如要同步的数据：layer_0页[0,40], layer_1页[0,15], 则总共分成4个layer_group。
```
layer_0_group_0: [0,15]
layer_0_group_1: [16,31]
layer_0_group_2: [32,40]
layer_1_group_0: [0,15]
```
cuda kernel实现为：
thread.x = 16 * num_token_per_page。一个线程拷贝一个token一层的数据
grid.x = num_layer_group。一个block拷贝一个layer_group的数据。

除了自定义算子，我们还进行了拼batch和多流的优化。我们会在数据量>40M时（约等于70B 8卡，按页拷贝token>1024，按层拷贝layer_group>320)时启用多流，经测试多流可以显著提高传输的性能，80M时可以提升27%，640M时可以提升63%。

![内存到显存拷贝优化-多流](images/h2d_streams.jpg "内存到显存拷贝优化-多流")
<center>图-9</center>


### Task的事件机制

为了实现PD分离，节点内和节点间增加了很多交互，叠加多线程的问题，代码的复杂度一下子高了很多，所以我们通过事件机制来简化之:

1. 代码更清晰
2. 不用考虑多线程加锁

![事件机制](images/event.jpg "事件机制")
<center>图-10</center>


### AllReduce & Gemm Overlap

系统瓶颈在PD分离之后发生了变化，一些不明显的瓶颈在独立的子系统中凸显出来。Prefill下面的AllReduce就是一个新的瓶颈，在A30 8卡 70B输入长度1024时，AllReduce耗时占比高达48%。AllReduce是一个通信操作，耗时占比大表明着算力的浪费。如何优化这个问题呢，有2个思路：
1.减少AllReduce的耗时。AllReduce是基于英伟达的Nccl库实现的，这个库是追求带宽的，而大模型推理是追求延迟的，数据量较小，有一定优化空间。开源界有一些针对大模型推理进行优化的AllReduce库。实测下来再A100下面会有一定提升，在A30下面提升不明显。
2.进行AllReduce&Gemm的Overlap。通过多流在进行AllReduce的时候同时进行Gemm，将浪费的算力利用起来。

我们实现了方案2，在长文本或者高Qps时，首字耗时可以减少10%~50%。

