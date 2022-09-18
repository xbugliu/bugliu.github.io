+++
title = "Windbg定位内存泄露的一种简单方法"
date = "2014-11-22"
slug = "2014/11/22/windbg-memory-leak"
tags =["windgb","内存泄露","tadb"]
description = "Windbg定位内存泄露的一直简单方法"
categories = ["开发"]
+++

前两天接到一个反映进程内存占用过G的投诉。问题是必現的，一定是内存泄露，应该容易定位，一同事远程看过现场，使用gflags和windbg试图找到泄露的堆栈，同事是一步步按照[这篇文章的方法][1]来的，但在最后一步Windbg没有找到出问题的堆栈，用户给我们远程的时间很短，无法深究gflags+windbg不灵验的原因，只得另辟蹊径。

## 步骤如下：

### 0. 安装windbg, 设置symbols, 用windbg attach到发生内存泄露的进程

### 1. 打印出heap的使用情况

```bash

0:003> !heap -s
LFH Key : 0x7ce97b7b
LFH Key : 0x7ce97b7b
Termination on corruption : ENABLED
Heap     Flags    Reserv Commit  Virt   Free   List    UCR    Virt  Lock Fast 
                  (k)    (k)     (k)    (k)    length  blocks cont. heap 
-----------------------------------------------------------------------------
002c0000 00000002 1024    372     1024   54    13      1      0     0    LFH
00010000 00008000 64      4       64 2   1     1       0      0  
00020000 00008000 64      64      64     62    1       1      0     0 
004d0000 00001002 1088    152     1088   7     4       2      0     0    LFH
007c0000 00001002 1088    188     1088   18    7       2      0     0    LFH
00880000 00001002 1280    276     1280   14    5       2      0     0    LFH
01db0000 00001002 64      12      64     2     3       1      0     0 
021f0000 00001002 15488   12024   15488  144   7       5      0     0    LFH
00810000 00001002 64      12      64     2     3       1      0     0 

```

很明显这一行：**021f0000 00001002 15488   12024   15488  144   7       5      0     0    LFH**是有异常的。

### 2. 显示异常heap的信息

```
0:003> !heap -stat -h 021f0000
heap @ 021f0000
heap @ 021f0000
group-by: TOTSIZE max-display: 20
  size   #blocks  total  ( %) (percent of total busy bytes)
  a45c   11d   -  b6fa6c (99.75)
  75a8   1     -  75a8   (0.25)
  20     1     -  20     (0.00)
```

上面11d块size为a45c的内存极有可能是泄露的内存。

### 3. 根据泄露内存的Size找到CallStack
```
0:003> bp ntdll!RtlAllocateHeap "j(poi(@esp+c) = 0x0a45c) 'k';'gc'"
0:003> g
0:003> g
Unable to deliver callback, Unable to deliver callback, 3131

ChildEBPChildEBP RetAddrRetAddr


0021de54 1000ba7e 0021de54 1000ba7e ntdll!RtlAllocateHeapntdll!RtlAllocateHeap


WARNING: Stack unwind information not available. Following frames may be wrong.
WARNING: Stack unwind information not available. Following frames may be wrong.
0021de6c 1000bbcc 0021de6c 1000bbcc mfnspstd32mfnspstd32++0xba7e0xba7e


0021de8c 1000beb1 0021de8c 1000beb1 mfnspstd32mfnspstd32++0xbbcc0xbbcc


0021deb8 1000ea4d 0021deb8 1000ea4d mfnspstd32mfnspstd32++0xbeb10xbeb1


0021dee4 75de9986 0021dee4 75de9986 mfnspstd32!WSPStartupmfnspstd32!WSPStartup++0x9d0x9d


0021e3b8 75de975b 0021e3b8 75de975b WS2_32!DPROVIDER::InitializeWS2_32!DPROVIDER::Initialize++0x1850x185


0021e3d8 75df5a2f 0021e3d8 75df5a2f WS2_32!DCATALOG::LoadProviderWS2_32!DCATALOG::LoadProvider++0x6d0x6d


0021e678 75df5fe8 0021e678 75df5fe8 WS2_32!DCATALOG::FindIFSProviderForSocketWS2_32!DCATALOG::FindIFSProviderForSocket++0x630x63


0021e68c 75de4204 0021e68c 75de4204 WS2_32!DSOCKET::FindIFSSocketWS2_32!DSOCKET::FindIFSSocket++0x370x37


0021e6cc 00d48444 0021e6cc 00d48444 WS2_32!setsockoptWS2_32!setsockopt++0xb00xb0


0021e6ec 00d4900c 0021e6ec 00d4900c t**b!OPENSSL_Applinktadb!OPENSSL_Applink++0x7c140x7c14


0021e71c 00d3e50c 0021e71c 00d3e50c t**b!OPENSSL_Applinktadb!OPENSSL_Applink++0x87dc0x87dc


0021e738 00d4a654 0021e738 00d4a654 t**b++0x4e50c0x4e50c


0021e768 00d44f79 0021e768 00d44f79 t**b!OPENSSL_Applinktadb!OPENSSL_Applink++0x9e240x9e24


0021f8c8 00d4a6a4 0021f8c8 00d4a6a4 t**b!OPENSSL_Applinktadb!OPENSSL_Applink++0x47490x4749


0021f8d8 00cfb1f7 0021f8d8 00cfb1f7 t**b!OPENSSL_Applinktadb!OPENSSL_Applink++0x9e740x9e74


0021f91c 7700ee1c 0021f91c 7700ee1c t**b++0xb1f70xb1f7


0021f928 7731377b 0021f928 7731377b kernel32!BaseThreadInitThunkkernel32!BaseThreadInitThunk++0xe0xe


0021f968 7731374e 0021f968 7731374e ntdll!__RtlUserThreadStartntdll!__RtlUserThreadStart++0x700x70


0021f980 00000000 0021f980 00000000 ntdll!_RtlUserThreadStartntdll!_RtlUserThreadStart++0x1b0x1b
```

### 4. 最后甄别CallStack是否真正的发生内存泄露

## 总结

此方法适宜，泄露亦重现，且泄露的size固定的情况


[1]:http://www.codeproject.com/Articles/31382/Memory-Leak-Detection-Using-Windbg
