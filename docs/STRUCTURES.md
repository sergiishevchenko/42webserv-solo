# Structures and Classes Reference

This document describes all structures and classes used in the webserv project.

---

## Structures

### Location

Represents a location block in the server configuration. Defines how requests to specific URL paths should be handled.

**Definition:**
```cpp
struct Location {
    std::string path;                                    // URL path pattern
    std::set<std::string> methods;                       // Allowed HTTP methods
    std::string root;                                    // Root directory for this location
    std::string index;                                   // Default index file
    bool autoindex;                                      // Enable directory listing
    std::string redirect;                                // Redirect URL
    std::string upload_store;                            // Upload directory path
    std::map<std::string, std::string> cgi_pass;         // CGI handler mappings (extension -> program)

    Location() : autoindex(false) {}
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `path` | `std::string` | URL path pattern (e.g., `/uploads`, `/cgi`) |
| `methods` | `std::set<std::string>` | Allowed HTTP methods (GET, POST, DELETE, etc.) |
| `root` | `std::string` | Root directory path for serving files from this location |
| `index` | `std::string` | Default file to serve when directory is requested |
| `autoindex` | `bool` | Enable directory listing (default: `false`) |
| `redirect` | `std::string` | URL to redirect requests to this location |
| `upload_store` | `std::string` | Directory path for file uploads |
| `cgi_pass` | `std::map<std::string, std::string>` | Maps file extensions to CGI program paths (e.g., `{".py": "/usr/bin/python"}`) |

**Usage Example:**
```cpp
Location location;
location.path = "/uploads";
location.methods.insert("GET");
location.methods.insert("POST");
location.upload_store = "www/uploads";
location.autoindex = false;
```

---

### ServerConfig

Represents a server block in the configuration. Defines server settings, listening addresses/ports, and location blocks.

**Definition:**
```cpp
struct ServerConfig {
    std::vector<std::pair<std::string, int> > listen;   // Listening interfaces and ports
    std::string root;                                   // Root directory for the server
    std::string index;                                  // Default index file
    size_t client_max_body_size;                        // Maximum request body size (bytes)
    std::map<int, std::string> error_pages;             // Custom error page mappings
    std::vector<Location> locations;                    // Location blocks

