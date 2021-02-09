# TCPSHM_Python
本项目用python封装C++基于共享队列网络通信模块 https://github.com/MengRao/tcpshm

构建方法如下：

1.安装SWIG3.0及以上版本

2.在Client和Server文件下分别运行build.sh生成要导入的python包，so和cxx文件为生成的中间文件

3.在python中导入生成的模块，并使用。

由于原项目直接通过SWIG包装为python模块报错，因此将原项目做了部分删改以适应SWIG的python封装。
主要修改部分：

  1.Echo类与其对应父类TCPSHM合并，将子类中的成员方法与变量写入父类中，完成两个类的合并，舍弃继承关系。
  
  2.将Conf结构体中的静态变量提出，定义为全局变量，剔除了项目中以Conf为模板参数的模板。
  
  3.cpupin.h中的函数写为TCPSHM类中的私有变量，舍弃cpupin.h头文件。
  
  4.舍弃ptcp_queue.h中的模板参数，用全局变量表示。 
