+++
title = "De Elevate Start Process"
date = "2013-07-31"
slug = "2013/07/31/de-elevate-start-process"
tags =["process","uac","token","进程"]
description = "管理员权限进程创建非管理员权限进程"
+++
Win7下有些进程需要以管理员权限启动，比如安装程序。但又需要这个具有管理员权限的进程启动一个非管理员权限的进程。要实现这一点，方法很简单，首先得到受限的Token，然后由这个Token调用[CreateProcessAsUser][1]。

<pre><code>

#include "stdafx.h"
#include &lt;Windows.h&gt;
#include &lt;UserEnv.h&gt;
#include &lt;WinBase.h&gt;
#include &lt;WinSafer.h&gt;

#pragma comment(lib, "Userenv.lib")

// 获取不含管理员权限的Token
HANDLE CreateNormalUserToken()
{
	SAFER_LEVEL_HANDLE hLevel = NULL;
	if (!SaferCreateLevel(SAFER_SCOPEID_USER, SAFER_LEVELID_NORMALUSER, SAFER_LEVEL_OPEN, &hLevel, NULL))
	{
		return NULL;
	}

	HANDLE hRestrictedToken = NULL;
	if (!SaferComputeTokenFromLevel(hLevel, NULL, &hRestrictedToken, 0, NULL))
	{
		hRestrictedToken = NULL;
	}
	SaferCloseLevel(hLevel);
	return hRestrictedToken;
};

// 从管理员进程，创建非管理员进程
BOOL DeElevateStartProcess(TCHAR* cmd)
{
	const HANDLE hRestToken = CreateNormalUserToken();
	if (NULL == hRestToken)
	{
		return FALSE;
	}

	DWORD dwCreationFlags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE;
	LPVOID pEnv = NULL;
	if(CreateEnvironmentBlock(&pEnv, hRestToken, TRUE))
	{
		dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
	}
	else
	{
		pEnv = NULL;
	}

	PROCESS_INFORMATION pi;
	STARTUPINFO si;	
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb= sizeof(STARTUPINFO);
	si.lpDesktop = _T("winsta0\\default"); //default input desktop for the interactive window station
	ZeroMemory(&pi, sizeof(pi));

	const BOOL bResult = CreateProcessAsUser(hRestToken,    // client's access token
						NULL,                   // file to execute
						cmd,      // command line
						NULL,                   // pointer to process SECURITY_ATTRIBUTES
						NULL,                   // pointer to thread SECURITY_ATTRIBUTES
						FALSE,                  // handles are not inheritable
						dwCreationFlags,        // creation flags
						pEnv,                   // pointer to new environment block
						NULL,                   // name of current directory
						&si,                    // pointer to STARTUPINFO structure
						&pi                     // receives information about new process
						);
	if (bResult)
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	if (pEnv)
	{
		DestroyEnvironmentBlock(pEnv);
	}
	CloseHandle(hRestToken);
	return bResult;
}

int _tmain(int argc, _TCHAR* argv[])
{
	TCHAR cmd[] = _T("notepad.exe");
	DeElevateStartProcess(cmd);
	return 0;
}

</code></pre>

###需要创建出的进程支持拖拽的看这里：[创建支持拖拽的进程][2]

  [1]: http://msdn.microsoft.com/en-us/library/ms682429.aspx
  [2]: /blog/2013/08/06/process-can-drag-drop/
