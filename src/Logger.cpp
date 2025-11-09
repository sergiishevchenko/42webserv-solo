#include "Logger.hpp"
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

Logger::Logger()
    : currentLevel_(LOG_INFO), outputStream_(&std::cout),
      errorStream_(&std::cerr) {}

Logger::~Logger() {}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) { currentLevel_ = level; }

LogLevel Logger::getLogLevel() const { return currentLevel_; }

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

std::string Logger::getLevelColor(LogLevel level) const {
    switch (level) {
        case LOG_DEBUG:
            return "\033[36m"; // Cyan
        case LOG_INFO:
            return "\033[32m"; // Green
        case LOG_WARNING:
            return "\033[33m"; // Yellow
        case LOG_ERROR:
            return "\033[31m"; // Red
        default:
            return "\033[0m"; // Reset
    }
}

std::string Logger::getTextColor(LogLevel level) const {
    switch (level) {
        case LOG_DEBUG:
            return "\033[0;96m"; // Bright Cyan
        case LOG_INFO:
            return "\033[0;94m"; // Bright Blue
        case LOG_WARNING:
            return "\033[0;93m"; // Bright Yellow
        case LOG_ERROR:
            return "\033[0;91m"; // Bright Red
        default:
            return "\033[0m"; // Reset
    }
}

bool Logger::isTerminal(std::ostream& stream) const {
    const char* no_color = std::getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') {
        return false;
    }

    const char* force_color = std::getenv("FORCE_COLOR");
    if (force_color != NULL && force_color[0] != '\0') {
        return true;
    }

    if (&stream == &std::cout) {
        return isatty(STDOUT_FILENO) != 0;
    } else if (&stream == &std::cerr) {
        return isatty(STDERR_FILENO) != 0;
    }
    return false;
}

std::ostream& Logger::log(LogLevel level, const std::string& prefix) {
    if (level < currentLevel_) {
        static std::ostringstream nullStream;
        return nullStream;
    }

    std::ostream& stream =
        (level >= LOG_ERROR) ? *errorStream_ : *outputStream_;

    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    bool useColors = isTerminal(stream);

    if (useColors) {
        stream << "\033[90m[" << buffer << "]\033[0m ";
        stream << getLevelColor(level) << "[" << getLevelString(level)
               << "]\033[0m";
    } else {
        stream << "[" << buffer << "] [" << getLevelString(level) << "]";
    }

    if (!prefix.empty()) {
        if (useColors) {
            stream << " \033[1m[" << prefix << "]\033[0m";
        } else {
            stream << " [" << prefix << "]";
        }
    }
    stream << " ";

    if (useColors) {
        stream << getTextColor(level);
    }

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