    ServerConfig() : client_max_body_size(1048576) {}   // Default: 1 MB
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `listen` | `std::vector<std::pair<std::string, int> >` | List of interface:port pairs to listen on (e.g., `("127.0.0.1", 8080)`) |
| `root` | `std::string` | Root directory path for serving files |
| `index` | `std::string` | Default index file name |
| `client_max_body_size` | `size_t` | Maximum allowed size of request body in bytes (default: 1048576 = 1 MB) |
| `error_pages` | `std::map<int, std::string>` | Maps HTTP error codes to custom error page paths (e.g., `{404: "/errors/404.html"}`) |
| `locations` | `std::vector<Location>` | List of location blocks for path-specific configuration |

**Usage Example:**
```cpp
ServerConfig server;
server.listen.push_back(std::make_pair("127.0.0.1", 8080));
server.listen.push_back(std::make_pair("0.0.0.0", 8080));
server.root = "www";
server.index = "index.html";
server.client_max_body_size = 10485760;  // 10 MB
server.error_pages[404] = "/errors/404.html";
```

---

## Classes

### ConfigParser

Parses and validates server configuration files. Supports nginx-like configuration syntax with server and location blocks.

**Definition:**
```cpp
class ConfigParser {
   public:
    ConfigParser();
    ~ConfigParser();

    bool loadFromFile(const std::string& filepath);
    std::string getLastError() const;
    const std::vector<ServerConfig>& getServers() const;
    bool validate() const;

   private:
    // ... implementation details
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `ConfigParser()` | - | Default constructor |
| `~ConfigParser()` | - | Destructor |
| `loadFromFile(const std::string& filepath)` | `bool` | Loads and parses configuration from file. Returns `true` on success, `false` on error. |
| `getLastError() const` | `std::string` | Returns the last error message encountered during parsing or validation |
| `getServers() const` | `const std::vector<ServerConfig>&` | Returns a const reference to the parsed server configurations |
| `validate() const` | `bool` | Validates the parsed configuration. Returns `true` if valid, `false` otherwise. |

**Usage Example:**
```cpp
ConfigParser parser;

if (!parser.loadFromFile("config/example.conf")) {
    std::cerr << "Error: " << parser.getLastError() << std::endl;
    return 1;
}

if (!parser.validate()) {
    std::cerr << "Validation error: " << parser.getLastError() << std::endl;
    return 1;
}

const std::vector<ServerConfig>& servers = parser.getServers();
for (size_t i = 0; i < servers.size(); ++i) {
    const ServerConfig& server = servers[i];
    // Use server configuration...
}
```

**Configuration File Format:**

The parser supports nginx-like configuration syntax:

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    index index.html;
    client_max_body_size 10485760;
    
    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;
    
    location /uploads {
        methods GET POST;
        upload_store www/uploads;
        autoindex on;
    }
    
    location /cgi {
        methods GET POST;
        cgi_pass .py /usr/bin/python;
        cgi_pass .php /usr/bin/php;
    }
    
    location / {
        methods GET;
        root www/public;
        index index.html;
    }
}
```

**Error Handling:**

- If `loadFromFile()` returns `false`, use `getLastError()` to get the error message
- If `validate()` returns `false`, use `getLastError()` to get validation error details
- Common errors include:
  - File not found
  - Invalid syntax
  - Missing required directives
  - Invalid port numbers
  - Invalid configuration values

---

### Socket

Manages a network socket for listening or client connections. Handles socket creation, binding, listening, and accepting connections. Supports non-blocking mode and close-on-exec flag.

**Definition:**
```cpp
class Socket {
   public:
    Socket();
    ~Socket();

    bool bind(const std::string& host, int port);
    bool listen(int backlog = 128);
    int accept();
    void close();
    bool setNonBlocking();
    bool setCloseOnExec();

    int getFd() const;
    bool isValid() const;
    std::string getHost() const;
    int getPort() const;

   private:
    int fd_;              // File descriptor of the socket
    std::string host_;    // Host address (IP) the socket is bound to
    int port_;            // Port number the socket is bound to

    Socket(const Socket&);            // Copy constructor (disabled)
    Socket& operator=(const Socket&); // Assignment operator (disabled)
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `Socket()` | - | Default constructor. Initializes socket with invalid file descriptor (`fd_ = -1`). |
| `~Socket()` | - | Destructor. Automatically closes the socket if it's still open by calling `close()`. |
| `bind(const std::string& host, int port)` | `bool` | Creates a TCP socket, configures it (SO_REUSEADDR, non-blocking, close-on-exec), and binds it to the specified host and port. Returns `true` on success, `false` on error. |
| `listen(int backlog)` | `bool` | Starts listening for incoming connections. `backlog` specifies the maximum length of the queue of pending connections (default: 128). Returns `true` on success. |
| `accept()` | `int` | Accepts a new connection from the listening socket. Returns the file descriptor of the new client socket, or -1 on error. The returned socket is in blocking mode by default. |
| `close()` | `void` | Closes the socket by calling `::close(fd_)` and sets `fd_` to -1. Safe to call multiple times. |
| `setNonBlocking()` | `bool` | Sets the socket to non-blocking mode using `fcntl(F_SETFL, O_NONBLOCK)`. Returns `true` on success. Must be called on a valid socket (fd_ >= 0). |
| `setCloseOnExec()` | `bool` | Sets the close-on-exec flag using `fcntl(F_SETFD, FD_CLOEXEC)`. This ensures the socket is closed when a new program is executed. Returns `true` on success. |
| `getFd() const` | `int` | Returns the file descriptor of the socket. Returns -1 if socket is not initialized or closed. |
| `isValid() const` | `bool` | Returns `true` if the socket is valid (fd_ >= 0), `false` otherwise. |
| `getHost() const` | `std::string` | Returns the host address the socket is bound to (e.g., "127.0.0.1" or "0.0.0.0"). Empty string if not bound. |
| `getPort() const` | `int` | Returns the port number the socket is bound to. Returns 0 if not bound. |

**Private Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `fd_` | `int` | File descriptor of the socket. -1 indicates invalid/uninitialized socket. |
| `host_` | `std::string` | IP address the socket is bound to (e.g., "127.0.0.1", "0.0.0.0"). |
| `port_` | `int` | Port number the socket is bound to. |

**Features:**
- **Automatic socket creation**: Creates TCP socket (AF_INET, SOCK_STREAM) in `bind()`
- **Non-blocking mode**: All sockets are automatically set to non-blocking mode
- **Close-on-exec flag**: Sockets are automatically configured with FD_CLOEXEC for security
- **SO_REUSEADDR**: Allows address reuse to avoid "Address already in use" errors
- **Flexible binding**: Supports both specific interfaces (e.g., "127.0.0.1") and any interface ("0.0.0.0" or empty string)
- **RAII**: Automatic cleanup in destructor
- **Non-copyable**: Copy constructor and assignment operator are disabled to prevent accidental copying

**Socket Lifecycle:**

1. **Construction**: `Socket()` - Creates object with `fd_ = -1` (invalid)
2. **Binding**: `bind(host, port)` - Creates socket, configures it, and binds to address
3. **Listening**: `listen(backlog)` - Starts listening for connections
4. **Accepting**: `accept()` - Accepts new connections (can be called multiple times)
5. **Closing**: `close()` or destructor - Closes the socket

**System Calls Used:**

- `socket(AF_INET, SOCK_STREAM, 0)` - Creates TCP socket
- `setsockopt(..., SO_REUSEADDR, ...)` - Enables address reuse
- `fcntl(..., F_SETFL, O_NONBLOCK)` - Sets non-blocking mode
- `fcntl(..., F_SETFD, FD_CLOEXEC)` - Sets close-on-exec flag
- `bind(...)` - Binds socket to address and port
- `listen(...)` - Starts listening for connections
- `accept(...)` - Accepts new connection
- `close(...)` - Closes file descriptor

**Usage Example - Basic:**
```cpp
Socket socket;

// Bind to localhost on port 8080
if (!socket.bind("127.0.0.1", 8080)) {
    std::cerr << "Failed to bind socket" << std::endl;
    return 1;
}

// Start listening
if (!socket.listen(128)) {
    std::cerr << "Failed to listen" << std::endl;
    return 1;
}

// Accept connections in a loop
while (true) {
    int client_fd = socket.accept();
    if (client_fd >= 0) {
        // Handle client connection
        // ...
        ::close(client_fd);
    }
}

socket.close();
```

**Usage Example - In Server Class:**
```cpp
// In Server::init()
const ServerConfig& server_config = config.getServers()[0];
for (size_t i = 0; i < server_config.listen.size(); ++i) {
    const std::string& host = server_config.listen[i].first;
    int port = server_config.listen[i].second;

    Socket* socket = new Socket();
    if (!socket->bind(host, port)) {
        delete socket;
        continue;  // Skip this address
    }

    if (!socket->listen()) {
        delete socket;
        continue;  // Skip this address
    }

    listening_sockets_.push_back(socket);
    // Register socket file descriptor in poll()
}

// In Server::acceptNewConnection()
int client_fd = socket->accept();
if (client_fd >= 0) {
    // Set client socket to non-blocking
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    
    // Create Connection object
    Connection* conn = new Connection(client_fd, client_ip);
    connections_[client_fd] = conn;
}
```

**Error Handling:**
- `bind()` returns `false` if:
  - Socket creation fails (`socket()` returns -1)
  - Setting SO_REUSEADDR fails
  - Setting non-blocking mode fails
  - Setting close-on-exec flag fails
  - Invalid host address (for specific interfaces)
  - Binding fails (port already in use, permission denied, etc.)
- `listen()` returns `false` if:
  - Socket is invalid (fd_ < 0)
  - `listen()` system call fails
- `accept()` returns -1 on error:
  - Socket is invalid (fd_ < 0)
  - `accept()` system call fails (check `errno` for details)
  - In non-blocking mode, returns -1 with `errno = EAGAIN` if no connections are pending
- `setNonBlocking()` returns `false` if:
  - Socket is invalid (fd_ < 0)
  - `fcntl()` fails
- `setCloseOnExec()` returns `false` if:
  - Socket is invalid (fd_ < 0)
  - `fcntl()` fails
- All errors are logged using the Logger system

**Important Notes:**

1. **Non-blocking mode**: All sockets created by `bind()` are automatically set to non-blocking mode. This is essential for event-driven I/O using `poll()` or `select()`.

2. **Client sockets from accept()**: The socket returned by `accept()` is in blocking mode by default. You should set it to non-blocking mode after accepting:
   ```cpp
   int client_fd = socket.accept();
   if (client_fd >= 0) {
       fcntl(client_fd, F_SETFL, O_NONBLOCK);
       // Use client_fd...
   }
   ```

3. **Address reuse**: SO_REUSEADDR allows the server to restart immediately without waiting for the TIME_WAIT state to expire. This is especially useful during development.

4. **Close-on-exec**: FD_CLOEXEC ensures that the socket is closed when a new program is executed (e.g., in CGI scripts). This prevents file descriptor leaks.

5. **Host binding**:
   - `"127.0.0.1"` - Binds to localhost only (accessible only from local machine)
   - `"0.0.0.0"` or `""` - Binds to all interfaces (accessible from network)
   - Specific IP (e.g., `"192.168.1.100"`) - Binds to specific network interface

6. **One socket per address**: Each `Socket` object represents one listening socket. To listen on multiple addresses, create multiple `Socket` objects.

7. **Memory management**: `Socket` objects are typically managed by the `Server` class. They are created with `new` and stored in `listening_sockets_` vector. The `Server` destructor deletes all socket objects.

---

### Connection

Manages a client connection. Tracks the client's file descriptor, IP address, last activity time, and request start time for timeout handling.

**Definition:**
```cpp
class Connection {
   public:
    Connection(int fd, const std::string& client_ip, int client_port,
               const std::string& server_host, int server_port);
    ~Connection();

