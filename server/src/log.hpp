#ifndef LOG_H
#define LOG_H

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
               << "[" << logger->_get_current_time() << "]" \
               << "[pid]" << getpid() \
               << "[tid]" << std::this_thread::get_id()    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__;    \
            logger->_fatal << ss.str() << std::endl;    \
        }

#define LOG_WARNING(logger, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[WARNING]"   \
               << "[" <<logger->_get_current_time() << "]" \
               << "[pid]" << getpid() \
               << "[tid]" << std::this_thread::get_id()    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__;    \
            logger->_warning << ss.str() << std::endl;    \
        }

#define LOG_NOTICE(logger, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[NOTICE]"   \
               << "[" <<logger->_get_current_time() << "]" \
               << "[pid]" << getpid() \
               << "[tid]" << std::this_thread::get_id()    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__;    \
            logger->_notice << ss.str() << std::endl;    \
        }

namespace tinyRPC {
    class logger {
    public:
        std::ofstream _fatal, _warning, _notice;
        const std::string _filename;

    private:
        logger(const std::string&);

    public:
        ~logger();
        void _split_log();
        std::string _get_current_time() const;
        static logger* create_logger(const std::string& f = "rpc_server");
    };
}

#endif
