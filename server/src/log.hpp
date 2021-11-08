#ifndef LOG_H
#define LOG_H

#include <ctime>
#include <string>
#include <fstream>

namespace tinyRPC {
    class log {
    private:
        std::string _path, _filename;

        std::string _get_current_time() const;

    public:
        log(const std::string& p = "",
            const std::string& n = "log.txt");

        ~log();

        void out(const char* dev,
                 const char* err,
                 int level);
    };
}

#endif
