#ifndef NETWORK_H
#define NETWORK_H

#include "timer.hpp"
#include "thread_pool.hpp"
#include <string>
#include <cstring>
#include <cstdlib>

extern "C" {
    #include <fcntl.h>
    #include <unistd.h>
    #include <signal.h>
    #include <sys/time.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <sys/epoll.h>
}

namespace jrNetWork {
    enum ErrorCode {ESOCKET, EBIND, ELISTEN, EIO_INIT, EREGIST, EUNREGIST, EUES_PIPE, EDISPATCH, ESTOP, EREAD, ESEND, NOERROR};

    class TCP_server {
    private:
        using uint = unsigned int;

    private:
        int serverfd;
        int epollfd;
        epoll_event ee;
        /* Epoll and THread pool Max Task Num */
        uint max_task_num;
        /* File descriptors for unified event source */
        static int uesfd[2];
        /* Epoll events array */
        epoll_event* events;
        /* Timer heap */
        timer_container tc;
        /* Thread pool */
        jrThreadPool::thread_pool t_pool;

    private:
        /* Init socket, return server file descriptor */
        ErrorCode socket_init(uint port);
        /* Init IO model(Linux: epoll), return epoll file descriptor */
        ErrorCode io_init();
        /* Init Unified Event Source(UES) */
        ErrorCode ues_init();
        /* Unified event source handler */
        static void ues_handler(int);

    public:
        /* Init thread pool */
        TCP_server(uint max_task_num, uint max_pool_size = std::thread::hardware_concurrency());
        /* Close all connection */
        ~TCP_server();
        /* Init socket and IO model */
        ErrorCode init(uint port);
        /* Register event into IO model */
        ErrorCode regist_event(int event);
        /* Unregister event fromIO model */
        ErrorCode unregist_event(int event);
        /* Read data from connected socket */
        std::string get_data(int client) const;
        /* Write data into connected socket */
        ErrorCode set_data(int client, const std::string& data);
        /* Close connection */
        void close_conn(int fd);
        /* Thread pool + Dispatch = Reactor */
        template<typename F, typename... Args>
        ErrorCode event_loop(uint timeout_period_sec, F&& f, Args&&... args);
    };

    int jrNetWork::TCP_server::uesfd[2];

    TCP_server::TCP_server(uint max_task_num, uint max_pool_size)
        : max_task_num(max_task_num), events(new epoll_event[max_task_num]), t_pool(max_task_num, max_pool_size) {

    }

    TCP_server::~TCP_server() {
        close(uesfd[0]);
        close(uesfd[1]);
        close(this->epollfd);
    }

