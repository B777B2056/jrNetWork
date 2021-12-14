#ifndef DISPATCH_H
#define DISPATCH_H

#include "log.hpp"
#include "timer.hpp"
#include "socket.hpp"
#include "thread_pool.hpp"
#include <functional>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>

namespace jrNetWork {
    class EventDispatch {
        /* ======== Unix ======== */
    private:
        /* Epoll */
        int epollfd;    // epoll file descriptor
        epoll_event ee; // epoll event object
        epoll_event* events;    // epoll event object's array
        void epoll_init();  // epoll init, create epoll file descriptor
        bool regist_epoll_event(int event); // Register event into IO model
        bool unregist_epoll_event(int event);   // Unregister event from IO model
        /* Unified event source */
        static int uesfd[2];
        static void ues_transfer(int sig);   // Wrire signal to uesfd
        void ues_init();    // Init Unified Event Source(UES)
        bool ues_handler(); // Handle system signal, ret value is flag of timeout connection

        /* ======== Unix/Windows ======== */
    private:
        using TaskHandlerType = std::function<void(TCP::Socket*)>;
        using TimeoutHandlerType = std::function<void(TCP::Socket*)>;
        /* TCP socket object */
        TCP::Socket socket;
        /* Timer heap */
        TimerContainer tc;
        /* Thread pool */
        uint max_task_num;  // Thread pool Max Task Num
        jrThreadPool::ThreadPool thread_pool;
        /* Task handler */
        TaskHandlerType task_handler;
        /* Timeout handler */
        TimeoutHandlerType timeout_handler;
        /* Socket init */
        void socket_init(uint port);
        /* Init IO model(Linux: epoll) */
        void io_init();

    public:
        /* Init thread pool and IO model */
        EventDispatch(uint port, uint max_task_num,
                      uint max_pool_size=std::thread::hardware_concurrency(), std::string path="");
        /* Set event handler, the last argument must be client's TCPSocket object's reference */
        template<typename F, typename... Args>
        void set_event_handler(F&& event_handler, Args&&... args);
        /* Set timeout handler(default timeout handler is closing timeout connection) */
        void set_timeout_handler(TimeoutHandlerType handler);
        /* Event Loop */
        void event_loop(uint timeout_period_sec);
    };

    int jrNetWork::EventDispatch::uesfd[2];

    EventDispatch::EventDispatch(uint port, uint max_task_num, uint max_pool_size, std::string path)
        : events(new epoll_event[max_task_num]), socket(TCP::Socket::IO_NONBLOCKING),
          max_task_num(max_task_num), thread_pool(max_task_num, max_pool_size) {
        jrNetWork::logger_path = path;
        socket_init(port);
        io_init();
        timeout_handler = [this](TCP::Socket* client)->void
                          {
                            unregist_epoll_event(client->socket_fd);
                            client->disconnect();
                          };
    }

