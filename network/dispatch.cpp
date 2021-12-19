#include "dispatch.hpp"

namespace jrNetWork {
    int jrNetWork::UnifiedEventSource::uesfd[2];

    static std::string error_handle(std::string msg) { return msg + strerror(errno); }

    void UnifiedEventSource::ues_transfer(int sig) {
        write(uesfd[1], reinterpret_cast<char*>(&sig), 1);
    }

    UnifiedEventSource::UnifiedEventSource() {
        if(-1 == pipe(uesfd)) {
            std::string msg = jrNetWork::error_handle("Unified Event Source pipe failed: ");
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
    }

    UnifiedEventSource::~UnifiedEventSource() {
        ::close(uesfd[0]);
        ::close(uesfd[1]);
    }

    bool UnifiedEventSource::handle() {
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
                    LOG(Logger::Level::WARNING, "Client connection timeout");
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

    EventDispatch::EventDispatch(uint port, uint max_task_num, uint max_pool_size, std::string path)
        : multiplexer(new MultiplexerEpoll(max_task_num)),
          socket(std::make_shared<TCP::Socket>(TCP::Socket::IO_NONBLOCKING)),
          thread_pool(max_task_num, max_pool_size) {
        jrNetWork::logger_path = path;
        socket_init(port);
        // Register server socket
        if(!multiplexer->regist_event(Event<IO_Model_EPOLL>{UnifiedEventSource::uesfd[0], EPOLLIN | EPOLLET})) {
            std::string msg = jrNetWork::error_handle("Unified Event Source regist failed");
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        fd_socket_table.clear();
        timeout_handler = [](std::shared_ptr<TCP::Socket> client)->void
                          {
                            client->disconnect();
                          };
    }

    EventDispatch::~EventDispatch() {
        socket->disconnect();
        delete multiplexer;
    }

    void EventDispatch::socket_init(uint port) {
        /* Bind ip address and port */
        socket->bind(port);
        /* Listen target port */
        socket->listen();
        /* Regist into epoll model */
        if(!multiplexer->regist_event(Event<IO_Model_EPOLL>{socket->socket_fd, EPOLLIN | EPOLLET})) {
            throw jrNetWork::error_handle("Regist socket failed");
        }
    }

    void EventDispatch::accept_handle(uint timeout_period_sec) {
        /* Accept connection */
        std::shared_ptr<TCP::Socket> client = socket->accept();
        /* Register client into epoll */
        if(!client) {
            LOG(Logger::Level::WARNING, jrNetWork::error_handle("Connection accept failed: "));
            return ;
        }
        if(!multiplexer->regist_event(Event<IO_Model_EPOLL>{client->socket_fd, EPOLLIN | EPOLLET})) {
            LOG(Logger::Level::WARNING, jrNetWork::error_handle("Connection epoll regist failed: "));
            return ;
        }
        fd_socket_table[client->socket_fd] = client;
        /* Add timer into container */
        tc.add_timer(client, timeout_period_sec, timeout_handler);
        LOG(Logger::Level::NOTICE, "Connection accepted, client ip: " + client->get_ip_from_socket());
    }

    void EventDispatch::event_handle(uint timeout_period_sec, Event<IO_Model_EPOLL> current) {
        if(current.type & EPOLLIN) {
            if(current.event == UnifiedEventSource::uesfd[0]) {
                if(ues.handle()) {
                    /* Handle timeout task */
                    tc.tick();  // Awake timer
                    ::alarm(timeout_period_sec);  // Reset alarm
                }
            } else {
                /* Build up */
                int fd = current.event;
                auto task = [this, fd]()->void
                {
                   // Execute user-specified logic
                   task_handler(fd_socket_table[fd]);
                   // If the data has not been sent at one time,
                   // it will be pushed into the buffer (completed by TCP::Socket),
                   // and then register the EPOLLOUT event to wait for the next sending.
                   if(!fd_socket_table[fd]->is_send_all()) {
                       multiplexer->regist_event(Event<IO_Model_EPOLL>{fd, EPOLLOUT | EPOLLET});
                   }
                };
                /* Add task into thread pool */
                if(!thread_pool.add_task(task)) {
                    LOG(Logger::Level::WARNING, "Thread pool is full");
                }
            }
        } else if(current.type & EPOLLOUT) {
            buffer_handle(current.event);
        }
    }

    void EventDispatch::buffer_handle(int client) {
        /* Send the unfinished data before */
        if(!fd_socket_table[client]->is_send_all()) {
            /* Send data in buffer */
            std::string pre_data = fd_socket_table[client]->send_buffer.get_data();
            uint pre_sent_size = fd_socket_table[client]->send(pre_data);
            /* A part is sent, and the rest is restored to the buffer */
            if(pre_sent_size < pre_data.length()) {
                fd_socket_table[client]->send_buffer.append(pre_data.begin(), pre_data.end());
            } else {
                multiplexer->unregist_event(Event<IO_Model_EPOLL>{client, EPOLLOUT | EPOLLET});
            }
        }
    }

    void EventDispatch::event_loop(uint timeout_period_sec) {
        if(!task_handler) {
            std::string msg = "No event handler binded!";
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        ::alarm(timeout_period_sec);
        while(true) {
            int event_n = multiplexer->wait();
            if(-1 == event_n) {
                if(errno != EINTR) {
                    LOG(Logger::Level::FATAL, jrNetWork::error_handle("IO wait error: "));
                    break;
                }
            }
            auto& events = multiplexer->get_event_list();
            for(int i = 0; i < event_n; ++i) {
                if(events[i].event == socket->socket_fd) {
                    accept_handle(timeout_period_sec);
                } else {
                    event_handle(timeout_period_sec, events[i]);
                }
            }
        }
    }
}
