#include "io_model.hpp"

namespace jrNetWork {
#ifdef __unix__
    int jrNetWork::IOModel::uesfd[2];

    IOModel::IOModel() {
        fd_socket_table.clear();
    }

    IOModel::~IOModel() {
        ::close(uesfd[0]);
        ::close(uesfd[1]);
        ::close(epollfd);
        delete[] events;
    }

    void IOModel::io_model_init(std::shared_ptr<TCP::Socket> socket, uint max_task_num) {
        this->socket = socket;
        this->max_task_num = max_task_num;
        epoll_init(max_task_num);
    }

    void IOModel::epoll_init(uint max_task_num) {
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
        write(uesfd[1], reinterpret_cast<char*>(&sig), 1);
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
        if(!regist_epoll_event(socket->socket_fd, EPOLLIN) || !regist_epoll_event(uesfd[0], EPOLLIN)) {
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
                                              + fd_socket_table[uesfd[0]]->get_ip_from_socket());
                    break;
                case SIGTERM:
                case SIGINT:    // Stop server
                    LOG(Logger::Level::FATAL, "Server interrupt by system signal");
                    exit(1);
                case SIGPIPE:
                    break;
            }
        }
        return false;
    }

    bool IOModel::regist_epoll_event(int event, uint32_t type) {
        ee.events = type | EPOLLET;
        ee.data.fd = event;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, event, &ee) == -1) {
            LOG(Logger::Level::WARNING, std::string("Epoll event regist failed: ") + strerror(errno));
            return false;
        }
        return true;
    }

    bool IOModel::unregist_epoll_event(int event, uint32_t type) {
        ee.events = type | EPOLLET;
        ee.data.fd = event;
        if(epoll_ctl(epollfd, EPOLL_CTL_DEL, event, &ee) == -1) {
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
            bool is_timeout = false;
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
                    std::shared_ptr<TCP::Socket> client = socket->accept();
                    /* Register client into epoll */
                    if(!client) {
                        LOG(Logger::Level::WARNING, std::string("Connection accept failed: ") + strerror(errno));
                        continue;
                    }
                    if(!regist_epoll_event(client->socket_fd, EPOLLIN)) {
                        LOG(Logger::Level::WARNING, std::string("Connection epoll regist failed: ") + strerror(errno));
                        continue;
                    }
                    fd_socket_table[client->socket_fd] = client;
                    /* Add timer into container */
                    tc.add_timer(client, timeout_period_sec, timeout_handler);
                    LOG(Logger::Level::NOTICE, "Connection accepted, client ip: " + client->get_ip_from_socket());
                } else if(events[i].events & EPOLLIN) {
                    if(currentfd == uesfd[0]) {
                        is_timeout = ues_handler();
                    } else {
                        auto task = [this, task_handler, currentfd]()->void
                        {
                           // Execute user-specified logic
                           task_handler(fd_socket_table[currentfd]);
                           // If the data has not been sent at one time,
                           // it will be pushed into the buffer (completed by TCP::Socket),
                           // and then register the EPOLLOUT event to wait for the next sending.
                           if(!fd_socket_table[currentfd]->is_send_all()) {
                               regist_epoll_event(currentfd, EPOLLOUT);
                           }
                        };
                        /* Add task into thread pool */
                        if(!thread_pool.add_task(task)) {
                            LOG(Logger::Level::WARNING, "Thread pool is full");
                        }
                    }
                } else if(events[i].events & EPOLLOUT) {
                    /* Send the unfinished data before */
                    if(!fd_socket_table[currentfd]->is_send_all()) {
                        /* Send data in buffer */
                        std::string pre_data = fd_socket_table[currentfd]->send_buffer.get_data();
                        uint pre_sent_size = fd_socket_table[currentfd]->send(pre_data);
                        /* A part is sent, and the rest is restored to the buffer */
                        if(pre_sent_size < pre_data.length()) {
                            fd_socket_table[currentfd]->send_buffer.append(pre_data.begin(), pre_data.end());
                        } else {
                            unregist_epoll_event(currentfd, EPOLLOUT);
                        }
                    }
                }
                /* Handle timeout task */
                if(is_timeout) {
                    tc.tick();
                    alarm(timeout_period_sec);
                }
            }
        }
    }

#elif defined _WIN32

#endif
}
