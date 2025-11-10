#include "Socket.hpp"
#include "Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

Socket::Socket() : fd_(-1), host_(""), port_(0) {}

Socket::~Socket() { close(); }

bool Socket::setNonBlocking() {
    if (fd_ < 0) {
        return false;
    }
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        LOG_ERROR() << "fcntl F_GETFL failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR() << "fcntl F_SETFL O_NONBLOCK failed: " << strerror(errno)
                    << std::endl;
        return false;
    }
    return true;
}

bool Socket::setCloseOnExec() {
    if (fd_ < 0) {
        return false;
    }
    int flags = fcntl(fd_, F_GETFD, 0);
    if (flags < 0) {
        LOG_ERROR() << "fcntl F_GETFD failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd_, F_SETFD, flags | FD_CLOEXEC) < 0) {
        LOG_ERROR() << "fcntl F_SETFD FD_CLOEXEC failed: " << strerror(errno)
                    << std::endl;
        return false;
    }
    return true;
}

bool Socket::bind(const std::string& host, int port) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LOG_ERROR() << "socket() failed: " << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR() << "setsockopt SO_REUSEADDR failed: " << strerror(errno)
                    << std::endl;
        close();
        return false;
    }

    if (!setNonBlocking()) {
        close();
        return false;
    }

    if (!setCloseOnExec()) {
        close();
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_aton(host.c_str(), &addr.sin_addr) == 0) {
            LOG_ERROR() << "Invalid host address: " << host << std::endl;
            close();
            return false;
        }
    }

    if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR() << "bind() failed for " << host << ":" << port << ": "
                    << strerror(errno) << std::endl;
        close();
        return false;
    }

    host_ = host;
    port_ = port;
    return true;
}

bool Socket::listen(int backlog) {
    if (fd_ < 0) {
        LOG_ERROR() << "listen() called on invalid socket" << std::endl;
        return false;
    }
    if (::listen(fd_, backlog) < 0) {
        LOG_ERROR() << "listen() failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

int Socket::accept() {
    if (fd_ < 0) {
        return -1;
    }
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(fd_, (struct sockaddr*)&client_addr, &client_len);
    return client_fd;
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}
