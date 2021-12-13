#include "socket.hpp"

namespace jrNetWork {
    TCP::Socket::Socket(Flag is_blocking) : blocking_flag(is_blocking) {
        socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == this->socket_fd) {
            throw std::string("Socket create failed: ") + strerror(errno);
        }
    }

    TCP::Socket::Socket(int fd, Flag blocking_flag) : blocking_flag(blocking_flag), socket_fd(fd){

    }

    void TCP::Socket::connect(const std::string &ip, uint port, uint timeout) {
        // init struct
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
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
        int flag;
        if(blocking_flag == NONBLOCKING) {
            // Set connect non-blocking
            flag = fcntl(socket_fd, F_GETFL);
            flag |= O_NONBLOCK;
            fcntl(socket_fd, F_SETFL, flag);
        }
        if(-1 == ::connect(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            if(blocking_flag==BLOCKING || (blocking_flag==NONBLOCKING && errno!=EINTR && errno!=EINPROGRESS)) {
                ::close(socket_fd);
                throw std::string("Connect failed: ") + strerror(errno);
            }
        }
        // Check connection is OK(NON-BLOCKING Mode)
        if(blocking_flag == NONBLOCKING) {
            int n, error;
            fd_set wset;
            timeval tval;
            FD_ZERO(&wset);
            FD_SET(socket_fd, &wset);
            tval.tv_sec = timeout;
            tval.tv_usec = 0;
            if ((n = select(socket_fd+1, NULL, &wset, NULL, 10 ? &tval : NULL)) == 0) {
                ::close(socket_fd);  /* timeout */
                errno = ETIMEDOUT;
                throw std::string("Connect failed: ") + strerror(errno);
            }
            if (FD_ISSET(socket_fd, &wset)) {
                socklen_t len = sizeof(error);
                int code = ::getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                if (code < 0 || error) {
                    ::close(socket_fd);
                    if (error)
                        errno = error;
                    throw std::string("Connect failed: ") + strerror(errno);
                }
                fcntl(socket_fd, F_SETFL, flag);
            } else {
                throw "select error: sockfd not set";
            }
        }
    }

    void TCP::Socket::bind(uint port) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == ::bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
            throw std::string("Bind failed: ") + strerror(errno);
        }
    }

    void TCP::Socket::listen(int backlog) {
        if(-1 == ::listen(socket_fd, backlog)) {
            throw std::string("Listen failed: ") + strerror(errno);
        }
    }

    std::shared_ptr<TCP::Socket> TCP::Socket::accept() {
        int clientfd = ::accept4(socket_fd, NULL, NULL, blocking_flag==NONBLOCKING ? SOCK_NONBLOCK : 0);
        if(-1 == clientfd) {
            return nullptr;
        }
        return std::shared_ptr<TCP::Socket>(new TCP::Socket(clientfd, blocking_flag));
    }

    void TCP::Socket::close() {
        ::close(socket_fd);
    }

    std::pair<std::string, bool> TCP::Socket::recv(uint length) {
        bool ret_flag = true;
        std::vector<char> temp;
        if(blocking_flag == BLOCKING) {
            std::string ret;
            temp.resize(length, 0);
            int flag = ::recv(socket_fd, &temp[0], length, 0);
            if(flag > 0) {
                ret.append(temp.begin(), temp.begin()+flag);
            } else {
                if(flag < 0) {
                    ret_flag = false;
                } else {
                    ::close(socket_fd);
                }
            }
            return std::make_pair(ret, flag);
        } else {
            /*
             * Read all the data in the system buffer at one time and store it in the Buffer
             * (to prevent the complete content from being read when epoll is set to ET),
             * the user actually reads the specified length of data from the Buffer.
             */
            while(true) {
                temp.resize(length, 0);
                int flag = ::recv(socket_fd, &temp[0], length, MSG_DONTWAIT);
                if(flag < 0) {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) {
                        break;
                    } else if(errno==EINTR) {
                        continue;
                    } else {
                        flag = false;
                        break;
                    }
                } else if(flag == 0) {
                    ::close(socket_fd);
                    break;
                } else {
                    buffer.append_recv(temp.begin(), temp.begin()+flag);
                }
            }
            return std::make_pair(buffer.get_recv(length), ret_flag);
        }
    }

    bool TCP::Socket::send(std::string data) {
        if(blocking_flag == BLOCKING) {
            int length = data.length();
            const char* data_c = data.c_str();
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
            auto nonblocking_send = [this](const std::string& d)->uint
                                    {
                                        int sent_size = 0;
                                        const char* d_data_c = d.c_str();
                                        int d_len = d.length();
                                        while(true) {
                                            int flag = ::send(socket_fd, d_data_c, d_len-sent_size, MSG_DONTWAIT);
                                            if(flag < 0) {
                                                if(errno==EAGAIN || errno==EWOULDBLOCK) {
                                                    break;
                                                } else if(errno==EINTR) {
                                                    continue;
                                                } else {
                                                    return 0;
                                                }
                                            } else if(flag == 0) {
                                                ::close(socket_fd);
                                                break;
                                            } else {
                                                sent_size += flag;
                                            }
                                        }
                                        return sent_size;
                                    };
            if(buffer.send_buffer_size() > 0) {
                /* Send data in buffer */
                std::string pre_data = buffer.get_send();
                uint pre_sent_size = nonblocking_send(pre_data);
                /* Send failed, internal error, see errno */
                if(pre_sent_size == 0)
                    return false;
                /* A part is sent, and the rest is restored to the buffer header */
                if(pre_sent_size < pre_data.length()) {
                    buffer.push_front_send(pre_data.begin(), pre_data.end());
                }
            }
            /* Send data */
            uint sent_size = nonblocking_send(data);
            /* Send failed, internal error, see errno */
            if(sent_size == 0)
                return false;
            /* Part of it is sent, and the remainder is added to the end of the buffer */
            if(sent_size < data.length()) {
                buffer.append_send(data.begin(),data.end());
            }
        }
        return true;
    }

    std::string TCP::Socket::get_ip_from_socket() const {
        std::string address;
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        if(::getpeername(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_size) == 0)
            address += inet_ntoa(addr.sin_addr);
        return  address;
    }

    TCP::Socket::Socket(const TCP::Socket& s) : blocking_flag(s.blocking_flag), socket_fd(s.socket_fd) {

    }

    TCP::Socket::Socket(TCP::Socket&& s) : blocking_flag(s.blocking_flag), socket_fd(s.socket_fd) {

    }

    TCP::Socket& TCP::Socket::operator=(const TCP::Socket& s) {
        socket_fd = s.socket_fd;
        blocking_flag = s.blocking_flag;
        return *this;
    }

    TCP::Socket& TCP::Socket::operator=(TCP::Socket&& s) {
        socket_fd = s.socket_fd;
        blocking_flag = s.blocking_flag;
        return *this;
    }

    bool TCP::operator==(const TCP::Socket& lhs, const TCP::Socket& rhs) {
        return lhs.socket_fd==rhs.socket_fd && lhs.blocking_flag==rhs.blocking_flag;
    }

    bool TCP::operator!=(const TCP::Socket& lhs, const TCP::Socket& rhs) {
        return !(lhs==rhs);
    }
}
