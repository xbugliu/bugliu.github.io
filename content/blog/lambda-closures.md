+++
title = "C++11系列-lambda函数"
date = "2013-08-11"
slug = "2013/08/11/lambda-closures"
tags =["C++","C++11","lambda","匿名函数","闭包"]
description = "C++11的lambda简明教程，介绍了lambda为什么会出现，lambda的基本语、用处、用法、如何实现及各种注意事项"
+++
{% img pull-left /images/posts/lambda-closures/lambda.png 66 90 'lambda'%}
C++11一个最激动人心的特性是支持创建lambda函数（有时称为闭包）。这意味着什么？一个Lambda函数是一个可以内联写在你代码中的函数（通常也会传递给另外的函数，类似于仿函数或函数指针）。使用Lambda，创建机动函数会更简单，而以前你必须创建一个有名函数。在这篇文章中，我先用一些例子解释为什么lambda很酷，然后我会讲解可能会用到的关于lambda的所有细节。

##为什么Lambda很酷
想象你有一个地址簿类，并且你想要提供一个可供检索的函数。你可能会提供一个简单的函数，接受一个字符串然后返回满足所有字符串的地址。有时有些用户可能希望这样。不过假如他们只是想检索域名或者检索用户名并且忽略域名结果；或者检索出现在其他列表中的所有Email地址。这里可能有许多可能的检索方式。除了类中集成所有这些搜索选项，提供一个通用的查找方法，这个方法接受一个查找规则的函数，这样不是更好些吗？让我们叫这个函数findMatchingAddresses，它接受一个函数或仿函数对象。
```cpp
#include <string>
#include <vector>

class AddressBook
{
    public:
    // 使用模板可以是我们忽略函数、仿函数和Lambda的不同
    template<typename Func>
    std::vector<std::string> findMatchingAddresses (Func func)
    { 
        std::vector<std::string> results;
        for ( auto itr = _addresses.begin(), end = _addresses.end(); itr != end; ++itr )
        {
            // 调用传递到findMatchingAddresses的函数并检测是否匹配规则
            if ( func( *itr ) )
            {
                results.push_back( *itr );
            }
        }
        return results;
    }

    private:
    std::vector<std::string> _addresses;
};
```
任何人可以传递一个包含地址查找逻辑的函数给findMatchingAddresses。假如这个函数返回真，则得到相应的地址，地址将被返回。这种方式在以前的C++中一样支持，不过却遭遇一个致命缺陷：创建函数非常不方便。你必须先在其他地方定义好函数，你才能使用它。这就是Lambda出现的原因。
##基本Lambda语法
在我们解决这个问题之前，让我们看一下真实的lambda基本语法。
```cpp
#include <iostream>

using namespace std;

int main()
{
    auto func = [] () { cout << "Hello world"; };
    func(); // now call the function
}
```
好，你找到lambda了吗？它以[]开始。这个标识，叫做捕获指定器，它告诉编译器我们要创建一个lambda表达式。你将看到[](或者里面有变量）在每一个lambda函数的开始。

接着，像其他函数一样，我们需要一个参数列表：()。返回值呢？答案是我们不需要指定。在C++11中，假如编译器可以推导lambda函数的返回值，它将帮你做这件事而不需你显式指定。在这个例子里，编译器知道函数没有返回值。我们只是有一个打印“hello world"的函数体。这一行事实上不会触发关于打印的任何事：我们仅仅是创建了一个函数在这里。基本上相当于定义了一个普通函数。

我们在下面一行调用了这个lambda函数：func()，像调用其它普通函数一样。顺便看到，配合auto做这些事情是多么简单！你不用担心函数指针的丑陋语法。
##在我们的例子中应用Lambda
让我们看看怎样将lambda应用到我们地址簿例子里，首先我们创建一个查找包含“.org"的email地址的简单函数。
```cpp
AddressBook global_address_book;

vector<string> findAddressesFromOrgs ()
{
    return global_address_book.findMatchingAddresses( 
        // we're declaring a lambda here; the [] signals the start
        [] (const string& addr) { return addr.find( ".org" ) != string::npos; } 
    );
}
```
<!-- more -->
再一次，我们以捕获指示符[]开始，但这一次我们有一个参数：地址，并且我们检测地址中是否含有“.org"。再一次说明，lambda的函数体并没有在这里执行；它只会在函数findMatchingAddresses内，当函数变量被使用时，lambda中的代码才会执行。换句话说，findMatchingAddresses的每个循环中会调用lambda函数，并传给它一个地址作为参数，然后这个函数检测地址是否包含“.org"。
##变量捕获
虽然这些简单的lambda用法也不错，但变量捕获才是成就lambda卓越的秘方。假如你想创建一个查找包含指定名字的短函数。如果可以写出这样的代码是不是非常不错？
```cpp
// read in the name from a user, which we want to search
string name;
cin>> name;
return global_address_book.findMatchingAddresses( 
    // 注意lambda函数使用了变量 'name'
    [&] (const string& addr) { return addr.find( name ) != string::npos; } 
);
```
可以证明示例代码是合法的，并且它展现了lambda函数的价值。我们可以获取声明在lambda函数之外的变量(name)，并在lambda之内使用。当findMatchingAddresses调用我们的lambda函数，函数体会被执行，当addr.find被调用，它处理用户代码传进的name。为了使这可以运行的唯一要做的事是捕获变量。我用[&]捕获指示做这件事，而不是用[]。[]是告诉编译器不捕获任何变量，而[&]是告诉编译器去捕获变量。

是不是不可思议？我们创建了一个简单的可以捕获变量的函数，并将它传给find函数，所有这些只用了几行代码。如果不用C++11实现这些，我们需要创建一个仿函数或者给AddressBook类添加一个特殊方法。用C++11，我们可以轻易实现一个简单的接口函数，但支持各种检索的功能。

只是好玩，我们想查找email地址小于某个特殊长度的地址。我们可以再一次轻松实现：
```cpp
int min_len = 0;
cin >> min_len;
return global_address_book.find( [&] (const string& addr) { return addr.length() >= min_len; } );
```
你将习惯于"})"，这是lambda结束的标准语法，你开始阅读lambda相关代码或在你自己的代码中使用lambda越多，你将越多的看到这个小的代码片段。
##Lambda和STL
毋庸质疑，lambda最大的一个优势是在使用STL中的算法(algorithms)库时。以前使用像for_each这样的算法是个体力活。然而现在使用for_each或其他STL算法就好像自己写普通循环一样。对比一下：
```cpp
vector<int> v;
v.push_back( 1 );
v.push_back( 2 );
//...
for ( auto itr = v.begin(), end = v.end(); itr != end; itr++ )
{
    cout << *itr;
}
```
和：
```cpp
vector<int> v;
v.push_back( 1 );
v.push_back( 2 );
//...
for_each( v.begin(), v.end(), [] (int val)
{
    cout << val;
} );
```
要我说后一种代码更漂亮，好在它的可读性和结构，也像个普通循环，并且可以利用上for_each可以提供的普通循环没有的一些优势，比如保证你有正确的结束条件。现在你可能会想，这会不会影响性能？意想不到的结论是for_each和普通循环有一样的性能，有时甚至更快（原因是循环展开）。

我希望STL的例子告诉你lambda不仅仅是创建函数的一种简便方式，它创造了一种新的编码方式，当你的代码作为数据处理函数时，你可以抽象处理特殊数据结构的方式。for_each适用于List，但是如果有处理“树”的类似函数是不是很酷？所有你要做的只是写处理每个节点的代码，而无需关心遍历算法。这种一个函数管理数据，将具体的数据处理过程委托到另一个函数的分解方式很有用。使用lambda，C++允许我们这种新的编程方式。这是我们以前没有的，但for_each不是新的，只不过以前我们不想用罢了。
##继续新的lambda语法
其实参数列表像返回值一样都是可选的，如果你想创建一个不带参数的函数的话。或许最短的lambda是这样的：
```cpp
[]{}
```
这是一个即没有参数又什么也不干的函数。一个稍有内容的函数：
```cpp
using namespace std;
#include <iostream>

int main()
{
    [] { cout << "Hello, my Greek friends"; }();
}
```
个人来讲，我不认可省略参数列表的价值。我认为[]和()的组合结构帮助lambda函数在代码上更出色。
###返回值
如果你的lambda函数没有return语句，则默认返回void。假如你有一个简单的返回语句，编译器将推导返回值的类型：
```cpp
[](){return 1;} //编译器知道这是返回一个整型
```
假如你写一个更复杂些的lambda函数，不止一个返回值，你应该指定返回类型（有些编译器，像GCC，即使你有多于一个返回值也不需要你这样做，但标准不保证这一点）。
Lambda函数利用[C++11可选的新返回值语法][1]将返回值放在函数的后边。事实上假如你想指定返回类型，你一定要这样做。这里有一个显式指定返回值类型的简单例子：
```cpp
[] () -> int { return 1; } // 现在你告诉编译器你想要什么
```
###抛异常指示（throw)
虽然C++标准委员会决定不赞成使用throw指示符，但throw还没被移除C++。这里有许多检测throw指示符的静态代码检测工具，像PC link。假如你使用这些工具中的一个去进行编译时异常检测，你肯定想知道你的lambda函数会抛出什么异常。这样做的主要原因可能是当你传递一个lambda函数到另一个函数中，而这个函数期望你的lambda只能抛出指定的异常。给你的lambda函数添加一个异常指示，将允许PC link这样的工具去帮你检测。如果你想这样做是可以的。这有一个无参且不抛出异常的lambda函数：
```cpp
[] () throw () { /* 你不希望抛出异常*/ }
```
##Lambda函数是如何实现的？
变量捕获的魔法是如何运作的？其实lambda实现的方法是创建一个简略的类。这个类重载了operator()，所以表现的像个普通函数。一个lambda函数是这个类的实例。当这个类构造的时候，所有捕获的变量被传送到类中并保存为成员变量。事实上这类似于以前就支持的functor。C++11的优势是这一切都变得非常简单。你可以在任意时候使用它，而不仅仅是极少的特殊场合去写一整个的类。

C++为性能计，实际上提供了好几种灵活的捕捉变量的方式，所有这些都是靠捕捉指示控制的[]。你已经看到了两种情况，[]中什么也没有则不捕获变量，用&则变量以引用捕获。如果你创建了一个空[]的lambda函数，C++将创建一个普通的函数而不是类。这里有完整的捕获选项：
    
    []	不捕获任何变量
    [&]	以引用方式捕获所有变量
    [=]	用值的方式捕获所有变量（可能被编译器优化为const &)
    [=, &foo] 以引用捕获foo, 但其余变量都靠值捕获
    [bar] 以值方式捕获bar; 不捕获其它变量
    [this] 捕获所在类的this指针
