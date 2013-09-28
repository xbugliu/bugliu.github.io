#include "stdafx.h"
#include <functional>
#include <Windows.h>
#include <winternl.h>
#include <vector>
#include <ntstatus.h>
#include <iostream>

// NtQueryObject枚举出的内核对象的类型信息的结构
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

// 一个句柄信息的数据结构
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
	ULONG ProcessId;
	BYTE ObjectTypeNumber;
	BYTE Flags;
	USHORT Handle;
	PVOID Object;
	ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

// 获取一个句柄的详细信息
// 可以获取类型名和内核对象的名字
// bType - 获取句柄的类型名
std::wstring QueryHandleNameInfo(HANDLE handle, BOOL bType)
{
	std::wstring strName;
	const HMODULE hDll = LoadLibrary(_T("ntdll.dll"));
	if (hDll == NULL)
	{
		return strName;
	}
	typedef NTSTATUS (NTAPI *NtQueryObjectFunc)(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG);
	NtQueryObjectFunc NtQueryObject_ = (NtQueryObjectFunc)GetProcAddress(hDll, "NtQueryObject");

	do 
	{
		if (NtQueryObject_ == NULL )
		{
			break;
		}
		// 获取信息
		const DWORD ObjectNameInformation = 1;
		OBJECT_INFORMATION_CLASS infoType = bType ? ObjectTypeInformation : 
                                                            OBJECT_INFORMATION_CLASS(ObjectNameInformation);
		std::vector<BYTE> objVec(256);
		ULONG bytesOfRead = 0;	
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		do 
		{
			status = NtQueryObject_(handle, infoType, (void*)objVec.data(), objVec.size(), &bytesOfRead);
			if (STATUS_INFO_LENGTH_MISMATCH == status)
			{
				objVec.resize(objVec.size() * 2);
				continue;
			}
			break;
		} while (TRUE);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		objVec.resize(bytesOfRead);

		if (bType)
		{
			const OBJECT_TYPE_INFORMATION *pObjType = 
                                                       reinterpret_cast<OBJECT_TYPE_INFORMATION*>(objVec.data());
			strName = std::wstring(pObjType->TypeName.Buffer, pObjType->TypeName.Length / sizeof(WCHAR));
		}
		else
		{
			const UNICODE_STRING *pObjName = reinterpret_cast<UNICODE_STRING*>(objVec.data());
			strName = std::wstring(pObjName->Buffer, pObjName->Length / sizeof(WCHAR));
		}

	} while (FALSE);

	FreeLibrary(hDll);
	return strName;
}

// 遍历句柄辅助类
class WalkHandleHelper
{
public:
	WalkHandleHelper(const SYSTEM_HANDLE_TABLE_ENTRY_INFO& handleInfo, const HANDLE& handle)
        :m_HandleInfo(handleInfo), m_Handle(handle){}
	DWORD GetProcessID() const {return m_HandleInfo.ProcessId;}
	std::wstring GetTypeName() const
	{
		return QueryHandleNameInfo(m_Handle, TRUE);
	}
	std::wstring GetObjectName() const
	{
		return QueryHandleNameInfo(m_Handle, FALSE);
	}

private:
	const SYSTEM_HANDLE_TABLE_ENTRY_INFO& m_HandleInfo;
	const HANDLE& m_Handle;
};

// 枚举系统的句柄
void WalkHandle(const std::function<void(const WalkHandleHelper&)> &functor)
{
	const HMODULE hDll = LoadLibrary(_T("ntdll.dll"));
	if (hDll == NULL)
	{
		return;
	}

	// 使用NtQuerySystemInformation检索SystemHandleInformation(16)即可获得系统中所有的句柄信息
	const DWORD SystemHandleInformation = 16;		

	// 通过SystemHandleInformation检索到的系统中所有句柄的数据结构
	typedef struct _SYSTEM_HANDLE_INFORMATION
	{
		ULONG HandleCount; 
		SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
	} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;


	typedef NTSTATUS (NTAPI *NtQuerySystemInformationFunc)(ULONG, PVOID, ULONG, PULONG);
	NtQuerySystemInformationFunc NtQuerySystemInformation_ = (NtQuerySystemInformationFunc)
                                                                 GetProcAddress(hDll, "NtQuerySystemInformation");
	const HANDLE hCurProcess = GetCurrentProcess();
	do 
	{
		if (NULL == NtQuerySystemInformation_)
		{
			break;
		}
		// 获取系统句柄表
		std::vector<BYTE> vecData(512);
		ULONG bytesOfRead = 0;
		NTSTATUS status;
		do 
		{
			status = NtQuerySystemInformation_(SystemHandleInformation, vecData.data(), vecData.size(), &bytesOfRead);
			if (STATUS_INFO_LENGTH_MISMATCH == status)
			{
				vecData.resize(vecData.size()*2);
				continue;
			}
			break;
		} while (TRUE);
		if (!NT_SUCCESS(status))
		{
			break;
		}
		vecData.resize(bytesOfRead);

		PSYSTEM_HANDLE_INFORMATION pSysHandleInfo = (PSYSTEM_HANDLE_INFORMATION)vecData.data();
		for (int i=0; i<pSysHandleInfo->HandleCount; ++i)
		{
			const HANDLE hOwnProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, 
                                                               pSysHandleInfo->Handles[i].ProcessId);
			if (NULL == hOwnProcess)
			{
				continue;
			}
			HANDLE hDuplicate = NULL;
			// 必须把Handle放入自己的进程中，否则无法获取其他进程拥有Handle的信息
			if (!DuplicateHandle(hOwnProcess, (HANDLE)pSysHandleInfo->Handles[i].Handle, hCurProcess, 
                            &hDuplicate,0,0, DUPLICATE_SAME_ACCESS))
			{
				CloseHandle(hOwnProcess);
				continue;
			}
			WalkHandleHelper helper(pSysHandleInfo->Handles[i], hDuplicate);
			functor(helper);

			CloseHandle(hDuplicate);
			CloseHandle(hOwnProcess);
		}

	} while (FALSE);

	FreeLibrary(hDll);
}

// 枚举当前进程的互斥量
void EnumCurProcessMutex()
{
	std::wcout<<_T("Find The Mutex Opened By Current Process:")<<std::endl;
	const DWORD dwCurProcess = GetCurrentProcessId();
	WalkHandle([&](const WalkHandleHelper& helper)
	{
		const std::wstring strMutant(_T("Mutant"));
		if (helper.GetProcessID() == dwCurProcess && helper.GetTypeName().compare(strMutant) == 0)
		{			
			std::wcout<<_T("ObjectName: ")<<helper.GetObjectName().c_str()<<std::endl;
		}

	});
}

int _tmain(int argc, _TCHAR* argv[])
{
	CreateMutex(NULL, TRUE, NULL);
	CreateMutex(NULL, TRUE, _T("TestMutex"));
	CreateMutex(NULL, TRUE, _T("Global\\TestGlobalMutex"));
	EnumCurProcessMutex();
	return 0;
}

