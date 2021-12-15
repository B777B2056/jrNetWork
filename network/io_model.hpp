#ifndef IO_MODEL_H
#define IO_MODEL_H

#include "log.hpp"
#include "timer.hpp"
#include "socket.hpp"
#include "thread_pool.hpp"
#include <cstring>

#ifdef __linux__
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#elif defined _WIN32

#endif

namespace jrNetWork {
    using TaskHandlerType = std::function<void(TCP::Socket*)>;
    using TimeoutHandlerType = std::function<void(TCP::Socket*)>;

    class IOModel {
#ifdef __linux__
        /* ======== Unix:Epoll ======== */
    private:
        uint max_task_num;
        TCP::Socket socket;
        /* Epoll */
        int epollfd;    // epoll file descriptor
        epoll_event ee; // epoll event object
        epoll_event* events;    // epoll event object's array
        /* Unified event source */
        static int uesfd[2];
        static void ues_transfer(int sig);   // Wrire signal to uesfd
        void ues_init();    // Init Unified Event Source(UES)
        bool ues_handler(); // Handle system signal, ret value is flag of timeout connection
#elif defined _WIN32
        /* ======== Windows:IOCP ======== */
#endif

    public:
        IOModel() = default;  // Epoll init
        ~IOModel(); // Destroy resource
        void epoll_init(TCP::Socket& socket, uint max_task_num);  // epoll init, create epoll file descriptor
        bool regist_io_event(TCP::Socket* event);    // Register event into IO model
        bool unregist_io_event(TCP::Socket* event);  // Unregister event from IO model
        void io_handler(uint timeout_period_secm,
                             TimeoutHandlerType timeout_handler, TaskHandlerType task_handler,
                             TimerContainer&tc,  jrThreadPool::ThreadPool& thread_pool);   // Epoll io multiplexing
    };
}

#endif
