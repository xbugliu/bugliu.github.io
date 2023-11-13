---
title: "大模型推理 - FasterTransformer调试技巧"
date: 2023-10-10T20:39:55+08:00
draft: false
tags: ["大模型推理","FasterTransformer"]
categories: ["开发"]
---


FasterTransformer(FT)有三难，一个kernel算子理解难，一个是对精度麻烦，再一个就是调试难，今天讲一下FasterTransformer调试的技巧。

## 如何编译DEBUG版本

DEBUG版本很有用，gdb调试时会有更多信息。但FT 通过`CMake  -DCMAKE_BUILD_TYPE=DEBUG`编译DEBUG版本会[报错][1]，可以采用如下方式，修改CMakeLists.txt:

```
# 找到： set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")，然后替换为：

if(FT_BUILD_DEBUG)
  message("build debug is true")
  set(CMAKE_CXX_FLAGS_RELEASE "-O0 -g")
else()
  message("build debug is false")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()
```

用`cmake -DCMAKE_BUILD_TYPE=Release -DFT_BUILD_DEBUG=1` 即可编译DEBUG版本。

## 如何定位莫名其妙的Cuda错误

假如发生如下类似报错：
```
[FT][ERROR] CUDA runtime error: an illegal memory access was encountered /data/src/fastertransformer/utils/memory_utils.cu:153
```
此时通过日志是无法定位的，通过gdb调试看到的调用栈往往也发现不了问题所在，这是因为FT里面的Cuda  kernel大部分是异步的，程序的报错已经不是第一次现场, FT内部实现了一个`syncAndCheck`函数用于等待cuda kernel调用并检测错误，在调用kernel后跟一个syncAndCheck，可以第一时间发现错误。

Release版本默认syncAndCheck是不打卡的，可以通过`FT_DEBUG_LEVEL=DEBUG`打开。
效果如下：

```
terminate called after throwing an instance of 'std::runtime_error'
  what():  [FT][ERROR] CUDA runtime error: an illegal memory access was encountered /data/src/fastertransformer/layers/attention_layers/GptPagedMQAttentionLayer.cc:244 

[a4adc150d095:52152] *** Process received signal ***
[a4adc150d095:52152] Signal: Aborted (6)
[a4adc150d095:52152] Signal code:  (-6)
[a4adc150d095:52152] [ 0] /usr/lib/x86_64-linux-gnu/libpthread.so.0( 0x14420)[0x7f1121301420]
[a4adc150d095:52152] [ 1] /usr/lib/x86_64-linux-gnu/libc.so.6(gsignal 0xcb)[0x7f1112d5d00b]
[a4adc150d095:52152] [ 2] /usr/lib/x86_64-linux-gnu/libc.so.6(abort 0x12b)[0x7f1112d3c859]
[a4adc150d095:52152] [ 3] /usr/lib/x86_64-linux-gnu/libstdc.so.6( 0x9e911)[0x7f1113114911]
[a4adc150d095:52152] [ 4] /usr/lib/x86_64-linux-gnu/libstdc.so.6( 0xaa38c)[0x7f111312038c]
[a4adc150d095:52152] [ 5] /usr/lib/x86_64-linux-gnu/libstdc.so.6( 0xaa3f7)[0x7f11131203f7]
[a4adc150d095:52152] [ 6] /usr/lib/x86_64-linux-gnu/libstdc.so.6( 0xaa6a9)[0x7f11131206a9]
[a4adc150d095:52152] [ 7] /data/build/lib/libtransformer-shared.so(fastertransformer::syncAndCheck(char const*, int) 0x481)[0x7f11137a33f1]
[a4adc150d095:52152] [ 8] /data/build/lib/libtransformer-shared.so(fastertransformer::GptPagedMQAttentionLayer<__half>::forward(fastertransformer::TensorMap*, fastertransformer::TensorMap*, fastertransformer::AttentionWeight<__half, __half> const*, fastertransformer::batch::WorkerInputMetaData<__half>*) 0x4f9)[0x7f1114087929]
[a4adc150d095:52152] [ 9] /data/build/lib/libtransformer-shared.so(fastertransformer::TensorParallelGptContextMQAttentionLayer<__half>::forward(fastertransformer::TensorMap*, fastertransformer::TensorMap*, fastertransformer::AttentionWeight<__half, __half> const*, fastertransformer::batch::WorkerInputMetaData<__half>*) 0x4a8)[0x7f111406ce48]
```

