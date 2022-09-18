+++
title = "Smart GetProcAddress之实现"
date = "2013-07-20"
slug = "2013/07/20/smart-getprocaddress"
tags =["C++","模板"]
description = "尝试一种更智能实现GetProcAddress的方式"
categories = ["开发"]
+++
Windows下有过编程经验的朋友肯定用过这个函数：[GetProcAddress][1]，作用呢，就是从加载的动态库中获取指定函数名的函数入口地址，函数使用方法简单，一般是如下流程：


DLL导出函数的头文件:
```cpp
void WINAPI func1(int);
void WINAPI func2(int,int);
```


动态加载DLL调用上面两个函数:
```cpp
typedef void (WINAPI *FUNC1)(int);
typedef void (WINAPI *FUNC2)(int,int);
FUNC1 func1 = (FUNC1)GetProcAddress(hDLL, _T("func1");
FUNC2 func2 = (FUNC2)GetProcAddress(hDLL, _T("func2");
func1(1);
func2(1, 2);
```

以上是主流的代码写法，但其实稍有问题：
1. 要定义一套函数类型，且违反DRY，枯燥乏味，影响代码美观
2. 隐式类型转换是魔鬼，如果DLL实现变化，则调用出错，比如：

LL导出函数的头文件：fun2的参数变成了三个，调用非出错不可。
```cpp
void WINAPI func1(int);
void WINAPI func2(int,int,int);
```

好，问题来了，如何避免？使用模板，方案1：
```cpp
// 封装一个智能GetProcAddress
template<typename T>
T SmartGetProcAddress(HModule hModule, TChar* pFuncName, T)
{
    return (T)GetProcAddress(hModule, pFuncName);
}

// 使用方法如下：注意1. 要引用对应头文件，2. auto（自动类型推导）关键字从VS2010开始支持
auto func1 = SmartGetProcAddress(hDLL, _T("func1"), &func1);
```

看起来我们解决了DRY问题，也不用写繁琐的typedef了，于是我们开始Build，很快你就发现Link错误，unresolved external symbol，没注意取地址&func1，已静态依赖于DLL，看来此路不通。取地址的目的是从头文件中获取函数的类型，还有什么方法可以获取函数的类型呢？

方案2：使用decltype
```cpp
// 封装一个智能GetProcAddress
template<typename T>
T SmartGetProcAddress(HModule hModule, TChar* pFuncName)
{
    return (T)GetProcAddress(hModule, pFuncName);
}

// 使用方法如下：
auto func1 = SmartGetProcAddress<decltype(func1)>(hDLL, _T("func1"));
```

使用decltype可以解决，函数取地址（&)会静态依赖DLL问题，这里的关键是decltype(func1)这种写法的支持，最初这种写法在g++4.7测试是支持的，不过后来再VS2012、VS2010上测试好像都不支持，所以SmartGetProcAddress只能存在于理论阶段了。

  [1]: http://msdn.microsoft.com/en-us/library/ms683212%28v=vs.85%29.aspx
  [2]: http://en.cppreference.com/w/cpp/language/decltype
