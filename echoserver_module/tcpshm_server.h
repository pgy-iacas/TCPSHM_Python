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
  static const int64_t NanoInSecond = 1000000000LL;

  static const uint32_t MaxNewConnections = 5;
  static const uint32_t MaxShmConnsPerGrp = 4;
  static const uint32_t MaxShmGrps = 1;
  static const uint32_t MaxTcpConnsPerGrp = 4;
  static const uint32_t MaxTcpGrps = 1;

  // echo server's TcpQueueSize should be larger than that of client if client is in fast mode
  // otherwise server's send queue could be blocked and ack_seq can only be sent through HB which is slow
  static const uint32_t TcpQueueSize = 3000;       // must be a multiple of 8
  static const uint32_t TcpRecvBufInitSize = 1000; // must be a multiple of 8
  static const uint32_t TcpRecvBufMaxSize = 2000;  // must be a multiple of 8
  static const bool TcpNoDelay = true;

  static const int64_t NewConnectionTimeout = 3 * NanoInSecond;
  static const int64_t ConnectionTimeout = 10 * NanoInSecond;
  static const int64_t HeartBeatInverval = 3 * NanoInSecond;

  using ConnectionUserData = char;


#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "tcpshm_conn.h"
#include <bits/stdc++.h>
#include <sched.h>
#include "timestamp.h"
using namespace std;
using namespace tcpshm;


class TcpShmServer
{
public:
    using Connection = TcpShmConnection;
    using LoginMsg = LoginMsgTpl;
    using LoginRspMsg = LoginRspMsgTpl;


    TcpShmServer();

    TcpShmServer(char* server_name,char* ptcp_dir)
    : ptcp_dir_(ptcp_dir) {
        strncpy(server_name_, server_name, sizeof(server_name_) - 1);
        server_name_[sizeof(server_name_) - 1] = 0;
        mkdir(ptcp_dir_.c_str(), 0755);
        for(auto& conn : conn_pool_) {
            conn.init(ptcp_dir, server_name_);
        }
        int cnt = 0;
        for(auto& grp : shm_grps_) {
            for(auto& conn : grp.conns) {
                conn = conn_pool_ + cnt++;
            }
        }
        for(auto& grp : tcp_grps_) {
            for(auto& conn : grp.conns) {
                conn = conn_pool_ + cnt++;
            }
        }
    }

    ~TcpShmServer() {
        Stop();
    }


    
    void Run(const char* listen_ipv4, short listen_port) {
        if(!Start(listen_ipv4, listen_port)) return;
        vector<thread> threads;
        // create threads for polling tcp
        for(int i = 0; i < MaxTcpGrps; i++) {
          threads.emplace_back([this, i]() {
            if (do_cpupin) cpupin(4 + i);
            while (!stopped) {
              PollTcp(now(), i);
            }
          });
        }

        // create threads for polling shm
        for(int i = 0; i < MaxShmGrps; i++) {
          threads.emplace_back([this, i]() {
            if (do_cpupin) cpupin(4 + MaxTcpGrps + i);
            while (!stopped) {
              PollShm(i);
            }
          });
        }

        // polling control using this thread
        while(!stopped) {
          PollCtl(now());
        }

        for(auto& thr : threads) {
            thr.join();
        }
        Stop();
        cout << "Server stopped" << endl;
    }


