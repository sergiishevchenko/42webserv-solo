# Poll System Call Documentation

This document explains how the `poll()` system call works in general and how it is specifically implemented in the 42webserv project.

---

## Table of Contents

1. [What is poll()?](#what-is-poll)
2. [How poll() Works](#how-poll-works)
3. [poll() in 42webserv](#poll-in-42webserv)
4. [Event Loop Architecture](#event-loop-architecture)
5. [File Descriptor Management](#file-descriptor-management)
6. [Event Handling](#event-handling)
7. [Non-Blocking I/O](#non-blocking-io)
8. [Performance Considerations](#performance-considerations)

---

## What is poll()?

`poll()` is a system call that allows a program to monitor multiple file descriptors simultaneously to see if I/O is possible on any of them. It is part of the POSIX standard and is available on both Linux and macOS.

### Basic Purpose

- **Monitor multiple file descriptors** for readiness to perform I/O operations
- **Block until events occur** or timeout expires
- **Return information** about which file descriptors are ready for I/O

### Why Use poll()?

- **Efficient**: Single system call can monitor hundreds of file descriptors
- **Non-blocking**: Can be used with non-blocking sockets for event-driven I/O
- **Scalable**: Better than blocking on individual file descriptors
- **Cross-platform**: Available on both Linux and macOS (unlike `epoll` which is Linux-only)

---

## How poll() Works

### Function Signature

```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

### Parameters

1. **`fds`**: Array of `struct pollfd` structures
2. **`nfds`**: Number of file descriptors in the array
3. **`timeout`**: Timeout in milliseconds (-1 = wait indefinitely, 0 = return immediately)

### Return Value

- **> 0**: Number of file descriptors with events
- **0**: Timeout occurred (no events)
- **-1**: Error occurred (check `errno`)

### struct pollfd

```c
struct pollfd {
    int   fd;       // File descriptor to monitor
    short events;   // Events to monitor (input)
    short revents;  // Events that occurred (output)
};
```

### Event Flags

- **`POLLIN`**: Data is available for reading
- **`POLLOUT`**: Data can be written without blocking
- **`POLLERR`**: Error condition occurred
- **`POLLHUP`**: Hang up (connection closed)
- **`POLLNVAL`**: Invalid file descriptor

### Example Usage

```c
struct pollfd pfd;
pfd.fd = socket_fd;
pfd.events = POLLIN | POLLOUT;
pfd.revents = 0;

int result = poll(&pfd, 1, 1000); // Wait up to 1 second

if (result > 0) {
    if (pfd.revents & POLLIN) {
        // Data available for reading
        recv(pfd.fd, buffer, size, 0);
    }
    if (pfd.revents & POLLOUT) {
        // Ready for writing
        send(pfd.fd, data, size, 0);
    }
}
```

---

## poll() in 42webserv

### Overview

The 42webserv project uses `poll()` to implement an event-driven, single-threaded HTTP server that can handle multiple concurrent connections efficiently.

### Key Components

1. **`poll_fds_` vector**: Stores all file descriptors being monitored
2. **Event loop**: Continuously calls `poll()` and processes events
3. **Non-blocking sockets**: All sockets are set to non-blocking mode
4. **Connection management**: Tracks active connections and their states

### Implementation Location

- **Header**: `include/Server.hpp`
- **Implementation**: `src/Server.cpp`
- **Main loop**: `Server::run()`

---

## Event Loop Architecture

### Main Event Loop

The server runs a continuous event loop in `Server::run()`:

```108:118:src/Server.cpp
void Server::setupPollFds() {
    poll_fds_.clear();

    for (size_t i = 0; i < listening_sockets_.size(); ++i) {
        addPollFd(listening_sockets_[i]->getFd(), POLLIN);
    }

    for (std::map<int, Connection*>::iterator it = connections_.begin(); it != connections_.end(); ++it) {
        addPollFd(it->first, POLLIN | POLLOUT);
    }
}
```

### Loop Structure

```343:373:src/Server.cpp
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
```

### Loop Steps

1. **Setup**: Rebuild `poll_fds_` vector with all file descriptors
2. **Wait**: Call `poll()` with 1 second timeout
3. **Process**: Handle events for ready file descriptors
4. **Cleanup**: Remove timed-out connections
5. **Repeat**: Continue until server stops

---

## File Descriptor Management

### Adding File Descriptors

When a new listening socket is created:

```82:88:src/Server.cpp
void Server::addPollFd(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    poll_fds_.push_back(pfd);
}
```

**Listening sockets** are added with `POLLIN` to detect new connections:

```69:69:src/Server.cpp
            addPollFd(socket->getFd(), POLLIN);
```

**Client connections** are added with `POLLIN | POLLOUT` to monitor both read and write readiness:

```116:116:src/Server.cpp
        addPollFd(it->first, POLLIN | POLLOUT);
```

### Removing File Descriptors

When a connection is closed:

```90:97:src/Server.cpp
void Server::removePollFd(int fd) {
    for (std::vector<struct pollfd>::iterator it = poll_fds_.begin(); it != poll_fds_.end(); ++it) {
        if (it->fd == fd) {
            poll_fds_.erase(it);
            break;
        }
    }
}
```

### Updating File Descriptors

To change the events being monitored:

```99:106:src/Server.cpp
void Server::updatePollFd(int fd, short events) {
    for (std::vector<struct pollfd>::iterator it = poll_fds_.begin(); it != poll_fds_.end(); ++it) {
        if (it->fd == fd) {
            it->events = events;
            break;
        }
    }
}
```

---

## Event Handling

### Event Processing

The `handlePollEvents()` function processes all events returned by `poll()`:

```295:341:src/Server.cpp
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
```

### Event Types Handled

1. **`POLLERR`**: Error condition - close connection
2. **`POLLHUP`**: Connection closed - close connection
3. **`POLLIN` on listening socket**: New connection - call `accept()`
4. **`POLLIN` on client socket**: Data available - read request
5. **`POLLOUT` on client socket**: Ready to write - send response

### New Connection Handling

When `POLLIN` is detected on a listening socket:

```120:180:src/Server.cpp
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
```

### Client Read Handling

When `POLLIN` is detected on a client socket:

```182:246:src/Server.cpp
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
```

---

## Non-Blocking I/O

### Why Non-Blocking?

All client sockets are set to non-blocking mode to prevent the server from blocking when reading or writing:

```129:134:src/Server.cpp
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        LOG_ERROR() << "Failed to set non-blocking mode for client: "
                    << strerror(errno) << std::endl;
        ::close(client_fd);
        return;
    }
```

### Non-Blocking Behavior

- **`recv()`**: Returns `-1` with `errno == EAGAIN` or `EWOULDBLOCK` if no data is available
- **`send()`**: Returns `-1` with `errno == EAGAIN` or `EWOULDBLOCK` if buffer is full
- **`accept()`**: Returns `-1` with `errno == EAGAIN` or `EWOULDBLOCK` if no pending connections

### Handling EAGAIN/EWOULDBLOCK

The server handles these errors gracefully:

```194:200:src/Server.cpp
    if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR() << "recv() failed for fd " << fd << ": "
                        << strerror(errno) << std::endl;
            closeConnection(fd);
        }
        return;
    }
```

---

## Performance Considerations

### Timeout Value

The server uses a 1-second timeout:

```355:355:src/Server.cpp
        int timeout = 1000;
```

This allows:
- **Regular cleanup**: Timed-out connections are checked every second
- **Responsiveness**: Server doesn't block indefinitely
- **Resource management**: Idle connections are closed promptly

### Connection Limits

The server enforces a maximum connection limit:

```155:160:src/Server.cpp
    if (connections_.size() >= max_connections_) {
        LOG_WARNING() << "Maximum connections limit reached, rejecting connection from "
                      << client_ip << ":" << client_port << std::endl;
        ::close(client_fd);
        return;
    }
```

### Timeout Management

Connections are automatically closed if they exceed timeout limits:

```266:293:src/Server.cpp
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
```

### Vector Rebuilding

The `poll_fds_` vector is rebuilt on each iteration. This is acceptable for moderate connection counts but could be optimized for very high connection counts by:
- Only adding/removing changed file descriptors
- Using a more efficient data structure
- Batching updates

---

## Summary

The `poll()` system call is the core of the 42webserv event-driven architecture:

1. **Single-threaded**: One event loop handles all connections
2. **Non-blocking**: All I/O operations are non-blocking
3. **Efficient**: Monitors multiple file descriptors in one system call
4. **Scalable**: Can handle hundreds of concurrent connections
5. **Cross-platform**: Works on both Linux and macOS

The implementation follows a standard event-driven server pattern:
- Setup file descriptors → Wait for events → Process events → Repeat

This architecture allows the server to handle multiple clients concurrently without the overhead of threads or processes.

---

## References

- [poll() System Call Manual](https://man7.org/linux/man-pages/man2/poll.2.html)
- [POSIX poll() Specification](https://pubs.opengroup.org/onlinepubs/9699919799/functions/poll.html)
- [Non-blocking I/O](https://en.wikipedia.org/wiki/Asynchronous_I/O)