注意最后一个捕获选项，如果你已经指定了一个默认的捕获（=或者&）那么也包含this。但是能捕获this指针的能力非常重要，这意味着写函数时你不需要区分局部变量和类属性的不同，两者都可以获取到。酷的是你不需显式指定this指针。它真的像你在写一个内联函数。
```cpp
class Foo
{
public:
    Foo () : _x( 3 ) {}
    void func ()
    {
        // a very silly, but illustrative way of printing out the value of _x
        [this] () { cout << _x; } ();
    }

private:
        int _x;
};

int main()
{
    Foo f;
    f.func();
}
```
###捕获引用的优缺点
以引用捕获变量时，可以在lambda函数内修改局部变量的值。这也意味着从一个函数中返回一个lambda函数，你不能以引用捕获变量，因为引用的值在函数返回时已经无效了。
##lambda函数的类型是什么？
创建lambda函数的一个原因是有些人创建了一个希望接受lambda函数的函数。我们已经看到了我们使用模板去接收lambda函数作为参数，并且使用auto去接这个lambda函数作为一个局部变量。但是你如何命名指定的lambda函数？因为像前面看到的一样，每一个lambda函数都实现为一个独立的类，所以即使是拥有相同类型和返回值的lambda函数也是不同的类型。但C++11提供了一个便捷的外敷类去存储任何类型的函数，lambda函数、仿函数和函数指针。
###std::function
新的std::function是传递lambda函数的最好的方式，不管是传递参数还是返回值。它允许你在模板中指定参数列表和返回值的确切类型。这里有AddressBook的例子，这次我们使用std::function代替模板。注意我们用到了'functional'头文件。
```cpp
#include <functional>
#include <vector>

class AddressBook
{
    public:
    std::vector<string> findMatchingAddresses (std::function<bool (const string&)> func)
    { 
        std::vector<string> results;
        for ( auto itr = _addresses.begin(), end = _addresses.end(); itr != end; ++itr )
        {
            // 调用传递到findMatchingAddresses的函数并检测是否匹配规则
            if ( func( *itr ) )
            {
                results.push_back( *itr );
            }
        }
        return results;
    }

    private:
    std::vector<string> _addresses;
};
```
std::function较模板的一大优势是，使用模板你必须将整个函数放到头文件中，而std::function则不用。当你的代码变化频繁并且被好多代码文件引用时，这会非常有用。

