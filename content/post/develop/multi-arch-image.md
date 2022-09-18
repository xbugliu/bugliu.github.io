---
title: "多架构镜像的构建"
date: 2022-03-01T19:33:37+08:00
draft: false
categories: ["开发"]
---


## 多架构的镜像

我们在Docker Hub官网上会看的有些镜像的Tag是支持多个OS/ARCH的，比如[nginx][3]:
{{% center %}}
![image](/images/posts/multi-arch-image/nginx_docker.png)
{{% /center %}}


这样的好处是: 在不同的架构(arm64, amd64)下只需要使用相同的名字`nginx:latest`，简单清晰。而不需要`nginx-amd64:latest`、`nginx-arm64:latest`这样的特殊处理。这个特性需要docker daemon和docker registry同时支持(pull&push)，在2015年docker registry支持了这个特性，参见：[Image Manifest Version 2, Schema 2][1]。


## 程序编译

我们的服务需要同时支持Linux下Arm64和amd64两种架构，首先需要将编译生成指定架构的程序。

### Golang

对于Golang编写的程序，采用`GOOS=linux GOARCH=amd64`的环境变量可以方便的编译输出各种架构的程序。

### C++

对于C++来说，就没有Golang那样方便，需要用到交叉编译技术，交叉编译环境可以自己配置，当然可以借助别人配置好的镜像，比如[dockcross][2]。基于dockcross，基本不用改变Makefile或者MakeList文件。需要注意：

* 假如依赖的第三方库非源码构建，需要自行安装(sudo apt install gcc-6-base:arm64)或者手工放置到lib目录
* 如果依赖的三方都以源码编译，则没有安装库的问题

使用方法：

```shell
1. 进入容器: docker run -ti --rm -v /data/source:/data/source dockcross/linux-arm64 bash

2. 安装三方库（option）

3. cd /data/source

4. make
```

## 镜像构建

多架构的程序编译出来后，下面是将其打包成镜像的过程，这里面有2中方法：

1. docker manifest

```
# step1: 分别构建多平台镜像，并推送

# AMD64
$ docker build -t your-username/multiarch-example:manifest-amd64 --build-arg ARCH=amd64/ .
$ docker push your-username/multiarch-example:manifest-amd64

# ARM32V7
$ docker build -t your-username/multiarch-example:manifest-arm32v7 --build-arg ARCH=arm32v7/ .
$ docker push your-username/multiarch-example:manifest-arm32v7

# ARM64V8
$ docker build -t your-username/multiarch-example:manifest-arm64v8 --build-arg ARCH=arm64v8/ .
$ docker push your-username/multiarch-example:manifest-arm64v8

# step2: 设置多架构的manifest

$ docker manifest create \
your-username/multiarch-example:manifest-latest \
--amend your-username/multiarch-example:manifest-amd64 \
--amend your-username/multiarch-example:manifest-arm32v7 \
--amend your-username/multiarch-example:manifest-arm64v8

# step3: 推送manifest

docker manifest push your-username/multiarch-example:manifest-latest
```

2. docker buildx

```
  $ docker buildx build \
--push \
--platform linux/arm/v7,linux/arm64/v8,linux/amd64 \
--tag your-username/multiarch-example:buildx-latest .
```

2种方法对比，明显buildx操作更简洁，也是我们推荐的方法，下面详细讲解一下注意事项。

## Docker Buildx

### 版本要求

* Docker版本： >=19.03
* Linux内核版本：>=4.8

### 配置

```
# 配置模拟器
docker run --privileged --rm tonistiigi/binfmt --install all

# 配置buildx环境

export DOCKER_CLI_EXPERIMENTAL=enabled
docker buildx create --use --name mybuilder

# 查看buildx配置

docker buildx ls
```

### 准备Dockerfile

```txt
FROM alpine
ARG TARGETARCH

COPY /out/${TARGETARCH}/myapp /bin/
```

这里面`TARGETARCH`是关键，表示buildx传进来的架构(amd64、arm64), 可以控制构建过程。

### build
```
export DOCKER_CLI_EXPERIMENTAL=enabled
$ docker buildx build \
--push \
--platform linux/arm64,linux/amd64 --tag your-username/multiarch-example:buildx-latest .
```

[1]: https://github.com/distribution/distribution/blob/release/2.3/docs/spec/manifest-v2-2.md
[2]: https://github.com/dockcross/dockcross
[3]: https://hub.docker.com/_/nginx?tab=tags

参考：https://www.docker.com/blog/multi-arch-build-and-images-the-simple-way/