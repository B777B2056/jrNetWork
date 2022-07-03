#include "Log.h"

namespace jrNetWork 
{
    Logger::Logger(std::ostream& notice, std::ostream& warn, std::ostream& fatal)
        : _noticeOut(notice)
        , _warnOut(warn)
        , _fatalOut(fatal)
    {

    }

    Logger& Logger::createLogger() 
    {
        static Logger logger;
        return logger;
    }

    static std::string getCurrentTime() 
    {
        std::stringstream ss;
        auto itt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        ss << std::put_time(gmtime(&itt), "%Y-%m-%e-%X");
        return ss.str();
    }

    static std::string getCurrentProcessId() 
    {
        return std::to_string(::getpid());
    }

    std::ostream& Logger::_getOstream(Level level)
    {
        switch (level)
        {
        case FATAL:
            return _fatalOut;
        case WARNING:
            return _warnOut;
        case NOTICE:
            return _noticeOut;
        default:
            return _noticeOut;
        }
    }

    std::ostream& Logger::output(Level level, const std::string& file, int line, const std::string& func)
    {
        std::stringstream ss;   
        switch (level) 
        {
        case FATAL:   
            ss << "[FATAL]"; 
            break;  
        case WARNING:   
            ss << "[WARNING]"; 
            break;  
        case NOTICE:   
            ss << "[NOTICE]"; 
            break;  
        default:
            break;
        }   
        ss << "[" << getCurrentTime() << "]" 
            << "[File " << file << "]"    
            << "[Line " << line << "]"
            << "[Function " << func << "]"
            << ": "; 
        return _getOstream(level) << ss.str();
    }
}

