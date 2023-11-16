---
title: "《Rust in Action》读书笔记"
date: 2022-04-11T11:49:51+08:00
draft: false
categories: ["阅读"]
slug: 2022/04/11/rust-in-action/
---

# Ch1: Rust介绍

## rust的安全性

rust程序员无需关心：
* 悬挂指针
* 数据竞争
* 内存溢出
* 迭代器失效
* 整形溢出(debug模式)

这些都是编译时保证的，除了安全还会带来额外的优势，比如如下代码：
```
fn for_each(v: &Vec<T>) {
     for item in 0..v.len() {
        work(v[things]);
     }
}
```
因为`v`是Vec<T>的只读引用，所以在for..in循环中，无需进行运行时的边界判断。

## 灵活性

Rust可以灵活的控制内存访问和内存布局，如下面例子：
```
use std::rc::Rc;
use std::sync::{Arc, Mutex};

fn main() {
    let a = 10; // 内存在栈上

    let b = Box::new(20); // 在堆上，只有一个owner

    let c = Rc::new(Box::new(30)); // 在堆上，多owner

    let d = Arc::new(Mutex::new(40)); // 在堆上，多owner, 可多线程访问

    println!("a: {:?}, b: {:?}, c: {:?}, d: {:?}", a, b, c, d);
}
```

灵活性的同时，切记Rust的三个原则：
1. 数据默认是只读的
2. 安全是核心
3. 安全基本上是编译时保证，不引入运行时开销


# ch2 语言基础

## 显式生命周期

所有对象都有生命周期，这是Rust的安全检查系统的一个基石，有生命周期才保证所有对象的访问是有效的。一般无需手工指定生命周期，编译器可以自行推导，但在返回引用的函数中，需要显式指定生命周期，辅助编译器来决策访问的安全性。

```rust
fn test<'a>(i: &'a i32, j: &'a i32) ->  &'a i32 {
    if i % 2 == 0 {
        i
    }else {
        j
    }
}
```

同一个生命周期下的所有对象，必须保证在最后一次访问的时候都是有效的。


## 泛型
```
fn add<T>(i: T, j: T) -> T { 
 i + j
}
```
上面定义了一个可以接受任意类型T的add函数，和C++里面的template类似，但遗憾的是上面的代码并不会报错：` consider restricting type parameter T`。
需要显示约定T支持`+`操作。

```
fn add<T: std::ops::Add<Output = T>>(i: T, j: T) -> T {
 i + j
}
```

这样带来和C++模板方法不一样的设计，C++中会特化函数时检测T是否支持`+`，若不满足则报错，这相当于把问题抛给了调用方，调用方需要深入模板方法的源码探索编译失败的原因。而Rust是在定义泛型函数时检查合法性，要求限制T的范围，调用方无需关注实现细节。

`std::ops::Add`是Rust里面的操作符，是一种Trait, 可以类比其他语言里面的接口。

## 数组

Array是定长的同类变量的集合，有2种定义方式：
```
    let a1 = [1, 2, 3]; // 里面包含1, 2, 3三个数
    let a2 = [1; 10];  // 里面包含10个1
```

有以下注意事项：
1. [T; n]是数组的类型，长度n作为类型的一部分。所以[i32, 1]和[i32, 2]不是一个类型。
2. 数组的索引取值是有运行时区间检查的。

## Vector

Vec<T>是变长的容器，比Array更灵活。
```
    let v1: Vec<i32> = Vec::with_capacity(5); // 预留size 5
    let mut v2 : Vec<i32> = Vec::new(); // 分配一个空的vec
    v2.push(3); // 插入元素
```

## 切片

Slice是Array、Vector、String的视图，由一个指针和长度来表示。
```
    let a = [1, 2, 3];
    let s = &a[1..2];
    print!("len of slice={}", s.len()); // len of slice=1
```

## 三方库

crate是Rust中三方库的术语，类比于其他语言的package, library。

## rustup

rustup可以用来切换编译器的版本，或者rustup doc查看文档。

# ch3 复合结构

## 特殊的返回类型
```
fn no_return() { // 没有返回值
    print!("no return");
}

fn no_return_explicit() -> () { // 显式定义没有返回值
    print!("no return");
}

fn infinite() -> ! { // 返回'Never'结果，表示函数总也不会返回
    loop {

    }
}
```

## 结构体

结构体是一种可以包含多个成员的复合机构，不支持继承。没有构造函数，约定通过new创建一个新的结构体实例。

