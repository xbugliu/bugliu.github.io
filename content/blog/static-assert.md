+++
title = "C++中的静态断言"
date = "2013-12-10"
slug = "2013/12/10/static-assert"
tags =["C++ 静态断言","C++ Static Assert","C++ static_assert","C++ 约束"]
description = "什么是静态断言，为什么使用静态断言，如何使用静态断言"
+++

##什么是静态断言
断言（Assert)是报告代码状态错误的技术手段。Windows下的C++开发肯定都知道ASSERT或ATLASSERT，这两个宏生成DEBUG版本下的断言，另外还有assert，是C/C++提供的断言函数，效果和以上两个宏是一样的。

那什么是静态断言？上面介绍的普通断言是运行时检测的，静态断言是编译期检测的，所以被称之为静态断言（static assert）。最早知道编译期检测是在Matthew wilson的<a href="http://www.amazon.cn/gp/product/B008A4Y2R0/ref=as_li_ss_tl?ie=UTF8&camp=536&creative=3132&creativeASIN=B008A4Y2R0&linkCode=as2&tag=bringmeluck-23" rel="external nofollow" title="" target="_blank">《Imperfect C++》</a>中，里面称其为约束（constraints)，并抱怨C++为什么不支持如此常用的功能（C++11已有改善）。

那使用静态断言有什么好处呢？

##静态断言的好处

1. 更早的报告错误，我们知道构建是早于运行的，更早的错误报告意味着开发成本的降低
2. 无法忽略的错误，对于Assert类似DEBUG下的断言，有时候被不会被执行到，即使执行到也会遭到一些开发的忽视，而静态断言的错误是无法忽视的，因为构建失败了。
3. 减少运行时开销，静态断言是编译期检测的，减少了运行时开销

那如何使用实现静态断言？
##使用静态断言
静态断言作用的对象一般是编译时已知的状态。任何可以成为模板类参数的内容都可以作为静态断言的对象。

下面看一个静态断言的例子：
```cpp
template <typename D,typename B>
struct has_base
{
     ~has_base()
    {
        void(*p)(D*,B*) = constraints;
    }
private:
    static void constraints(D *pd,B *pb)
   {
        pb = pd; 
   }
};
```
这个例子最早出自Bjarne Stroustrup之手，用于检测一个类型是否是另一个类型的父类（类型相等和void*的情况没有考虑，用起来像这样子：
```cpp
class CBase {};
class CDer: public CBase {};
has_base<CDer, CBase> a;
has_base<CBase, CDer> b; //编译错误
```
它的工作原理是，成员函数constraints试图把D的指针转化成B的指针，这一般只在B是D的派生类时成立。而且constraints函数永远不会被调到，所以没有运行时开销。

在看一个例子：
```cpp
enum personType
{
    person_child,
    person_adult,
    person_woman,
    person_man,
    person_count    
};


void ProcessPersonType(personType ptVal)
{
  char dummy[person_count > CHAR_MAX ? -1 : 1];
  char cVal = ptVal;
  // do something with cVal
}

```
上面例子里，试图将枚举类型的值转化成cVal,但可能有溢出的问题，所以设置一个静态断言：`char dummy[person_count > UCHAR_MAX ? 1 : -1];`，原理是如果personType的最大值大于char类型的最大值，则表达式为`char dummy[-1]`，我们知道这是编译不过的，所以会提示我们出问题了。

以上方式虽然可以实现静态断言的效果，但提示的信息，可能和我们断言想要报告的完全无关，所以并不是最完美的解决方案。

##C++11中的静态断言

C++11中增加了static_assert支持静态断言，用法相当简单，static_asset接受两个参数，第一个为要断言的内容，第二个为显示的错误提示。
```cpp
template<int n>                                 
struct Factorial                                    
{
 static_assert(n > 1, "n must bigger than 1");  
 enum{ value=n * Factorial<n-1>::value };   
};  
  
  
template<>                                          
struct Factorial<0>                                    
{  
 enum{ value=1 };  
};  
```
上面是计算阶乘的一个模板，我们使用static_assert检测n的值大于1，如果不满足则，编译错误，提示为："n must bigger than 1"。

static_assert是编译器很容易实现的一个特性，一般的编译器最新版本应该都已支持。
