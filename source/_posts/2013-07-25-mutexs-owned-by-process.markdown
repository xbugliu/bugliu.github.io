---
layout: post
title: "获取进程打开的互斥量"
date: 2013-07-25 21:36
comments: true
categories: C++ Windows
keywords: mutex,handle,process,互斥量,内核对象,句柄,线程
description: 获取指定进程打开的互斥量的一种方法,也适用于检索其它内核对象
---
最近有个任务涉及到区分【同进程名进程】，所以想到了用进程"拥有"的互斥量来区分这些进程。所以下面实现了获取指定进程"拥有"哪些互斥量的方法，当然这种方法也适合各种内核对象（FILE、REG...)。

    1. 使用NtQuerySystemInformation检索SystemHandleInformation(16)即可获得系统中所有的句柄信息：
通过SystemHandleInformation检索到的系统中所有句柄的数据结构是这样定义的：
{% codeblock lang:cpp %}
typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG HandleCount; 
	SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;
{% endcodeblock%}

其中SYSTEM_HANDLE_TABLE_ENTRY_INFO是一个句柄信息的数据结构
{% codeblock lang:cpp %}
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
	ULONG ProcessId;
	BYTE ObjectTypeNumber;
	BYTE Flags;
	USHORT Handle;
	PVOID Object;
	ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;
{% endcodeblock%}

    2. 将句柄复制到当前进程
想要获取一个句柄的详细信息，必须将其拷贝到当前进程，对于一个句柄数据类型：SYSTEM_HANDLE_TABLE_ENTRY_INFO，我们可以使用其第四个成员Handle，然后使用DuplicateHandle将这个句柄复制到当前进程。

    3. 获取句柄的类型信息
由2中获取的复制到当前进程的句柄，调用函数NtQueryObject，指定获取ObjectNameInformation(1)即可获取句柄的类型信息，获取到的句柄的类型信息结构是这样的：
{% codeblock lang:cpp %}
typedef struct _OBJECT_TYPE_INFORMATION
{
	UNICODE_STRING TypeName;
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG TotalPagedPoolUsage;
	ULONG TotalNonPagedPoolUsage;
	ULONG TotalNamePoolUsage;
	ULONG TotalHandleTableUsage;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	ULONG HighWaterPagedPoolUsage;
	ULONG HighWaterNonPagedPoolUsage;
	ULONG HighWaterNamePoolUsage;
	ULONG HighWaterHandleTableUsage;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccessMask;
	BOOLEAN SecurityRequired;
	BOOLEAN MaintainHandleCount;
	ULONG PoolType;
	ULONG DefaultPagedPoolCharge;
	ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;
{% endcodeblock%}
其中第一个成员TypeName即是句柄类型的类型名

    4. 获取句柄的名字
调用函数NtQueryObject，指定获取ObjectNameInformation即可获取句柄的类型信息，获取到的句柄的名字数据结构是UNICODE_STRING类型。

    5. 找到当前进程占有的互斥量
由获取的句柄的类型信息和句柄所在的进程ID，即可找到当前进程拥有的互斥量，同时我们也得到了互斥量的名字。

###完整代码如下：
{% include_code  cpp/get_process_mutex.cpp %}