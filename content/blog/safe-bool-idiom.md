+++
title = "Safe Bool Idiom"
date = "2013-11-01"
slug = "2013/11/01/safe-bool-idiom"
tags =["C++惯用法safe bool与C++11中的显式操作符"]
description = "C++惯用法safe bool与C++11中的显式操作符简介"
+++

> C++是一个学语法都能让人入迷的奇葩语言，有各种的奇技淫巧。比如这里的许多的惯用法：[More C++ Idioms](http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms)，虽凝聚了C++程序员的聪明才智，但都是特定时期的产物，
相信都会被冲到C++语言演化长河的河滩上，仅供后人瞻仰（或者是C++本身）。让我们从Safe bool idiom说起。

##Safe bool idiom
什么是safe bool idiom？就是为自定义类型（class）提供检测真假的能力，而又不会带来副作用。

###为类(Class)提供检测真假的能力
方法有二，第一种简单直白，提供一个返回bool类型的函数，比如下面的isValid成员函数：
```cpp
class CData
{
 public:
  bool IsValid() const;
};
int main()
{
  CData data;
  if (data.IsValid())
  {
   //dosomething
  }
}
```
这是直观而不易出错的。但多多少少有些强迫症的人会说，如果能像检测内置bool类型一样检测data对象不是更好的保证了代码的语法一致性？
```cpp
int main()
{
  CData data;
  if (data)
  {
   //dosomething
  }
}
```
同时他又急于向人们展示，"我会使用操作符重载哦“。于是第二种方法出来了，重载bool类型转化操作符：
```cpp
class CData
{
 public:
  operator bool() const;
};
int main()
{
  CData data;
  if (data)
  {
   //dosomething
  }
}
```
漂亮的外表后面的东西可能是有毒的，比如毒蘑菇、巫婆的毒苹果和传说中的红颜祸水们。软件开发也概莫能外，这个漂亮的解决方案后面有问题。

###bool操作符的副作用
假使有一个简单的指针外敷类：
```cpp
template<typename T>
class CPtr
{
  T *ptr;
  public:
   operator bool() const 
   {
    return ptr != nullptr;
   }
};

int main()
{
  CPtr<float> p1;
  CPtr<int>   p2;
  
  if (p1 == p2) 
  {
   //天知道会怎样
   //something 
  }
  
}

```
有人不小心拿两个不同类型的类对象来比较，不幸的是编译器并没有报错，因为17行隐式调用了operator==(bool,bool)，后面的结果真真天知道。
这可如何是好？C++社区里最不缺人才，很快有人想出解决方案：

###Safe bool实现
Safe Bool正式的提出是这里：[The Safe Bool Idiom](http://www.artima.com/cppsource/safebool.html)，方法就是写一个类型转化操作符，这个操作符返回一个可以进行 **if** 判断的特有类型：
```cpp
class Testable 
{
    bool ok_;
    typedef void (Testable::*bool_type)() const;
    void this_type_does_not_support_comparisons() const {}
  public:
    explicit Testable(bool b=true):ok_(b) {}

    operator bool_type() const {
      return ok_==true ? 
        &Testable::this_type_does_not_support_comparisons : 0;
    }
};

class TestableOther 
{
    bool ok_;
    typedef void (TestableOther::*bool_type)() const;
    void this_type_does_not_support_comparisons() const {}
  public:
    explicit TestableOther(bool b=true):ok_(b) {}

    operator bool_type() const {
      return ok_==true ? 
        &Testable::this_type_does_not_support_comparisons : 0;
    }
};

int main()
{
  Testable testable;
  TestableOther testableother;
  if (testable)
  {
    //something
  }
  if (testable == testableother) //编译错误
  {
   
  }
  
}

```
上面代码，利用的是bool_type是函数指针类型，所以可以进行 **if** 判断，且不同类的bool_type是不同的，直接比较会编译错误。
这就是safe bool Idiom，详细的代码可以参看这里：[More C++ Idioms/Safe bool](http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/)。
但我认为这个方案是顾此失彼，会引发新的问题，比如类中重载operator int操作符怎么办？

##C++11的做法
C++11的基因支持safe bool，方法就是使用[explicit](http://en.cppreference.com/w/cpp/language/explicit)修饰operator：
```cpp
struct Testable
{
    explicit operator bool() const {
          return false;
    }
};
 
int main()
{
  Testable a, b;
  if (a)      { /*do something*/ }  
  if (a == b) { /*do something*/ }  // 编译错误
}
```
**explicit** 在C++11以前是只能用于修饰构造函数，但在C++11中可以用来修饰操作符，上面代码中的operator bool（）加上**explicit**表式其无法隐式转化为bool。
这个解决方案，干净漂亮，无副作用。

##总结
C++11前后的两种Safe Bool的解决方案比较，优劣立现。站在实用的角度上，C++11出现后，C++中好多“高端技术”已经不需要学习，比如StackOver上列出的这些：[what C++ idioms are deprecated in C++11](http://stackoverflow.com/questions/9299101/what-c-idioms-are-deprecated-in-c11)。这些东西就像毛笔字一样，可以仅供专家与爱好者把玩了。


