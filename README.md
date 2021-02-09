# TCPSHM_Python
本项目用python封装C++基于共享队列网络通信模块 https://github.com/MengRao/tcpshm

构建方法如下：

    1.在linux系统下安装SWIG3.0及以上版本。
    
    2.在Server文件夹下运行build.sh，生成tcpshm_server.so和tcpshm_server_wrap.cxx文件为生成的附加文件，tcpshm_server.py即为生成的服务器端的python模块

    3.运行Run_Server.py文件启动服务器端。

    4.在Client文件夹下运行build.sh，生成tcpshm_client.so和tcpshm_client_wrap.cxx文件为生成的附加文件，tcpshm_client.py即为生成的客户端python模块

    5.运行Run_Client.py文件启动客户端即可进行通信。
    

由于原项目直接通过SWIG包装为python模块报错，因此将原项目做了部分删改以适应SWIG的python封装。
主要修改部分：

    1.Echo类与其对应父类TCPSHM合并，将子类中的成员方法与变量写入父类中，完成两个类的合并，舍弃继承关系。
  
    2.将Conf结构体中的静态变量提出，定义为全局变量，剔除了项目中以Conf为模板参数的模板。
  
    3.cpupin.h中的函数写为TCPSHM类中的私有变量，舍弃cpupin.h头文件。
  
    4.舍弃ptcp_queue.h中的模板参数，用全局变量表示。 
    
    5.其他微小改动
