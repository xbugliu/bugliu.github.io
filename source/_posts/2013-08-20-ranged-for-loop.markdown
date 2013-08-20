---
layout: post
title: "C++11系列-区间迭代"
date: 2013-08-20 22:50
comments: true
categories: C++ C++11
keywords: C++,for,C++11,算法,for_each
description: 本文详细讲解了C++11中ranged for loop的用法
---
在我前面[介绍C++11][1]的文章中，我提到C++11将会带来一些实用的改进。我的意思是它将移除一些不必要的打字和其它影响快速编码的壁垒。我前面讲过的[auto关键字][2]就是一个例子；现在我想讲一下区间迭代（range-based for loop）。
##区间迭代的基本语法
近来，基本上所有现代编程语言都有一种对一个区间写for循环的便捷方式。最终，C++也有了相同的概念；你可以给循环提供一个容器，它帮你迭代。前面我们已经在[什么是C++11][1]中看到了一些简单的例子。让我们回忆一下区间迭代的样子：
```cpp
vector<int> vec;
vec.push_back( 10 );
vec.push_back( 20 );
 
for (int i : vec ) 
{
    cout << i;
}
```
上面代码打印一个名叫vec的vector的内容，用i去捕获vector里面的值，直至vector的最后。你也可以用auto代替类型便利的迭代复杂的数据结构。例如，迭代一个map:
```cpp
map<string, string> address_book;
for ( auto address_entry : address_book )
{
            cout  << address_entry.first << " < " << address_entry.second << ">" << endl;
} 
```

##修改vector的值
假如你想修改你正在迭代的容器的值，或者你想避免拷贝大对象，你可以用引用的变量遍历。比如，下面的迭代对一个整形vector中每个元素的值加1。
```cpp
vector<int> vec;
vec.push_back( 1 );
vec.push_back( 2 );
 
for (int& i : vec ) 
{
    i++; // increments the value in the vector
}
for (int i : vec )
{
    // show that the values are updated
    cout << i << endl;
}
```
##区间意味着什么？
Strings,arrays,和所有的STL容器可以被新的区间迭代方式迭代。但是如果你想让你自己的数据结构使用这个新语法怎么办？

为了使这个数据结构可迭代，它必须类似于STL迭代器。

* 这个数据结构必须要有begin和end方法，成员方法和独立函数都行，这两个方法分别返回开始和结束的迭代器
* 迭代器支持*操作符、!=操作符、++方法（前缀形式，成员函数和独立函数都行）

就这些！实现这五个函数，你就可以有一个支持区间迭代的数据结构。因为begin、end可以是非成员函数，你甚至可以适配现有数据结构而不用实现STL风格的迭代器。所有你要做的是创建你自己的支持*、前缀++和!=的迭代器，并且定义好自己的begin、end。

区间迭代如此NICE。所以我怀疑大部分还不支持STL迭代模型的容器都会想添加某种适配方式以支持区间迭代。这里有一个小程序演示创建一个支持区间迭代的迭代器。这个例子里，我创建了一个固定Size是100的IntVector，并且可以被一个叫做Iter的类迭代。
```cpp
#include <iostream>
 
using namespace std;
 
class IntVector;
 
class Iter
{
    public:
    Iter (const IntVector* p_vec, int pos)
        : _pos( pos )
        , _p_vec( p_vec )
    { }
 
    // 这三个方法组成支持区间迭代的迭代器的基础
    bool
    operator!= (const Iter& other) const
    {
        return _pos != other._pos;
    }
 
    int operator* () const;
 
    const Iter& operator++ ()
    {
        ++_pos;
        return *this;
    }
 
    private:
    int _pos;
    const IntVector *_p_vec;
};
 
class IntVector
{
    public:
    IntVector ()
    {
    }
 
    int get (int col) const
    {
        return _data[ col ];
    }
    Iter begin () const
    {
        return Iter( this, 0 );
    }
 
    Iter end () const
    {
        return Iter( this, 100 );
    }
 
    void set (int index, int val)
    {
        _data[ index ] = val;
    }
 
    private:
   int _data[ 100 ];
};
 
int
Iter::operator* () const
{
     return _p_vec->get( _pos );
}
 

int main()
{
    IntVector v;
    for ( int i = 0; i < 100; i++ )
    {
        v.set( i , i );
    }
    for ( int i : v ) { cout << i << endl; }
}
```
注意这段代码中区间迭代时，不允许以引用修改IntVector中的元素。这是为了不使代码变长而影响代码的主要结构，所以并没添加返回引用类型的函数。
##区间迭代提升性能？
在我使用GCC4.6的有限的测试中，我并没看到区间迭代相对于标准STL迭代的性能提升，但好像可以和STL中的for_each拥有同样的性能。

##编译器支持
不幸的是，区间迭代的编译器支持的不好。MSVC11以后开始支持，GCC是4.6以后支持。

下一篇：静态表达式

上一篇：[lambda表达式][3]

译者：[toWriting.com](/)；翻译自：[Range-Based For Loops in C++11 - Cprogramming.com][4]

 [1]:/blog/2013/08/01/what-is-cpp11/
 [2]:/blog/2013/08/08/improved-type-inference-in-cpp11/
 [3]:/blog/2013/08/11/lambda-closures/
 [4]:http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html