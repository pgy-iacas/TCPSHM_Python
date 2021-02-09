
#pragma once


template<uint32_t N, uint16_t MsgType>
struct MsgTpl
{
    static const uint16_t msg_type = MsgType;
    int val[N];
};
typedef MsgTpl<1, 1> Msg1;
typedef MsgTpl<2, 2> Msg2;
typedef MsgTpl<3, 3> Msg3;
typedef MsgTpl<4, 4> Msg4;
const uint32_t NameSize = 16;
const uint32_t ShmQueueSize = 1024 * 1024; // must be power of 2
const bool ToLittleEndian = true; // set to the endian of majority of the hosts

using LoginUserData = char;
using LoginRspUserData = char;
const int64_t NanoInSecond = 1000000000LL;

const uint32_t TcpQueueSize = 2000;       // must be a multiple of 8
const uint32_t TcpRecvBufInitSize = 1000; // must be a multiple of 8
const uint32_t TcpRecvBufMaxSize = 2000;  // must be a multiple of 8
const bool TcpNoDelay = true;

const int64_t ConnectionTimeout = 10 * NanoInSecond;
const int64_t HeartBeatInverval = 3 * NanoInSecond;

using ConnectionUserData = char;





#include <string>
#include <array>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "tcpshm_conn.h"
#include <bits/stdc++.h>
#include "timestamp.h"
#include <sched.h>
using namespace tcpshm;
using namespace std;



class TcpShmClient
{
public:
    using Connection = TcpShmConnection;
    using LoginMsg = LoginMsgTpl;
    using LoginRspMsg = LoginRspMsgTpl;

    TcpShmClient(){}
    TcpShmClient(char* client_name, char* ptcp_dir,bool use_shm, const char* server_ipv4, short server_port)
        : ptcp_dir_(ptcp_dir) {
        strncpy(client_name_, client_name, sizeof(client_name_) - 1);
        mkdir(ptcp_dir_.c_str(), 0755);
        client_name_[sizeof(client_name_) - 1] = 0;
        conn_.init(ptcp_dir, client_name_);
        srand(time(NULL));
        cout<<"constructor:    "<<conn_.GetPtcpDir()<<endl;
        Run(use_shm,server_ipv4,server_port);
        }

    
    void Run(bool use_shm, const char* server_ipv4, short server_port){
        cout<<"Run:  "<<conn_.GetPtcpDir()<<endl;
        if(!Connect(use_shm, server_ipv4, server_port, 0)) return;
        // we mmap the send and recv number to file in case of program crash
        string send_num_file =
            string(conn_.GetPtcpDir()) + "/" + conn_.GetLocalName() + "_" + conn_.GetRemoteName() + ".send_num";
        string recv_num_file =
            string(conn_.GetPtcpDir()) + "/" + conn_.GetLocalName() + "_" + conn_.GetRemoteName() + ".recv_num";
        const char* error_msg;
        send_num = my_mmap<int>(send_num_file.c_str(), false, &error_msg);
        recv_num = my_mmap<int>(recv_num_file.c_str(), false, &error_msg);
        if(!send_num || !recv_num) {
            cout << "System Error: " << error_msg << " syserrno: " << strerror(errno) << endl;
            return;
        }
        cout << "client started, send_num: " << *send_num << " recv_num: " << *recv_num << endl;
        if(use_shm) {
            thread shm_thr([this]() {
                if(do_cpupin) cpupin(7);
                start_time = now();
                while(!conn_.IsClosed()) {
                    if(PollNum()) {
                        stop_time = now();
                        conn_.Close();
                        break;
                    }
                    PollShm();
                }
            });

            // we still need to poll tcp for heartbeats even if using shm
            while(!conn_.IsClosed()) {
              PollTcp(now());
            }
            shm_thr.join();
        }
        else {
            if(do_cpupin) cpupin(7);
            start_time = now();
            while(!conn_.IsClosed()) {
                if(PollNum()) {
                    stop_time = now();
                    conn_.Close();
                    break;
                }
                PollTcp(now());
            }
        }
        uint64_t latency = stop_time - start_time;
        Stop();
        cout << "client stopped, send_num: " << *send_num << " recv_num: " << *recv_num << " latency: " << latency
             << " avg rtt: " << (msg_sent > 0 ? (double)latency / msg_sent : 0.0) << " ns" << endl;

    }
    
    

    ~TcpShmClient() {
        Stop();
    }

