#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <sstream>

enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3
};

class Logger {
   public:
    Logger();
    ~Logger();

    static Logger& getInstance();
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    std::ostream& debug(const std::string& prefix = "");
    std::ostream& info(const std::string& prefix = "");
    std::ostream& warning(const std::string& prefix = "");
    std::ostream& error(const std::string& prefix = "");

   private:
    LogLevel currentLevel_;
    std::ostream* outputStream_;
    std::ostream* errorStream_;

    Logger(const Logger&);
    Logger& operator=(const Logger&);

    std::string getLevelString(LogLevel level) const;
    std::ostream& log(LogLevel level, const std::string& prefix);
};

#define LOG_DEBUG() Logger::getInstance().debug()
#define LOG_INFO() Logger::getInstance().info()
#define LOG_WARNING() Logger::getInstance().warning()
#define LOG_ERROR() Logger::getInstance().error()

#endif

