#ifndef DISPATCH_H
#define DISPATCH_H

#include "log.hpp"
#include "timer.hpp"
#include "socket.hpp"
#include "thread_pool.hpp"
#include <string>
#include <functional>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/epoll.h>

namespace jrNetWork {
    class EventDispatch {
        /* ======== Unix ======== */
    private:
        /* Epoll */
        int epollfd;    // epoll file descriptor
        epoll_event ee; // epoll event object
        epoll_event* events;    // epoll event object's array
        bool epoll_init();  // epoll init, create epoll file descriptor
        bool regist_epoll_event(int event); // Register event into IO model
        bool unregist_epoll_event(int event);   // Unregister event from IO model
        /* Unified event source */
        static int uesfd[2];
        static void ues_handler(int);   // Wrire signal to uesfd
        bool ues_init();    // Init Unified Event Source(UES)

        /* ======== Unix/Windows ======== */
    private:
        using TaskHandlerType = std::function<void(TCPSocket*)>;
        using TimeoutHandlerType = std::function<void(TCPSocket*)>;
        /* TCP socket object */
        TCPSocket* socket;
        /* Timer heap */
        TimerContainer tc;
        /* Thread pool */
        uint max_task_num;  // Thread pool Max Task Num
        jrThreadPool::ThreadPool thread_pool;
        /* Task handler */
        TaskHandlerType task_handler;
        /* Timeout handler */
        TimeoutHandlerType timeout_handler;
        /* Logger storage path */
        const std::string logger_path;
        /* Init IO model(Linux: epoll) */
        bool io_init();

    public:
        /* Init thread pool and IO model */
        EventDispatch(TCPSocket* socket, uint max_task_num,
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

    EventDispatch::EventDispatch(TCPSocket* socket, uint max_task_num, uint max_pool_size, std::string path)
        : events(new epoll_event[max_task_num]), socket(socket),
          max_task_num(max_task_num), thread_pool(max_task_num, max_pool_size), logger_path(path) {
        if(!io_init()) {
            std::string msg = std::string("IO init failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        timeout_handler = [this](TCPSocket* client)->void
                          {
                            unregist_epoll_event(client->socket_fd);
                            client->close();
                          };
    }

    bool EventDispatch::io_init() {
        return epoll_init();
    }

    bool EventDispatch::epoll_init() {
        epollfd = epoll_create(max_task_num);
        if(epollfd == -1) {
            LOG(Logger::Level::WARNING, std::string("Epoll create failed: ") + strerror(errno));
            return false;
        }
        return ues_init();
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

    void EventDispatch::ues_handler(int sig) {
        write(uesfd[1], reinterpret_cast<char*>(&sig), 1);
    }

    bool EventDispatch::ues_init() {
        if(-1 == pipe(uesfd))
            return false;
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
        return regist_epoll_event(socket->socket_fd) && regist_epoll_event(uesfd[0]);
    }

    template<typename F, typename... Args>
    void EventDispatch::set_event_handler(F&& event_handler, Args&&... args) {
        auto event_handler_bind = std::bind(std::forward<F>(event_handler),
                                            std::forward<Args>(args)...,
                                            std::placeholders::_1);
        task_handler = [&event_handler_bind](TCPSocket* client)->void
                       {
                            event_handler_bind(client);
                       };
    }

    void EventDispatch::set_timeout_handler(TimeoutHandlerType handler) {
        timeout_handler = [this, handler](TCPSocket* client)->void
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
                if(currentfd == socket->socket_fd) {
                    /* Accept connection */
                    std::shared_ptr<TCPSocket> client = socket->accept();
                    /* Register client into epoll */
                    if(!client || !regist_epoll_event(client->socket_fd))
                        continue;
                    /* Add timer into container */
                    tc.add_timer(client.get(), timeout_period_sec, timeout_handler);
                    LOG(Logger::Level::NOTICE, "Connection accepted, client ip: " + client->get_ip_from_socket());
                } else if(events[i].events & EPOLLIN)  {
                    if(currentfd == uesfd[0]) {
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
                                    LOG(Logger::Level::WARNING, "Client connection timeout, client ip: "
                                                              + TCPSocket(currentfd).get_ip_from_socket());
                                    break;
                                case SIGTERM:
                                case SIGINT:    // Stop server
                                    LOG(Logger::Level::FATAL, "Server interrupt by system signal");
                                    return ;
                                case SIGPIPE:
                                    break;
                            }
                        }
                    } else {
                        /* Add task into thread pool */
                        if(!thread_pool.add_task([this, currentfd]()->void
                                                 {
                                                    TCPSocket client(currentfd);
                                                    task_handler(&client);
                                                 })) {
                            LOG(Logger::Level::WARNING, "thread pool is full");
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
    }
}

#endif