这样我们就可以确定问题的大概位置，然后检查相关代码定位问题。

## 如何定位kernel内部显存问题

通过syncAndCheck能找到出问题的kernel函数，但在kernel实现复杂的情况下，还是会一筹莫展。比如有一个kernel函数有十几个buf参数，实现也很复杂，如何知道哪一行的问题呢？此时就需要用到[compute-sanitizer][3]了。

首先修改CMakeLists.txt,  让nvcc编译行号：
```
 target_compile_options(paged_attention_kernels PRIVATE $<$<COMPILE_LANGUAGE:CUDA>: --generate-line-info >)
```
然后通过compute-sanitizer启动你的程序：
```
/usr/local/cuda/bin/compute-sanitizer your_server args
```

compute-sanitizer会将结果输出到终端：
```
========= Invalid __global__ write of size 2 bytes
=========     at 0xd80 in /data/src/fastertransformer/kernels/paged_attention/paged_attention_kernels.cu:162:void add_fusedQKV_bias_transpose_and_cache_kernel_rotary_embedding<(bool)1, __half>(T2 *, T2 *, T2 *, T2 *, T2 *, T2 *, const T2 *, int, int, const int *, const int *, const int *, const int *, int, int, int, int)
=========     by thread (63,0,0) in block (506,0,0)
=========     Address 0x7fcd07a0312c is out of bounds
=========     and is 12589 bytes after the nearest allocation at 0x7fcd079ffc00 of size 1024 bytes
=========     Saved host backtrace up to driver entry point at kernel launch time
=========     Host Frame: [0x2ea231]
=========                in /usr/lib/x86_64-linux-gnu/libcuda.so.1
=========     Host Frame: [0x1488c]
=========                in /usr/local/cuda/lib64/libcudart.so.11.0
=========     Host Frame:cudaLaunchKernel [0x6c318]
=========                in /usr/local/cuda/lib64/libcudart.so.11.0
=========     Host Frame:void invokeAddFusedQKVBiasTransposeAndCacheRotary_embedding<__half>(__half*, __half*, __half*, __half*, __half*, __half*, __half const*, int, int, int, int, int const*, int const*, int const*, int const*, int, int, int, int, CUstream_st*) [0xd3ab7a]
=========                in /data/build/lib/libtransformer-shared.so
=========     Host Frame:fastertransformer::GptPagedMQAttentionLayer<__half>::forward(fastertransformer::TensorMap*, fastertransformer::TensorMap*, fastertransformer::AttentionWeight<__half, __half> const*, fastertransformer::batch::WorkerInputMetaData<__half>*) [0xda3914]
=========                in /data/build/lib/libtransformer-shared.so
=========     Host Frame:fastertransformer::TensorParallelGptContextMQAttentionLayer<__half>::forward(fastertransformer::TensorMap*, fastertransformer::TensorMap*, fastertransformer::AttentionWeight<__half, __half> const*, fastertransformer::batch::WorkerInputMetaData<__half>*) [0xd88e48]
=========                in /data/build/lib/libtransformer-shared.so

```

上面的结果中有3个关键信息：
* 代码行号。 如上面的paged_attention_kernels.cu的第162行
* 异常类型。 write错误 out of bounds.
 * 调用栈。

根据这些信息可以精准的定位Bug原因。

## 常见问题

*  NCCL卡住

多卡触发，卡越多概率越大，表现为1. 通过nvidia-smi查看多卡中某些卡的util占用为0，其余为100%，但功率都很低 2. 通过gdb调试会发现阻塞在cudaMallocHost, cublasGemmStridedBatchedEx等Cuda函数

解决方法：export NCCL_LAUNCH_MODE=GROUP

* 启动报错: Caught signal 7 (Bus error: nonexistent physical address)

原因是系统资源[分配不足][2], 容器启动添加`--shm-size=10g`即可解决。




[1]: https://github.com/NVIDIA/FasterTransformer/issues/590
[2]: https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/troubleshooting.html#sharing-data
[3]: https://docs.nvidia.com/compute-sanitizer/ComputeSanitizer/index.html

