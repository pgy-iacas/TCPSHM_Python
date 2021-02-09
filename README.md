# TCPSHM_Python

本项目用python封装C++基于共享队列网络通信模块 https://github.com/MengRao/tcpshm 

构建方法如下：

1. 安装SWIG3.0及以上版本
2. 在Client和Server文件下分别运行build.sh生成要导入的python包，so和cxx文件为生成的中间文件
3. 在python中导入生成的模块，并使用。
  
