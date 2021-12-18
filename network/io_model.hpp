#ifndef IO_MODEL_H
#define IO_MODEL_H

#include "log.hpp"
#include "timer.hpp"
#include "socket.hpp"
#include "thread_pool.hpp"
#include <map>
#include <cstring>

#ifdef __linux__
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#elif defined _WIN32

#endif

namespace jrNetWork {
    using TaskHandlerType = std::function<void(std::shared_ptr<TCP::Socket>)>;
    using TimeoutHandlerType = std::function<void(std::shared_ptr<TCP::Socket>)>;

    class IOModel {
    private:
        /* Unix/Windows */
        uint max_task_num;
        std::shared_ptr<TCP::Socket> socket;

#ifdef __linux__
        /* ======== Unix:Epoll ======== */
    private:
        std::map<int, std::shared_ptr<TCP::Socket>> fd_socket_table;
        /* Epoll */
        int epollfd;    // epoll file descriptor
        epoll_event ee; // epoll event object
        epoll_event* events;    // epoll event object's array
        void epoll_init(uint max_task_num);  // epoll init, create epoll file descriptor
        bool regist_epoll_event(int event, uint32_t type);    // Register event into epoll wait
        bool unregist_epoll_event(int event, uint32_t type);  // Unregister event from epoll wait
        /* Unified event source */
        static int uesfd[2];
        static void ues_transfer(int sig);   // Wrire signal to uesfd
        void ues_init();    // Init Unified Event Source(UES)
        bool ues_handler(); // Handle system signal, ret value is flag of timeout connection
#elif defined _WIN32
        /* ======== Windows:IOCP ======== */
#endif

    public:
        IOModel();
        ~IOModel(); // Destroy resource
        void io_model_init(std::shared_ptr<TCP::Socket> socket, uint max_task_num);  // epoll init, create epoll file descriptor
        void io_handler(uint timeout_period_secm,
                        TimeoutHandlerType timeout_handler, TaskHandlerType task_handler,
                        TimerContainer&tc,  jrThreadPool::ThreadPool& thread_pool);   // Epoll io multiplexing
    };
}

#endif
