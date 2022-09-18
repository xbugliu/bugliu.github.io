+++
title = "《C++标准库第二版 上册》笔记"
date = "2013-12-18"
slug = "2013/12/18/read-notes-of-cpp-standard-1"
tags =["《C++标准库》笔记"]
categories = ["阅读"]
description = "《C++标准库第二版 上册》笔记"
+++

p63 a pair<> using a type that has only a nonconstant copy constructor will no longer compile.

p64 std::piecewise_construct is passed as the first argument is class Foo forced to use a constructor that takes the elements of the tuple rather than a tuple as a whole.

p65 The make_pair() function template enables you to create a value pair without writing the types explicitly.

p67 pair used in functions that return two values.

p68 tuples extend the concept of pairs to an arbitrary number.we can access elements with the get<>() function template.

p70 A tuple type can be a reference. For example:

```cpp
string s;
tuple<string&> t(s);
```
       
For element access, you must know the index of element at complier time.It is also a complier error if you pass a invalid index.
Make_tuple can create tuple without special the type of element.
By using references with make_tuple(), you can extract values of a tuple back to some other variables. e.g.
       
```cpp
std::tuple<int, float> t(1, 1.5);
int i = 0;
float f = 2.0;
make_tuple(std::ref(i), std::ref(f)) = t;
```
       
p72 std::tie() creates a tuple of references. the use of std::ignore allows ignoring tuple elements while parsing with tie()

p75 You can initialize a two-element tuple with a pair.Also, you can assign a pair to a two-element tuple.

p76 Tow types of smart pointer:

* shared_ptr. Multiple smart pointer can refer to the same object.
* unique_ptr. only one smart pointer can refer to this object at a time.

p77 shared_ptr. you can define other ways to clean up object.

p78 shared_ptr can't assign a new ordinary pointer. but can use reset function to reset the pointer.

p79 assigning a nullptr to a shared_ptr would delete the ownship.

p80 Smart pointer, when use new[] to create an array of object, you must define your own deleter.

p85 weak_ptr allows sharing but not owning an object.

p90 You should always directly initialize a smart pointer the moment you create the object with its associated resource.

p95 Aliasing constructor,The constructor taking another shared_pointer and an additional raw pointer.

p96 shared pointers are not thread safe.

p101 You can't copy or assgin a unique pointer if you use the ordinary copy semantics, but can use move semantics.

p102 assgining nullptr is also possible, which has the same effect as calling reset().

p103 The Reason that no std::move is necessary in the return statement of source() is than according to the language rules of c++11, the compiler will try a move automaticallly.

p105 The c++ stl provides a partial spartial specialization of class unique_ptr for array: std::unique_ptr`<std::string[]>`

p110 unique_ptr not necessarily defined as T*

p115 The new concept of numeric limits has two advantages: first, it offers more type safety. second, it enables a programmer to write templates that evaluate these limits.

p119 all members of numeric_limits are declared as constexpr.

p121 you can query for any arbitrary type whether or not it has numeric limits defined.

p125 Type Traits:
         
* Type Predicates
* Type Relations
* Type Modifiers
         
p130 A reference to a constant type is not constant

p136 Note that swap provides an exception specification

p158 Not that time_t usually is just the number of seconds since the UNXI epoch. bu this is not guaranteed.

p166 The marjor advantage of iterators is that they offer a small but common interface for any arbitrary container type.

p166 The concept of STL is base on a separation of data and operations. The data is managed by container classes, and the operations are defined by configurable algorithms. iterators are the glue between these two components.

p167 There are three general kinds of containers:

* Sequence contains are ordered collections in which every element has a certain postion.
* Associative containers are sorted collections in which the postion of an element depends on its value due to a certain sorting criterion: set multiset map multimap.
* Unorderd containers. neither the order of insertion nor the value of the inserted element has an influence on the postion of the element, and the position might change over the lifetime of the container: unordered_set,undered_multiset,undered_map,undered_multimap.

p170 size() is provided for any container class execept singly linked lists(class forward_list)

p170 deque rhymes with "check"

p171 the push_front is not provided for vectors, because it would have a bad runtime for vectors, but is is possible to insert an element at the beginning of a vector.

p171 using array. with a safer and more convenient interface.

p176 forward_list is in principle just a limited list.

