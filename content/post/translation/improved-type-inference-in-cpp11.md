+++
title = "C++11系列-改进的类型推导：auto、decltype和新的函数语法"
date = "2013-08-08"
slug = "2013/08/08/improved-type-inference-in-cpp11"
tags =["C++11","C++","auto","decltype","类型推导","自动类型"]
description = "介绍了C++11中的几个自动推导类型的关键字用法及新的函数定义语法"
categories = ["翻译"]

+++
C++11引入了一些新的实用的类型推导能力，这意味着你可以花费更少的时间去写那些编译器已经知道的东西。当然有些时候你需要帮助编译器或者你的编程伙伴。但是C++11，你可以在一些乏味的东西上花更少的时间，而多去关注逻辑本身。
## auto之乐
我们先快速回顾一下auto，万一你没有读[第一篇C++11文章][1]中关于auto的部分。在C++11中，如果编译器在定义一个变量的时候可以推断出变量的类型，不用写变量的类型，你只需写auto即可。
```cpp
int x = 4;
```
现在可以这样写：
```cpp
auto x = 4;
```
这当然不是auto预期的用途！它会在模板和迭代器的配合使用中闪耀光芒：
```cpp
vector<int> vec;
auto itr = vec.iterator();
```
其它时候auto也会非常有用。比如，你有一些下面格式的代码：
```cpp
template <typename BuiltType, typename Builder>
void
makeAndProcessObject (const Builder& builder)
{
    BuiltType val = builder.makeObject();
    // do stuff with val
}
```
上面的代码，我们看到这里需要两个模板参数：一个是Builder对象的类型，另一个是Builder创建出的对象的类型。糟糕的是创建出的类型无法被推导出，所以每次你必须这样调用：
```cpp
MyObjBuilder builder;
makeAndProcessObject<MyObj>( builder );
```
但是auto立即将丑陋的代码一扫无余，当Builder创建对象时不用写特殊代码了，你可以让C++帮你做：
```cpp
template <typename Builder>
void
makeAndProcessObject (const Builder& builder)
{
    auto val = builder.makeObject();
    // do stuff with val
}
```
现在你仅需一个模板参数，而且这个参数可以在函数调用的时候轻松推导：
```cpp
MyObjBuilder builder;
makeAndProcessObject( builder );
```
这样更易调用了，并且没丢失可读性，却更清晰了。
## decltype和新的返回值语法
现在你可能会说auto就这样吗，假如我想返回Builder创建的对象怎么办？我还是需要提供一个模板参数作为返回值的类型。好！这充分证明了标准委员有一群聪明的家伙，对这个问题他们早想好了一个完美的解决方案。这个方案由两部分组成：decltype和新的返回值语法。
### 新的返回值语法
让我们讲一下新的返回值语法，这个语法还能看到auto的另一个用处。在以前版本的C和C++中，返回值的类型必须写在函数的前面：
```cpp
int multiply(int x, int y);
```
在C++11中，你可以把返回类型放在函数声明的后面，用auto代替前面的返回类型，像这样：
```cpp
auto multiply(int x, int y) -> int;
```
但是为什么我要这样用？让我们看一个证明这个语法好处的例子。一个包含枚举的类：
```cpp
class Person
{
public:
    enum PersonType { ADULT, CHILD, SENIOR };
    void setPersonType (PersonType person_type);
    PersonType getPersonType ();
private:
    PersonType _person_type;
};
```
我们写了一个简单的类，里面有一个类型PersonType表明Person是小孩、成人和老人。不做特殊考虑，我们定义这些成员方法时会发生什么？
第一个设置方法，很简单，你可以使用枚举类型PersonType而不会有错误：
```cpp
void Person::setPersonType (PersonType person_type)
{
    _person_type = person_type;
}
```
而第二个方法却是一团糟。简单的代码却编译不过：
```cpp
// 编译器不知道PersonType是什么，因为PersonType会在Person类之外使用
PersonType Person::getPersonType ()
{
    return _person_type;
}
```
你必须要这样写，才能使返回值正常工作
```cpp
Person::PersonType Person::getPersonType ()
{
    return _person_type;
}
```
这可能不算大问题，不过会容易出错，尤其是牵连进模板的时候。

这就是新的返回值语法引进的原因。因为函数的返回值出现在函数的最后，而不是前面，你不需要补全类作用域。当编译器解析到返回值的时候，它已经知道返回值属于Person类，所以它也知道PersonType是什么。
```cpp
auto Person::getPersonType () -> PersonType
{
    return _person_type;
}
```
好，这确实不错，但它真的能帮助我们什么吗？我们还不能使用新的返回值语法去解决我们之前的问题，我们能吗？不能，让我们介绍新的概念：decltype。
## decltype
decltype是auto的反面兄弟。auto让你声明了一个指定类型的变量，decltype让你从一个变量（或表达式）中得到类型。我说的是什么？
```cpp
int x = 3;
decltype(x) y = x; // 相当于 auto y = x;
```
可以对基本上任何类型使用decltype，包括函数的返回值。嗯，听起来像个熟悉的问题，假如我们这样写：
```cpp
decltype( builder.makeObject() )
```
我们将得到makeObject的返回值类型，这能让我们指定makeAndProcessObject的返回类型。我们可以整合进新的返回值语法：
```cpp
template <typename Builder>
auto
makeAndProcessObject (const Builder& builder) -> decltype( builder.makeObject() )
{
    auto val = builder.makeObject();
    // do stuff with val
    return val;
}
```
这仅适用于新的返回值语法，因为旧的语法下，我们在声明函数返回值的时候无法引用函数参数，而新语法，所有的参数都是可访问的。
## auto：引用、指针和常量
下面要确定的一个问题是auto如何处理引用：
```cpp
int& foo();

auto bar = foo(); // int& or int?
```
答案是在C++11中，auto处理引用时默认是值类型，所以下面的代码bar是int。不过你可以指定&作为修饰符强制它作为引用：
```cpp
int& foo();

auto bar = foo(); // int
auto& baz = foo(); // int&
```
不过，假如你有一个指针auto则自动获取指针类型：
```cpp
int* foo();

auto p_bar = foo(); // int*
```
但是你也可以显式指定表明变量是一个指针：
```cpp
int* foo();
auto *p_baz = foo(); // int*
```
当处理引用时，你一样可以标记const，如果需要的话：
```cpp
int& foo();

const auto& baz = foo(); // const int&
```
或者指针：
```cpp
int* foo();
const int* const_foo();
const auto* p_bar = foo(); // const int*
auto p_bar = const_foo(); // const int*
```
所有这些都很自然，并且这遵循C++模板中类型推导的规则。

下一篇：[Lambda表达式简明教程][3]

上一篇：[什么是C++11][1]

译者：[toWriting.com](/)；翻译自：[C++11 - Auto, Decltype and return values after functions - Cprogramming.com][2]

 [1]:/blog/2013/08/01/what-is-cpp11/
 [2]:http://www.cprogramming.com/c++11/c++11-auto-decltype-return-value-after-function.html
 [3]:/blog/2013/08/11/lambda-closures/
