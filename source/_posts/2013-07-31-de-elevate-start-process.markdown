---
layout: post
title: "降权启动进程"
date: 2013-07-31 22:01
comments: true
categories: C++ Windows
keywords: process,uac,token,进程
description: 管理员权限进程创建非管理员权限进程
---
Win7下有些进程需要以管理员权限启动，比如安装程序。但又需要这个具有管理员权限的进程启动一个非管理员权限的进程。要实现这一点，方法很简单，首先得到受限的Token，然后由这个Token调用[CreateProcessAsUser][1]。

{% include_code  WIN7/VS2010 cpp/de_elevate_start_process.cpp %}

###需要创建出的进程支持拖拽的看这里：[创建支持拖拽的进程][2]

  [1]: http://msdn.microsoft.com/en-us/library/ms682429.aspx
  [2]: /blog/2013/08/06/process-can-drag-drop/