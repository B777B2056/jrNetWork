#include "network.hpp"

namespace jrNetWork {
    TCP_server::~TCP_server() {
        close(this->epollfd);
    }

    ErrorCode TCP_server::socket_init(uint port, uint max_task_num) {
        // Create server socket file description
        this->serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == this->serverfd)
            return ESOCKET;
        // Bind ip address and port
        sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == bind(this->serverfd, reinterpret_cast<sockaddr*>(&addr), sizeof(struct sockaddr_in)))
            return EBIND;
        // Listen target port
        if(-1 == listen(this->serverfd, max_task_num))
            return ELISTEN;
        return  NOERROR;
    }

    ErrorCode TCP_server::io_init(uint max_task_num) {
        this->epollfd = epoll_create(max_task_num);
        return this->epollfd==-1 ? EIO_INIT : NOERROR;
    }

    ErrorCode TCP_server::init(uint port, uint max_task_num) {
        ErrorCode s = this->socket_init(port, max_task_num);
        if(s != NOERROR)
            return s;
        ErrorCode e = this->io_init(max_task_num);
        if(e != NOERROR)
            return e;
        return NOERROR;
    }

    int TCP_server::get_serverfd() const {
        return this->serverfd;
    }

    int TCP_server::get_epollfd() const {
        return this->epollfd;
    }

    ErrorCode TCP_server::regist_event(int event) {
        // Register server socket
        this->ee.events = EPOLLIN | EPOLLET;
        this->ee.data.fd = event;
        if(-1 == epoll_ctl(this->epollfd, EPOLL_CTL_ADD, event, &this->ee))
            return EREGIST;
        return NOERROR;
    }

    ErrorCode TCP_server::unregist_event(int event) {
        // Register server socket
        this->ee.events = EPOLLIN;
        this->ee.data.fd = event;
        if(-1 == epoll_ctl(this->epollfd, EPOLL_CTL_DEL, event, &this->ee))
            return EUNREGIST;
        return NOERROR;
    }

    void TCP_server::close_conn(int fd) {
        close(fd);
    }
}
