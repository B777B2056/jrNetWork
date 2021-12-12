#include "socket.hpp"

namespace jrNetWork {
    TCPSocket::TCPSocket(bool is_blocking) : is_blocking(is_blocking) {
        socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == this->socket_fd) {
            std::string msg = std::string("Socket create failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    TCPSocket::TCPSocket(int fd, bool is_blocking) : is_blocking(is_blocking), socket_fd(fd){

    }

    void TCPSocket::connect(const std::string &ip, uint port) {
        // init struct
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        // find host ip by name through DNS service
        auto hpk = ::gethostbyname(ip.c_str());
        if(!hpk) {
            std::string msg = std::string("IP address parsing failed: ") + strerror(errno);
//            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        // fill the struct
        addr.sin_family = AF_INET;		// protocol
        addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr *)(hpk->h_addr_list[0])));		// server IP
        addr.sin_port = htons(port);		// target process port number
        // connect
        if(-1 == ::connect(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            std::string msg = std::string("Connect failed: ") + strerror(errno);
//            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    void TCPSocket::bind(uint port) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == ::bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
            std::string msg = std::string("Bind failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    void TCPSocket::listen(int backlog) {
        if(-1 == ::listen(socket_fd, backlog)) {
            std::string msg = std::string("Listen failed: ") + strerror(errno);
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
    }

    std::shared_ptr<TCPSocket> TCPSocket::accept() {
        int clientfd = ::accept(this->socket_fd, NULL, NULL);
        if(-1 == clientfd) {
            LOG(Logger::Level::WARNING, std::string("Connection accept failed: ") + strerror(errno));
            return nullptr;
        }
        return std::shared_ptr<TCPSocket>(new TCPSocket(clientfd));
    }

    void TCPSocket::close() {
        ::close(socket_fd);
    }

    std::string TCPSocket::recv(uint length) {
        std::string ret;
        std::vector<char> buffer(length, 0);
        buffer.shrink_to_fit();
        if(is_blocking) {
            int flag = ::recv(socket_fd, &buffer[0], buffer.size(), 0);
            if(flag < 0) {
                if(errno != EINTR) {
                    LOG(Logger::Level::WARNING, std::string("Receive data failed: ") + strerror(errno));
                }
            } else if(flag == 0) {
                close();
            } else {
                ret.append(buffer.begin(), buffer.begin()+flag);
            }
        } else {
            int size = 0;
            while(true) {
                int flag = ::recv(socket_fd, &buffer[0], buffer.size(), MSG_DONTWAIT);
                if(flag < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK)
                        break;
                    else if(errno != EINTR) {
                        LOG(Logger::Level::WARNING, std::string("Receive data failed: ") + strerror(errno));
                        return "";
                    }
                } else if(flag == 0) {
                    close();
                    break;
                } else {
                    size += flag;
                }
            }
            ret.append(buffer.begin(), buffer.begin()+size);
        }
        return ret;
    }

    bool TCPSocket::send(const std::string &data) {
        int length = data.length();
        const char* data_c = data.c_str();
        if(is_blocking) {
            /* Insure complete sent data */
            for(int size = 0; size < length; ) {
                int flag = ::send(socket_fd, data_c+size, length-size, 0);
                if(flag == -1) {
                    if(errno!=EINTR) {
                        LOG(Logger::Level::WARNING, std::string("Send data failed: ") + strerror(errno));
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
            for(int size = 0; size < length; ) {
                int flag = ::send(socket_fd, data_c+size, length-size, MSG_DONTWAIT);
                if(flag < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK)
                        break;
                    else if(errno != EINTR) {
                        LOG(Logger::Level::WARNING, std::string("Receive data failed: ") + strerror(errno));
                        return false;
                    }
                } else if(flag == 0) {
                    close();
                    break;
                } else {
                    size += flag;
                }
            }
        }
        return true;
    }

    bool TCPSocket::send(std::string &&data) {
        int length = data.length();
        const char* data_c = data.c_str();
        if(is_blocking) {
            /* Insure complete sent data */
            for(int size = 0; size < length; ) {
                int flag = ::send(socket_fd, data_c+size, length-size, 0);
                if(flag == -1) {
                    if(errno!=EINTR) {
                        LOG(Logger::Level::WARNING, std::string("Send data failed: ") + strerror(errno));
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
            for(int size = 0; size < length; ) {
                int flag = ::send(socket_fd, data_c+size, length-size, MSG_DONTWAIT);
                if(flag < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK)
                        break;
                    else if(errno != EINTR) {
                        LOG(Logger::Level::WARNING, std::string("Receive data failed: ") + strerror(errno));
                        return false;
                    }
                } else if(flag == 0) {
                    close();
                    break;
                } else {
                    size += flag;
                }
            }
        }
        return true;
    }

    std::string TCPSocket::get_ip_from_socket() const {
        std::string address;
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        if(::getpeername(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_size) == 0)
            address += inet_ntoa(addr.sin_addr);
        return  address;
    }

    TCPSocket::TCPSocket(const TCPSocket& s) : is_blocking(s.is_blocking), socket_fd(s.socket_fd) {

    }

    TCPSocket::TCPSocket(TCPSocket&& s) : is_blocking(s.is_blocking), socket_fd(s.socket_fd) {

    }

    TCPSocket& TCPSocket::operator=(const TCPSocket& s) {
        socket_fd = s.socket_fd;
        is_blocking = s.is_blocking;
        return *this;
    }

    TCPSocket& TCPSocket::operator=(TCPSocket&& s) {
        socket_fd = s.socket_fd;
        is_blocking = s.is_blocking;
        return *this;
    }

    bool operator==(const TCPSocket& lhs, const TCPSocket& rhs) {
        return lhs.socket_fd==rhs.socket_fd && lhs.is_blocking==rhs.is_blocking;
    }

    bool operator!=(const TCPSocket& lhs, const TCPSocket& rhs) {
        return !(lhs==rhs);
    }
}
