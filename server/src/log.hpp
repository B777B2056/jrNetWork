#ifndef LOG_H
#define LOG_H

#include <ctime>
#include <chrono>
#include <thread>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

/* Print log with level */
#define LOG(level, msg) \
        {   \
            std::stringstream ss;   \
            switch(level) { \
                case jrNetWork::Logger::Level::FATAL:   \
                    ss << "[FATAL]"; \
                    break;  \
                case jrNetWork::Logger::Level::WARNING:   \
                    ss << "[WARNING]"; \
                    break;  \
                case jrNetWork::Logger::Level::NOTICE:   \
                    ss << "[NOTICE]"; \
                    break;  \
            }   \
            ss << "[" << jrNetWork::Logger::get_current_time() << "]" \
               << "[tid" << std::this_thread::get_id() << "]"    \
               << ":" << msg   \
               << ",at file " << __FILE__  \
               << ",line " << __LINE__ \
               << ",function " << __func__; \
            jrNetWork::Logger::createLogger().output_log(level, ss.str());   \
        }

namespace jrNetWork {
    class Logger {
    private:
        std::ofstream fatal, warning, notice;
        const std::string file_path, filename_base;
        Logger();

    public:
        enum Level { NOTICE, WARNING, FATAL };

        static Logger& createLogger();
        ~Logger();
        static std::string get_current_time();
        static std::string get_current_process_id();
        void output_log(Level level, std::string msg);
    };
}

#endif