    // connect and login to server, may block for a short time
    // return true if success
    bool Connect(bool use_shm,
                  const char* server_ipv4,
                  uint16_t server_port,
                  const char& login_user_data) 
    {
        if(!conn_.IsClosed()) {
            OnSystemError("already connected", 0);
            return false;
        }
        conn_.TryCloseFd();
        const char* error_msg;
        if(!server_name_) {
            std::string last_server_name_file = std::string(ptcp_dir_) + "/" + client_name_ + ".lastserver";
            server_name_ = (char*)my_mmap<ServerName>(last_server_name_file.c_str(), false, &error_msg);
            if(!server_name_) {
                OnSystemError(error_msg, errno);
                return false;
            }
            strncpy(conn_.GetRemoteName(), server_name_, sizeof(ServerName));
        }


        MsgHeader sendbuf[1 + (sizeof(LoginMsg) + 7) / 8];
        sendbuf[0].size = sizeof(MsgHeader) + sizeof(LoginMsg);
        sendbuf[0].msg_type = LoginMsg::msg_type;
        sendbuf[0].ack_seq = 0;
        LoginMsg* login = (LoginMsg*)(sendbuf + 1);
        strncpy(login->client_name, client_name_, sizeof(login->client_name));
        strncpy(login->last_server_name, server_name_, sizeof(login->last_server_name));
        //cout<<"client_name:::"<<login->client_name<<endl<<"server_nane:::"<<login->last_server_name<<endl;                   //panadd
        login->use_shm = use_shm;
        login->client_seq_start = login->client_seq_end = 0;
        login->user_data = login_user_data;

                
        if(server_name_[0] &&
           (!conn_.OpenFile(use_shm, &error_msg))) {


            OnSystemError(error_msg, errno);
            return false;
        }
        int fd;
        if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            OnSystemError("socket", errno);
            return false;
        }
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
            OnSystemError("setsockopt SO_RCVTIMEO", errno);
            close(fd);
            return false;
        }

        if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
            OnSystemError("setsockopt SO_RCVTIMEO", errno);
            close(fd);
            return false;
        }
        int yes = 1;
        if(TcpNoDelay && setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0) {
            OnSystemError("setsockopt TCP_NODELAY", errno);
            close(fd);
            return false;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, server_ipv4, &(server_addr.sin_addr));
        server_addr.sin_port = htons(server_port);
        bzero(&(server_addr.sin_zero), 8);

        if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            OnSystemError("connect", errno);
            close(fd);
            return false;
        }

        sendbuf[0].template ConvertByteOrder<ToLittleEndian>();
        login->ConvertByteOrder();
        int ret = send(fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
        if(ret != sizeof(sendbuf)) {
            OnSystemError("send", ret < 0 ? errno : 0);
            close(fd);
            return false;
        }

        MsgHeader recvbuf[1 + (sizeof(LoginRspMsg) + 7) / 8];
        ret = recv(fd, recvbuf, sizeof(recvbuf), 0);
        if(ret != sizeof(recvbuf)) {
            OnSystemError("recv", ret < 0 ? errno : 0);
            close(fd);
            return false;
        }
        LoginRspMsg* login_rsp = (LoginRspMsg*)(recvbuf + 1);
        recvbuf[0].template ConvertByteOrder<ToLittleEndian>();
        login_rsp->ConvertByteOrder();
        if(recvbuf[0].size != sizeof(MsgHeader) + sizeof(LoginRspMsg) || recvbuf[0].msg_type != LoginRspMsg::msg_type ||
           login_rsp->server_name[0] == 0) {
           OnSystemError("Invalid LoginRsp", 0);
            close(fd);
            return false;
        }
        if(login_rsp->status != 0) {
            if(login_rsp->status == 1) { // seq number mismatch
                sendbuf[0].template ConvertByteOrder<ToLittleEndian>();
                login->ConvertByteOrder();
                OnSeqNumberMismatch(sendbuf[0].ack_seq,
                                            login->client_seq_start,
                                                login->client_seq_end,
                                                recvbuf[0].ack_seq,
                                                login_rsp->server_seq_start,
                                                login_rsp->server_seq_end);
            }
            else {
                OnLoginReject(login_rsp);
            }
            close(fd);
            return false;
        }
        login_rsp->server_name[sizeof(login_rsp->server_name) - 1] = 0;
        // check if server name has changed
        if(strncmp(server_name_, login_rsp->server_name, sizeof(ServerName)) != 0) {
            conn_.Release();
            strncpy(server_name_, login_rsp->server_name, sizeof(ServerName));
            strncpy(conn_.GetRemoteName(), server_name_, sizeof(ServerName));
            if(!conn_.OpenFile(use_shm, &error_msg)) {
                OnSystemError(error_msg, errno);
                close(fd);
                return false;
            }
            conn_.Reset();
        }
        fcntl(fd, F_SETFL, O_NONBLOCK);
        int64_t now = OnLoginSuccess(login_rsp);

        conn_.Open(fd, recvbuf[0].ack_seq, now);
        return true;
    }

    // we need to PollTcp even if using shm
    void PollTcp(int64_t now) {
        if(!conn_.IsClosed()) {
            MsgHeader* head = conn_.TcpFront(now);
            if(head) OnServerMsg(head);
        }
        if(conn_.TryCloseFd()) {
            int sys_errno;
            const char* reason = conn_.GetCloseReason(&sys_errno);
            OnDisconnected(reason, sys_errno);
        }
    }

    // only for using shm
    void PollShm() {
        MsgHeader* head = conn_.ShmFront();
        if(head) OnServerMsg(head);
    }

    // stop the connection and close files
    void Stop() {
        if(server_name_) {
            my_munmap<ServerName>(server_name_);
            server_name_ = nullptr;
        }
        conn_.Release();
    }

    // get the connection reference which can be kept by user as long as TcpShmClient is not destructed
    Connection& GetConnection() {
        return conn_;
    }


