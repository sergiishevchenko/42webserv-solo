#include "Connection.hpp"
#include "Logger.hpp"
#include <unistd.h>
#include <ctime>

Connection::Connection(int fd, const std::string& client_ip, int client_port,
                       const std::string& server_host, int server_port)
    : fd_(fd), client_ip_(client_ip), client_port_(client_port),
      server_host_(server_host), server_port_(server_port),
      last_activity_(time(NULL)), request_start_time_(0) {}

Connection::~Connection() { close(); }

void Connection::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
