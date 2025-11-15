#include "Server.hpp"
#include "Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <algorithm>

Server::Server() : running_(false), connection_timeout_(60) {}

Server::~Server() { stop(); }

bool Server::init(const ConfigParser& config) {
    const std::vector<ServerConfig>& servers = config.getServers();
    if (servers.empty()) {
        LOG_ERROR() << "No server configurations found" << std::endl;
        return false;
    }

    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig& server_config = servers[i];
        for (size_t j = 0; j < server_config.listen.size(); ++j) {
            const std::string& host = server_config.listen[j].first;
            int port = server_config.listen[j].second;

            Socket* socket = new Socket();
            if (!socket->bind(host, port)) {
                delete socket;
                LOG_ERROR()
                    << "Failed to bind " << host << ":" << port << std::endl;
                continue;
            }

            if (!socket->listen()) {
                delete socket;
                LOG_ERROR() << "Failed to listen on " << host << ":" << port
                            << std::endl;
                continue;
            }

            listening_sockets_.push_back(socket);
            addPollFd(socket->getFd(), POLLIN);
            LOG_INFO() << "Listening on " << host << ":" << port << std::endl;
        }
    }

    if (listening_sockets_.empty()) {
        LOG_ERROR() << "No listening sockets created" << std::endl;
        return false;
    }

    return true;
}

void Server::addPollFd(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    poll_fds_.push_back(pfd);
}

void Server::removePollFd(int fd) {
    for (std::vector<struct pollfd>::iterator it = poll_fds_.begin(); it != poll_fds_.end(); ++it) {
        if (it->fd == fd) {
            poll_fds_.erase(it);
            break;
        }
    }
}

void Server::updatePollFd(int fd, short events) {
    for (std::vector<struct pollfd>::iterator it = poll_fds_.begin(); it != poll_fds_.end(); ++it) {
        if (it->fd == fd) {
            it->events = events;
            break;
        }
    }
}

void Server::setupPollFds() {
    poll_fds_.clear();

    for (size_t i = 0; i < listening_sockets_.size(); ++i) {
        addPollFd(listening_sockets_[i]->getFd(), POLLIN);
    }

    for (std::map<int, Connection*>::iterator it = connections_.begin(); it != connections_.end(); ++it) {
        addPollFd(it->first, POLLIN | POLLOUT);
    }
}

void Server::acceptNewConnection(Socket* socket) {
    int client_fd = socket->accept();
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR() << "accept() failed: " << strerror(errno) << std::endl;
        }
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        LOG_ERROR() << "Failed to set non-blocking mode for client: "
                    << strerror(errno) << std::endl;
        ::close(client_fd);
        return;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    std::string client_ip = "unknown";
    if (getpeername(client_fd, (struct sockaddr*)&client_addr, &client_len) ==
        0) {
        client_ip = inet_ntoa(client_addr.sin_addr);
    }

    Connection* conn = new Connection(client_fd, client_ip);
    connections_[client_fd] = conn;
    LOG_INFO() << "New connection from " << client_ip << " (fd: " << client_fd
               << ")" << std::endl;
}

void Server::handleClientRead(int fd) {
    std::map<int, Connection*>::iterator it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    Connection* conn = it->second;
    conn->updateActivity();

    char buffer[4096];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR() << "recv() failed for fd " << fd << ": "
                        << strerror(errno) << std::endl;
            closeConnection(fd);
        }
        return;
    }

    if (bytes_read == 0) {
        LOG_INFO() << "Connection closed by client (fd: " << fd << ")"
                   << std::endl;
        closeConnection(fd);
        return;
    }

    buffer[bytes_read] = '\0';
    LOG_DEBUG() << "Received " << bytes_read << " bytes from fd " << fd
                << std::endl;

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: 13\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += "Hello, World!";

    ssize_t bytes_sent = send(fd, response.c_str(), response.length(), 0);
    if (bytes_sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR() << "send() failed for fd " << fd << ": "
                        << strerror(errno) << std::endl;
            closeConnection(fd);
        }
    } else {
        LOG_DEBUG() << "Sent " << bytes_sent << " bytes to fd " << fd
                    << std::endl;
        closeConnection(fd);
    }
}

