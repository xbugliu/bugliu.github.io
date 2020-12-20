+++
title = "This Pointer"
date = "2013-09-01"
slug = "2013/09/01/this-pointer"
tags =["C++","this","this指针","thunk"]
description = "深入讲解每个C++程序员都知道的this指针知识,挖掘其背后的秘密!"
+++
this指针想必每个C++程序员都是再熟悉不过的了，我们每天的编程工作都会用到它，我们以为它是最忠实的朋友，不会给我们惹麻烦，但其实它可能不是你想象的样子！
##this指针的偏移 - 某次强制转化引发的血案
这是一个真实的案例，发生在12年6月份，让我用简单的例子还原一下现场。假设有一组派生关系的类CBrid继承于CAnimal，我们构造一个CBrid对象并赋值到CAnimal指针，然后由于某些原因需要把这个基类CAnimal指针强制转化成void*(真实情况是Windows下的LPARAM），然后再强制转化回CBrid指针：
{% codeblock 例1 lang:cpp%}
class CAnimal
{
public:
CAnimal(){}
~CAnimal(){}
protected:
string m_strName;	
};

class CBird: public CAnimal
{
public:
CBird(): m_bCanFly(true)
{
m_strName = "Bird";
}
~CBird(){}
virtual void Fly()
{
cout<<"type: "<<m_strName<<std::endl;
cout<<"is fly: "<<m_bCanFly<<std::endl;
}
protected:
bool m_bCanFly; 
};

//主函数
int main()
{
 CAnimal* pAnimal = new CBird;
 void* pCmd = (void*)pAnimal;
 //一些操作
 CBird* pBird = (CBird*)pCmd;  
 if (pBird != nullptr)
 {
   pBird->Fly();
 }
}
{% endcodeblock %}
上面代码36行，pBrid要飞，但没飞起来，在我的开发环境下，程序挂在了这一行。那天是一个刚毕业很聪明的小伙子发现的这个问题，他还尝试过这样调用：
{% codeblock 例2 lang:cpp%}
CAnimal* pAnimal = new CMammal;
CBird* pBird = (CBird*)pAnimal;

if (pBird != nullptr)
{
  pBird->Fly();
}
{% endcodeblock %}
这样调用确是没有问题的，和第一个例子唯一的差别就是没有中间的Void*转化。
记得当时是我们周会的时间，于是拿出来和大家讨论，惭愧的是我们十几个人，竟然没人能说出其中原因，要知道我们中也有三个工作5年左右的同事。后来我打开调试器，跟踪了一下这两段代码的汇编代码，终于发现了蛛丝马迹：
```c
CBird* pBird = (CBird*)pAnimal;
cmp         dword ptr [pAnimal],0  
je          main+1C1h (33F681h)  
mov         eax,dword ptr [pAnimal]  
sub         eax,4  
mov         dword ptr [ebp-178h],eax  
jmp         main+1CBh (33F68Bh)  
mov         dword ptr [ebp-178h],0  
mov         ecx,dword ptr [ebp-178h]  
mov         dword ptr [pBird],ecx  
```
猫腻就在第5行，编译器先取基类指针pAnimal的值然后减了4，赋值给了派生类指针pBird，看到这里我才隐隐约约感觉是虚表的问题，CBird有一个虚函数，而基类CAnimal没有。当时我还没看《深度探索C++对象模型》，相信看过这本书的人一眼就能看出端倪，接着我验证一下我的猜想：
{% codeblock 例3 lang:cpp%}
lass CAnimal
{
public:
CAnimal(){}
~CAnimal(){}
protected:
string m_strName;	
};


class CMammal: public CAnimal
{
public:
CMammal()
{
m_strName = "Mammal";
}
void IsEatMeat() const
{
cout<<"type: "<<m_strName<<std::endl;
cout<<"is eat meat: "<<std::boolalpha<<m_bEatMeat<<std::endl;
}
private:
bool m_bEatMeat;
};

int main()
{
 CAnimal* pAnimal = new CMammal;
 void* pCmd = (void*)pAnimal;
 CMammal* pMammal = (CMammal*)pCmd;

 if (pMammal != nullptr)
 {
  pMammal->IsEatMeat();
 }
}
{% endcodeblock %}
上面的代码运行正常，和例1的区别就是CMamal没有虚函数，而CBrid有。所以说，**沿着继承链类型转化时，this指针可能会发生偏移，以确保this指针总能指向subobject**。而强转中如果中间有void*这种没有类型信息的东西，会使编译器丢失这种偏移。

**this指针说白了就是对象基址，角色是成员变量寻址基址，偏移的目的是为了使成员变量寻址正确**，影响对像内存布局的东西都可能使this指针偏移（具体编译器可能不同）：

 1. 虚表，子类有而派生类没有
 2. 多重继承，子类与第n(n>1)个派生类
 3. 虚继承

待续：this指针的偏移策略