    ErrorCode TCP_server::socket_init(uint port) {
        /* Create server socket file description */
        this->serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == this->serverfd)
            return ESOCKET;
        /* Bind ip address and port */
        sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == bind(this->serverfd, reinterpret_cast<sockaddr*>(&addr), sizeof(struct sockaddr_in)))
            return EBIND;
        /* Listen target port */
        if(-1 == listen(this->serverfd, this->max_task_num))
            return ELISTEN;
        return  NOERROR;
    }

    ErrorCode TCP_server::io_init() {
        this->epollfd = epoll_create(this->max_task_num);
        return this->epollfd==-1 ? EIO_INIT : NOERROR;
    }

    void TCP_server::ues_handler(int sig) {
        write(uesfd[1], reinterpret_cast<char*>(&sig), 1);
    }

    ErrorCode TCP_server::ues_init() {
        if(-1 == pipe(uesfd))
            return EUES_PIPE;
        // Set ues fd write non-blocking
        int flag = fcntl(uesfd[1], F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(uesfd[1], F_SETFL, flag);
        // Alarm-time bind
        signal(SIGALRM, ues_handler);
        // SIG sig bind
        signal(SIGINT, ues_handler);
        signal(SIGTERM, ues_handler);
        signal(SIGPIPE, ues_handler);
        // Register server socket
        if(this->regist_event(this->serverfd) == jrNetWork::EREGIST)
            return EREGIST;
        if(this->regist_event(uesfd[0]) == jrNetWork::EREGIST)
            return EREGIST;
        return NOERROR;
    }

    ErrorCode TCP_server::init(uint port) {
        ErrorCode s = this->socket_init(port);
        if(s != NOERROR)
            return s;
        ErrorCode e = this->io_init();
        if(e != NOERROR)
            return e;
        ErrorCode u = this->ues_init();
        if(u != NOERROR)
            return u;
        return NOERROR;
    }

    ErrorCode TCP_server::regist_event(int event) {
        this->ee.events = EPOLLIN | EPOLLET;
        this->ee.data.fd = event;
        if(-1 == epoll_ctl(this->epollfd, EPOLL_CTL_ADD, event, &this->ee))
            return EREGIST;
        return NOERROR;
    }

    ErrorCode TCP_server::unregist_event(int event) {
        this->ee.events = EPOLLIN | EPOLLET;
        this->ee.data.fd = event;
        if(-1 == epoll_ctl(this->epollfd, EPOLL_CTL_DEL, event, &this->ee))
            return EUNREGIST;
        return NOERROR;
    }

    std::string TCP_server::get_data(int client) const {
        char buffer;
        int recv_size;
        std::string str;
        while(true) {
            recv_size = recv(client, &buffer, 1, 0);
            if(buffer == '#')
                break;
            if(-1 == recv_size) {
//                    LOG_WARNING(log, std::string("Read error: ") + strerror(errno));
                break;
            } else if(0 == recv_size) {
                break;
            } else {
                str += buffer;
            }
        }
        return str;
    }

    ErrorCode TCP_server::set_data(int client, const std::string& data) {
        if(-1 == send(client, data.c_str(), data.length(), 0))
            return ESEND;
        return NOERROR;
    }

    void TCP_server::close_conn(int fd) {
        close(fd);
    }

    template<typename F, typename... Args>
    ErrorCode TCP_server::event_loop(uint timeout_period_sec, F&& f, Args&&... args) {
        alarm(timeout_period_sec);
        while(true) {
            bool have_timeout = false;
            int event_cnt = epoll_wait(this->epollfd, events, this->max_task_num, -1);
            if(-1 == event_cnt) {
                if(errno != EINTR)
                    return EDISPATCH;
//                    LOG_WARNING(log, std::string("Epoll signal fd error: ") + strerror(errno));
            }
            for(int i = 0; i < event_cnt; ++i) {
                int curfd = events[i].data.fd;
                if(curfd == this->serverfd) {
                    /* Accept connection */
                    int client = accept(this->serverfd, nullptr, nullptr);
                    if(-1 == client) {
                        // Ignore "Interrupted system call", reason see README.md
//                        if(errno != EINTR)
//                            LOG_WARNING(log, std::string("Accept connection error: ") + strerror(errno));
                        continue;
                    }
                    /* Register client into epoll */
                    if(this->regist_event(client) == EREGIST) {
//                        LOG_WARNING(log, std::string("Client epoll error, client fd: ") + strerror(errno));
                        continue;
                    }
                    /* Timer init */
                    tc.add_timer(timer(client, timeout_period_sec,
                                       [this](int client)->void
                                        {
                                            this->unregist_event(client);
                                            this->close_conn(client);
                                        }
                                      )
                                 );
//                    LOG_NOTICE(log, "Connection accepted, client ip: " + log.get_ip_from_fd(client));
                } else if(events[i].events & EPOLLIN)  {
                    if(curfd == uesfd[0]) {
                        /* Handle signals */
                        char sig[32];
                        memset(sig, 0, sizeof(sig));
                        int num = read(uesfd[0], sig, sizeof(sig));
                        if(num <= 0)
                            continue;
                        for(auto j = 0; j < num; ++j) {
                            switch(sig[j]) {
                                case SIGALRM:
                                    have_timeout = true;    // timeout task
//                                    LOG_WARNING(log, "Client connection timeout, client ip: " + log.get_ip_from_fd(curfd));
                                    break;
                                case SIGTERM:
                                case SIGINT:    // Stop server
//                                    LOG_NOTICE(log, "Server interrupt by system signal");
                                    return ESTOP;
                                case SIGPIPE:
                                    break;
                            }
                        }
                    } else {
                        /* Build a task handler for passing to thread pool */
                        auto f_bind = std::bind(std::forward<F>(f), std::forward<Args>(args)..., std::placeholders::_1);
                        auto task_handler = [this, f_bind](int client)->void {
                            this->set_data(client, f_bind(this->get_data(client)));
                        };
                        /* Add task into thread pool */
                        if(!this->t_pool.add_task(task_handler, curfd)) {
//                            LOG_WARNING(log, "thread pool is full");
                        }
                    }
                }
                if(have_timeout) {
                    /* Handle timeout task */
                    tc.tick();
                    alarm(timeout_period_sec);
                }
            }
        }
        return NOERROR;
    }
}

#endif
