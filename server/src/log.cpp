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

    void logger::_split_log() {

    }

//    logger* logger::create_logger(const std::string& f) {
//        return new logger(f);
//    }
}

