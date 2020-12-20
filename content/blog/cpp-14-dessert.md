+++
title = "C++14-大餐后的甜点"
date = "2014-08-25"
slug = "2014/08/25/cpp-14-dessert"
tags =["C++14 范型lambda"]
description = ""
categories = ["开发"]
+++

这次C++标准委员会快速的通过了最新的C++标准:C++14，要知道C++11可是一再跳票后的产物。此次快速的发布的缘由可能和C++14的改动较小有关，C++之父Bjarne Stroustrup也说，相比与C++11来说，C++14的改动是[谨小甚微的][1]。相信C++14不会给我们带来像C++11那样的震撼，所以我们只能期待下一个版本C++17了。

{% img pull-right /images/posts/cpp-14-dessert/wg21-timeline.png %}


但Bjarne还说了，C++永远是心向开发者的，C++14将给开发者大开方面之门。关于C++14更详细的细节可以看维基百科:[C++14][2],这里讲下自己感兴趣的特性：

###语言改变

**范型lambda**

在C++11下，如果你想要打印出一个数的平方，可能需要这样：

{% codeblock lang:cpp %}
auto square_int = [](int x) { return x * x; };
auto square_double = [](double x) { return x * x; };

std::cout<<square_int(10)<<std::endl;
std::cout<<square_int(10.1)<<std::endl;

{% endcodeblock%}

为了保持函数的局部性，我们才选择的lambda，但C++11的lambda却导致多个类型时代码膨胀且重复，此时我们需要回过头来借助全局的模板了。

但C++14可以完美的解决上面的问题，因为C++14中lambda的参数可以用auto代替具体的类型：

{% codeblock lang:cpp %}
auto square = [](auto x) { return x * x; };

std::cout<<square_int(10)<<std::endl;
std::cout<<square_int(10.1)<<std::endl;

{% endcodeblock%}

**auto返回类型**

C++11支持auto关键字，用于变量的自动类型推导。但由于时间限制，C++标准委员会并没有让auto也支持函数的返回值类型自动推导，现在C++14支持了。这将会在返回类内部类型的成员函数书写上减少好多工作量：

{% codeblock lang:cpp %}
struct Wiget 
{
  enum Status{show, hide}
  auto getStatus(); 
};
auto Wiget::getStatus() { return show; }
{% endcodeblock%}



###编译器支持
曾经被标准折磨的死去活来的编译器如今越挫越勇。标准出来的快，编译器支持的更快。CLang（3.4）半年前就宣布已完全支持C++14(draft)特性（语言和库）。本人电脑上的GCC4.9.1也已部分支持C++14特性。但公司的开发环境要支持C++14可就难了。

参考：

https://isocpp.org/std/status

http://cpprocks.com/c1114-compiler-and-library-shootout/

http://cpprocks.com/an-overview-of-c14-language-features/

http://llvm.org/releases/3.4/tools/clang/docs/ReleaseNotes.html

https://solarianprogrammer.com/2014/08/28/cpp-14-lambda-tutorial/

[1]:http://electronicdesign.com/dev-tools/bjarne-stroustrup-talks-about-c14
[2]:http://en.wikipedia.org/wiki/C%2B%2B14
[3]:/blog/2013/08/11/lambda-closures/
