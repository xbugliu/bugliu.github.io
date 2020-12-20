+++
title = "Unix Background Job"
date = "2013-08-03"
slug = "2013/08/03/unix-background-job"
tags =["linux","backgroud","process","job","后台任务"]
description = "Linux后台进程管理"
+++
当你执行一个要花费很长时间的unix脚本或者命令时，你可以在后台执行这个任务。

在这篇文章中，让我们回顾一下如何执行一个任务到后台、将任务切换到前台、查看所有后台任务和结束一个后台任务。

##1. 执行一个后台任务
在命令后面添加[&][1]就可以将任务执行到后台。

比如，当你执行一个可能耗时很长的查找命令，你可以像下面例子中一样将它执行到后台。下面的例子将查找root目录下24小时内被修改过的文件：
```bash
$ find / -ctime -1 > /tmp/changed-file-list.txt &
```
##2. 通过CTRL-Z和bg命令将当前任务切换到后台
你也可以将一个已经运行的前台任务切换到后台：

* 组合键 ‘CTRL+Z’ 将暂停当前任务。
* 执行bg将任务切换到后台执行

像下面的例子，假如你忘记将任务执行到后台，你无需结束当前任务再启动一个新的后台任务。你可以暂停当前任务然后将它切换到后台：

```bash
$ find / -ctime -1 > /tmp/changed-file-list.txt

$ [CTRL-Z]
[2]+  Stopped                 find / -ctime -1 > /tmp/changed-file-list.txt

$ bg
```
##3. 使用jobs命令查看所有后台任务
你可以使用**jobs**命令列出所有后台任务。一个jobs命令的可能输出如下：

```bash
$ jobs
[1]   Running                 bash download-file.sh &
[2]-  Running                 evolution &
[3]+  Done                    nautilus .
```
##4. 通过fg命令将后台任务切换到前台
你可以通过**fg**命令将后台任务切换到前台。如果执行fg命令不带参数，则将最近的后台任务切换到前台。
```bash
$ fg
```
假如你有多个后台任务，你又想将指定任务切换到前台，那你可以先执行jobs命令列出所有任务号和对应命令。下面的例子，fg %1将一号任务(download-file.sh)切换到前台。
```bash
$ jobs
[1]   Running                 bash download-file.sh &
[2]-  Running                 evolution &
[3]+  Done                    nautilus .

$ fg %1
```
##5. 通过kill %结束指定的后台任务
假如你想杀死指定的后台任务，用**kill** %任务号就行了。下面是杀死2号任务的例子：
```bash
$ kill %2
```
译者：[toWriting.com][3]；翻译自：[Bg, Fg, &, Ctrl-Z – 5 Examples to Manage Unix Background Jobs][2]


  [1]:https://en.wikipedia.org/wiki/Ampersand
  [2]:http://www.thegeekstuff.com/2010/05/unix-background-job/
  [3]:http://toWriting.com
