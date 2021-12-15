#include "dispatch.hpp"

namespace jrNetWork {
    EventDispatch::EventDispatch(uint port, uint max_task_num, uint max_pool_size, std::string path)
        : socket(TCP::Socket::IO_NONBLOCKING), thread_pool(max_task_num, max_pool_size)  {
        jrNetWork::logger_path = path;
        socket_init(port);
        io_model.epoll_init(socket, max_task_num);
        timeout_handler = [this](TCP::Socket* client)->void
                          {
                            io_model.unregist_io_event(client);
                            client->disconnect();
                          };
    }

    EventDispatch::~EventDispatch() {
        socket.disconnect();
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

    void EventDispatch::set_timeout_handler(TimeoutHandlerType handler) {
        timeout_handler = [this, handler](TCP::Socket* client)->void
                          {
                              io_model.unregist_io_event(client);
                              handler(client);
                          };
    }

    void EventDispatch::event_loop(uint timeout_period_sec) {
        if(!task_handler) {
            std::string msg = "No event handler binded!";
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        io_model.io_handler(timeout_period_sec, timeout_handler, task_handler, tc, thread_pool);
    }
}