    void EventDispatch::socket_init(uint port) {
        try {
            /* Bind ip address and port */
            socket.bind(port);
            /* Listen target port */
            socket.listen();
        } catch (const std::string& msg) {
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    void EventDispatch::io_init() {
        epoll_init();
    }

    void EventDispatch::epoll_init() {
        epollfd = epoll_create(max_task_num);
        if(epollfd == -1) {
            std::string msg = std::string("Epoll create failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        ues_init();
    }

    bool EventDispatch::regist_epoll_event(int event) {
        ee.events = EPOLLIN | EPOLLET;
        ee.data.fd = event;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, event, &ee) == -1) {
            LOG(Logger::Level::WARNING, std::string("Epoll event regist failed: ") + strerror(errno));
            return false;
        }
        return true;
    }

    bool EventDispatch::unregist_epoll_event(int event) {
        ee.events = EPOLLIN | EPOLLET;
        ee.data.fd = event;
        if(epoll_ctl(epollfd, EPOLL_CTL_DEL, event, &ee) == -1) {
            LOG(Logger::Level::WARNING, std::string("Epoll event unregist failed: ") + strerror(errno));
            return false;
        }
        return true;
    }

    void EventDispatch::ues_transfer(int sig) {
        if(-1 == write(uesfd[1], reinterpret_cast<char*>(&sig), 1)) {
            std::string msg = std::string("Unified Event Source transfer failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    void EventDispatch::ues_init() {
        if(-1 == pipe(uesfd)) {
            std::string msg = std::string("Unified Event Source pipe failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        // Set ues fd write non-blocking
        int flag = fcntl(uesfd[1], F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(uesfd[1], F_SETFL, flag);
        // Alarm-time bind
        signal(SIGALRM, ues_transfer);
        // SIG sig bind
        signal(SIGINT, ues_transfer);
        signal(SIGTERM, ues_transfer);
        signal(SIGPIPE, ues_transfer);
        // Register server socket
        if(!regist_epoll_event(socket.socket_fd) || !regist_epoll_event(uesfd[0])) {
            std::string msg = "Unified Event Source epoll regist failed";
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    bool EventDispatch::ues_handler() {
        /* Handle signals */
        char sig[32];
        memset(sig, 0, sizeof(sig));
        int num = read(uesfd[0], sig, sizeof(sig));
        if(num <= 0)
            return false;
        for(int i = 0; i < num; ++i) {
            switch(sig[i]) {
                case SIGALRM:
                    return true;    // timeout task
                    LOG(Logger::Level::WARNING, "Client connection timeout, client ip: "
                                              + TCP::Socket(uesfd[0], socket.blocking_flag).get_ip_from_socket());
                    break;
                case SIGTERM:
                case SIGINT:    // Stop server
                    LOG(Logger::Level::FATAL, "Server interrupt by system signal");
                    exit(1);
                case SIGPIPE:
                    TCP::Socket(uesfd[0], socket.blocking_flag).disconnect();
                    break;
            }
        }
        return false;
    }

    template<typename F, typename... Args>
    void EventDispatch::set_event_handler(F&& event_handler, Args&&... args) {
        auto event_handler_bind = std::bind(std::forward<F>(event_handler),
                                            std::forward<Args>(args)...,
                                            std::placeholders::_1);
        task_handler = [event_handler_bind](TCP::Socket* client)->void
                       {
                            event_handler_bind(client);
                       };
    }

    void EventDispatch::set_timeout_handler(TimeoutHandlerType handler) {
        timeout_handler = [this, handler](TCP::Socket* client)->void
                          {
                              unregist_epoll_event(client->socket_fd);
                              handler(client);
                          };
    }

    void EventDispatch::event_loop(uint timeout_period_sec) {
        if(!task_handler) {
            std::string msg = "No event handler binded!";
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        alarm(timeout_period_sec);
        while(true) {
            bool have_timeout = false;
            int event_cnt = epoll_wait(epollfd, events, max_task_num, -1);
            if(-1 == event_cnt) {
                /* Debugger may send SIGTRAP signal, it causes epoll_wait return -1 and has EINTR error. */
                if(errno != EINTR) {
                    LOG(Logger::Level::WARNING, std::string("Epoll wait error: ") + strerror(errno));
                    return ;
                }
            }
            for(int i = 0; i < event_cnt; ++i) {
                int currentfd = events[i].data.fd;
                if(currentfd == socket.socket_fd) {
                    /* Accept connection */
                    std::shared_ptr<TCP::Socket> client = socket.accept();
                    /* Register client into epoll */
                    if(!client) {
                        LOG(Logger::Level::WARNING, std::string("Connection accept failed: ") + strerror(errno));
                        continue;
                    }
                    if(!regist_epoll_event(client->socket_fd)) {
                        LOG(Logger::Level::WARNING, std::string("Connection epoll regist failed: ") + strerror(errno));
                        continue;
                    }
                    /* Add timer into container */
                    tc.add_timer(client.get(), timeout_period_sec, timeout_handler);
                    LOG(Logger::Level::NOTICE, "Connection accepted, client ip: " + client->get_ip_from_socket());
                } else if(events[i].events & EPOLLIN)  {
                    if(currentfd == uesfd[0]) {
                        have_timeout = ues_handler();
                    } else {
                        /* Add task into thread pool */
                        if(!thread_pool.add_task([this, currentfd]()->void
                                                 {
                                                    TCP::Socket client(currentfd, socket.blocking_flag);
                                                    task_handler(&client);
                                                 })) {
                            LOG(Logger::Level::WARNING, "thread pool is full");
                        }
                    }
                } else if(events[i].events & EPOLLOUT) {

                }
                /* Handle timeout task */
                if(have_timeout) {
                    tc.tick();
                    alarm(timeout_period_sec);
                }
            }
        }
    }
}

#endif