p177 The major advantage of associative containers is that finding an element with a specific value is rather fast.

p177 you can consider a set as a special kind of map, in which the value is identical to the key. in fact, all these are implemented by using the same basic implementation of a binary tree.

p180 C++11 guarantees that newly inserted elements are inserted at the end of equivalent elements that multisets and multimaps already contain.

p180 unordered containers are typically implemented as a hash table. the goal is that each element has its own position so that you have fast access to each element.

p185 associative array: an array whose index is not an integer value.

p188 an iterator is an object that can iterate over elements. iterators share the same interface but have different types.

p189 every container defines two iterator types:

* container::iterator is provided to iterator over elements in read/write mode
* container::const_iterator is provided to iterator over elements in read mode

p199 To write generic code you should not use special operations for random-access iterators.

p199 Algorithms are not member functions of the container classer but instead are global functions that operator wich iterators. this concept reduces the amount of code and increases the power and the flexibility of the library.

p208 Multiple Ranges, make sure that the second and additional ranges have at least as many elements as the first range.

p209 associative and unordered containers cannot be used as a destination for overwriting algorithms.

p210 Interator Adapters

p210 Insert Interators solve the problem of algorithms that write to destination that does not have enough room.

p212 general insertor call insert()

p213 `istream_iterator<string>()` calls the default constructor of iterators that creates a so-called end-of-stream iterator.

p214 reverse iterators rbegin()\rend()

p220 distance if iterator were random-access, you could with "-"

p221 to make agorithms as flexible as possible there are good reason not requrie that interator know their container.

p223 a container might have member functions that provide much better performance that algorithms

p226 predicates must stateless

p232 Lambda no default constructor and no assignment operator.

p235 function object are functions with states

* fnctction object has its own type
* function object may faster than ordinary functions.

p241 Binder to combine predefined function objects with other values or use special cases.

p248 it's turns out that exception specifications could cause performance penalties, so they were replaced by noexcept with c++11.

p249 if you need a transaction-safe container, you should use a list.

p254 container's operators are not safe in the sense that they check for every possible error.

p257 since c++11 you can use move constructor

p258 Move Sytax: The contents of the container on the right-hand side are undefined afterwared:

p258 forware_list not provided size()

p260 all containers except vectors and deques guarantee that iterators an references to elements remianing valid if other elemenets are deleteed.

p262 array<> default initialized.

p267 array you must not pass an iterator as the address of the first element.

p271 vectors： capacity(), which returns the number of elements a vector could contain in its actual memory.

p271 If the only reason for initialization is to reserve memory, you should use reserve()

p271 it is not possible to call reserve for vectors to shrink the capacity.

p282 For vector<bool> the return type of subscript operator is an auxiliary class.

p283 Deque is typically implemented as a bunch of individual blocks.

p284 Deque provide no support to control the capacity and the momnet of reallocation.Howerer reallocation may perform better than for vectors because according to their typical internal structure, deques don't have to copy all elemenets on reallocation.

p284 Blocks of memory might get freed when they are no longer used(implementation specific)

p286 You could say that lists are transaction safe.


p301 The design goal to have "zero space or time overhead relative to a hand-written C-stype single linked list.

p303 forward_list provide no support size(), but you can use std::distance(list.begin(),list.end())


p315 you cant change set's value from interator

p316 not that the sorting criterion is also used to check for equivalence of two elements in the same container.

p338 std::for_each(coll.begin(), coll.end(), [] (decltype(coll)::value_type &elem) {}
p342 piecewise_construct emplace.

p345 [] is slower than the insert()

p366 unorder container

p366 unorder containers are optimized for fast searching of elements

p386 using the noninvasive approach is simple, you need only objects that are able to iterate over the elements of an array by using the STL iterator interface.

p386 Any Thing that behaves like an iterator is an iterator.

p437 forward iterators, it is guaranteed that for two forward iterators that refor to the same elemnet, operator == yields true and that they will refer to the same value after both are incremented.

p442 to be able to change container and iterator types, you should use advance

p460 for insert iterator, a bad hint might even be worse than no hint.

p471 writing user-defined iterators

p476 each function object has its own type

p479 by default, function objects are passed by value rather than by reference.

p483 for_each can return a value

p485 a predicate should always be stateless. you should declare operator() as const. but lambda not exist this problem.

p494 bind also could bind data member.
