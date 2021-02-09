%module tcpshm_client
%{
#include<iostream>
#include"tcpshm_client.h"
%}


class TcpShmClient{
public:

    TcpShmClient(char* client_name, char* ptcp_dir,bool use_shm, const char* server_ipv4, short server_port);
    //TcpShmClient(char* client_name, char* ptcp_dir);
    void Run(bool use_shm, const char* server_ipv4, short server_port);


};
