#include "log.hpp"

namespace jrNetWork {
    std::string logger_path;

    Logger::Logger() {
        filename_base = logger_path + "process"+get_current_process_id()+"_"+get_current_time()+"_";
        fatal.open(filename_base + "Fatal.log", std::ios::out | std::ios::app);
        if(!fatal.is_open()) {
            throw "Fatal logger file create FAILED.";
        }
        warning.open(filename_base + "Warning.log", std::ios::out | std::ios::app);
        if(!warning.is_open()) {
            throw "Warning logger file create FAILED.";
        }
        notice.open(filename_base + "Notice.log", std::ios::out | std::ios::app);
        if(!notice.is_open()) {
            throw "Notice logger file create FAILED.";
        }
    }

    Logger& Logger::createLogger() {
        static Logger logger;
        return logger;
    }

    Logger::~Logger() {
        fatal.close();
        warning.close();
        notice.close();
    }

    std::string Logger::get_current_time() {
        std::stringstream ss;
        auto itt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        ss << std::put_time(gmtime(&itt), "%Y-%m-%e-%X");
        return ss.str();
    }

    std::string Logger::get_current_process_id() {
        std::string pid = std::to_string(::getpid());
        return pid;
    }

    void Logger::output_log(Level level, std::string msg) {
        switch(level) {
            case jrNetWork::Logger::Level::FATAL:
                fatal << msg << std::endl;
                break;
            case jrNetWork::Logger::Level::WARNING:
                warning << msg << std::endl;
                break;
            case jrNetWork::Logger::Level::NOTICE:
                notice << msg << std::endl;
                break;
        }
    }
}

