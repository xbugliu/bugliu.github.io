+++
title = "Boost Lambda vs Stand Lambda"
date = "2013-12-13"
slug = "2013/12/13/boost-lambda-vs-stand-lambda"
tags =["C++11 Lambda","C++ Lambda","boost Lambda","boost Lambda实例","boost Lambda用法"]
description = "boost Lambda使用的几个例子"
+++

我们知道C++11已经支持了Lambda表达式，大部分新的编译器都已支持Lambda。但boost里面的Lambda还是有存在的必要，因为并不是每个人都有选择自己编译环境的权利。

让我们通过几个例子对比C++11.Lambda来学习一下boost.Lambda的用法，详细的C++11.Lambda用法可以看这里：[C++11系列-Lambda表达式][1]

###1.a boost.Lambda：构造一个functor

boost.Lambda的使用是基于placeholder: `boost::Lambda::_1`,`boost::Lambda::_2`,`boost::Lambda::_3`可以理解为Lambda表达式的第一、第二、第三个参数。Lambda的出现利索地解决了STL算法库函数的使用不便。让我们首先看一个最简单的boost.Lambda与std::for_each结合的例子:

```cpp
std::vector<int> vecIn;
vecIn.push_back(1);
vecIn.push_back(2);
vecIn.push_back(3);

std::cout<<"the orgin values in vecIn："<<std::endl;
std::for_each(vecIn.begin(), vecIn.end(), std::cout<<boost::Lambda::_1<<",");
std::cout<<std::endl;
```

上面的例子，使用std::for_each遍历vector中的元素并打印内容。boost::Lambda::_1这个占位符表示for_each遍历时传进来的第一个参数，`std::cout<<boost::Lambda::_1<<","`可以理解为临时创建出来的匿名函数，函数的定义是`void(int)`。

让我们对比一下C++11标准的写法:
###1.b C++11.Lambda：构造一个functor

```cpp
std::vector<int> vecIn;
vecIn.push_back(1);
vecIn.push_back(2);
vecIn.push_back(3);

std::cout<<"the orgin value in vecIn："<<std::endl;
std::for_each(vecIn.begin(), vecIn.end(), [](const int & val)
{
std::cout<<val<<",";
});
std::cout<<std::endl;
```
一眼望去竟是boost的Lambda用法简洁。

###2.a boost.Lambda：修改参数的内容

上面的例子，我们使用Lambda表达式，借助于for_each，对vector中的元素进行了访问，那我们可以修改vector的内容吗？

```cpp
std::cout<<"the values in vecIn after Square："<<std::endl;
std::for_each(vecIn.begin(), vecIn.end(), boost::Lambda::_1 *= boost::Lambda::_1);
std::for_each(vecIn.begin(), vecIn.end(), std::cout<<boost::Lambda::_1<<",");//打印
std::cout<<std::endl;
```
上面的例子，功能是对vecIn中的值求平方。占位符boost::Lambda::_1直接用引用的方式得到了传入的参数（vecIn的元素），结果直接改写进了vecIn。

###2.b C++11.Lambda：修改参数的内容

修改1.b函数定义为引用即可

```cpp
std::cout<<"the values in vecIn after Square："<<std::endl;
std::for_each(vecIn.begin(), vecIn.end(), [](int & val)
{
val *= val;
});
//打印：
std::for_each(vecIn.begin(), vecIn.end(), [](const int & val)
{
std::cout<<val<<",";
});
std::cout<<std::endl;
```
###3.a boost.Lambda：变量捕获

boost.Lambda可以很方便的以引用的方式捕获local变量，看例子：

```cpp
int sum = 0;
std::cout<<"the sum of values in vecIn："<<std::endl;
std::for_each(vecIn.begin(), vecIn.end(), sum += boost::Lambda::_1);
std::cout<<sum<<std::endl;
```

###3.b C++11.Lambda：变量捕获

C++11捕获变量，可控性更强，我们可以指定捕获方式：

```cpp
int sum = 0;
std::cout<<"the sum of values in vecIn："<<std::endl;
std::for_each(vecIn.begin(), vecIn.end(), [&sum](int & val)
{
sum += val;
});
std::cout<<sum<<std::endl;
```

###4.a boost.Lambda：返回值

有时需要匿名函数返回结果，比如作为std::find_if类似函数的Predicate函数时，这种能力boost.Lambda当然也是胜任的，整个Lambda表达式的结果，即是函数的返回值。

```cpp
boost::function<bool(int)> IsOdd = boost::Lambda::_1 % 2 != 0; 

std::cout<<"100 is Odd？ "<<std::boolalpha<<IsOdd(100)<<std::endl;
```
上面的例子创建了一个检测参数是否是奇数的匿名函数，并保存到IsOdd对象中，然后对其进行了调用。例子同样演示了如何将一个boost.Lambda表达式存储起来，后续使用的方法。

###4.b C++11.Lambda：返回值

C++11的返回值，必须使用return语句，返回值的类型有些时候也需要显示指定。
```cpp
std::function<bool(int)> IsOdd = (std::function<bool(int)>)[](int val){return val % 2 != 0;};

std::cout<<"100 is Odd？ "<<std::boolalpha<<IsOdd(100)<<std::endl;
```

###5. 上面四个例子的运行结果

    the orgin values in vecIn：
    1,2,3,
    the values in vecIn after Square：
    1,4,9,
    the sum of values in vecIn：
    14
    100 is Odd？ false

[1]:/blog/2013/08/11/lambda-closures/
