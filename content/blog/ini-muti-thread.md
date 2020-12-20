+++
title = "Ini Muti Thread"
date = "2013-12-17"
slug = "2013/12/17/ini-muti-thread"
tags =["writeprivateprofilestring 线程安全","writeprivateprofilestring 多线程","INI 多线程"]
description = "writeprivateprofilestring函数不是线程安全"
+++

INI是Windows系统下人们喜闻乐见的一种配置存储方式。Windows提供了一套简单的接口操作INI文件，但它们并不是线程安全的，对于这一点，这些函数比如[WritePrivateProfileString][1]的文档中并没有提到。
据[这篇文章][2]介绍:

##WritePrivateProfileString:

* WritePrivateProfileString内部使用NtCreateFile访问文件，共享方式设置为：FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE。使用NtLockFile，FailImmediately设置为False，ExlusiveLock设置为True来锁定文件。
* 这意味着WritePrivateProfileString是非线程安全的，是进程安全的（非远程机器）。

我们目前的软件有大量的并发操作INI的行为，没出现过什么问题只能说是幸运了（亦或是不幸）。同事并不太相信以上结论，让我们用事实证明，写一段并发操作INI的代码：

```cpp
#include "stdafx.h"
#include <Windows.h>
#include <vector>
#include <string>
#include <process.h>
#include <cassert>

std::wstring GetIniName()
{
	std::wstring strIniName;
	strIniName.resize(MAX_PATH);
	strIniName.resize(GetModuleFileName(NULL, const_cast<TCHAR*>(strIniName.data()), strIniName.size()));
	strIniName += _T(".ini");
	return strIniName;
}

void WriteIniInThread(void* pText)
{
	const TCHAR *pSec = (TCHAR*)pText;
	assert(pSec);

	std::wstring strVal;
	std::wstring strIni = GetIniName();
	for (int i=0; i<1000; ++i)
	{
		strVal = std::to_wstring(_Longlong(i));
		WritePrivateProfileString(pSec, strVal.data(), strVal.data(), strIni.data());
	}

}

int _tmain(int argc, _TCHAR* argv[])
{
	std::vector<std::wstring> threadTexts;
	std::vector<HANDLE> threadHandles;
	for (int i = 0; i<60; ++i)
	{
		std::wstring strText = _T("Thread");
		strText += std::to_wstring(_Longlong(i));
		threadTexts.push_back(strText);
		threadHandles.push_back(HANDLE(_beginthread(WriteIniInThread, 0, (void*)threadTexts[i].data())));
	}

	WaitForMultipleObjects(threadHandles.size(), threadHandles.data(), TRUE, INFINITE);
	return 0;
}

```
代码很简单，开60个线程同时往一个INI文件里写东西，让我们对比一下多线程操作INI和非多线程操作的结果，左侧是不使用多线程操作的结果（只贴出前50行）：

{% img pull-left /images/posts/ini-muti-thread/result.png %}

{% img pull-left /images/posts/ini-muti-thread/error_result.png %}

</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>
</br>

结果一目了然。（多线程同时操作INI时，每次的结果可能都不一样的）。

--------------------------------------------------------------------------------------
[1]: http://msdn.microsoft.com/en-us/library/windows/desktop/ms725501(v=vs.85).aspx
[2]: http://mfctips.com/tag/getprivateprofilestring/