假如你想检测std::function是否含有一个有效的函数，把它当作boolean就可以了：
```cpp
std::function<int ()> func;
// 检测是否包含函数
if ( func ) 
{
    // if we did have a function, call it
    func();
}
```
###关于函数指针的提示
在最终的C++11标准中，假如你有一个指定空捕获列表的lambda函数，那它将像普通函数一样并可以被赋值到一个函数指针。这有一个作为指针使用空捕获列表lambda的例子：
```cpp
typedef int (*func)();
func f = [] () -> int { return 2; };
f();
```
这样是可以的，因为lambda函数没有捕获组，那也就不需要自己的类。它可以被编译成普通函数，运行被传递给普通函数。不幸的是这个特性没有被包含到MSVC10中，它被加入到标准的时间太晚了。
##使用Lambda实现委托
让我们在看一个lambda函数的例子，这次我们创建一个委托。当调用一个普通函数时，你只需要知道这个函数。而调用类的成员函数时，你需要知道两件东西：成员函数和类对象。这是func()和obj.method()的不同。要调用一个成员方法，你需要两者，仅仅将函数地址传进去是不够的，你需要一个对象去调用这个函数。

让我们看一个例子：
```cpp
#include <functional>
#include <string>

class EmailProcessor
{
public:
    void receiveMessage (const std::string& message)
    {
        if ( _handler_func ) 
        {
            _handler_func( message );
        }
        // other processing
    }
    void setHandlerFunc (std::function<void (const std::string&)> handler_func)
    {
        _handler_func = handler_func;
    }

private:
        std::function<void (const std::string&)> _handler_func;
};
```
这是注册回调函数到类里的很典型的模式，当感兴趣的事情发生时会调用回调函数。接着我们希望另一个类负责跟踪最长的消息（为什么这么做，或许你是一个无聊的管理员）。总之我们创建了如下的类：
```cpp
#include <string>

class MessageSizeStore
{
    MessageSizeStore () : _max_size( 0 ) {}
    void checkMessage (const std::string& message ) 
    {
        const int size = message.length();
        if ( size > _max_size )
        {
            _max_size = size;
        }
    }
    int getSize ()
    {
        return _max_size;
    }

private:
    int _max_size;
};
```
如果我们想让checkMessage在消息来时被调用，我们该怎么做？我们不能只传进checkMessage自己。它是个成员方法，所以它需要一个对象。
```cpp
EmailProcessor processor;
MessageSizeStore size_store;
processor.setHandlerFunc( checkMessage ); // 这行不通
```
我们需要绑定setHandleFunc和size_store变量的方法。恩，听起来是lambda的拿手好戏！
```cpp
EmailProcessor processor;
MessageSizeStore size_store;
processor.setHandlerFunc( 
        [&] (const std::string& message) { size_store.checkMessage( message ); } 
);
```
够酷吧，我们这里把lambda使成了胶水代码，允许我们传递普通函数到setHandleFunc，实际上调用的还是委托类的成员函数。
##总结
lambda函数会不会出现在C++代码的各种地方？我觉得会。我开始使用lambda函数在更富生产力的代码中，它们出现在所有的地方：一些为精简代码、一些为支持单元测试、一些代替以前用宏实现的代码。Year，我想lambda比其它希腊字母要酷。


下一篇：[区间迭代][3]

上一篇：[如何用auto、decltype和新的函数语法编写更好的代码][1]

译者：[toWriting.com](/)；翻译自：[C++11 - Lambda Closures, the Definitive Guide - Cprogramming.com][2]
 [1]:/blog/2013/08/08/improved-type-inference-in-cpp11/
 [2]:http://www.cprogramming.com/c++11/c++11-lambda-closures.html
 [3]:/blog/2013/08/20/ranged-for-loop/
