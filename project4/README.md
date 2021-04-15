---
title: README
date: 2021-04-15 23:32:49
tags: 
categories: 
- project4
- geekOS
- course
---



# 错误解决

直接用源文件  [操作系统 Project4.zip](references\操作系统 Project4.zip)  会报 `project0` 下的 `Makefile` 两个错误和`project0` 下的 `.bochsrc` 的错误。这时，你不能直接拿`project0` 下的 `Makefile` 和 `project0` 下的 `.bochsrc` 覆盖资料里的文件，而是需要手动修正，因为行号不一样。或者用我编辑好的  [.bochsrc](data\.bochsrc) 、 [Makefile](data\Makefile) 来覆盖出错的文件

# 启动

`bulid` 目录下：

```sh
make clean && make depend && make
```

```
bochs
```

# 参考

 [GeekOS操作系统Project4](references\GeekOS操作系统Project4_二叉排序树的检索，插入和删除-CSDN博客.html) 及其资料源文件 [操作系统 Project4.zip](references\操作系统 Project4.zip) 





