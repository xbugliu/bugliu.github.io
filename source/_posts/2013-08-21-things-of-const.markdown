---
layout: post
title: "const 二三事"
date: 2013-08-21 23:36
comments: true
categories: C++
keywords: C++,const,常量
description: 讲解了C++中的重要的const的历史，用法，用处及使用过程中要注意的地方，最后讲解了一个为了实现逻辑const的技巧
---
相信任何一个C++程序员都听说并使用过const。const在最初的C语言中是没有的，后来Bjarne Stroustrup和Dennis Ritchie讨论提出了Readonly机制，最初的Readonly机制简单的就是想利用操作系统的能力，提供一种可以使变量是只读的能力。Readonly通过被加进C语言的决议，并命名为const，但可能是标准委员会的官僚导致这项决议迟迟没有执行。后来Bjarne Stroustrup就自己把const加入进C++中（当时还不叫C++），并逐渐演变成现在的样子。

窃以为尽一切可能的使用const是任何一个合格的C++程序员应该遵守的事情，就像开车要系安全带一样。但好像人们并不喜欢用const，在我有限的C++编程生涯中，我接触到的有意识的会尽量用const的，除了我好像只有一人。不喜欢用const的结果同样可能会和开车不系安全带是一样的，希望你不会出事！

##const的作用
 * 避免魔数

```cpp
const int max_path = 260;
char [max_path];
```
使用自解释的变量名声明const变量，代替魔数，会增加代码的可维护性，勿以善小而不为呀！

 * 内存优化 对于POD类型的变量，const往往能将其优化到只读内存存储

 * 降低API的复杂度

比如有下面这样一个类
```cpp
class Array
{
  public:
    int getCount() const;
  private:
    //etc
};
``` 
将成员函数getCount指定为const（const不能修饰非成员函数和static成员函数），可以使调用它的客户端代码相信Array的对象是没有变化的，函数的调用不会产生副作用。

或者：
```cpp
int getMaxValue(const std::vector<int>& srcVec);

void fun()
{
    std::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    
    int maxVal = getMaxValue(vec);    
    
    //继续使用vec
    auto size = vec.size();
}
```
参数使用const修饰，可以逻辑上保证参数不会被调用的函数修改，一样是降低复杂度

 * 防止错误的发生

看下面的例子：
```cpp
int getMaxVal()
{
    const int maxVal = getMaxVal();
    int maxCacheVal = 0;
    if (NeedFetchCache())
    {
        //etc
        maxVal = getCacheMaxVal();//拼写错误，本来该是maxCacheVal
    }
    
    return max(maxCacheVal, maxVal);
     
}
```

如果不用const，例子里拼写错误导致的bug只能留待自测、QA或用户来发现了，如果maxVal是const，那编译器不会让你通过的，这样const帮助我们将这种错误绞杀于萌芽。
##两种const
* 物理const

物理const是Bjarne Stroustrup最初想要实现的Readonly。目的是把POD类型的变量存储进只读存储区，比如：
```cpp
 const int i = 200;
```
i将被优化进只读内存，效果相当于C语言中的宏。

* 逻辑const

逻辑const，一般修饰成员函数，表明调用函数不用引起对象逻辑上的变化：
```cpp
class Array
{
  public:
    size_t size() const
    {
        ++m_calledCount;//无法修改
        return m_size;    
    }
  private:
    size_t m_size;
    int m_calledCount;
};
```
上面例子中的size()函数修饰为const，则表明调用它的过程中，对象一般是不能变化的，所以无法改变成员变量的值。

逻辑const也指修饰非POD类型的变量，主要帮助编译器做语法检测：
```cpp
const std::string str1 = "towriting.com";
auto size = str1.size();
str1.push_back("!"); //无法调用非const成员函数
```
const的对象只能调用const版的成员函数。例子中的str1调用了非const函数push_back会引起编译错误，因为const对象不应该改变对象的“值"。

##鲜为人知的特性
 * 内部链接
 
大家知道全局变量的定义必须是唯一的，但const修饰的变量具有内部链接的属性，比如有两个编译单元文件test_const_one.cpp和test_const_ohter.cpp分别定义了全局变量g_var：
```cpp
//test_const_one.cpp

const int g_var = 1;
int main()
{
  //something
}

//test_const_other.cpp
const int g_var = 2;
int testconst
{
  //something
}

```
但编译是没有问题的，因为g_var只在本编译单元可见
```cpp
towriting.com@debian:~/workspace/snippets/cpp$g++ test_const_one.cpp test_const_other.cpp
``` 

 * 影响虚函数的覆盖

比如有两个类，CSuperButton继承CButton，并且子类“重写”了基类的GetWidth()函数：
```cpp
class CButton
{
  public:
   virtual int GetWidth() const {return 100;}
};

class CSuperButton: public CButton
{
  public:
   virtual int GetWidth() {return 0;}
};

int main()
{
  CButton *pBtn = new CSuperButton();
  std::cout<<pBtn->GetWidth()<<std::endl;
  return 0;
}
```

如果这是一道面试题，问输出的结果是多少时，我相性不少人会答错。不卖关子，结果是100，因为子类的GetWidth没有用const修饰而基类使用了const，所以无法覆盖。

 * 影响函数的重载

影响函数的重载有两种，一种是通过const修饰成员函数，比如：
 
```cpp
class CIntArray
{
public:
  int& At(int index);
  const int& At(int index) const;
};
```
CIntArray原本提供了一个非const的At函数用于获取内容，并且可以通过引用的返回值修改对象。但同时也要提供一个const版的At供const对象使用（如果返回成员变量，返回值也要用const）。

另一种影响重载的是，通过修饰参数，比如：
```cpp
void doSomething(int i);
void doSomething(const int& i);
```

 * "浅"const

我们前面讲了成员const函数无法修改成员变量，但指针的成员变量为什么好像会被修改？
```cpp
class CObj
{
public:
  void doSomething() const
  {
     *m_pointer = 1;
  }
private:
  int *m_pointer;
};
```
上面的代码编译是没有问题的，const的成员函数doSomething好像"修改"了成员变量的值。指针的成员变量有什么特殊的吗，为什么可以这样？原因很简单，doSomething并没有修改m_pointer的值，m_pointer是指针，只是修改了指针指向的内容而已。
