+++
title = "Process Can Drag Drop"
date = "2013-08-06"
slug = "2013/08/06/process-can-drag-drop"
Categories = [keywords = ["Windows","UAC","Integrity","drag","drop","无法拖拽","拖拽"]
description = 降权创建支持拖拽的进程
+++
Win7下管理员权限的进程一般不支持拖拽，除非启动一个管理员权限的Explorer。前一段时间，同事给我提出一个需求：管理员权限进程创建非管理员权限进程，方法很简单，见前面的文章：[降权启动进程][1]。

而后的一次交谈，才知道同事的真正目的是，创建出的进程支持拖拽。我只是创建出了非管理员的进程，而是否管理员与支持拖拽并没有直接联系，决定拖拽的是**User Interface Privilege Isolation** ([UIPI][2])特性。

根据UIPI，低Integrity的进程无法向高Integrity的进程发送任意消息，这导致高Integrity看起来不支持拖拽。所以要使创建的进程支持拖拽，要满足Integrity低于或等于Explorer进程的Integrity值。

前面文章[降权启动进程][1]中，我们通过函数CreateNormalUserToken取得了受限的Token，只要修改此Token的Integrity即可使创建的进程拥有合适的Integrity，我们实现一个修改TokenIntegrity值的函数：
```cpp
BOOL ChangeTokenIntegrity(HANDLE &hToken) 
{  
    SID_IDENTIFIER_AUTHORITY MLAuthority = SECURITY_MANDATORY_LABEL_AUTHORITY;  
    PSID pIntegritySid = NULL;  
    if (!AllocateAndInitializeSid(&MLAuthority, 1, SECURITY_MANDATORY_MEDIUM_RID, 
                                  0, 0, 0, 0, 0, 0, 0, &pIntegritySid))  
    {   
        return FALSE;  
    }  
    TOKEN_MANDATORY_LABEL tml = {0};  
    tml.Label.Attributes = SE_GROUP_INTEGRITY;  
    tml.Label.Sid = pIntegritySid;
    
    const BOOL bRet = SetTokenInformation(hToken, TokenIntegrityLevel, &tml, 
                                          (sizeof(tml) + GetLengthSid(pIntegritySid)));  
    if (pIntegritySid)  
    {   
        FreeSid(pIntegritySid);  
    }  
    return bRet; 
}
```
上面第五行AllocateAndInitializeSid函数的第三个参数，这里取值SECURITY_MANDATORY_MEDIUM_RID是因为Explorer进程的Integrity一般是Medium。当然如果有必要也可以根据获取的Explorer进程的Integrity设置这个值。

参考：[Windows Integrity Mechanism Design][3]
 [1]:/blog/2013/07/31/de-elevate-start-process/
 [2]:http://en.wikipedia.org/wiki/User_Interface_Privilege_Isolation
 [3]:http://msdn.microsoft.com/en-us/library/bb625963.aspx
