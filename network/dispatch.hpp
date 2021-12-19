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
#include <type_traits>
#include <fcntl.h>
#include <signal.h>

namespace jrNetWork {
    using TaskHandlerType = std::function<void(std::shared_ptr<TCP::Socket>)>;
    using TimeoutHandlerType = std::function<void(std::shared_ptr<TCP::Socket>)>;

    std::string error_handle(std::string msg);

    class UnifiedEventSource {
    public:
        static int uesfd[2];
        static void ues_transfer(int sig);   // Wrire signal to uesfd
        UnifiedEventSource();    // Init Unified Event Source(UES)
        ~UnifiedEventSource();
        bool handle(); // Handle system signal, ret value is flag of timeout connection
    };

    template<typename iomodel>
    class EventDispatch {
    private:
        using EventType = typename EventType<iomodel>::Type;

    private:
        MultiplexerBase<iomodel>* multiplexer;
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
        void event_handle(uint timeout_period_sec, Event<iomodel> current);
        /* Send data in buffer */
        void buffer_handle(Event<iomodel> current);

    private:
        /* Tag dispatch */
        void ctor(uint max_task_num, std::true_type);
        void ctor(uint max_task_num, std::false_type);

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

    template<typename iomodel>
    EventDispatch<iomodel>::EventDispatch(uint port, uint max_task_num, uint max_pool_size, std::string path)
        : socket(std::make_shared<TCP::Socket>(TCP::Socket::IO_NONBLOCKING)),
          thread_pool(max_task_num, max_pool_size) {
        jrNetWork::logger_path = path;
        ctor(max_task_num, std::integral_constant<bool, std::is_same<iomodel, IO_Model_POLL>::value>());
        socket_init(port);
        // Register server socket
        if(!multiplexer->regist_event(Event<iomodel>{UnifiedEventSource::uesfd[0], EventType::READ_IN})) {
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

    template<typename iomodel>
    void EventDispatch<iomodel>::ctor(uint max_task_num, std::true_type) {
        multiplexer = new MultiplexerPoll(max_task_num);
    }

    template<typename iomodel>
    void EventDispatch<iomodel>::ctor(uint max_task_num, std::false_type) {
        multiplexer = new MultiplexerEpoll(max_task_num);
    }

    template<typename iomodel>
    EventDispatch<iomodel>::~EventDispatch() {
        socket->disconnect();
        delete multiplexer;
    }

    template<typename iomodel>
    template<typename F, typename... Args>
    void EventDispatch<iomodel>::set_event_handler(F&& handler, Args&&... args) {
        auto event_handler_bind = std::bind(std::forward<F>(handler),
                                            std::forward<Args>(args)...,
                                            std::placeholders::_1);
        task_handler = [event_handler_bind](std::shared_ptr<jrNetWork::TCP::Socket> client)->void
                       {
                            event_handler_bind(client);
                       };
    }

    template<typename iomodel>
    template<typename F, typename... Args>
    void EventDispatch<iomodel>::set_timeout_handler(F&& handler, Args&&... args) {
        auto handler_bind = std::bind(std::forward<F>(handler),
                                      std::forward<Args>(args)...,
                                      std::placeholders::_1);
        timeout_handler = [handler_bind](std::shared_ptr<TCP::Socket> client)->void
                          {
                              handler_bind(client);
                          };
    }

    template<typename iomodel>
    void EventDispatch<iomodel>::event_loop(uint timeout_period_sec) {
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
                if(multiplexer->is_connection_event(i)) {
                    accept_handle(timeout_period_sec);
                } else if(multiplexer->is_readable_event(i)) {
                    event_handle(timeout_period_sec, events[i]);
                } else if(multiplexer->is_writeable_event(i)) {
                    buffer_handle(events[i]);
                }
            }
        }
    }

    template<typename iomodel>
    void EventDispatch<iomodel>::socket_init(uint port) {
        /* Bind ip address and port */
        socket->bind(port);
        /* Listen target port */
        socket->listen();
        /* Regist into epoll model */
        if(!multiplexer->set_listened(Event<iomodel>{socket->socket_fd, EventType::READ_IN})) {
            throw jrNetWork::error_handle("Regist socket failed");
        }
    }

    template<typename iomodel>
    void EventDispatch<iomodel>::accept_handle(uint timeout_period_sec) {
        /* Accept connection */
        std::shared_ptr<TCP::Socket> client = socket->accept();
        /* Register client into epoll */
        if(!client) {
            LOG(Logger::Level::WARNING, jrNetWork::error_handle("Connection accept failed: "));
            return ;
        }
        if(!multiplexer->regist_event(Event<iomodel>{client->socket_fd, EventType::READ_IN})) {
            LOG(Logger::Level::WARNING, jrNetWork::error_handle("Connection epoll regist failed: "));
            return ;
        }
        fd_socket_table[client->socket_fd] = client;
        /* Add timer into container */
        tc.add_timer(client, timeout_period_sec, timeout_handler);
        LOG(Logger::Level::NOTICE, "Connection accepted, client ip: " + client->get_ip_from_socket());
    }

    template<typename iomodel>
    void EventDispatch<iomodel>::event_handle(uint timeout_period_sec, Event<iomodel> current) {
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
                   multiplexer->regist_event(Event<iomodel>{fd, EventType::WRITE_OUT});
               }
            };
            /* Add task into thread pool */
            if(!thread_pool.add_task(task)) {
                LOG(Logger::Level::WARNING, "Thread pool is full");
            }
        }
    }

    template<typename iomodel>
    void EventDispatch<iomodel>::buffer_handle(Event<iomodel> current) {
        int client = current.event;
        /* Send the unfinished data before */
        if(!fd_socket_table[client]->is_send_all()) {
            /* Send data in buffer */
            std::string pre_data = fd_socket_table[client]->send_buffer.get_data();
            uint pre_sent_size = fd_socket_table[client]->send(pre_data);
            /* A part is sent, and the rest is restored to the buffer */
            if(pre_sent_size < pre_data.length()) {
                fd_socket_table[client]->send_buffer.append(pre_data.begin(), pre_data.end());
            } else {
                multiplexer->unregist_event(Event<iomodel>{client, EventType::WRITE_OUT});
            }
        }
    }
}

#endif
