%module tcpshm_server
%{
#include<iostream>
#include"tcpshm_server.h"
%}



class TcpShmServer{
public:
    TcpShmServer(char* client_name, char* ptcp_dir);
    void Run(const char* listen_ipv4, short listen_port);
};



