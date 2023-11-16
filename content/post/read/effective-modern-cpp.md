---
title: "《Effective Modern C++》读书笔记"
date: 2022-04-08T13:45:07+08:00
categories: ["阅读"]
slug: 2022/04/08/effective-modern-cpp/
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

# item4 查看推导的类型

* 通过IDE
* 通过编译错误
* 通过typeid或者boost::typeindex

但需要注意，ide或者typeid都不一定精确。

# item5 auto

auto可以减少问题，比如未初始化，性能损失，也可以较少拼写。

auto在承接lambda时比std::function有更好的性能。
```
  std::unordered_map<std::string, int> m;
  for (const std::pair<std::string, int>& p : m) { // 类型错误，应该是std::pair<const std::string, int>, 会带来临时对象，应该换成auto

  }
```

推荐尽量多的使用auto, auto的最大副作用是浏览代码是不知道类型，这个完全可以通过IDE的提示来解决。

## item6 当出现非预期推导时，为auto使用显式类型转换

```
  std::vector<bool> features() {}
  process(bool) {}
  auto b = features()[5];
  process(b); // 未知行为，类型是std::vector<bool>::reference
```

关于为何返回std::vector<bool>::reference, 具体见: [std::vector<bool>](https://en.cppreference.com/w/cpp/container/vector_bool)。总之在有中间隐式代理类型的情况下，auto就不那么好用了。

出现这种情况最好的方式是给一个显式的类型转换。

## item7 初始化()和{}的不同。

初始化的三种方式：

```
int x = 0;
int x(0);
int x{0}; // c++11引入，可以用来给容器初始化一些值，也可以给成员变量初始化值。 
```
C++11之前的初始化无法表达给一个容器初始化几个值。

{}可以作为统一的初始化方式，可以应用在所有场景，并且独有：

```
float a, b, c;
int x{a+b+c}; // 编译错误
int y = a+b+c; //ok

widget w3{}; // 可以解决most vexing parse问题。
```
[most vexing parse](https://en.wikipedia.org/wiki/Most_vexing_parse)

但{}初始化也不是完美的，在参数有std::initializer_list时，编译器会优先将{}转换成initializer_list。

## 优先使用nullptr

```
 void f(int);
 void f(void*);
```

基于整型和指针重载会产生二义性，而使用nullptr可以解决这个问题。使用nullptr也可以让代码更明确。

#item 9 优先使用alias而不是typedef。

#item 10 使用带scope的enum。

```
  enum Color {Write, Black, Red};
  enum class Color {Write, Black, Red}; // enum class, with scope
```

enum class除了不会污染scope之外，还有一个好处是强类型。另外一个优势是enum class支持前置声明，而c++98的num是不支持的，因为不知道num的size。

#11 使用deleted

deleted比private不实现函数的优势是编译期更好的警告，deleted一般是要Public。

deleted作用于普通函数：
```
  bool isLucky(int);
  bool isLucky(bool) = deleted;
  bool isLucky(bool) = deleted;
```

# item 12 总是使用override
新奇的特性：
```
class Widget {
 void doWork() &; // 作用于左值的this
 void doWork() &&; // 作用于右值的this
}
```

因为override需要派生类的函数定义满足一定的形似性，所以有可能你写的override的虚函数并没有override，而override关键字可以帮你提醒。

```
  struct Widget {
      Data s;
      Data& data() {
          return s;
      }
  };

  int main()
  {

      Widget w1;
      auto d = w1.data(); // copy data
  }
```

# item13 使用const_iterator

使用cbegin和cend。

# item14 假如函数不会抛异常，定义noexcept

跳读

# item15 尽可能的使用constexpr

所有的constexpr值是const的，但反之不然。
自定义类型也可以是constexpr。

#item16 const函数必须要保证线程安全

单个变量可以使用atomic, 但多个变量只能使用Mutex保证线程安全。

#item17 了解自动生成的函数

自动生成的方法，一般是内联和Public的。如果在子类的虚构中调用，且父类的析构是虚函数，则生成都函数也是虚函数。

默认生成的函数，只有在使用的时候，才会自动生成。

拷贝构造和赋值操作符是相互独立的。但移动构造和移动赋值是有关系的，定义了其中一个，编译器则不会自动生成另一个。

如果定义了拷贝构造或赋值操作符，则不会自动生成移动函数，反之亦然。

定义了析构函数后，将不会自动生成move函数。

#item18 unique_ptr

unique_ptr不允许赋值，但可以reset。
如果没有自定义的delete函数，unique_ptr的size和裸指针是一样的。
unique_ptr<[]>是数组形式。
unique_ptr可以转化成shared_ptr。

#item19 shared_ptr

成本：
1. dynamic内存，为了保存引用计数
2. 引用计数的原子性

和unique_ptr不同的是，deleter不是类型的一部分。shared_ptr对象会指向一个control block，里面包含引用计数，弱引用技术。

推荐使用mark_shared, 不应该把要给raw指针传给2个shared_ptr。 

# item20 weak_prt

weak本身不能解引用。将weak_ptr转成shared_ptr的2中方式：
```

std::weak_ptr<Widget> wpw(spw);
auto spw1 = wp2.lock();
std::shared_ptr<Widget> spw2(wpw); // 如果wpw expired, 则抛出异常                      
```

weak_ptr也指向和shared_ptr相同的控制快。

# item21 优先使用make_shared

make_shared的好处是：
1. 防止写错，将一个指针赋值给2个shared_ptr
2. 防止异常时，资源泄露

make_shared的不足，std::initializer_list不支持。

# item22 Impl模式时，注意实现相关的成员函数

impl模式的好处是减少对头文件的依赖，加快构建速度。
但std::unique_ptr<Impl>时需要实现Impl的一些默认函数，否则会编译失败，因为编译器默认生成函数时，并不知道Impl的实现。

# item23 理解std::move和std::forward

std::move和std::foward并不总是有效。他们都是编译时的东西，不会产生任何运行时代码。

std::move只是将输入转成右值，没有move，只有cast。
std::foward只有在输入是右值的时候才转换右值。

# item24 

模板参数T&&, 只有输入是右值时，才是右值引用。auto&&同理。

# item26 避免对universal reference重载。

universal reference是最贪婪的匹配。


// todo
