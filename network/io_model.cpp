#include "io_model.hpp"

namespace jrNetWork {
#ifdef __unix__
    int jrNetWork::IOModel::uesfd[2];

    IOModel::~IOModel() {
        ::close(uesfd[0]);
        ::close(uesfd[1]);
        ::close(epollfd);
        delete[] events;
    }

    void IOModel::epoll_init(TCP::Socket &socket, uint max_task_num) {
        this->socket = socket;
        this->max_task_num = max_task_num;
        this->events = new epoll_event[max_task_num];
        epollfd = epoll_create(max_task_num);
        if(epollfd == -1) {
            std::string msg = std::string("Epoll create failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        ues_init();
    }

    void IOModel::ues_transfer(int sig) {
        if(-1 == write(uesfd[1], reinterpret_cast<char*>(&sig), 1)) {
            std::string msg = std::string("Unified Event Source transfer failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    void IOModel::ues_init() {
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
        TCP::Socket temp(uesfd[0]);
        if(!regist_io_event(&socket) || !regist_io_event(&temp)) {
            std::string msg = "Unified Event Source epoll regist failed";
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    bool IOModel::ues_handler() {
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

    bool IOModel::regist_io_event(TCP::Socket* event) {
        ee.events = EPOLLIN | EPOLLET;
        ee.data.fd = event->socket_fd;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, event->socket_fd, &ee) == -1) {
            LOG(Logger::Level::WARNING, std::string("Epoll event regist failed: ") + strerror(errno));
            return false;
        }
        return true;
    }

    bool IOModel::unregist_io_event(TCP::Socket* event) {
        ee.events = EPOLLIN | EPOLLET;
        ee.data.fd = event->socket_fd;
        if(epoll_ctl(epollfd, EPOLL_CTL_DEL, event->socket_fd, &ee) == -1) {
            LOG(Logger::Level::WARNING, std::string("Epoll event unregist failed: ") + strerror(errno));
            return false;
        }
        return true;
    }

    void IOModel::io_handler(uint timeout_period_sec,
                                      TimeoutHandlerType timeout_handler, TaskHandlerType task_handler,
                                      TimerContainer&tc, jrThreadPool::ThreadPool& thread_pool) {
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
                    if(!regist_io_event(client.get())) {
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
                        if(!thread_pool.add_task([this, task_handler, currentfd]()->void
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

#elif defined _WIN32

#endif
}
