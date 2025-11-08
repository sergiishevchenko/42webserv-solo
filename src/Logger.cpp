#include "Logger.hpp"
#include <ctime>
#include <iomanip>

Logger::Logger() : currentLevel_(LOG_INFO), outputStream_(&std::cout), errorStream_(&std::cerr) {}

Logger::~Logger() {}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel_ = level;
}

LogLevel Logger::getLogLevel() const {
    return currentLevel_;
}

std::string Logger::getLevelString(LogLevel level) const {
    switch (level) {
        case LOG_DEBUG:
            return "DEBUG";
        case LOG_INFO:
            return "INFO";
        case LOG_WARNING:
            return "WARN";
        case LOG_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

std::ostream& Logger::log(LogLevel level, const std::string& prefix) {
    if (level < currentLevel_) {
        static std::ostringstream nullStream;
        return nullStream;
    }

    std::ostream& stream = (level >= LOG_ERROR) ? *errorStream_ : *outputStream_;

    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    stream << "[" << buffer << "] [" << getLevelString(level) << "]";
    if (!prefix.empty()) {
        stream << " [" << prefix << "]";
    }
    stream << " ";

    return stream;
}

std::ostream& Logger::debug(const std::string& prefix) {
    return log(LOG_DEBUG, prefix);
}

std::ostream& Logger::info(const std::string& prefix) {
    return log(LOG_INFO, prefix);
}

std::ostream& Logger::warning(const std::string& prefix) {
    return log(LOG_WARNING, prefix);
}

std::ostream& Logger::error(const std::string& prefix) {
    return log(LOG_ERROR, prefix);
}

