---
title: "Kubernetes三种探针的使用场景"
date: 2022-03-22T18:34:25+08:00
draft: false
---

## Liveness Probe


Liveness最好理解，是保活探针。如果发现容器僵死（如：死锁、死循环）则Kill掉容器，达到重启容器的效果。Liveness会在整个Pod生命期检测（先不考虑StartupProbe）。

## Readiness Probe

Readiness也好理解，是就绪探针。如果发现容器当前不可用（如：负载高、下游Pod不可用），则设置容器的状态为未就绪，这样新的请求就不会触达。Readiness也会在整个Pod生命期检测，而不是部分人理解的只在启动时检测。

Readiness是温和的，给容器机会，直到它改正。Liveness是暴力的，一旦发现容器出问题，则立即Kill。另外Liveness和Readiness是没有依赖关系的。

## Startup Probe

Startup是k8s 1.16引入的新特性。Startup的使用场景是什么呢？

试想有一个存储Server, 加载数据是耗时操作，初始数据小，1秒可完成，数据大后要数分钟。在没有Startup之前，要给Liveness一个较大的initialDelaySeconds，否则Loading时会Kill容器。但一个较大的initialDelaySeconds对小数据量又不太友好。Startup即是解决这个问题的，配置了Startup Probe, 则Startup未就绪前不会触发Liveness和Readiness。Starup就绪后，Liveness和Readiness才会接手。

所以Startup的使用场景是启动时间不稳定或启动耗时较长时。若非此场景则不必配置Startup。