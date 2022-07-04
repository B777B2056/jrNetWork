#include "Socket.h"
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>

namespace jrNetWork {
    TCP::Socket::Socket(IO_MODE blockingFlag)
        : _id(::socket(AF_INET, SOCK_STREAM, 0))
        , _blockingFlag(blockingFlag)
    {
        if(-1 == _id) 
        {
            throw std::string("TCP Socket create failed: ") + strerror(errno);
        }
    }

    void TCP::Socket::connect(std::string ip, std::uint16_t port)
    {
        sockaddr_in addr;
        // init struct
        ::memset(&addr, 0, sizeof(addr));
        // find host ip by name through DNS service
        auto hpk = ::gethostbyname(ip.c_str());
        if (!hpk) 
        {
            std::string msg = std::string("IP address parsing failed: ") + strerror(errno);
            throw msg;
        }
        // fill the struct
        addr.sin_family = AF_INET;		// protocol
        addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr*)(hpk->h_addr_list[0])));		// server IP
        addr.sin_port = ::htons(port);		// target process port number
        // connect
        if (-1 == ::connect(_id, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) 
        {
            ::close(_id);
            throw std::string("Connect failed: ") + strerror(errno);
        }
    }

    void TCP::Socket::disconnect() 
    {
        ::close(_id);
    }

    void TCP::Socket::bind(std::uint16_t port)
    {
        sockaddr_in addr;
        ::memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (-1 == ::bind(_id, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) 
        {
            throw std::string("Bind failed: ") + strerror(errno);
        }
    }

    void TCP::Socket::listen(int backlog) 
    {
        if (-1 == ::listen(_id, backlog)) 
        {
            throw std::string("Listen failed: ") + strerror(errno);
        }
    }

    std::shared_ptr<TCP::Socket> TCP::Socket::accept() 
    {
        int clientfd = ::accept(_id, NULL, NULL);
        if (-1 == clientfd) 
        {
            return nullptr;
        }
        auto cltPtr = std::make_shared<TCP::Socket>(_blockingFlag);
        cltPtr->_id = clientfd;
        return cltPtr;
    }

    std::string TCP::Socket::recv(std::uint32_t length)
    {
        std::string temp(length, 0);
        if(_blockingFlag == IO_BLOCKING) 
        {
            int size = ::recv(_id, &temp[0], length, 0);
            if(size < 0) 
            {
                if(errno != EINTR) 
                {
                    return "";
                }
            } 
            else if(size == 0)
            {
                return "";
            }
            return temp;
        } 
        else 
        {
            /*
             * Read all the data in the system buffer at one time and store it in the Buffer
             * (to prevent the complete content from being read when epoll is set to ET),
             * the user actually reads the specified length of data from the Buffer.
             */
            for(;;)
            {
                //temp.resize(length, 0);
                int size = ::recv(_id, &temp[0], length, MSG_DONTWAIT);
                if(size < 0) 
                {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) 
                    {
                        break;
                    } 
                    else if(errno==EINTR) 
                    {
                        continue;
                    } 
                    else 
                    {
                        return "";
                    }
                } 
                else if(size == 0) 
                {
                    return "";
                } 
                else 
                {
                    _recvBuffer.append(temp.begin(), temp.begin()+size);
                }
            }
            return _recvBuffer.getData(length);
        }
    }

    bool TCP::Socket::send(std::string data) 
    {
        int length = data.length();
        const char* data_c = data.c_str();
        if(_blockingFlag == IO_BLOCKING) 
        {
            /* Insure complete sent data
             * After sending, check whether the number of bytes sent is equal to the number of data bytes,
             * if not, continue sending until all data is sent.
             */
            for(int size = 0; size < length; ) 
            {
                int flag = ::send(_id, data_c+size, length-size, 0);
                if(flag == -1) 
                {
                    if(errno != EINTR) 
                    {
                        return false;
                    } 
                    else 
                    {
                        continue;
                    }
                } 
                else if(flag == 0) 
                {
                    break;
                }
                size += flag;
            }
        } 
        else 
        {
            /* Send data */
            std::size_t sent_size = 0;
            while(true) 
            {
                int flag = ::send(_id, data_c, length-sent_size, MSG_DONTWAIT);
                if(flag < 0) 
                {
                    if(errno==EAGAIN || errno==EWOULDBLOCK) 
                    {
                        break;
                    } 
                    else if(errno==EINTR) 
                    {
                        continue;
                    } 
                    else 
                    {
                        break;
                    }
                } 
                else if(flag == 0) 
                {
                    break;
                } 
                else 
                {
                    sent_size += flag;
                }
            }
            /* Send failed, internal error, see errno */
            if (sent_size == 0)
            {
                return false;
            } 
            /* Part of it is sent, and the remainder is added to the buffer */
            if(sent_size < data.length()) 
            {
                _sendBuffer.append(data.begin()+sent_size, data.end());
            }
        }
        return true;
    }

    bool TCP::Socket::is_send_all() const 
    {
        return _sendBuffer.empty();
    }

    std::string TCP::Socket::get_ip_from_socket() const 
    {
        std::string address;
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        if(::getpeername(_id, reinterpret_cast<sockaddr*>(&addr), &addr_size) == 0)
            address += inet_ntoa(addr.sin_addr);
        return address;
    }

    UDP::Socket::Socket() 
    {
        _id = ::socket(AF_INET, SOCK_DGRAM, 0);
        if(-1 == _id) 
        {
            throw std::string("UDP Socket create failed: ") + strerror(errno);
        }
    }

    void UDP::Socket::set_peer_info(std::string ip, uint port) 
    {
        auto hpk = ::gethostbyname(ip.c_str());
        if(!hpk) 
        {
            std::string msg = std::string("IP address parsing failed: ") + strerror(errno);
            throw msg;
        }
        ::memset(&_peerAddr, 0, sizeof(sockaddr_in));
        _peerAddr.sin_family = AF_INET;
        _peerAddr.sin_port = htons(port);
        _peerAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr *)(hpk->h_addr_list[0])));
    }

    void UDP::Socket::bind(std::uint16_t port)
    {
        ::memset(&addr, 0, sizeof(sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == ::bind(_id, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) 
        {
            throw std::string("Bind failed: ") + strerror(errno);
        }
    }

    std::string UDP::Socket::recvfrom(std::string ip, std::uint16_t port, std::uint32_t length)
    {
        set_peer_info(ip, port);
        std::string ret(length, 0);
        if(-1 == ::recvfrom(_id, &ret[0], length, 0, reinterpret_cast<sockaddr*>(&_peerAddr), &_peerSize))
        {
            return "";
        }
        return ret;
    }

    bool UDP::Socket::sendto(std::string ip, std::uint16_t port, std::string data)
    {
        set_peer_info(ip, port);
        int length = data.length();
        const char* data_c = data.c_str();
        for(int size = 0; size < length; ) 
        {
            int flag = ::sendto(_id, data_c+size, length-size, 0, reinterpret_cast<sockaddr*>(&_peerAddr), _peerSize);
            if(flag == -1) 
            {
                if(errno != EINTR) 
                {
                    return false;
                }
                else 
                {
                    continue;
                }
            } 
            else if(flag == 0) 
            {
                break;
            }
            size += flag;
        }
        return true;
    }
}
