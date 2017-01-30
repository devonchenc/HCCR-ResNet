# HCCR-ResNet

## 1.下载数据库

从中科院自动化所网站下载汉字手写体数据库[CASIA Online and Offline Chinese Handwriting Databases，](http://www.nlpr.ia.ac.cn/databases/handwriting/Download.html/)

这里使用的是HWDB1.1 Offline数据库，包含了国标GB2312-80收录的一级汉字，共计3755个字符。数据库分为训练集和测试集，其中训练集为240个人的手写字符，测试集为60个人。

## 2.图像预处理

原来的数据库格式为*.gnt后缀名的文件，文件格式如下，

名称 | 类型 | 长度 | Comment
---|---|---|---|---|---
图像大小 | unsigned int | 4字节 | 单张图像字节数
GB码 | char | 2字节 | "啊"=0xb0a1 Stored as 0xa1b0 |
宽 | unsigned short | 2字节 | 列数
高 | unsigned short | 2字节 | 行数
数据 | unsigned char | 字节数=宽*高 | 按行存储

注意到这里的每张字符图像尺寸不同，所以需要进行resize预处理，这里将其尺寸统一resize为56x56，再在图像上下左右各补充4个像素的白边，最终补成64x64的图像。

由于原图像灰度区间较窄，因此在训练网络前对图像做对比度增强，将每张图像的灰度拉伸到0~255，有助于识别效果提升。

## 3.图像转为LMDB格式

我们需要将上一步预处理好的图像转为Caffe支持的数据库格式，Caffe支持LMDB、LevelDB和HDF5格式的数据，这里我们选择转为LMDB格式。数据全部转换完将近9GB大小。此处有个坑，Windows下无法创建大于2GB的LMDB库，哪怕是使用64位库也不行。

先将3755个汉字字符的GB码按照它们在图像库中出现的顺序，生成0~3754的序号，这些序号保存在HWDB1.1_character.txt文件中。这些序号用来作为它们训练时用的标签并保存在LMDB数据库中。

如果觉得所有字符数据全部使用太大，可以取其中的前100或1000个字符进行训练（参数subclass可以用来控制字符集大小）。

## 4.训练神经网络

这里采用两种网络进行训练，一个是与MNIST类似的深度卷积网络HCCR3755_cnn_solver.prototxt，另外一个是深度残差网络HCCR3755_res20_solver.prototxt。

对于3755字符识别分类，分别迭代10000次之后，前者网络可以达到91.19%精度，而后者则高达97.23%的精度。
