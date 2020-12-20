+++
title = "Android 6 Dot 0 Openssl Crash"
date = "2015-10-11"
slug = "2015/10/11/android-6-dot-0-openssl-crash"
Categories = [keywords = ["Android 6.0 openssl Crash"]
description = Android 6.0 openssl Crash
+++

Android 6.0发布将近半年，预计本月推出正式版本。所以开发者们要重视起APP在Android 6.0下的兼容性问题。最近我们的App就遭遇到因6.0更换了OpenSSL库而导致的一个Crash。

##Crash的原因：

异常

```bash
java.lang.unsatisfiedlinkerror dlopen failed cannot locate symbol "openssl_add_all_algorithms_noconf"
```

你的产品依赖了openssl，而google在Android 6.0中使用了google自己的一个基于openssl分支boringssl，而这个分支在API和ABI上都不兼容openssl。
{% img  /images/posts/android_6/openssl_crash.png %}


##Google为什么切换openssl
1. 众所周知openssl存在着很多的问题，包括去年爆出的[Heartbleed][2]。目前依然有大量补丁没有被合入openssl主干，而google觉得其中很多补丁非常重要，所以google创建新的分支合入这些补丁。
2. google以为这个改动很小，绝大多数开发者是不会受影响的。（但据说google自己的youtube一样受影响，可见google想当然了).
3. google只对NDK标准接口负责，第三方库的接口，google是不保证的。

##你的APK中招了吗?
是不是你还在窃喜自己没有基于NDK开发的so，所以没有依赖openSSL。但你可能使用了第三方的so库，而这些库可能用到了openSSL。所以你的产品可能直接或间接依赖openssl. 这里有一个在线检测工具，可以测试自己是否中招。[Find apps with the OpenSSL / Android M crash flaw][3];


##解决方案
1. 静态链接openssl。优点是一劳永逸的解决问题，缺点是APK的增大size。
2. APK中打包libssl.so、libcrypto.so。如果你依赖的第三方库依赖了openssl, 而你没有权限去改它的代码，这是你唯一的解决方法。同样有size的问题。
3. NDK中使用jni调用java层的加密相关代码，缺点是复杂。


参考：

1. [Sygic and Waze navigation apps not working on Android M][1]
2. [Finding a Hidden Flaw that Will Crash Apps on Android M][4]

[1]:https://code.google.com/p/android-developer-preview/issues/detail?id=2410
[2]:https://www.us-cert.gov/ncas/alerts/TA14-098A
[3]:https://searchlight.sourcedna.com/search
[4]:https://sourcedna.com/blog/20150806/predicting-app-crashes-on-android-m.html



