# FurRendering

基于Mitsuba的毛发渲染器以及离线渲染降噪

## 1. 降噪(自创 效果一般勿怪~)

先看一下效果吧 

![32](https://github.com/luoshuifan/Fur/assets/109076683/513750c9-464c-4998-a9b2-ab05993814a5)
> 32SPP的带着噪点的渲染结果

![ALL](https://github.com/luoshuifan/Fur/assets/109076683/92f9f50c-7bce-4531-a513-a8f9d34c116b)
> 32SPP 去噪之后的效果，可以看见还是有一些效果滴~

总之，在保留渲染细节的同时，能降低一些高频噪点所带来的影响，同时能一定程度上保留一些高光效果。自我满足了 ฅ՞•ﻌ•՞ ต

吐槽一下，毛发这种细节怪，真的难搞，学识浅薄了，痛苦…………(っ◞‸◟c)…………

言归正传，记录一下整体流程
### 1.1 流程
先看一下算法流程图吧 
![image](https://github.com/luoshuifan/Fur/assets/109076683/3560f719-2485-443b-b54d-4a5b532cee30)
> 整体流程图 将就着看吧~

其实整体框架或者思路来源于link [Perceptual error optimization for Monte Carlo rendering](extension://bfdogplmndidlpjfhoijckpakkdjkkil/pdf/viewer.html?file=https%3A%2F%2Fsampling.mpi-inf.mpg.de%2Fpublications%2F2022-chizhov-perception%2F2022-chizhov-perception.pdf)

