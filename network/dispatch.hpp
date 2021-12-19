#ifndef DISPATCH_H
#define DISPATCH_H

#include "log.hpp"
#include "io_model.hpp"
#include "socket.hpp"
#include "timer.hpp"
#include "thread_pool.hpp"
#include <map>
#include <memory>
#include <functional>
#include <fcntl.h>
#include <signal.h>

namespace jrNetWork {
    using TaskHandlerType = std::function<void(std::shared_ptr<TCP::Socket>)>;
    using TimeoutHandlerType = std::function<void(std::shared_ptr<TCP::Socket>)>;

    class UnifiedEventSource {
    public:
        static int uesfd[2];
        static void ues_transfer(int sig);   // Wrire signal to uesfd
        UnifiedEventSource();    // Init Unified Event Source(UES)
        ~UnifiedEventSource();
        bool handle(); // Handle system signal, ret value is flag of timeout connection
    };

    class EventDispatch {
    private:
        MultiplexerBase<IO_Model_EPOLL>* multiplexer;
        std::map<int, std::shared_ptr<TCP::Socket>> fd_socket_table;
        /* Unified event source */
        UnifiedEventSource ues;
        /* TCP socket object */
        std::shared_ptr<TCP::Socket> socket;
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
        /* Accept connection */
        void accept_handle(uint timeout_period_sec);
        /* Event handler */
        void event_handle(uint timeout_period_sec, Event<IO_Model_EPOLL> current);
        /* Send data in buffer */
        void buffer_handle(int client);

    public:
        /* Init thread pool and IO model */
        EventDispatch(uint port, uint max_task_num,
                      uint max_pool_size=std::thread::hardware_concurrency(), std::string path="");
        /* Release connection resources */
        ~EventDispatch();
        /* Set event handler, the last argument must be client's TCPSocket object's reference */
        template<typename F, typename... Args>
        void set_event_handler(F&& handler, Args&&... args);
        /* Set timeout handler(default timeout handler is closing timeout connection) */
        template<typename F, typename... Args>
        void set_timeout_handler(F&& handler, Args&&... args);
        /* Event Loop */
        void event_loop(uint timeout_period_sec);
    };

    template<typename F, typename... Args>
    void EventDispatch::set_event_handler(F&& handler, Args&&... args) {
        auto event_handler_bind = std::bind(std::forward<F>(handler),
                                            std::forward<Args>(args)...,
                                            std::placeholders::_1);
        task_handler = [event_handler_bind](std::shared_ptr<jrNetWork::TCP::Socket> client)->void
                       {
                            event_handler_bind(client);
                       };
    }

    template<typename F, typename... Args>
    void EventDispatch::set_timeout_handler(F&& handler, Args&&... args) {
        auto handler_bind = std::bind(std::forward<F>(handler),
                                      std::forward<Args>(args)...,
                                      std::placeholders::_1);
        timeout_handler = [handler_bind](std::shared_ptr<TCP::Socket> client)->void
                          {
                              handler_bind(client);
                          };
    }
}

#endif
