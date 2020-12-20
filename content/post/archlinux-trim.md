+++
title = "Archlinux Trim"
date = "2014-06-28"
slug = "2014/06/28/archlinux-trim"
Categories = ["linux trim","ssd trim"]
description = "linux ssd trim"
+++

新买得一块SSD，听说开启TRIM才能更好的发挥SSD的性能，Linux并没有默认开启TRIM，但开启还是比较简单。

首先要检测SSD是否支持TRIM:

```bash
sudo hdparm -I /dev/sda | grep "TRIM supported"
```
如果支持则会出现："Data Set Management TRIM supported"

如果SSD支持TRIM, 则可以开启TRIM了, 这里介绍常用的两种方法。

###方法1：修改[fstab][1], 添加discard属性
```bash
sudo vim /etc/fstab
```

下面是我机器上fstab的配置
> UUID=27dd31b4-8aa4-4043-b921-540a312c056c       /               ext4            rw,relatime,data=ordered,**discard**        0 1

> 
> UUID=42f79958-0776-4b2f-8aa3-db827bf257b6       /home           ext4            rw,relatime,data=ordered,**discard**        0 2





###方法2：使用fstrim定期执行trim任务
以我用的archlinux为例，首先安装并运行[cron][2]服务：
```bash
sudo pacman -S cronie
sudo systemctl start cronie
sudo systemctl enable cronie
```

然后创建一个任务配置:
```bash
sudo gedit /etc/cron.daily/trim
```

并将下面的内容拷贝到里面：
```
#!/bin/sh
LOG=/var/log/trim.log
echo "*** $(date -R) ***" >> $LOG
fstrim -v / >> $LOG
fstrim -v /home >> $LOG
```


参考：

1. http://www.webupd8.org/2013/01/enable-trim-on-ssd-solid-state-drives.html

2. https://wiki.archlinux.org/index.php/Solid_State_Drives


[1]: http://en.wikipedia.org/wiki/Fstab
[2]: http://en.wikipedia.org/wiki/Cron