    int getFd() const;
    std::string getClientIp() const;
    int getClientPort() const;
    std::string getServerHost() const;
    int getServerPort() const;
    time_t getLastActivity() const;
    time_t getRequestStartTime() const;
    void updateActivity();
    void startRequest();
    void resetRequest();

    void close();
    bool isValid() const;

    RequestParser& getRequestParser();
    void resetRequestParser();

   private:
    int fd_;
    std::string client_ip_;
    int client_port_;
    std::string server_host_;
    int server_port_;
    time_t last_activity_;
    time_t request_start_time_;
    RequestParser parser_;
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `Connection(int fd, const std::string& client_ip)` | - | Constructor. Creates a connection with the given file descriptor and client IP address. Initializes `last_activity_` to current time. |
| `~Connection()` | - | Destructor. Closes the connection if it's still open. |
| `getFd() const` | `int` | Returns the file descriptor of the client socket. |
| `getClientIp() const` | `std::string` | Returns the IP address of the client. |
| `getLastActivity() const` | `time_t` | Returns the timestamp of the last activity on this connection. |
| `getRequestStartTime() const` | `time_t` | Returns the timestamp when the current request started (0 if no active request). |
| `updateActivity()` | `void` | Updates the last activity timestamp to the current time. |
| `startRequest()` | `void` | Marks the start of a new request by setting `request_start_time_` to current time. |
| `resetRequest()` | `void` | Resets the request start time (sets to 0) when request is completed. |
| `close()` | `void` | Closes the connection by closing the file descriptor. |
| `isValid() const` | `bool` | Returns `true` if the connection is valid (fd >= 0). |

**Features:**
- Automatic activity tracking for timeout handling
- Client IP address storage for logging and debugging
- Automatic cleanup on destruction

**Usage Example:**
```cpp
int client_fd = socket.accept();
if (client_fd >= 0) {
    std::string client_ip = "127.0.0.1";
    Connection conn(client_fd, client_ip);
    
    conn.updateActivity();
    
    if (conn.isValid()) {
        // ...
    }
    
    // Connection is automatically closed when it goes out of scope
}
```

**Timeout Handling:**
- The `last_activity_` field is used to track when the connection was last active (for idle timeout)
- The `request_start_time_` field tracks when a request started processing (for request timeout)
- The Server class uses both to implement connection and request timeouts
- Call `updateActivity()` whenever data is sent or received on the connection
- Call `startRequest()` when a new request begins parsing
- Call `resetRequest()` when a request is completed

---

### Server

Main server class that implements the HTTP server with non-blocking I/O using `poll()`. Manages multiple listening sockets, client connections, and handles events in an event loop.

**Definition:**
```cpp
class Server {
   public:
    Server();
    ~Server();

    bool init(const ConfigParser& config);
    void run();
    void stop();

   private:
    std::vector<Socket*> listening_sockets_;
    std::map<int, Connection*> connections_;
    std::vector<struct pollfd> poll_fds_;
    bool running_;
    time_t connection_timeout_;
    time_t request_timeout_;
    size_t max_connections_;

    void setupPollFds();
    void handlePollEvents();
    void acceptNewConnection(Socket* socket);
    void handleClientRead(int fd);
    void handleClientWrite(int fd);
    void closeConnection(int fd);
    void cleanupTimedOutConnections();
    void addPollFd(int fd, short events);
    void removePollFd(int fd);
    void updatePollFd(int fd, short events);
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `Server()` | - | Constructor. Initializes the server with default settings. |
| `~Server()` | - | Destructor. Stops the server and cleans up all connections and sockets. |
| `init(const ConfigParser& config)` | `bool` | Initializes the server by creating listening sockets for all server blocks in the configuration. Returns `true` on success. |
| `run()` | `void` | Starts the event loop. Blocks until `stop()` is called. Handles incoming connections and client I/O events. |
| `stop()` | `void` | Stops the server, closes all connections, and cleans up resources. |

**Private Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `setupPollFds()` | `void` | Rebuilds the `poll_fds_` vector from all listening sockets and active connections. |
| `handlePollEvents()` | `void` | Processes events from `poll()`. Handles new connections, client reads, and client writes. |
| `acceptNewConnection(Socket* socket)` | `void` | Accepts a new connection from a listening socket. Rejects connection if `max_connections_` limit is reached. Sets `client_max_body_size` from config to RequestParser. |
| `handleClientRead(int fd)` | `void` | Handles read events from a client connection. Parses HTTP requests and tracks request start time for timeout handling. |
| `handleClientWrite(int fd)` | `void` | Handles write events from a client connection. Updates activity timestamp. |
| `closeConnection(int fd)` | `void` | Closes a client connection and removes it from the connections map and poll_fds. |
| `cleanupTimedOutConnections()` | `void` | Closes connections that exceed `connection_timeout_` (idle) or `request_timeout_` (request processing) limits. |
| `addPollFd(int fd, short events)` | `void` | Adds a file descriptor to the poll_fds vector. |
| `removePollFd(int fd)` | `void` | Removes a file descriptor from the poll_fds vector. |
| `updatePollFd(int fd, short events)` | `void` | Updates the events mask for a file descriptor in poll_fds. |

**Features:**
- Non-blocking I/O using `poll()` system call
- Multiple listening sockets (multiple ports/interfaces)
- Connection management with activity tracking
- Automatic timeout handling for idle connections (60 seconds default)
- Request timeout handling (30 seconds default)
- Maximum connections limit (1000 default) to prevent resource exhaustion
- Body size validation during request parsing
- Event-driven architecture
- Graceful shutdown support

**Internal Data Structures:**

| Field | Type | Description |
|-------|------|-------------|
| `listening_sockets_` | `std::vector<Socket*>` | List of listening sockets (one per interface:port combination) |
| `connections_` | `std::map<int, Connection*>` | Map of active client connections (fd -> Connection*) |
| `poll_fds_` | `std::vector<struct pollfd>` | File descriptors for `poll()` system call |
| `running_` | `bool` | Flag indicating if the server is running |
| `connection_timeout_` | `time_t` | Timeout in seconds for idle connections (default: 60) |
| `request_timeout_` | `time_t` | Timeout in seconds for processing a single request (default: 30) |
| `max_connections_` | `size_t` | Maximum number of simultaneous connections (default: 1000) |

**Usage Example:**
```cpp
ConfigParser parser;
if (!parser.loadFromFile("config/server.conf")) {
    std::cerr << "Error: " << parser.getLastError() << std::endl;
    return 1;
}

if (!parser.validate()) {
    std::cerr << "Validation error: " << parser.getLastError() << std::endl;
    return 1;
}

Server server;
if (!server.init(parser)) {
    std::cerr << "Failed to initialize server" << std::endl;
    return 1;
}

server.run();
```

**Event Loop:**
1. `setupPollFds()` builds the poll_fds vector from all listening sockets and connections
2. `poll()` waits for events on all file descriptors
3. `handlePollEvents()` processes events:
   - `POLLIN` on listening socket → `acceptNewConnection()`
   - `POLLIN` on client socket → `handleClientRead()`
   - `POLLOUT` on client socket → `handleClientWrite()`
   - `POLLERR` or `POLLHUP` → `closeConnection()`
4. `cleanupTimedOutConnections()` removes idle connections
5. Loop repeats until `running_` is set to `false`

**Error Handling:**
- `init()` returns `false` if no listening sockets could be created
- Connection errors are logged and connections are closed automatically
- Timeout errors result in automatic connection cleanup
- The server continues running even if individual connections fail

**Platform Support:**
- Uses `poll()` system call (available on both Linux and macOS)
- Non-blocking sockets work on all POSIX-compliant systems
- Connection timeout handling is portable

---

## Data Structures Reference

### struct pollfd

Used by the `poll()` system call to monitor file descriptors for events. Part of the Server class implementation.

**Definition:**
```c
struct pollfd {
    int fd;        // File descriptor to monitor
    short events;  // Events to monitor (POLLIN, POLLOUT, etc.)
    short revents; // Events that occurred (set by poll())
};
```

**Events:**
- `POLLIN` - Data is available for reading
- `POLLOUT` - Data can be written without blocking
- `POLLERR` - Error condition
- `POLLHUP` - Hang up (connection closed)

**Example:**
```cpp
struct pollfd pfd;
pfd.fd = socket_fd;
pfd.events = POLLIN | POLLOUT;
pfd.revents = 0;

int result = poll(&pfd, 1, 1000);
if (result > 0) {
    if (pfd.revents & POLLIN) {
        // Data available for reading
    }
    if (pfd.revents & POLLOUT) {
        // Ready for writing
    }
}
```

### std::map<int, Connection*> connections_

Maps file descriptors to Connection objects in the Server class.

**Example:**
```cpp
std::map<int, Connection*> connections_;
Connection* conn = new Connection(client_fd, "127.0.0.1");
connections_[client_fd] = conn;

Connection* conn = connections_[client_fd];
```

### std::vector<Socket*> listening_sockets_

Stores listening sockets in the Server class.

**Example:**
```cpp
std::vector<Socket*> listening_sockets_;
Socket* socket = new Socket();
socket->bind("127.0.0.1", 8080);
socket->listen();
listening_sockets_.push_back(socket);
```

---

## System Socket Structures

These are structures provided by the operating system for socket programming. They are defined in system headers (`<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`).

### struct sockaddr_in

Represents an IPv4 socket address (IP address + port). Used with `bind()`, `connect()`, `accept()`, and other socket functions.

**Definition:**
```c
#include <netinet/in.h>

struct sockaddr_in {
    short            sin_family;   // Address family (AF_INET for IPv4)
    unsigned short   sin_port;     // Port number (in network byte order)
    struct in_addr   sin_addr;     // IP address (in network byte order)
    char             sin_zero[8];  // Padding (unused, should be zero)
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `sin_family` | `short` | Address family. Always `AF_INET` for IPv4 sockets. |
| `sin_port` | `unsigned short` | Port number in **network byte order** (use `htons()` to convert from host byte order). |
| `sin_addr` | `struct in_addr` | IP address structure (see below). Contains the IP address in **network byte order**. |
| `sin_zero[8]` | `char[8]` | Padding bytes. Should be set to zero. Not used. |

**Important Notes:**

1. **Byte Order**: Port and IP address must be in **network byte order** (big-endian). Use:
   - `htons()` - host to network short (for port)
   - `htonl()` - host to network long (for IP address)
   - `ntohs()` - network to host short (when reading port)
   - `ntohl()` - network to host long (when reading IP)

2. **IP Address Values:**
   - `INADDR_ANY` (0.0.0.0) - Listen on all interfaces
   - `INADDR_LOOPBACK` (127.0.0.1) - Localhost only
   - Specific IP address - Use `inet_addr()` or `inet_aton()`

**Usage Example:**
```c
#include <netinet/in.h>
#include <arpa/inet.h>

struct sockaddr_in addr;

// Initialize structure
memset(&addr, 0, sizeof(addr));  // Zero out the structure
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);                    // Convert port to network byte order
addr.sin_addr.s_addr = INADDR_ANY;              // Listen on all interfaces (0.0.0.0)

// Or bind to specific IP:
inet_aton("127.0.0.1", &addr.sin_addr);        // Set to localhost
// Or:
addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Alternative method

// Use with bind()
bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr));
```

**Common Patterns:**

```c
// Bind to all interfaces (0.0.0.0)
addr.sin_addr.s_addr = INADDR_ANY;

// Bind to localhost only
addr.sin_addr.s_addr = inet_addr("127.0.0.1");

// Bind to specific IP
addr.sin_addr.s_addr = inet_addr("192.168.1.100");

// Get IP address as string (when reading from socket)
char ip_str[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);
printf("IP: %s\n", ip_str);

// Get port (convert from network to host byte order)
int port = ntohs(addr.sin_port);
printf("Port: %d\n", port);
```

---

### struct in_addr

Represents an IPv4 address. Used as a field in `struct sockaddr_in`.

**Definition:**
```c
#include <netinet/in.h>

struct in_addr {
    unsigned long s_addr;  // IP address in network byte order (32-bit)
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `s_addr` | `unsigned long` | 32-bit IPv4 address in **network byte order**. Use `htonl()` or `inet_addr()` to set it. |

**Usage Example:**
```c
struct in_addr addr;

// Set IP address
addr.s_addr = inet_addr("127.0.0.1");        // Returns network byte order
// Or:
inet_aton("127.0.0.1", &addr);                // Alternative method

// Use in sockaddr_in
struct sockaddr_in sock_addr;
sock_addr.sin_addr = addr;
```

**Common Values:**
- `INADDR_ANY` - 0.0.0.0 (all interfaces)
- `INADDR_LOOPBACK` - 127.0.0.1 (localhost)
- `INADDR_BROADCAST` - 255.255.255.255 (broadcast address)

---

### struct sockaddr

Generic socket address structure. Used as a base type for casting to specific address types (like `sockaddr_in`).

**Definition:**
```c
#include <sys/socket.h>

struct sockaddr {
    unsigned short sa_family;  // Address family (AF_INET, AF_INET6, etc.)
    char           sa_data[14]; // Address data (size varies by family)
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `sa_family` | `unsigned short` | Address family (e.g., `AF_INET` for IPv4, `AF_INET6` for IPv6) |
| `sa_data[14]` | `char[14]` | Address data. Format depends on `sa_family`. For IPv4, this contains IP and port. |

**Important Notes:**

1. **Never use directly**: This structure is too generic. Always use `struct sockaddr_in` for IPv4 and cast to `struct sockaddr*` when passing to system calls.

2. **Casting Pattern**: All socket functions accept `struct sockaddr*`, but you pass `struct sockaddr_in*` cast to `struct sockaddr*`:

```c
struct sockaddr_in addr;
// ... fill addr ...

// Cast to sockaddr* when calling system functions
bind(fd, (struct sockaddr*)&addr, sizeof(addr));
connect(fd, (struct sockaddr*)&addr, sizeof(addr));
accept(fd, (struct sockaddr*)&addr, &addr_len);
```

3. **Size**: Always pass the size of the actual structure (`sizeof(sockaddr_in)`) to system calls, not `sizeof(sockaddr)`.

**Why This Structure Exists:**

- Provides a common interface for different address families (IPv4, IPv6, Unix domain sockets, etc.)
- Socket functions can work with any address type through this generic interface
- The actual address format is determined by `sa_family`

**Usage Example:**
```c
struct sockaddr_in addr_in;
// ... initialize addr_in ...

// Cast to generic sockaddr* for system calls
bind(socket_fd, (struct sockaddr*)&addr_in, sizeof(addr_in));

// When accepting connections:
struct sockaddr_in client_addr;
socklen_t client_len = sizeof(client_addr);
int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
```

---

## Notes on Socket Structures

1. **Memory Initialization**: Always zero out socket structures before use:
   ```c
   struct sockaddr_in addr;
   memset(&addr, 0, sizeof(addr));  // Important!
   ```

2. **Byte Order**: Always use network byte order functions (`htons`, `htonl`, `ntohs`, `ntohl`) when working with ports and IP addresses.

3. **Size Parameter**: Always pass the correct size (`sizeof(sockaddr_in)`) to system calls, not `sizeof(sockaddr)`.

4. **Casting**: Always cast `sockaddr_in*` to `sockaddr*` when passing to system functions.

5. **Platform Differences**: These structures are standardized by POSIX and work the same on Linux and macOS.

---

## Notes

- All string fields are empty by default (except `client_max_body_size` which defaults to 1 MB)
- Collections (vectors, maps, sets) are empty by default
- The `autoindex` field in `Location` defaults to `false`
- Configuration files support comments starting with `#`
- Directives must end with `;` (semicolon)
- Block structures use `{` and `}` braces
- All sockets are created in non-blocking mode
- Connections are automatically tracked for timeout handling
- The server uses `poll()` for event-driven I/O (compatible with Linux and macOS)

---

### HttpResponse

Represents an HTTP response with status code, headers, and body. Used to build and format HTTP responses before sending to clients.

**Definition:**
```cpp
class HttpResponse {
   public:
    HttpResponse();
    ~HttpResponse();

    void setStatus(int code, const std::string& reason);
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);
    void setBody(const char* data, std::size_t size);
    void setKeepAlive(bool keep_alive);

    std::string toString() const;
    std::string getStatusLine() const;
    std::string getHeaders() const;
    std::string getBody() const;
    int getStatusCode() const;

    static std::string getReasonPhrase(int status_code);
    static std::string getCurrentDate();

   private:
    int status_code_;
    std::string reason_phrase_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `HttpResponse()` | - | Constructor. Initializes response with default status 200 OK and sets Server and Date headers. |
| `~HttpResponse()` | - | Destructor. |
| `setStatus(int code, const std::string& reason)` | `void` | Sets HTTP status code and reason phrase. If reason is empty, uses standard reason phrase. |
| `setHeader(const std::string& name, const std::string& value)` | `void` | Sets an HTTP header. Overwrites existing header with same name. |
| `setBody(const std::string& body)` | `void` | Sets response body from string. Automatically sets Content-Length header. |
| `setBody(const char* data, std::size_t size)` | `void` | Sets response body from binary data. Automatically sets Content-Length header. |
| `setKeepAlive(bool keep_alive)` | `void` | Sets Connection header to "keep-alive" or "close". |
| `toString() const` | `std::string` | Returns complete HTTP response as string (status line + headers + blank line + body). |
| `getStatusLine() const` | `std::string` | Returns HTTP status line (e.g., "HTTP/1.1 200 OK\r\n"). |
| `getHeaders() const` | `std::string` | Returns all headers formatted as HTTP headers (e.g., "Content-Type: text/html\r\n"). |
| `getBody() const` | `std::string` | Returns response body. |
| `getStatusCode() const` | `int` | Returns HTTP status code. |
| `getReasonPhrase(int status_code)` | `static std::string` | Returns standard reason phrase for status code (e.g., "OK" for 200, "Not Found" for 404). |
| `getCurrentDate()` | `static std::string` | Returns current date in HTTP format (RFC 7231). |

**Supported Status Codes:**

| Code | Reason Phrase |
|------|---------------|
| 200 | OK |
| 201 | Created |
| 204 | No Content |
| 301 | Moved Permanently |
| 302 | Found |
| 400 | Bad Request |
| 403 | Forbidden |
| 404 | Not Found |
| 405 | Method Not Allowed |
| 411 | Length Required |
| 413 | Payload Too Large |
| 414 | URI Too Long |
| 431 | Request Header Fields Too Large |
| 500 | Internal Server Error |
| 501 | Not Implemented |
| 505 | HTTP Version Not Supported |

**Usage Example:**
```cpp
HttpResponse response;
response.setStatus(200, "OK");
response.setHeader("Content-Type", "text/html");
response.setBody("<html><body>Hello</body></html>");
response.setKeepAlive(true);

std::string http_response = response.toString();
// Sends: "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 32\r\nConnection: keep-alive\r\nDate: ...\r\nServer: webserv/1.0\r\n\r\n<html><body>Hello</body></html>"
```

**Features:**
- Automatic Date and Server headers
- Automatic Content-Length calculation
- Support for both text and binary body data
- Standard HTTP status codes with reason phrases
- Proper HTTP/1.1 formatting

---

### RequestHandler

Processes HTTP requests by matching them to server configuration, finding appropriate location blocks, serving files or directories, and generating responses.

**Definition:**
```cpp
class RequestHandler {
   public:
    RequestHandler();
    ~RequestHandler();

