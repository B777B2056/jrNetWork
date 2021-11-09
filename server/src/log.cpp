#include "log.hpp"

namespace tinyRPC {
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

    void logger::_split_log() {

    }

    std::string logger::_get_current_time() const {
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        tm* ptm = localtime(&tt);
        char date[60] = {0};
        sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
                (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
                (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
        return std::string(date);
    }

    logger* logger::create_logger(const std::string& f) {
        return new logger(f);
    }
}

