#ifndef DISPATCH_H
#define DISPATCH_H

#include "io_model.hpp"
#include <functional>

namespace jrNetWork {
    class EventDispatch {
    private:
        /* ======== Unix/Windows ======== */
        /* TCP socket object */
        TCP::Socket socket;
        /* Timer heap */
        TimerContainer tc;
        /* Thread pool */
        jrThreadPool::ThreadPool thread_pool;
        /* Task handler */
        TaskHandlerType task_handler;
        /* Timeout handler */
        TimeoutHandlerType timeout_handler;
        /* Socket init */
        void socket_init(uint port);
        /* Init IO model(Linux: epoll) */
        void io_init();

    private:
        /* ======== Unix ======== */
        IOModel io_model;

    public:
        /* Init thread pool and IO model */
        EventDispatch(uint port, uint max_task_num,
                      uint max_pool_size=std::thread::hardware_concurrency(), std::string path="");
        /* Release connection resources */
        ~EventDispatch();
        /* Set event handler, the last argument must be client's TCPSocket object's reference */
        template<typename F, typename... Args>
        void set_event_handler(F&& event_handler, Args&&... args);
        /* Set timeout handler(default timeout handler is closing timeout connection) */
        void set_timeout_handler(TimeoutHandlerType handler);
        /* Event Loop */
        void event_loop(uint timeout_period_sec);
    };

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
}

#endif
