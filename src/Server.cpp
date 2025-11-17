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
#include <sstream>

static std::string reasonPhrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 411:
            return "Length Required";
        case 413:
            return "Payload Too Large";
        case 414:
            return "URI Too Long";
        case 431:
            return "Request Header Fields Too Large";
        case 500:
            return "Internal Server Error";
        default:
            return "HTTP Status";
    }
}

Server::Server() : running_(false), connection_timeout_(60), request_timeout_(30), max_connections_(1000) {}

Server::~Server() { stop(); }

bool Server::init(const ConfigParser& config) {
    config_ = config;
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
            socket_to_address_[socket->getFd()] = std::make_pair(host, port);
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
    int client_port = 0;
    if (getpeername(client_fd, (struct sockaddr*)&client_addr, &client_len) ==
        0) {
        client_ip = inet_ntoa(client_addr.sin_addr);
        client_port = ntohs(client_addr.sin_port);
    }

    std::string server_host = "0.0.0.0";
    int server_port = 0;
    std::map<int, std::pair<std::string, int> >::iterator addr_it =
        socket_to_address_.find(socket->getFd());
    if (addr_it != socket_to_address_.end()) {
        server_host = addr_it->second.first;
        server_port = addr_it->second.second;
    }

    if (connections_.size() >= max_connections_) {
        LOG_WARNING() << "Maximum connections limit reached, rejecting connection from "
                      << client_ip << ":" << client_port << std::endl;
        ::close(client_fd);
        return;
    }

    Connection* conn = new Connection(client_fd, client_ip, client_port,
                                      server_host, server_port);
    
    size_t max_body_size = 1048576;
    const std::vector<ServerConfig>& servers = config_.getServers();
    if (!servers.empty()) {
        max_body_size = servers[0].client_max_body_size;
        for (size_t i = 0; i < servers.size(); ++i) {
            if (servers[i].client_max_body_size > max_body_size) {
                max_body_size = servers[i].client_max_body_size;
            }
        }
    }
    conn->getRequestParser().setMaxBodySize(max_body_size);
    
    connections_[client_fd] = conn;
    LOG_INFO() << "New connection from " << client_ip << ":" << client_port
               << " (fd: " << client_fd << ")" << std::endl;
}

void Server::handleClientRead(int fd) {
    std::map<int, Connection*>::iterator it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    Connection* conn = it->second;
    conn->updateActivity();

    char buffer[4096];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);

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

    LOG_DEBUG() << "Received " << bytes_read << " bytes from fd " << fd
                << std::endl;

    RequestParser& parser = conn->getRequestParser();
    
    if (parser.getRequest().method.empty() && parser.getRequest().path.empty()) {
        conn->startRequest();
    }
    
    RequestParser::ParseResult result =
        parser.consume(buffer, static_cast<std::size_t>(bytes_read));

    if (result == RequestParser::PARSE_ERROR) {
        LOG_WARNING() << "Malformed request from fd " << fd << ": "
                      << parser.getError() << std::endl;
        int status_code = 400;
        if (parser.getError().find("too large") != std::string::npos) {
            status_code = 413;
        }
        sendErrorResponse(fd, status_code, parser.getError());
        closeConnection(fd);
        return;
    }

    if (result == RequestParser::PARSE_COMPLETE) {
        const HttpRequest& request = parser.getRequest();
        LOG_INFO() << "Parsed request " << request.method << " " << request.path
                   << " (fd: " << fd << ")" << std::endl;
        handleHttpRequest(fd, request);
        if (request.keep_alive) {
            conn->resetRequest();
            conn->resetRequestParser();
        } else {
            closeConnection(fd);
        }
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
        Connection* conn = it->second;
        bool should_close = false;
        
        if (now - conn->getLastActivity() > connection_timeout_) {
            LOG_INFO() << "Closing idle connection (fd: " << it->first << ")"
                       << std::endl;
            should_close = true;
        } else if (conn->getRequestStartTime() > 0 && 
                   now - conn->getRequestStartTime() > request_timeout_) {
            LOG_INFO() << "Closing request timeout connection (fd: " << it->first << ")"
                       << std::endl;
            should_close = true;
        }
        
        if (should_close) {
            to_close.push_back(it->first);
        }
    }

    for (size_t i = 0; i < to_close.size(); ++i) {
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

bool Server::sendAll(int fd, const std::string& data) {
    std::size_t total = 0;
    while (total < data.size()) {
        ssize_t sent =
            send(fd, data.c_str() + total, data.size() - total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            LOG_ERROR() << "send() failed for fd " << fd << ": "
                        << strerror(errno) << std::endl;
            return false;
        }
        total += static_cast<std::size_t>(sent);
    }
    return total == data.size();
}

void Server::handleHttpRequest(int fd, const HttpRequest& request) {
    std::map<int, Connection*>::iterator it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    Connection* conn = it->second;
    HttpResponse response = request_handler_.handleRequest(
        request, config_, conn->getServerHost(), conn->getServerPort());

    std::string response_str = response.toString();
    if (!sendAll(fd, response_str)) {
        closeConnection(fd);
    }
}

void Server::sendErrorResponse(int fd, int status_code, const std::string& message) {
    std::string phrase = reasonPhrase(status_code);
    std::ostringstream body;
    body << status_code << " " << phrase << "\n";
    if (!message.empty()) {
        body << message << "\n";
    }
    std::string body_str = body.str();

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << phrase << "\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Content-Length: " << body_str.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body_str;

    sendAll(fd, response.str());
}
