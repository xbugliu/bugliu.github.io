+++
title = "Forbid Auto Install Driver"
date = "2013-08-03"
slug = "2013/08/03/forbid-auto-install-driver"
Categories = [keywords = ["windows","driver","驱动"]
description = 暴力禁止windows自动安装驱动
+++
{% img right /images/posts/forbid-auto-install-driver/auto_install_driver_tip.png 'windows自动安装驱动' 'windows系统有时候给人的感觉很智能，很勤快，比如插入一个手机，它会自动帮你安装上相应驱动（前提是它能找到对应驱动）。但这种殷勤的行为不是人人都需要的，而且这时候你又找不到制止这种行为的入口，你一定很窝火。' %}


前两天我需要手动安装一个手机的根节点驱动，这个驱动属于系统基础驱动，所以一插入手机windows就帮你装上了，这不是我想要的。当我用遍了google、yahoo、ixquick、duckduckgo、baidu、soso各种搜索引擎仍然没有找到有效的禁用windows自动安装驱动的办法后，我只好尝试暴力对抗了。

很快用ProcessMonitor定位到安装驱动的进程：
{% img right /images/posts/forbid-auto-install-driver/auto_install_driver.png %}


不想让系统自动安装驱动，破坏drvinst进程就可以达到。写个简单的脚本，插入手机前跑起来就OK了（win7系统开启UAC则需管理员权限运行）：
<pre><code>
import os
import time
import subprocess

def RunLoop():
    while(True):
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        subprocess.call('taskkill /F /IM drvinst.exe', startupinfo=startupinfo)
        time.sleep(0.5)


if __name__ == "__main__":
    RunLoop()
</code></pre>
**注意：**如果破坏drvinst进程后发现系统对新插入的设备不再自动安装驱动，则设备管理器中扫描检测硬件改动即可。

