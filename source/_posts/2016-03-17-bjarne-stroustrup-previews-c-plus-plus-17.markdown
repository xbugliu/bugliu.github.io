---
layout: post
title: "Bjarne Stroustrup previews C++ 17"
date: 2016-03-17 23:17:22 +0000
comments: true
categories: C++
keywords: C++17,C++
description: C++之父关于C++17的访问
---

本文是infoWorld关于C++17对C++之父进行的采访，原文见[这里](0)

--------------------------------------------------------------------------

**InfoWorld**:C++17什么时候可用?

**Stroustrup**:C++17将会在2017年的某个时间正式发布，可能是在秋季。主要特性的实现会准时或提前完成。其实有些部分现在已经实现了。

**InfoWorld**:你认为C++17有哪些重大的新特性?

**Stroustrup**:如何定义重大呢，假如一个语言特性或者库会影响你对这门语言的看法并且影响你的代码结构，那我认为它就是重要的。按照这个定义，很遗憾，我的答案是：
对于绝大多数人来说，C++17并没有重大的特性。请不要把"C++17并无重大改进”这样的文字加粗或者作为文章的标题，因为这是不准确的、是不必要的煽动。我很看好文件库(file system library)和
并行算法(parallel algorithms)。它们特别有用，会使有些任务变简单，但对于大部分人来说，这并不是重大的特性。

然而，我认为重大的特性以其它的方式存在。一些重大的特性会出现在TS(Technical Specifications)中。比如：concepts, networking, more concurrency stuff, ranges (Standard Template Library 2), modules, coroutines. 
它们肯定不会被包含在C++17中，但它们确实存在着。

**InfoWorld**:C++17的哪些[改进][2]对开发者影响最大呢？

* (parts of) Library Fundamentals TS v1
* Parallelism TS v1 * File System TS v1
* Special math functions
* hardware_*_interference_size * .is_always_lockfree() * clamp()
* non-const .data() for string

**Stroustrup**:这因人而异，因你做的事情而异。对我来说，我认为并行算法是最重要的，文件系统库也是很方便的特性. `optional`, `any`和`string_view`这些基础库中的东西也很重要. 另外STL中其它小的改进也很多.
如果你是搞数学的，那么数学库对你是不可或缺的，现在也都在标准库中了。

**InfoWorld**:7月份能知道C++17所有的特性(facets)吗?

**Stroustrup**:我希望可以，2016年6月下旬，我们会在芬兰奥卢确定一些小的提议,比如:

* Dynamic memory allocation for overaligned data (for better vectorization)
* Template parameter deduction for constructors (make many"make functions" redundant)
* constexpr_if (a compile-time if)
* Refining Expression Evaluation Order for Idiomatic C++ (finally, we can eliminate bugs from people accidentally relying on undefined order of evaluation)
* Default comparisons (==, !=, <, <=, >, and >=)
* Operator Dot (smart references)
* Generalizing the Range-Based For Loop (for sentinel-based and counted ranges)
* Structured bindings (simple use of multiple return types)

只要不是运气太差，这些都可以确定下来的。当然，投票统计之前所有事情都是无法确定的。标准委员会会争取共识，一张否决票顶五张赞成票。和16年3月份通过的决议比较，假如本次通过了大部分决议那C++17将变得更加有趣。

**InfoWorld**:随着`constexpr lambdas`进入C++17，C++是不是朝着函数式编程语言更进了一步。这对开发者有什么影响呢？

**Stroustrup**:自从1994年STL起，C++平稳而谨慎的增加着函数式编程技术的使用。`constexpr lambdas`只是编译时特性的一个简单扩展。如果`structured bindings`的决议C++17通过了的话，
多返回值将被支持，就像一些函数式编程语言的函数一样。

**InfoWorld**:`Concepts`这个可以改善编译时诊断的技术，没有被加入C++17是不是一件遗憾的事情？

**Stroustrup**:对我来说是的，这是个巨大的遗憾。从2004年，我就和Gabriel Dos Reis及其他几个人一起，在这个问题上研究了好几年，并且用Andrew Sutton的实现测试了三年的时间。我觉得可以
正式发布这个特性了，但标准委员会的大多数人因为各种原因不同意。一个`Concept`是对类型和值的集合的编译时断言(predicate)。我认为“更好的错误提示"是`Concept`最基础且重要的一个优势：我们可以对
我们泛型函数（模板）的参数设定要求。这样可以促成更好的设计、更好的接口、更高效的代码。

**InfoWorld**:你能不能回答下，为什么`modules`和`co-routines`也不会放到c++17.

**Stroustrup**:我很乐意看到`modules`可以更好的防范某些单元上下文中的改变(比如宏)和更好的编译速度，但是提议还没有为C++17准备好，所以只能加进TS了。我想最终`modules`会变成一个重要的特性。
它解决了C/C++里面长期存在的问题。MS C++(vs2015)和clang的某些版本已经有类似`modules`的支持。对于`co-routines`没有直接放到c++17而是放到TS中我很失望。我认为它在一些特殊的场景下很重要(pipelines and generators)。
MS Vc++（vs2015）也集成了这个特性。


**InfoWorld**:为什么你不延期发布一年，以使`concepts`、`modules`和`co-routines`等特性可以一起发布呢?

**Stroustrup**:我以前也被问过这个问题，我回答了no, 我们要保证c++17可以按期发布。一次延期会是一次很不好的先例，会导致后面更多的延期。假如c++17变成c++18, 我想c++20会变成c++22或者c++23。


[0]:http://www.infoworld.com/article/3044727/application-development/qa-bjarne-stroustrup-previews-c-17.html
[2]:http://meetingcpp.com/index.php/br/items/cpp17-and-other-future-highlights-of-cpp.html