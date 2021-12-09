#include "log.hpp"

namespace jrRPC {
    logger::logger(const std::string& f) : _filename(f) {
        _fatal.open(this->_filename + "_Fatal.log", std::ios::out | std::ios::app);
        _warning.open(this->_filename + "_Warning.log", std::ios::out | std::ios::app);
        _notice.open(this->_filename + "_Notice.log", std::ios::out | std::ios::app);
    }

    logger::~logger() {
        _fatal.close();
        _warning.close();
        _notice.close();
    }

    void logger::split_log() {

    }

    std::string logger::get_ip_from_fd(int fd) {
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        int res = getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &addr_size);
        return res > 0 ? inet_ntoa(addr.sin_addr) : "";
    }
}

