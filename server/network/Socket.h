#pragma once

#include "Buffer.h"
#include <memory>
#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace jrNetWork {
    template<class SocketType> class EventLoop;

    namespace TCP 
    {
        class Socket 
        {
            template<class SocketType> friend class jrNetWork::EventLoop;
        public:
            enum IO_MODE {IO_BLOCKING, IO_NONBLOCKING};

        private:
            int _id;
            IO_MODE _blockingFlag;
            Buffer _recvBuffer, _sendBuffer;

        public:
            /* Create socket file description */
            Socket(IO_MODE blockingFlag = IO_NONBLOCKING);
            /* Connect to server */
            void connect(std::string ip, std::uint16_t port);
            /* Close current connection */
            void disconnect();
            /* Bind ip address and port */
            void bind(std::uint16_t port);
            /* Listen target port */
            void listen(int backlog = 5);
            /* Accept client connection */
            std::shared_ptr<TCP::Socket> accept();
            /* Receive data frome stream by length */
            std::string recv(std::uint32_t length);
            /* Write data to stream */
            bool send(std::string data);
            /* Determine whether the data has been sent
             * (the return value is only meaningful for non-blocking mode)
             */
            bool is_send_all() const;
            /* Get current socket's ip address */
            std::string get_ip_from_socket() const;
        };
    }

    namespace UDP 
    {
        class Socket 
        {
        private:
            int _id;
            socklen_t _peerSize;
            sockaddr_in addr, _peerAddr;
            void set_peer_info(std::string ip, uint port);

        public:
            /* Create socket file description */
            Socket();
            void bind(std::uint16_t port);
            /* Receive data frome stream by length */
            std::string recvfrom(std::string ip, std::uint16_t port, std::uint32_t length);
            /* Write data to stream */
            bool sendto(std::string ip, std::uint16_t port, std::string data);
        };
    }
}