```rust
struct HostName(String); // newtype, 给String起了一个新名字
struct File { // 结构体
    name: String,
    data: Vec<u8>
}

impl File { // 结构体的实现部分
 fn new(name: &str) -> File { 
    File { 
    name: String::from(name), 
    data: Vec::new(), 
    }
 }
}

fn main() {

    let ordinary_string = String::from("baidu.com");
    let host = HostName ( ordinary_string.clone() );
    let f = File{
        name: String::from("text.txt"),
        data: vec![1]};

}
```

## 枚举

enum是一种包含多个已知变量的符合类型。
```
enum Suit {
 Clubs,
 Spades,
 Diamonds,
 Hearts,
}
```

enum相较于C++的enum更强大：
1. 可以通过match匹配
2. 支持impl定义方法
3. 枚举值还可以包含变量

```
enum Card {
 King(Suit), 
 Queen(Suit), 
 Jack(Suit), 
 Ace(Suit), 
 Pip(Suit, usize), 
}
```

## trait

#[derive(Debug)]是一个宏，给Type引入了Debug的Trait。可以通过impl for来为Type实现Trait。

```
impl Read for File { 
    fn read(self: &File, save_to: &mut Vec<u8>) -> Result<usize, String> {
        Ok(0) 
    }
}
```


# ch4 生命周期

## 术语：
* ownership
   所有变量都有唯一的所有者，当变量不在使用的时候，会触发资源回收。
* lifetime
   在变量的生命周期内访问变量是合法的。函数内局部变量在函数返回后失效。全局变量的生命周期和程序一致。
* borrow
  借用指的是获取了变量的访问权限。虽然一个变量有唯一的owner, 但是可以有多个借用者。

## 移动语义

内置类型总是Copy语意，其他默认总是移动语意:
```
fn test_int_fun(i: i32) {

}

fn test_string_fun(i: String) {

}

fn main() {
    let s1 = String::from("test");
    test_string_fun(s1);
    print!("s1{}", s1); // s1失效，已经被move到test_string_fun

    let i1 = 10;
    test_int_fun(i1);
    print!("i1{}", i1); // i1正常

}
```

移动发生的2个时机：
1. 赋值
2. 作为函数的参数或者返回值

## 析构

可以实现drop(&mut self)，来做一些清理的事情。

## 解决所有者的问题

1. 使用引用的方式传递参数
2. 变量使用更小的生命周期
3. 副本，对一些小对象可以复制一份新的数据。可以用Copy或者Clone trait, 区别是Copy是隐式的，Clone是显式调用。实现了Copy后，原理move的地方会变成Copy语义。
4. 使用Rc包装, Rc<T>类似于C++里面的shared_ptr。

# ch5 深入数据

同样的数据，可以有不同的解释，比如`let a: f32 = 42.42;`, 如果把a的内存强转为u32则它的值是`1110027796`。rust中的unsafe代码std::mem::transmute可以做这个转换。

整形是精确表达数值的，所以定长的整形有范围限制，超出则会溢出，Rust会在溢出时报错。

## 大小端

> When a value larger than byte is stored or serialized into multiple bytes, the choice of the order in which the component bytes are stored is called byte order, or endian, or endianness.

大小端，又叫做字节序，是字节的一种排列方式。和字节内的bit顺序无关。一般只有整形和浮点型受字节序影响，字符串属于单字节的数组不受字节序影响。

## 浮点数

用定长的字节表示任意范围的小数，所以浮点数是不精确的。是用`sign * m * pow(2, e)`的形式表达的sign是符号，m表示尾数(mantissa), e表示幂次方(exponent)。

各种浮点数的标准，是约定了sign, m和e三部分的分配和取值方式。

## CPU的指令也是数据

# ch6 内存

## Cow

写时复制的智能指针。

## 指针

可以从一个引用中强制获取指针。指针的解引用必须在unsafe中进行。
```
    let a: i64 = 42;
    let a_ptr = &a as *const i64; 
    println!("a: {} ({:p})", a, a_ptr);
    unsafe {
        println!("p val: {} ", *a_ptr);
    } 
```

## 智能指针

C++中的智能指针的作用主要是管理裸指针，防止资源泄露。而Rust中的智能指针主要有2个作用：

```
    let a = std::rc::Rc::new(3); // 内存在堆上分配
    let b = std::boxed::Box::new(3); // 可以有多个owner
```