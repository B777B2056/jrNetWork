#include "socket.hpp"

namespace jrNetWork {
    TCP::SocketBase::SocketBase(IO_MODE is_blocking) : blocking_flag(is_blocking) {
        socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == socket_fd) {
            throw std::string("TCP Socket create failed: ") + strerror(errno);
        }
    }

    void TCP::SocketBase::disconnect() {
        ::close(socket_fd);
    }

    std::pair<std::string, bool> TCP::SocketBase::recv(uint length) {
        bool ret_flag = true;
        std::vector<char> temp;
        if(blocking_flag == IO_BLOCKING) {
            std::string ret;
            temp.resize(length, 0);
            int size = ::recv(socket_fd, &temp[0], length, 0);
            if(size > 0) {
                ret.append(temp.begin(), temp.begin()+size);
            } else {
                if(size < 0) {
                    if(errno != EINTR) {
                        ret_flag = false;
                    }
                } else {
                    disconnect();
                    ret_flag = false;
                }
            }
            return std::make_pair(ret, ret_flag);
        } else {
            /*
             * Read all the data in the system buffer at one time and store it in the Buffer
             * (to prevent the complete content from being read when epoll is set to ET),
             * the user actually reads the specified length of data from the Buffer.
             */
            while(true) {
                temp.resize(length, 0);
                int size = ::recv(socket_fd, &temp[0], length, MSG_DONTWAIT);
                if(size < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) {
                        break;
                    } else if(errno==EINTR) {
                        continue;
                    } else {
                        ret_flag = false;
                        break;
                    }
                } else if(size == 0) {
                    disconnect();
                    break;
                } else {
                    recv_buffer.append(temp.begin(), temp.begin()+size);
                }
            }
            return std::make_pair(recv_buffer.get_data(length), ret_flag);
        }
    }

    bool TCP::SocketBase::send(std::string data) {
        int length = data.length();
        const char* data_c = data.c_str();
        if(blocking_flag == IO_BLOCKING) {
            /* Insure complete sent data
             * After sending, check whether the number of bytes sent is equal to the number of data bytes,
             * if not, continue sending until all data is sent.
             */
            for(int size = 0; size < length; ) {
                int flag = ::send(socket_fd, data_c+size, length-size, 0);
                if(flag == -1) {
                    if(errno != EINTR) {
                        return false;
                    } else {
                        continue;
                    }
                } else if(flag == 0) {
                    break;
                }
                size += flag;
            }
        } else {
            /* Send data */
            uint sent_size = 0;
            while(true) {
                int flag = ::send(socket_fd, data_c, length-sent_size, MSG_DONTWAIT);
                if(flag < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) {
                        break;
                    } else if(errno==EINTR) {
                        continue;
                    } else {
                        break;
                    }
                } else if(flag == 0) {
                    break;
                } else {
                    sent_size += flag;
                }
            }
            /* Send failed, internal error, see errno */
            if(sent_size == 0)
                return false;
            /* Part of it is sent, and the remainder is added to the buffer */
            if(sent_size < data.length()) {
                send_buffer.append(data.begin()+sent_size, data.end());
            }
        }
        return true;
    }

    bool TCP::SocketBase::is_send_all() const {
        return send_buffer.empty();
    }

    std::string TCP::SocketBase::get_ip_from_socket() const {
        std::string address;
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        if(::getpeername(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_size) == 0)
            address += inet_ntoa(addr.sin_addr);
        return address;
    }

    void TCP::ClientSocket::connect(std::string ip, uint port) {
        sockaddr_in addr;
        // init struct
        ::memset(&addr, 0, sizeof(addr));
        // find host ip by name through DNS service
        auto hpk = ::gethostbyname(ip.c_str());
        if(!hpk) {
            std::string msg = std::string("IP address parsing failed: ") + strerror(errno);
            throw msg;
        }
        // fill the struct
        addr.sin_family = AF_INET;		// protocol
        addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr *)(hpk->h_addr_list[0])));		// server IP
        addr.sin_port = htons(port);		// target process port number
        // connect
        if(-1 == ::connect(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            ::close(socket_fd);
            throw std::string("Connect failed: ") + strerror(errno);
        }
    }

    void TCP::ServerSocket::bind(uint port) {
        sockaddr_in addr;
        ::memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == ::bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
            throw std::string("Bind failed: ") + strerror(errno);
        }
    }

    void TCP::ServerSocket::listen(int backlog) {
        if(-1 == ::listen(socket_fd, backlog)) {
            throw std::string("Listen failed: ") + strerror(errno);
        }
    }

    std::shared_ptr<TCP::ClientSocket> TCP::ServerSocket::accept() {
        int clientfd = ::accept(socket_fd, NULL, NULL);
        if(-1 == clientfd) {
            return nullptr;
        }
        return std::shared_ptr<TCP::ClientSocket>(new TCP::ClientSocket(clientfd, blocking_flag));
    }

    UDP::Socket::Socket() {
        socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if(-1 == socket_fd) {
            throw std::string("UDP Socket create failed: ") + strerror(errno);
        }
    }

    void UDP::Socket::set_peer_info(std::string ip, uint port) {
        auto hpk = ::gethostbyname(ip.c_str());
        if(!hpk) {
            std::string msg = std::string("IP address parsing failed: ") + strerror(errno);
            throw msg;
        }
        ::memset(&peer_addr, 0, sizeof(sockaddr_in));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(port);
        peer_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr *)(hpk->h_addr_list[0])));
    }

    void UDP::Socket::bind(uint port) {
        ::memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == ::bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
            throw std::string("Bind failed: ") + strerror(errno);
        }
    }

    std::pair<std::string, bool> UDP::Socket::recvfrom(std::string ip, uint port, uint length) {
        set_peer_info(ip, port);
        char* data = new char[length];
        if(-1 == ::recvfrom(socket_fd, data, length, 0, reinterpret_cast<sockaddr*>(&peer_addr), &peer_sz)) {
            delete[] data;
            return std::make_pair("", false);
        }
        std::string ret(data);
        delete[] data;
        return std::make_pair(ret, true);
    }

    bool UDP::Socket::sendto(std::string ip, uint port, std::string data) {
        set_peer_info(ip, port);
        int length = data.length();
        const char* data_c = data.c_str();
        for(int size = 0; size < length; ) {
            int flag = ::sendto(socket_fd, data_c+size, length-size, 0, reinterpret_cast<sockaddr*>(&peer_addr), peer_sz);
            if(flag == -1) {
                if(errno != EINTR) {
                    return false;
                } else {
                    continue;
                }
            } else if(flag == 0) {
                break;
            }
            size += flag;
        }
        return true;
    }
}
