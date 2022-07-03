#pragma once

#include <ctime>
#include <chrono>
#include <thread>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <sstream>
#include <exception>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>

/* Print log with level */
#define LOG(level) jrNetWork::Logger::createLogger().output(level, __FILE__, __LINE__, __func__)
        

#define LOGNOTICE() LOG(jrNetWork::Logger::Level::NOTICE)
#define LOGWARN() LOG(jrNetWork::Logger::Level::WARNING)
#define LOGFATAL() LOG(jrNetWork::Logger::Level::FATAL)

namespace jrNetWork 
{
    class Logger 
    {
    public:
        enum Level { NOTICE, WARNING, FATAL };

    private:
        std::ostream& _noticeOut;
        std::ostream& _warnOut;
        std::ostream& _fatalOut;
        Logger(std::ostream& notice = std::cout, 
               std::ostream& warn = std::cout, 
               std::ostream& fatal = std::cout);
        std::ostream& _getOstream(Level level);

    public:
        static Logger& createLogger();
        std::ostream& output(Level level, const std::string& file, int line, const std::string& func);
    };
}
