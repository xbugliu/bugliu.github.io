+++
title = "Openshift小技巧-热部署"
date = "2013-10-21"
slug = "2013/10/21/openshift-tips"
tags =["Openshift热部署"]
categories = ["开发"]
description = "如何热部署Openshift程序"
+++

最近试着用Openshift搭建了一个Octopress程序，搭建成功后，发现每次部署时，都会引起站点临时无法访问，这是自己使用Github的pages时没遇到的事情，用Google快速搜索下，原来要用热部署（hot deploy)实现不重启openshift程序部署文件，官方是支持的，方法很简单：

1. 切换到你的Openshift程序根目录
2. 创建文件hot_deploy到指定目录：


```bash
touch .openshift/markers/hot_deploy
```