void Server::handleClientWrite(int fd) {
    std::map<int, Connection*>::iterator it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    Connection* conn = it->second;
    conn->updateActivity();
}

void Server::closeConnection(int fd) {
    std::map<int, Connection*>::iterator it = connections_.find(fd);
    if (it != connections_.end()) {
        delete it->second;
        connections_.erase(it);
        LOG_DEBUG() << "Connection closed (fd: " << fd << ")" << std::endl;
    }
}

void Server::cleanupTimedOutConnections() {
    time_t now = time(NULL);
    std::vector<int> to_close;

    for (std::map<int, Connection*>::iterator it = connections_.begin(); it != connections_.end(); ++it) {
        if (now - it->second->getLastActivity() > connection_timeout_) {
            to_close.push_back(it->first);
        }
    }

    for (size_t i = 0; i < to_close.size(); ++i) {
        LOG_INFO() << "Closing timed out connection (fd: " << to_close[i] << ")"
                   << std::endl;
        closeConnection(to_close[i]);
    }
}

void Server::handlePollEvents() {
    for (size_t i = 0; i < poll_fds_.size(); ++i) {
        struct pollfd& pfd = poll_fds_[i];

        if (pfd.revents & POLLERR) {
            LOG_WARNING() << "POLLERR on fd " << pfd.fd << std::endl;
            if (connections_.find(pfd.fd) != connections_.end()) {
                closeConnection(pfd.fd);
            }
            continue;
        }

        if (pfd.revents & POLLHUP) {
            LOG_DEBUG() << "POLLHUP on fd " << pfd.fd << std::endl;
            if (connections_.find(pfd.fd) != connections_.end()) {
                closeConnection(pfd.fd);
            }
            continue;
        }

        bool is_listening = false;
        for (size_t j = 0; j < listening_sockets_.size(); ++j) {
            if (listening_sockets_[j]->getFd() == pfd.fd) {
                is_listening = true;
                break;
            }
        }

        if (is_listening) {
            if (pfd.revents & POLLIN) {
                for (size_t j = 0; j < listening_sockets_.size(); ++j) {
                    if (listening_sockets_[j]->getFd() == pfd.fd) {
                        acceptNewConnection(listening_sockets_[j]);
                        break;
                    }
                }
            }
        } else {
            if (pfd.revents & POLLIN) {
                handleClientRead(pfd.fd);
            }
            if (pfd.revents & POLLOUT) {
                handleClientWrite(pfd.fd);
            }
        }
    }
}

void Server::run() {
    if (listening_sockets_.empty()) {
        LOG_ERROR() << "No listening sockets. Call init() first." << std::endl;
        return;
    }

    running_ = true;
    LOG_INFO() << "Server started. Waiting for connections..." << std::endl;

    while (running_) {
        setupPollFds();

        int timeout = 1000;
        int poll_result = poll(&poll_fds_[0], poll_fds_.size(), timeout);

        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR() << "poll() failed: " << strerror(errno) << std::endl;
            break;
        }

        if (poll_result == 0) {
            cleanupTimedOutConnections();
            continue;
        }

        handlePollEvents();
        cleanupTimedOutConnections();
    }

    LOG_INFO() << "Server stopped" << std::endl;
}

void Server::stop() {
    running_ = false;

    for (std::map<int, Connection*>::iterator it = connections_.begin(); it != connections_.end(); ++it) {
        delete it->second;
    }
    connections_.clear();
    poll_fds_.clear();

    for (size_t i = 0; i < listening_sockets_.size(); ++i) {
        delete listening_sockets_[i];
    }
    listening_sockets_.clear();
}
