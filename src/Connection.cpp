#include "Connection.hpp"
#include "Logger.hpp"
#include <unistd.h>
#include <ctime>

Connection::Connection(int fd, const std::string& client_ip)
    : fd_(fd), client_ip_(client_ip), last_activity_(time(NULL)) {}

Connection::~Connection() { close(); }

void Connection::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
