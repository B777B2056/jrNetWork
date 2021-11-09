#ifndef LOG_H
#define LOG_H

#include <thread>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>

/* Print log with level */
#define LOG_FATAL(logger, tid, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[FATAL]"   \
               << " " << "thread id " << tid    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__ \
               << ", date time " << logger->_get_current_time();    \
            logger->_fatal << ss.str() << std::endl;    \
        }

#define LOG_WARNING(logger, tid, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[WARNING]"   \
               << " " << "thread id " << tid    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__ \
               << ", date time " << logger->_get_current_time();    \
            logger->_warning << ss.str() << std::endl;    \
        }

#define LOG_NOTICE(logger, tid, msg) \
        {   \
            std::stringstream ss;   \
            ss << "[NOTICE]"   \
               << " " << "thread id " << tid    \
               << ": " << msg   \
               << ", at file " << __FILE__  \
               << ", line " << __LINE__ \
               << ", function " << __func__ \
               << ", date time " << logger->_get_current_time();    \
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
