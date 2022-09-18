---
title: "Rust初体验"
date: 2022-03-19T19:55:14+08:00
draft: false
categories: ["开发"]
---

这两天用Rust写了一个通过Gitee拉取Github仓库的工具[Rusgit][1]，初步了解了Rust的一些特性。

* 工具链：rust-analyzer, cargo等
* 一些内置类型，&str和String
* 异常处理机制 Result<T>
* 继承 #derive
* 宏 print!
* 异步async、await和tokio
* 命令行解析 clap
* http请求 hyper
* json反序列化 serde
* 文件读写 std::io
* 启动进程 std::process
* 交叉构建 rust-build.action


## 工具链

* rust-analyzer是vscode下面的一个rust插件，补全、跳转、语法高亮、错误检测、智能提示。
* cargo负责包管理和编译。

整体使用顺畅，但涉及"Updating crates.io"时会有卡顿，是网络问题，可以通过更改国内的源来规避。

## &str和String 

* str是内置类型，&str在Rust中是borrow，类似于指向字符串字面量（string literal）或者String的一个指针，是只读的。
* String是一个类。String的内存是分配在堆上的。

## 异常处理Result<T, Error>

Rust的异常处理很特殊，不同于C++里面的返回错误码或者捕获异常，使用一个Result的枚举，成功则返回结果T, 失败返回Error。看起来和Golang里面返回多值一样，但实际用起来还是有很大不同。异常可以通过`?`传递的特性，减少了出错时的异常判断，还有可以用except\unwrap来处理非正常逻辑，都是其方便之处。但假如异常类型不匹配则需要写match，又稍显繁琐。

## derive

使用#derive来标记继承，enum和struct都可以derive。

## 宏函数
print!、write!等带`!`的都是宏函数。print!作为宏函数的优势：1. 避免了print变量时lifetime问题 2. 编译时format和参数检查，避免了C++中不匹配导致的内存溢出问题。

## 异步async、await

async可以标记一个函数为异步函数，await或者join使async函数执行。这些是Rust官方提供的关键字和函数。而tokio是社区的异步框架，来驱动async函数的运行。有一定的侵入性。

## 库

官方的库和三方库都有很多，结合cargo包管理，方便。

## rust-build

构建速度目前代码很小，没有明显的慢。跨平台交叉构建，没有Golang方便，需要自行安装toolchain。musl静态编译的版本好像用的多。RusGit在Windows正常执行，Linux下竟然Crash了，定位到openssl在musl下冲突，需要static link。


# 结论

设计新颖，安全性高，会持续关注和学习。

参考：
1. [what-benefits-are-there-with-making-println-a-macro][2]

[1]: https://github.com/xbugliu/rusgit
[2]: https://stackoverflow.com/questions/67509637/what-benefits-are-there-with-making-println-a-macro