    // start the server
    // return true if success
    bool Start(const char* listen_ipv4, uint16_t listen_port) {
        if(listenfd_ >= 0) {
            OnSystemError("already started", 0);
            return false;
        }

        if((listenfd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            OnSystemError("socket", errno);
            return false;
        }

        fcntl(listenfd_, F_SETFL, O_NONBLOCK);
        int yes = 1;
        if(setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            OnSystemError("setsockopt SO_REUSEADDR", errno);
            return false;
        }
        if(TcpNoDelay && setsockopt(listenfd_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0) {
           OnSystemError("setsockopt TCP_NODELAY", errno);
            return false;
        }

        struct sockaddr_in local_addr;
        local_addr.sin_family = AF_INET;
        inet_pton(AF_INET, listen_ipv4, &(local_addr.sin_addr));
        local_addr.sin_port = htons(listen_port);
        bzero(&(local_addr.sin_zero), 8);
        if(bind(listenfd_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            OnSystemError("bind", errno);
            return false;
        }
        if(listen(listenfd_, 5) < 0) {
            OnSystemError("listen", errno);
            return false;
        }
        return true;
    }

    // poll control for handling new connections and keep shm connections alive
    void PollCtl(int64_t now) {
        // every poll we accept only one connection
        if(avail_idx_ != MaxNewConnections) {
            NewConn& conn = new_conns_[avail_idx_];
            socklen_t addr_len = sizeof(conn.addr);
            conn.fd = accept(listenfd_, (struct sockaddr*)&(conn.addr), &addr_len);
            // we ignore errors from accept as most errno should be treated like EAGAIN
            if(conn.fd >= 0) {
                fcntl(conn.fd, F_SETFL, O_NONBLOCK);
                conn.time = now;
                avail_idx_ = MaxNewConnections;
            }
        }
        // visit all new connections, trying to read LoginMsg
        for(int i = 0; i < MaxNewConnections; i++) {
            NewConn& conn = new_conns_[i];
            if(conn.fd < 0) {
                avail_idx_ = i;
                continue;
            }
            int ret = ::recv(conn.fd, conn.recvbuf, sizeof(conn.recvbuf), 0);
            if(ret < 0 && errno == EAGAIN && now - conn.time <= NewConnectionTimeout) {
                continue;
            }
            if(ret == sizeof(conn.recvbuf)) {
                conn.recvbuf[0].template ConvertByteOrder<ToLittleEndian>();
                if(conn.recvbuf[0].size == sizeof(MsgHeader) + sizeof(LoginMsg) &&
                   conn.recvbuf[0].msg_type == LoginMsg::msg_type) {
                    // looks like a valid login msg
                    LoginMsg* login = (LoginMsg*)(conn.recvbuf + 1);
                    login->ConvertByteOrder();
                    if(login->use_shm) {
                        HandleLogin(now, conn, shm_grps_);
                    }
                    else {
                        HandleLogin(now, conn, tcp_grps_);
                    }
                }
            }

            if(conn.fd >= 0) {
                ::close(conn.fd);
                conn.fd = -1;
            }
            avail_idx_ = i;
        }

        for(auto& grp : shm_grps_) {
            for(int i = 0; i < grp.live_cnt;) {
                Connection& conn = *grp.conns[i];
                conn.TcpFront(now); // poll heartbeats, ignore return
                if(conn.TryCloseFd()) {
                    int sys_errno;
                    const char* reason = conn.GetCloseReason(&sys_errno);
                    OnClientDisconnected(conn, reason, sys_errno);
                    std::swap(grp.conns[i], grp.conns[--grp.live_cnt]);
                }
                else {
                    i++;
                }
            }
        }

        for(auto& grp : tcp_grps_) {
            for(int i = 0; i < grp.live_cnt;) {
                Connection& conn = *grp.conns[i];
                if(conn.TryCloseFd()) {
                    int sys_errno;
                    const char* reason = conn.GetCloseReason(&sys_errno);
                    OnClientDisconnected(conn, reason, sys_errno);
                    std::swap(grp.conns[i], grp.conns[--grp.live_cnt]);
                }
                else {
                    i++;
                }
            }
        }
    }

    // poll tcp for serving tcp connections
    void PollTcp(int64_t now, int grpid) {
        auto& grp = tcp_grps_[grpid];
        // force read grp.live_cnt from memory, it could have been changed by Ctl thread
        asm volatile("" : "=m"(grp.live_cnt) : :);
        for(int i = 0; i < grp.live_cnt; i++) {
            // it's possible that grp.conns is being swapped by Ctl thread
            // so some live conn could be missed, some closed one could be visited
            // even some conn could be visited twice, but those're all fine
            Connection& conn = *grp.conns[i];
            MsgHeader* head = conn.TcpFront(now);
            if(head) OnClientMsg(conn, head);
        }
    }

    // poll shm for serving shm connections
    void PollShm(int grpid) {
        auto& grp = shm_grps_[grpid];
        asm volatile("" : "=m"(grp.live_cnt) : :);
        for(int i = 0; i < grp.live_cnt; i++) {
            Connection& conn = *grp.conns[i];
            MsgHeader* head = conn.ShmFront();
            if(head) OnClientMsg(conn, head);
        }
    }

    void Stop() {
        if(listenfd_ < 0) {
            return;
        }
        ::close(listenfd_);
        listenfd_ = -1;
        for(int i = 0; i < MaxNewConnections; i++) {
            int& fd = new_conns_[i].fd;
            if(fd >= 0) {
                ::close(fd);
                fd = -1;
            }
        }
        avail_idx_ = 0;
        for(auto& grp : shm_grps_) {
            for(auto& conn : grp.conns) {
                conn->Release();
            }
            grp.live_cnt = 0;
        }
        for(auto& grp : tcp_grps_) {
            for(auto& conn : grp.conns) {
                conn->Release();
            }
            grp.live_cnt = 0;
        }
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





    struct NewConn
    {
        int64_t time;
        int fd = -1;
        struct sockaddr_in addr;
        MsgHeader recvbuf[1 + (sizeof(LoginMsg) + 7) / 8];
    };
    template<uint32_t N>
    struct alignas(64) ConnectionGroup
    {
        uint32_t live_cnt = 0;
        Connection* conns[N];
    };

    template<uint32_t N>
    void HandleLogin(int64_t now, NewConn& conn, ConnectionGroup<N>* grps) {
        MsgHeader sendbuf[1 + (sizeof(LoginRspMsg) + 7) / 8];
        sendbuf[0].size = sizeof(MsgHeader) + sizeof(LoginRspMsg);
        sendbuf[0].msg_type = LoginRspMsg::msg_type;
        sendbuf[0].template ConvertByteOrder<ToLittleEndian>();
        LoginRspMsg* login_rsp = (LoginRspMsg*)(sendbuf + 1);
        strncpy(login_rsp->server_name, server_name_, sizeof(login_rsp->server_name));
        login_rsp->status = 2;
        login_rsp->error_msg[0] = 0;

        LoginMsg* login = (LoginMsg*)(conn.recvbuf + 1);
        if(login->client_name[0] == 0) {
            strncpy(login_rsp->error_msg, "Invalid client name", sizeof(login_rsp->error_msg));
            ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
            return;
        }
        login->client_name[sizeof(login->client_name) - 1] = 0;
        int grpid = OnNewConnection(conn.addr, login, login_rsp);
        if(grpid < 0) {
            if(login_rsp->error_msg[0] == 0) { // user didn't set error_msg? set a default one
                strncpy(login_rsp->error_msg, "Login Reject", sizeof(login_rsp->error_msg));
            }
            ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
            return;
        }
        auto& grp = grps[grpid];
        for(int i = 0; i < N; i++) {
            Connection& curconn = *grp.conns[i];
            char* remote_name = curconn.GetRemoteName();
            if(remote_name[0] == 0) { // found an unused one, use it then
                strncpy(remote_name, login->client_name, sizeof(login->client_name));
            }
            if(strncmp(remote_name, login->client_name, sizeof(login->client_name)) != 0) {
                // client name does not match
                continue;
            }
            // match
            if(i < grp.live_cnt) {
                strncpy(login_rsp->error_msg, "Already loggned on", sizeof(login_rsp->error_msg));
                ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
                return;
            }

            const char* error_msg;
            if(!curconn.OpenFile(login->use_shm, &error_msg)) {
                // we can not mmap to ptcp or chm files with filenames related to local and remote name
                OnClientFileError(curconn, error_msg, errno);
                strncpy(login_rsp->error_msg, "System error", sizeof(login_rsp->error_msg));
                ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
                return;
            }
            uint32_t local_ack_seq = 0;
            uint32_t local_seq_start = 0;
            uint32_t local_seq_end = 0;
            uint32_t remote_ack_seq = conn.recvbuf[0].ack_seq;
            uint32_t remote_seq_start = login->client_seq_start;
            uint32_t remote_seq_end = login->client_seq_end;
            // if server_name has changed, reset the ack_seq
            if(strncmp(login->last_server_name, server_name_, sizeof(server_name_)) != 0) {
                curconn.Reset();
                remote_ack_seq = remote_seq_start = remote_seq_end = 0;
            }
            else {
                if(!curconn.GetSeq(&local_ack_seq, &local_seq_start, &local_seq_end, &error_msg)) {
                    OnClientFileError(curconn, error_msg, errno);
                    strncpy(login_rsp->error_msg, "System error", sizeof(login_rsp->error_msg));
                    ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
                    return;
                }
            }
            sendbuf[0].ack_seq = Endian<ToLittleEndian>::Convert(local_ack_seq);
            login_rsp->server_seq_start = local_seq_start;
            login_rsp->server_seq_end = local_seq_end;
            login_rsp->ConvertByteOrder();
            if(!CheckAckInQueue(remote_ack_seq, local_seq_start, local_seq_end) ||
               !CheckAckInQueue(local_ack_seq, remote_seq_start, remote_seq_end)) {
                OnSeqNumberMismatch(curconn,
                                                                 local_ack_seq,
                                                                 local_seq_start,
                                                                 local_seq_end,
                                                                 remote_ack_seq,
                                                                 remote_seq_start,
                                                                 remote_seq_end);
                login_rsp->status = 1;
                ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
                return;
            }

            // send Login OK
            login_rsp->status = 0;
            if(::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL) != sizeof(sendbuf)) {
                return;
            }
            curconn.Open(conn.fd, remote_ack_seq, now);
            conn.fd = -1; // so it won't be closed by caller
            // switch to live
            std::swap(grp.conns[i], grp.conns[grp.live_cnt++]);
            OnClientLogon(conn.addr, curconn);
            return;
        }
        // no space for new remote name
        strncpy(login_rsp->error_msg, "Max client cnt exceeded", sizeof(login_rsp->error_msg));
        ::send(conn.fd, sendbuf, sizeof(sendbuf), MSG_NOSIGNAL);
    }

    // check if seq_start <= ack_seq <= seq_end, considering uint32_t wrap around
    bool CheckAckInQueue(uint32_t ack_seq, uint32_t seq_start, uint32_t seq_end) {
        return (int)(ack_seq - seq_start) >= 0 && (int)(seq_end - ack_seq) >= 0;
    }

    // called with Start()
    // reporting errors on Starting the server
    void OnSystemError(const char* errno_msg, int sys_errno) {
        cout << "System Error: " << errno_msg << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by CTL thread
    // if accept the connection, set user_data in login_rsp and return grpid(start from 0) with respect to tcp or shm
    // else set error_msg in login_rsp if possible, and return -1
    // Note that even if we accept it here, there could be other errors on handling the login,
    // so we have to wait OnClientLogon for confirmation
    int OnNewConnection(const struct sockaddr_in& addr, const LoginMsg* login, LoginRspMsg* login_rsp) {
        cout << "New Connection from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
             << ", name: " << login->client_name << ", use_shm: " << (bool)login->use_shm << endl;
        // here we simply hash client name to uniformly map to each group
        auto hh = hash<string>{}(string(login->client_name));
        if(login->use_shm) {
            if(MaxShmGrps > 0) {
                return hh % MaxShmGrps;
            }
            else {
                strcpy(login_rsp->error_msg, "Shm disabled");
                return -1;
            }
        }
        else {
            if(MaxTcpGrps > 0) {
                return hh % MaxTcpGrps;
            }
            else {
                strcpy(login_rsp->error_msg, "Tcp disabled");
                return -1;
            }
        }
    }

    // called by CTL thread
    // ptcp or shm files can't be open or are corrupt
    void OnClientFileError(Connection& conn, const char* reason, int sys_errno) {
        cout << "Client file errno, name: " << conn.GetRemoteName() << " reason: " << reason
             << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by CTL thread
    // server and client ptcp sequence number don't match, we need to fix it manually
    void OnSeqNumberMismatch(Connection& conn,
                             uint32_t local_ack_seq,
                             uint32_t local_seq_start,
                             uint32_t local_seq_end,
                             uint32_t remote_ack_seq,
                             uint32_t remote_seq_start,
                             uint32_t remote_seq_end) {
        cout << "Client seq number mismatch, name: " << conn.GetRemoteName() << " ptcp file: " << conn.GetPtcpFile()
             << " local_ack_seq: " << local_ack_seq << " local_seq_start: " << local_seq_start
             << " local_seq_end: " << local_seq_end << " remote_ack_seq: " << remote_ack_seq
             << " remote_seq_start: " << remote_seq_start << " remote_seq_end: " << remote_seq_end << endl;
    }

    // called by CTL thread
    // confirmation for client logon
    void OnClientLogon(const struct sockaddr_in& addr, Connection& conn) {
        cout << "Client Logon from: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
             << ", name: " << conn.GetRemoteName() << endl;
    }

    // called by CTL thread
    // client is disconnected
    void OnClientDisconnected(Connection& conn, const char* reason, int sys_errno) {
        cout << "Client disconnected,.name: " << conn.GetRemoteName() << " reason: " << reason
             << " syserrno: " << strerror(sys_errno) << endl;
    }

    // called by APP thread
    void OnClientMsg(Connection& conn, MsgHeader* recv_header) {
        auto size = recv_header->size - sizeof(MsgHeader);
        MsgHeader* send_header = conn.Alloc(size);
        if(!send_header) return;
        send_header->msg_type = recv_header->msg_type;
        memcpy(send_header + 1, recv_header + 1, size);
        // if we call Push() before Pop(), there's a good chance Pop() is not called in case of program crash
        conn.Pop();
        conn.Push();
    }
    static volatile bool stopped;
    // set do_cpupin to true to get more stable latency
    bool do_cpupin = true;



private:

    char server_name_[NameSize];
    std::string ptcp_dir_;
    int listenfd_ = -1;

    NewConn new_conns_[MaxNewConnections];
    int avail_idx_ = 0;

    Connection conn_pool_[MaxShmConnsPerGrp * MaxShmGrps + MaxTcpConnsPerGrp * MaxTcpGrps];
    ConnectionGroup<MaxShmConnsPerGrp> shm_grps_[MaxShmGrps];
    ConnectionGroup<MaxTcpConnsPerGrp> tcp_grps_[MaxTcpGrps];
};
volatile bool TcpShmServer::stopped = false;
