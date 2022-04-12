---
title: "《Effective Modern C++》读书笔记"
date: 2022-04-08T13:45:07+08:00
draft: true
---

## introduction

能取到地址的是左值，否则为右值。函数的参数parameter总是左值，但传进来的arguments可能是右值。

callable object可以当做函数使用，lambda又叫做闭包。
声明引入了类型和名字，定义引入了实现。


## item1 模板类型推导

* 模板方法，参数是&&时，可以传入lvalue参数, 但会退化成lvalue。
* 普通方法参数是&&时，不能传lvalue（规范了一种move的语意）。
* 模板方法，parameter定义为T, 传入数组会退化成指针，传入T&, 则保留数组。

## item2 auto

auto的推导规则基本和模板类型推导一致。除了auto x = {1}这个形式，x会被推导成std::initializer_list。
auto在C++14中可以作为函数的返回值推导，还可以用在labmba中的参数类型推导。

## item3 decltype

decltype推导的类型（变量或表达式）一般总是原始类型。特殊情况是decltype作为返回值推导时，如果返回值是左值，则推导为T&：

```cpp
auto func1(int& val) -> decltype(val) {
    return val;
}

int main()
{
    int a = 1;
    func1(a) = 3; // func1 返回int&
}
```

decltype(auto)结合是C++14的一个特性，可以方便的推导返回值的类型。