private:

bool cpupin(int cpuid) {
    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    CPU_SET(cpuid, &my_set);
    if(sched_setaffinity(0, sizeof(cpu_set_t), &my_set)) {
        std::cout << "sched_setaffinity error: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}


    bool PollNum() {
        if(*send_num < MaxNum) {
            // for slow mode, we wait to recv an echo msg before sending the next one
            if(slow && *send_num != *recv_num) return false;
            // we randomly send one of the 4 msgs
            int tp = rand() % 4 + 1;
            switch(tp) {
                case 1: TrySendMsg<Msg1>(); break;
                case 2: TrySendMsg<Msg2>(); break;
                case 3: TrySendMsg<Msg3>(); break;
                case 4: TrySendMsg<Msg4>(); break;
            }
        }
        else {
            // if all echo msgs are got, we are done
            if(*send_num == *recv_num) return true;
        }
        return false;
    }

    template<class T>
    bool TrySendMsg() {
        MsgHeader* header = conn_.Alloc(sizeof(T));
        if(!header) return false;
        header->msg_type = T::msg_type;
        T* msg = (T*)(header + 1);
        for(auto& v : msg->val) {
            // convert to configurated network byte order, don't need this if you know server is using the same endian
            v = Endian<ToLittleEndian>::Convert((*send_num)++);
        }
        conn_.Push();
        msg_sent++;
        return true;
    }

    template<class T>
    void handleMsg(T* msg) {
        for(auto v : msg->val) {
            // convert from configurated network byte order
            Endian<ToLittleEndian>::ConvertInPlace(v);
            if(v != *recv_num) {
                cout << "bad: v: " << v << " recv_num: " << (*recv_num) << endl;
                exit(1);
            }
            (*recv_num)++;
        }
    }


private:
    // called within Connect()
    // reporting errors on connecting to the server
    void OnSystemError(const char* error_msg, int sys_errno) {
        cout << "System Error: " << error_msg << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called within Connect()
    // Login rejected by server
    void OnLoginReject(const LoginRspMsg* login_rsp) {
        cout << "Login Rejected: " << login_rsp->error_msg << endl;
    }

    // called within Connect()
    // confirmation for login success
    int64_t OnLoginSuccess(const LoginRspMsg* login_rsp) {
        cout << "Login Success" << endl;
        return now();
    }

    // called within Connect()
    // server and client ptcp sequence number don't match, we need to fix it manually
    void OnSeqNumberMismatch(uint32_t local_ack_seq,
                             uint32_t local_seq_start,
                             uint32_t local_seq_end,
                             uint32_t remote_ack_seq,
                             uint32_t remote_seq_start,
                             uint32_t remote_seq_end) {
        cout << "Seq number mismatch, name: " << conn_.GetRemoteName() << " ptcp file: " << conn_.GetPtcpFile()
             << " local_ack_seq: " << local_ack_seq << " local_seq_start: " << local_seq_start
             << " local_seq_end: " << local_seq_end << " remote_ack_seq: " << remote_ack_seq
             << " remote_seq_start: " << remote_seq_start << " remote_seq_end: " << remote_seq_end << endl;
    }

    // called by APP thread
    void OnServerMsg(MsgHeader* header) {
        // auto msg_type = header->msg_type;
        switch(header->msg_type) {
            case 1: handleMsg((Msg1*)(header + 1)); break;
            case 2: handleMsg((Msg2*)(header + 1)); break;
            case 3: handleMsg((Msg3*)(header + 1)); break;
            case 4: handleMsg((Msg4*)(header + 1)); break;
            default: assert(false);
        }
        conn_.Pop();
    }

    // called by tcp thread
    void OnDisconnected(const char* reason, int sys_errno) {
        cout << "Client disconnected reason: " << reason << " syserrno: " << strerror(sys_errno) << endl;
    }


private:
    char client_name_[NameSize];
    using ServerName = std::array<char, NameSize>;
    char* server_name_ = nullptr;
    std::string ptcp_dir_;
    Connection conn_;


private:
    static const int MaxNum = 10000000;
    int msg_sent = 0;
    uint64_t start_time = 0;
    uint64_t stop_time = 0;
    // set slow to false to send msgs as fast as it can
    bool slow = true;
    // set do_cpupin to true to get more stable latency
    bool do_cpupin = true;
    int* send_num;
    int* recv_num;
};
