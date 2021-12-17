#include "socket.hpp"

namespace jrNetWork {
    TCP::Socket::Socket(IO_MODE is_blocking) : blocking_flag(is_blocking) {
        socket_fd = ::socket(PF_INET, SOCK_STREAM, 0);
        if(-1 == socket_fd) {
            throw std::string("Socket create failed: ") + strerror(errno);
        }
    }

    void TCP::Socket::connect(const std::string &ip, uint port) {
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
        if(-1 == ::connect(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            ::close(socket_fd);
            throw std::string("Connect failed: ") + strerror(errno);
        }
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
        // Set connect non-blocking
        int flag = fcntl(socket_fd, F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(socket_fd, F_SETFL, flag);
        // connect
        if(-1 == ::connect(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            if(errno!=EINTR && errno!=EINPROGRESS) {
                ::close(socket_fd);
                throw std::string("Connect failed: ") + strerror(errno);
            }
        }
        // Check connection is OK(NON-BLOCKING Mode)
        fd_set rset, wset;
        timeval tval;
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(socket_fd, &rset);
        FD_SET(socket_fd, &wset);
        tval.tv_sec = timeout;
        tval.tv_usec = 0;
        flag = select(socket_fd+1, &rset, &wset, NULL, &tval);
        if(flag == -1) {
            throw std::string("Connect failed: ") + strerror(errno);
        } else if(flag == 0) {
            throw std::string("Connect timeout: ") + strerror(errno);
        } else {
            if (FD_ISSET(socket_fd, &rset) || FD_ISSET(socket_fd, &wset)) {
                if(get_ip_from_socket().empty()) {
                    throw std::string("Connect failed: ") + strerror(errno);
                }
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
        int clientfd = ::accept4(socket_fd, NULL, NULL, blocking_flag==IO_NONBLOCKING ? SOCK_NONBLOCK : 0);
        if(-1 == clientfd) {
            return nullptr;
        }
        return std::shared_ptr<TCP::Socket>(new TCP::Socket(clientfd, blocking_flag));
    }

    void TCP::Socket::disconnect() {
        ::close(socket_fd);
    }

    std::pair<std::string, bool> TCP::Socket::recv(uint length) {
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

    bool TCP::Socket::send(std::string data) {
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

    bool TCP::Socket::is_send_all() const {
        return send_buffer.empty();
    }

    std::string TCP::Socket::get_ip_from_socket() const {
        std::string address;
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        if(::getpeername(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_size) == 0)
            address += inet_ntoa(addr.sin_addr);
        return address;
    }

    bool TCP::operator==(const TCP::Socket& lhs, const TCP::Socket& rhs) {
        return lhs.socket_fd==rhs.socket_fd && lhs.blocking_flag==rhs.blocking_flag;
    }

    bool TCP::operator!=(const TCP::Socket& lhs, const TCP::Socket& rhs) {
        return !(lhs==rhs);
    }
}