    HttpResponse handleRequest(const HttpRequest& request,
                               const ConfigParser& config,
                               const std::string& server_host,
                               int server_port);

   private:
    // ... implementation details
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `RequestHandler()` | - | Default constructor. |
| `~RequestHandler()` | - | Destructor. |
| `handleRequest(...)` | `HttpResponse` | Processes HTTP request and returns appropriate response. Handles file serving, directory listing, error pages, etc. |

**Responsibilities:**
- Match requests to server configuration based on Host header and server address
- Find appropriate location block for request path
- Validate HTTP methods against location configuration
- Build file paths from request path and server/location root
- Handle GET requests: serve static files and directories
- Handle POST requests: upload files with size limits and upload_store support
- Handle DELETE requests: remove files with proper error handling
- Generate error pages (default or custom from config)
- Validate path safety (prevent directory traversal)

**Request Processing Flow:**
```
1. Find matching ServerConfig (by Host header and server address)
2. Find matching Location block (longest path prefix match)
3. Validate HTTP method (check if allowed in location config)
   - If not allowed: return 405 Method Not Allowed
4. Route to appropriate handler based on method:
   - GET → handleGet()
   - POST → handlePost()
   - DELETE → handleDelete()
   - Other → return 501 Not Implemented
5. Each handler:
   - Builds file path (server root + location root + request path)
   - Checks path safety (prevent .. escapes)
   - Performs method-specific operations
   - Returns appropriate HttpResponse
```

**HTTP Methods Supported:**

| Method | Handler | Description |
|--------|---------|-------------|
| GET | `handleGet()` | Serves static files and directories |
| POST | `handlePost()` | Uploads files with size validation |
| DELETE | `handleDelete()` | Removes files from server |

**Method Validation:**
- Checks if method is allowed in location's `methods` set
- If location has no methods specified, all methods are allowed
- Returns 405 Method Not Allowed if method is not permitted

**GET Method (handleGet):**
- Serves static files and directories
- Reads files in binary mode
- Determines Content-Type from file extension
- Returns 200 OK for successful file reads
- Returns 404 Not Found if file doesn't exist
- Returns 403 Forbidden for unsafe paths
- Returns 500 Internal Server Error on read errors
- Handles large files (non-blocking writes handled by Server class)

**POST Method (handlePost):**
- Validates request body size against `client_max_body_size`
  - Returns 413 Payload Too Large if exceeded
- Extracts filename from `Content-Disposition` header or uses request path
- Sanitizes filename (removes invalid characters like `/`, `\`)
- Uses `upload_store` from location config, or falls back to server root
- Creates upload directory if it doesn't exist
- Writes request body to file
- Returns 201 Created with `Location` header on success
- Returns 403 Forbidden for unsafe paths
- Returns 500 Internal Server Error on file creation/write errors

**DELETE Method (handleDelete):**
- Validates that target is a file (not a directory)
- Removes file using `unlink()` system call
- Returns 204 No Content on successful deletion
- Returns 403 Forbidden for directories or unsafe paths
- Returns 404 Not Found if file doesn't exist
- Returns 403 Forbidden if permission denied
- Returns 500 Internal Server Error on other errors

**Directory Handling:**
- Checks for index file (from server or location config)
- If index file exists: serves it
- If autoindex enabled: generates HTML directory listing
- If autoindex disabled and no index: returns 403 Forbidden

**Autoindex Generation:**
- Creates HTML page with directory listing
- Shows file names as links
- Appends "/" to directory names
- Sorts entries alphabetically
- Returns 200 OK with text/html content

**Error Pages:**
- Checks for custom error page in server config (error_page directive)
- If custom page exists and is readable: serves it
- Otherwise: generates default error page with status code and reason phrase

**Path Safety:**
- Normalizes paths (removes //, /./, trailing slashes)
- Ensures requested path is within server root
- Prevents directory traversal attacks (../)
- Returns 403 Forbidden for unsafe paths

**Content Type Detection:**
Supports common file types:
- HTML: text/html
- CSS: text/css
- JavaScript: application/javascript
- JSON: application/json
- Images: image/png, image/jpeg, image/gif, image/svg+xml
- Text: text/plain
- PDF: application/pdf
- XML: application/xml
- Default: application/octet-stream

**Private Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `isMethodAllowed(const std::string& method, const Location* location)` | `bool` | Checks if HTTP method is allowed in location config. Returns `true` if location has no methods specified or method is in the methods set. |
| `handleGet(const HttpRequest& request, const ServerConfig& server, const Location* location)` | `HttpResponse` | Handles GET requests: serves files or directories. |
| `handlePost(const HttpRequest& request, const ServerConfig& server, const Location* location)` | `HttpResponse` | Handles POST requests: uploads files with validation. |
| `handleDelete(const HttpRequest& request, const ServerConfig& server, const Location* location)` | `HttpResponse` | Handles DELETE requests: removes files. |

**Usage Example:**
```cpp
RequestHandler handler;
HttpResponse response = handler.handleRequest(request, config, "127.0.0.1", 8080);
std::string http_response = response.toString();
send(fd, http_response.c_str(), http_response.size(), 0);
```

**POST Request Example:**
```cpp
// Client sends POST with body and Content-Disposition header
// RequestHandler validates size, extracts filename, saves to upload_store
// Returns 201 Created with Location header
```

**DELETE Request Example:**
```cpp
// Client sends DELETE /path/to/file
// RequestHandler validates path, checks if file exists, removes it
// Returns 204 No Content on success
```

---

### RequestParser

Parses HTTP/1.1 requests from raw byte streams. Handles request line, headers, and body parsing with support for Content-Length and Transfer-Encoding: chunked. Validates body size against `client_max_body_size` limit.

**Definition:**
```cpp
class RequestParser {
   public:
    enum ParseResult { PARSE_INCOMPLETE, PARSE_COMPLETE, PARSE_ERROR };

    RequestParser();
    ParseResult consume(const char* data, std::size_t length);
    const HttpRequest& getRequest() const;
    const std::string& getError() const;
    void reset();
    void setMaxBodySize(std::size_t max_size);

   private:
    // ... implementation details
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `RequestParser()` | - | Constructor. Initializes parser in `STATE_REQUEST_LINE` state. |
| `consume(const char* data, std::size_t length)` | `ParseResult` | Processes incoming data and parses HTTP request. Returns `PARSE_INCOMPLETE` if more data needed, `PARSE_COMPLETE` if request fully parsed, `PARSE_ERROR` on error. |
| `getRequest() const` | `const HttpRequest&` | Returns the parsed HTTP request object. |
| `getError() const` | `const std::string&` | Returns error message if parsing failed. |
| `reset()` | `void` | Resets parser to initial state for parsing a new request. |
| `setMaxBodySize(std::size_t max_size)` | `void` | Sets maximum allowed body size. If body exceeds this limit during parsing, returns `PARSE_ERROR` with "Request body too large" message. Set to 0 to disable limit. |

**Parsing States:**
- `STATE_REQUEST_LINE` - Parsing HTTP request line (method, path, version)
- `STATE_HEADERS` - Parsing HTTP headers
- `STATE_BODY_CONTENT_LENGTH` - Reading body with Content-Length
- `STATE_BODY_CHUNK_SIZE` - Reading chunk size in chunked encoding
- `STATE_BODY_CHUNK_DATA` - Reading chunk data
- `STATE_BODY_CHUNK_CRLF` - Reading chunk delimiter
- `STATE_BODY_CHUNK_TRAILERS` - Reading chunk trailers
- `STATE_COMPLETE` - Request fully parsed
- `STATE_ERROR` - Parsing error occurred

**Body Size Validation:**
- Validates `Content-Length` header value against `max_body_size_` in `finalizeHeaders()`
- Validates body size incrementally during Content-Length body parsing
- Validates body size incrementally during chunked body parsing
- Returns `PARSE_ERROR` with "Request body too large" message if limit exceeded
- Server converts this error to HTTP 413 Payload Too Large response

**Supported Features:**
- HTTP/1.1 request line parsing
- Header parsing with continuation lines
- Content-Length body parsing
- Transfer-Encoding: chunked body parsing
- Path normalization and query string extraction
- Keep-alive detection
- Body size validation

**Usage Example:**
```cpp
RequestParser parser;
parser.setMaxBodySize(10485760); // 10 MB limit

char buffer[4096];
ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
RequestParser::ParseResult result = parser.consume(buffer, bytes_read);

if (result == RequestParser::PARSE_COMPLETE) {
    const HttpRequest& request = parser.getRequest();
    // Process request...
} else if (result == RequestParser::PARSE_ERROR) {
    std::string error = parser.getError();
    // Handle error (e.g., send 413 if "too large")
}
```

**Error Handling:**
- Returns `PARSE_ERROR` for malformed requests
- Returns `PARSE_ERROR` if body size exceeds `max_body_size_`
- Error message available via `getError()`
- Server should send appropriate HTTP error response (400 Bad Request or 413 Payload Too Large)

---

## Limits and Timeouts

The webserv implements several limits and timeouts to ensure robustness and prevent resource exhaustion:

### Body Size Limits

- **Configuration**: `client_max_body_size` in server config (default: 1 MB)
- **Validation Points**:
  - During request parsing in `RequestParser::finalizeHeaders()` (Content-Length header)
  - During Content-Length body parsing
  - During chunked body parsing
  - In `RequestHandler::handlePost()` before file upload
- **Response**: HTTP 413 Payload Too Large

### Connection Limits

- **Maximum Connections**: 1000 simultaneous connections (default)
- **Behavior**: New connections are rejected when limit is reached
- **Location**: `Server::acceptNewConnection()`

### Timeouts

1. **Idle Connection Timeout**: 60 seconds (default)
   - Tracks last activity time (`Connection::last_activity_`)
   - Closes connections that have been idle for longer than timeout
   - Location: `Server::cleanupTimedOutConnections()`

2. **Request Processing Timeout**: 30 seconds (default)
   - Tracks request start time (`Connection::request_start_time_`)
   - Closes connections if request processing exceeds timeout
   - Location: `Server::cleanupTimedOutConnections()`

### Timeout Handling

- Timeouts are checked in `Server::cleanupTimedOutConnections()` during each event loop iteration
- Both idle and request timeouts are checked for each connection
- Connections exceeding either timeout are automatically closed
- No hanging: every state has a timeout and error path
