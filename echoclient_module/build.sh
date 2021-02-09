swig -c++ -python -py3 tcpshm_client.i
g++ -shared -std=c++11 -fPIC -I/usr/include/python3.5m -lpython3.5m  tcpshm_client_wrap.cxx -o _tcpshm_client.so -fpermissive -lrt -lpthread
