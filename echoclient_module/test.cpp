#include<iostream>
#include"tcpshm_client.h"
using namespace std;

int main(){

    TcpShmClient tcp("client", "client");
    tcp.Run(false,"127.0.0.1",12345);


}