#ifndef LOG_H
#define LOG_H

#include <ctime>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>

/* Print log with level */
#define LOG_FATAL(logger, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[FATAL]"   \
               << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "]" \
               << "[pid " << getpid() << "]" \
               << "[tid " << std::this_thread::get_id() << "]"    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__;    \
            logger._fatal << ss.str() << std::endl;    \
        }

#define LOG_WARNING(logger, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[WARNING]"   \
               << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "]" \
               << "[pid " << getpid() << "]" \
               << "[tid " << std::this_thread::get_id() << "]"    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__;    \
            logger._warning << ss.str() << std::endl;    \
        }

#define LOG_NOTICE(logger, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[NOTICE]"   \
               << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << "]" \
               << "[pid " << getpid() << "]" \
               << "[tid " << std::this_thread::get_id() << "]"    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__;    \
            logger._notice << ss.str() << std::endl;    \
        }

namespace jrRPC {
    class logger {
    public:
        std::ofstream _fatal, _warning, _notice;
        const std::string _filename;

    public:
        logger(const std::string& f = "rpc_server");
        ~logger();
        void _split_log();
//        static logger* create_logger(const std::string& f = "rpc_server");
    };
}

#endif
