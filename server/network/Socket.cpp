#include "Socket.h"
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include "Log.h"

namespace jrNetWork {
    TCP::Socket::Socket(IO_MODE blockingFlag)
        : _id(::socket(AF_INET, SOCK_STREAM, 0))
        , _blockingFlag(blockingFlag)
    {
        if(-1 == _id) 
        {
            throw std::string("TCP Socket create failed: ") + strerror(errno);
        }
        int t;
        if (-1 == ::setsockopt(_id, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        {
            throw std::string("Setsockopt with SO_REUSEADDR failed: ") + strerror(errno);
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
        int clientfd = ::accept(_id, nullptr, nullptr);
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
        int size = 0;
        int flag = 0;
        std::string temp(length, 0);
        if(_blockingFlag == IO_BLOCKING) 
        {
            for(;;)
            {
                if (size == length)
                {
                    break;
                }
                flag = ::recv(_id, &temp[size], length - size, 0);
                if (flag < 0)
                {
                    if (errno != EINTR)
                    {
                        LOGNOTICE() << "Blocking, errno = " << errno << std::endl;
                        break;
                    }
                }
                else if (flag == 0)
                {
                    LOGNOTICE() << "Blocking, peer is closed." << std::endl;
                    break;
                } 
                else
                {
                    size += flag;
                }
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
            for (;;)
            {
                if (size == length)
                {
                    break;
                }
                flag = ::recv(_id, &temp[size], length - size, MSG_DONTWAIT);
                if(flag < 0)
                {
                    if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        LOGNOTICE() << "Nonblocking, errno = " << errno << " continue" << std::endl;
                        continue;
                    } 
                    else 
                    {
                        LOGNOTICE() << "Nonblocking, errno = " << errno << " break" << std::endl;
                        break;
                    }
                } 
                else if(flag == 0)
                {
                    LOGNOTICE() << "Nonblocking, peer is closed." << std::endl;
                    break;
                } 
                else 
                {
                    _recvBuffer.append(temp.begin() + size, temp.begin() + size + flag);
                    size += flag;
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
                int flag = ::send(_id, data_c + size, length - size, 0);
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
                int flag = ::send(_id, data_c, length - sent_size, MSG_DONTWAIT);
                if(flag < 0) 
                {
                    if(errno == EAGAIN || errno == EWOULDBLOCK) 
                    {
                        break;
                    } 
                    else if(errno == EINTR) 
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
            /* Part of it is sent, and the remainder is added to the buffer */
            if(sent_size < data.length()) 
            {
                _sendBuffer.append(data.begin()+sent_size, data.end());
            }
        }
        return true;
    }

    bool TCP::Socket::isSendAll() const 
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
