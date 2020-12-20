+++
title = "Windbg关联dmp文件"
date = "2014-12-07"
slug = "2014/12/07/windbg-dump-asso"
tags =["windbg","dmp"]
description = "windbg设置dmp文件的关联"
categories = ["开发"]
+++

最近一段时间和Crash斗争，每天必不可少的事情是分析dump，每天少则分析几个，多则分析几十个是常有的。而打开dump到进入windbg cmd窗口输入!analyze -v命令是机械枯燥的事情。Windbg是没有默认关联.dmp文件的，只能自己动手了。分享关联方法之前，看下我现在dump文件的打开方式：

{% img /images/posts/windbg-dump-asso/windbg_dmp.png  %}

我设置了三种打开方式：

  * 直接使用Windbg打开dump
  * 使用windbg打开dump并进行分析
  * 使用windbg打开dump，切换到32位，然后分析（针对加载的wow64)

###配置dmp关联的方法

#### 1. 设置dmp文件的关联

Windows下文件的关联指的是在explorer下，双击或通过右键打开文件时选择指定程序打开。文件的关联方式可以通过注册表进行配置，在HKEY_CLASSES_ROOT、HKEY_CURRENT_USER\Software\Classes、HKEY_LOCAL_MACHINE\Software\Classes有以 ".文件格式" 命名的键和另一个自定义的键里面保存具体的配置，用来配置这个文件格式的关联，比如我们要配置的dmp文件的关联，最终会是这样：
```
HKEY_CURRENT_USER\Software\Classes
   dmpfile
     DefaultIcon
       default = "C:\Program Files (x86)\Debugging Tools for Windows (x86)\windbg.exe",0
     shell
       Analyze with windbg
           command
              default=C:\Program Files (x86)\Debugging Tools for Windows (x86)\windbg.exe" -c "!analyze -v" -z "%1\"
       Analyze with windbg - wow64
           command
              default="C:\Program Files (x86)\Debugging Tools for Windows (x86)\windbg.exe" -c "!wow64exts.sw; !analyze -v" -z "%1"
       open
           command
              default="C:\Program Files (x86)\Debugging Tools for Windows (x86)\windbg.exe" -z "%1"
   .dmp
     default = dmpfile
```

当然，更改HKEY_CURRENT_USER是对当前用户有效，HKEY_LOCAL_MACHINE是对所有用户有效，而HKEY_CLASSES_ROOT是兼容的产物，不推荐在直接配置HKEY_CLASSES_ROOT。

#### 2. 去除用户自定义的打开方式-自定义程序

 如果你设置过打开方式里面的自定义程序，必须先删除这一项，否则我们上一步设置的文件关联将不会生效，打开方式自定义程序的配置也是保存在注册表中，具体位置在：
 
 ```
 HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.dmp\UserChoice
 ```
 
将这个UserChoice键删除即可。

#### 3. 到这里就讲完了，最后奉上一个bat:

<pre><code>
@echo off

set dbgpath=\"C:\Program Files (x86)\Debugging Tools for Windows (x86)\windbg.exe\"


REG ADD "HKCU\Software\Classes\.dmp" /f /d dmpfile

set val=%dbgpath%,0
REG ADD "HKCU\Software\Classes\dmpfile\DefaultIcon" /f /d "%val%"

set val=%dbgpath% -z \"%%1\"
REG ADD "HKCU\Software\Classes\dmpfile\Shell\open\command" /f /d "%val%

set val=%dbgpath% -c \"!wow64exts.sw; !analyze -v\" -z \"%%1\"
REG ADD "HKCU\Software\Classes\dmpfile\Shell\Analyze With Windbg  - wow64\command" /f /d "%val%"

set val=%dbgpath% -c \"!analyze -v\" -z \"%%1\"
REG ADD "HKCU\Software\Classes\dmpfile\Shell\Analyze With Windbg\command" /f /d "%val%"

REG DELETE "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.dmp\UserChoice" /f

set /p finish="finsh!"
</code></pre>


参考：

http://msdn.microsoft.com/en-us/library/cc144158%28VS.85%29.aspx

http://msdn.microsoft.com/en-us/library/windows/hardware/ff561306.aspx
