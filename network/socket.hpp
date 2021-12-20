#ifndef SOCKET_H
#define SOCKET_H

#include "log.hpp"
#include "buffer.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace jrNetWork {
    template<typename T> class EventDispatch;
    
    namespace TCP {
        class Socket {
        template<typename T> friend class jrNetWork::EventDispatch;
        friend bool operator==(const TCP::Socket& lhs, const TCP::Socket& rhs);

        public:
            enum IO_MODE {IO_BLOCKING, IO_NONBLOCKING};

        private:
            using uint = unsigned int;

        private:
            IO_MODE blocking_flag;
            Buffer recv_buffer, send_buffer;
            int socket_fd;
            Socket(int fd, IO_MODE blocking_flag = IO_BLOCKING)  : blocking_flag(blocking_flag), socket_fd(fd){}

        public:
            /* Create socket file description */
            Socket(IO_MODE blocking_flag = IO_BLOCKING);
            /* ========== Client socket API ========= */
            /* Connect to server */
            void connect(std::string ip, uint port); 
            /* ========== Server socket API ========= */
            /* Bind ip address and port */
            void bind(uint port);
            /* Listen target port */
            void listen(int backlog = 5);
            /* Accept client connection */
            std::shared_ptr<TCP::Socket> accept();
            /* ========== Shared API ========= */
            /* Close current connection */
            void disconnect();
            /* Receive data frome stream by length */
            std::pair<std::string, bool> recv(uint length);
            /* Write data to stream */
            bool send(std::string data);
            /* Determine whether the data has been sent
             * (the return value is only meaningful for non-blocking mode)
             */
            bool is_send_all() const;
            /* Get current socket's ip address */
            std::string get_ip_from_socket() const;
        };

        bool operator==(const TCP::Socket& lhs, const TCP::Socket& rhs);
        bool operator!=(const TCP::Socket& lhs, const TCP::Socket& rhs);
    }

    namespace UDP {
        class Socket {
        private:
            using uint = unsigned int;

        private:
            int socket_fd;
            socklen_t peer_sz;
            sockaddr_in addr, peer_addr;
            void set_peer_info(std::string ip, uint port);

        public:
            /* Create socket file description */
            Socket();
            void bind(uint port);
            /* Receive data frome stream by length */
            std::pair<std::string, bool> recvfrom(std::string ip, uint port, uint length);
            /* Write data to stream */
            bool sendto(std::string ip, uint port, std::string data);
        };
    }
}

#endif
