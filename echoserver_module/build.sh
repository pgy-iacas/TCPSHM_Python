swig -c++ -python -py3 tcpshm_server.i
g++ -shared -std=c++11 -fPIC -I/usr/include/python3.5m -lpython3.5m  tcpshm_server_wrap.cxx -o _tcpshm_server.so -fpermissive -lrt -lpthread
