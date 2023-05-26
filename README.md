# FurRendering

基于Mitsuba的毛发渲染器以及离线渲染降噪

## 1. 降噪(自创 效果一般勿怪~)

先看一下效果吧 

![32](https://github.com/luoshuifan/Fur/assets/109076683/513750c9-464c-4998-a9b2-ab05993814a5)
> 32SPP的带着噪点的渲染结果

![ALL](https://github.com/luoshuifan/Fur/assets/109076683/92f9f50c-7bce-4531-a513-a8f9d34c116b)
> 32SPP 去噪之后的效果，可以看见还是有一些效果滴~

其他降噪结果图
![32](https://github.com/luoshuifan/Fur/assets/109076683/141f267d-68d6-4fb7-80ff-d787945c09ba)
>32SPP

![scene_v0](https://github.com/luoshuifan/Fur/assets/109076683/4e5214e2-5ac1-412e-80a1-c3180c68c063)
>32SPP 可以看见能增加一些shadow，和细节上更加的平滑，其实细节的把控还是要调节GT步骤，GT越好，效果越好


总之，在保留渲染细节的同时，能降低一些高频噪点所带来的影响，同时能一定程度上保留一些高光效果。自我满足了 ฅ՞•ﻌ•՞ ต

吐槽一下，毛发这种细节怪，真的难搞，学识浅薄了，痛苦…………(っ◞‸◟c)…………

言归正传，记录一下整体流程
### 1.1 流程
先看一下算法流程图吧 

![image](https://github.com/luoshuifan/Fur/assets/109076683/3560f719-2485-443b-b54d-4a5b532cee30)
> 整体流程图 将就着看吧~

作为**InImage**，主要的输入有每个SPP采样的Albedo和渲染出来的Albedo以及法线，位置这些数据。

其实整体框架或者思路来源于[Perceptual error optimization for Monte Carlo rendering](https://sampling.mpi-inf.mpg.de/2022-chizhov-perception.html)

核心思想就是利用每个SPP的采样值来进行估计滴。

#### 1.1.1 GTImage 
怎么得到GTImage，其实方法很多，例如我使用联合双边滤波，也可以使用各种SDK了，例如Inter的OpenImageDenoise，等等了。

推荐一个大佬的做法[svgf](https://zhuanlan.zhihu.com/p/28288053)，大概我只使用了空间信息，假如是渲染视频，也可以加入时间信息来进行改进

#### 1.1.2 GlobalImage

主要就是收集像素周围的信息啦，毕竟假设我们不相信渲染结果以及相信GTImage了~

#### 1.1.3 LocalImage

主要就是根据SPP的信息来修正GlobalImage的误差，主要修正细节信息以及高光信息~

#### 1.1.4 FinalImage

输出就好了

## 2. 渲染
这个看看文献，学习一下闫老师以及各路大神的Paper就OK了~

## 3. 遇到的各种细节问题
假如效果一般，可能参数不对，调参是个体力活(●'◡'●)。以及可能是打光的问题或者h的计算，反正我体验到了**工程细节**这四个字的威力




