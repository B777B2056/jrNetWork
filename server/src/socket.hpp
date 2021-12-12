#ifndef SOCKET_H
#define SOCKET_H

#include "log.hpp"
#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <cstdlib>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace jrNetWork {
    class EventDispatch;

    class TCPSocket {
    friend class EventDispatch;
    friend bool operator==(const TCPSocket& lhs, const TCPSocket& rhs);

    private:
        using uint = unsigned int;

        /* ======== Unix/Windows ======== */
    private:
        bool is_blocking;

        /* ======== Unix ======== */
    private:
        int socket_fd;
        TCPSocket(int fd, bool is_blocking = true);

    public:
        /* Create socket file description */
        TCPSocket(bool is_blocking = true);
        /* ========== Client socket API ========= */
        /* Connect to server */
        void connect(const std::string& ip, uint port);
        /* ========== Server socket API ========= */
        /* Bind ip address and port */
        void bind(uint port);
        /* Listen target port */
        void listen(int backlog = 5);
        /* Accept client connection */
        std::shared_ptr<TCPSocket> accept();
        /* ========== Shared API ========= */
        /* Close current socket */
        void close();
        /* Receive data frome stream by length */
        std::string recv(uint length);
        /* Write data to stream */
        bool send(const std::string& data);
        bool send(std::string&& data);
        /* Get current socket's ip address */
        std::string get_ip_from_socket() const;

    public:
        TCPSocket(const TCPSocket& s);
        TCPSocket(TCPSocket&& s);
        TCPSocket& operator=(const TCPSocket& s);
        TCPSocket& operator=(TCPSocket&& s);
    };

    bool operator==(const TCPSocket& lhs, const TCPSocket& rhs);
    bool operator!=(const TCPSocket& lhs, const TCPSocket& rhs);
}

#endif
