#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <cstring>
#include <cstdlib>

extern "C" {
    #include <fcntl.h>
    #include <unistd.h>
    #include <signal.h>
    #include <errno.h>
    #include <sys/time.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <sys/epoll.h>
}

namespace jrNetWork {
    enum ErrorCode {ESOCKET, EBIND, ELISTEN, EIO_INIT, EREGIST, EUNREGIST, NOERROR};

    class TCP_server {
    private:
        int serverfd;
        int epollfd;
        epoll_event ee;

    private:
        /* Init socket, return server file descriptor */
        ErrorCode socket_init(uint port, uint max_task_num);
        /* Init IO model(Linux: epoll), return epoll file descriptor */
        ErrorCode io_init(uint max_task_num);

    public:
        TCP_server() = default;
        /* Close all connection */
        ~TCP_server();
        /* Init socket and IO model */
        ErrorCode init(uint port, uint max_task_num);
        /* Get server file descriptor */
        int get_serverfd() const;
        /* Get epoll file descriptor */
        int get_epollfd() const;
        /* Register event into IO model */
        ErrorCode regist_event(int event);
        /* Unregister event fromIO model */
        ErrorCode unregist_event(int event);
        /* Close connection */
        void close_conn(int fd);
    };
}

#endif
