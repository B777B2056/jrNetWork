#include "log.hpp"

namespace tinyRPC {
    log::log(const std::string& p, const std::string& n)
        : _path(p), _filename(n) {

    }

    log::~log() {

    }

    std::string log::_get_current_time() const {
        char date[60];
        time_t date_t = time(nullptr);
		strftime(date, 60, "%a, %d %b %Y %T", gmtime(&date_t));
        return std::string(date);
    }

    void log::out(const char* dev, const char* err, int level) {
        std::string s = (level == 1 ? "Fatal error" : "Warning");
        std::fstream f(this->_path+this->_filename);
        f << this->_get_current_time() << " " 
          << s << " "
          << dev << " " 
          << err 
          << std::endl;
    }
}

