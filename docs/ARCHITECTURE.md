# webserv Architecture: Component Interaction Diagram

## Overview

This document provides a detailed diagram of how different components of the webserv project interact with each other. It covers the complete flow from program startup to handling client requests and shutting down.

---

## Component Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│  - Entry point                                              │
│  - Signal handling                                          │
│  - Orchestrates initialization                              │
└────────────┬────────────────────────────────────────────────┘
             │
             ├─────────────────────────────────────┐
             │                                     │
             ▼                                     ▼
    ┌──────────────────┐              ┌──────────────────-┐
    │  ConfigParser    │              │     Server        │
    │                  │              │                   │
    │  - File parsing  │              │  - Event loop     │
    │  - Validation    │─────────────▶│  - Connection mgmt│
    │  - ServerConfig  │  provides    │  - Socket mgmt    │
    └──────────────────┘  config      └───────┬─────────--┘
                                              │
                   ┌─────────────────────────┼─────────────────────────┐
                   │                         │                         │
                   ▼                         ▼                         ▼
           ┌──────────────┐         ┌──────────────┐         ┌──────────────┐
           │    Socket    │         │  Connection  │         │    Logger    │
           │              │         │              │         │              │
           │  - bind()    │         │  - Client fd │         │  - Logging   │
           │  - listen()  │         │  - Activity  │         │  - Levels    │
           │  - accept()  │         │  - Timeout   │         │              │
           └──────┬───────┘         │  - HTTP req  │         └──────────────┘
                  │                 │    parser    │
                  │                 └──────┬───────┘
                  │                        │
                  ▼                        ▼
           ┌──────────────┐        ┌──────────────┐
           │   Network    │        │ RequestParser│
           │   events     │◀──────▶│  - Start line│
           │ (poll loop)  │        │  - Headers   │
           └──────────────┘        │  - Body/chunk│
                                   │  - Keep-alive│
                                   └──────┬───────┘
                                          │
                                          ▼
                                   ┌──────────────--┐
                                   │ RequestHandler │
                                   │  - Config match│
                                   │  - File serve  │
                                   │  - Autoindex   │
                                   │  - Error pages │
                                   └──────┬───────--┘
                                          │
                                          ▼
                                   ┌──────────────┐
                                   │ HttpResponse │
                                   │  - Status    │
                                   │  - Headers   │
                                   │  - Body      │
                                   └──────────────┘
```

---

## Component Responsibilities

### 1. main.cpp
**Role:** Application entry point and initialization coordinator

**Responsibilities:**
- Parse command-line arguments
- Initialize signal handlers (SIGINT, SIGTERM)
- Create and configure ConfigParser
- Create and initialize Server
- Start the server event loop

**Key Functions:**
- `main(int argc, char** argv)` - Entry point
- `signalHandler(int sig)` - Handles shutdown signals
- `printUsage(const char* progName)` - Displays usage information

---

### 2. ConfigParser
**Role:** Configuration file parser and validator

**Responsibilities:**
- Parse nginx-like configuration files
- Extract server blocks and directives
- Validate configuration data
- Provide ServerConfig structures to Server

**Key Data Structures:**
```cpp
struct ServerConfig {
    std::vector<std::pair<std::string, int>> listen;   // host:port pairs
    std::string root;                                  // Document root
    std::string index;                                 // Default index file
    size_t client_max_body_size;                       // Max request body size
    std::map<int, std::string> error_pages;            // Error page mappings
    std::vector<Location> locations;                   // Route configurations
};

struct Location {
    std::string path;                                  // Route path
    std::set<std::string> methods;                     // Allowed HTTP methods
    std::string root;                                  // Location-specific root
    std::string index;                                 // Location-specific index
    bool autoindex;                                    // Directory listing flag
    std::string redirect;                              // Redirect URL
    std::string upload_store;                          // Upload directory
    std::map<std::string, std::string> cgi_pass;       // CGI handler mappings
};
```

**Key Methods:**
- `loadFromFile(const std::string& filepath)` - Loads and parses config file
- `validate()` - Validates parsed configuration
- `getServers()` - Returns vector of ServerConfig structures

**Interaction Flow:**
```
main.cpp
  │
  ├─▶ ConfigParser::loadFromFile()
  │     │
  │     ├─▶ Reads file line by line
  │     ├─▶ Identifies server blocks
  │     ├─▶ Parses directives (listen, root, index, etc.)
  │     ├─▶ Parses location blocks
  │     └─▶ Populates servers_ vector
  │
  ├─▶ ConfigParser::validate()
  │     │
  │     ├─▶ Checks for at least one server block
  │     ├─▶ Validates listen directives
  │     ├─▶ Validates ports (1-65535)
  │     ├─▶ Checks required fields (root, etc.)
  │     └─▶ Returns validation result
  │
  └─▶ ConfigParser::getServers()
        └─▶ Returns const reference to servers_ vector
```

---

### 3. Socket
**Role:** Low-level socket operations wrapper

**Responsibilities:**
- Create and configure TCP sockets
- Bind sockets to host:port addresses
- Set up listening sockets
- Accept incoming connections
- Manage socket file descriptors

**Key Methods:**
- `bind(const std::string& host, int port)` - Binds socket to address
- `listen(int backlog = 128)` - Starts listening for connections
- `accept()` - Accepts new connection (returns client fd)
- `setNonBlocking()` - Sets O_NONBLOCK flag
- `setCloseOnExec()` - Sets FD_CLOEXEC flag
- `close()` - Closes socket file descriptor

**Socket Lifecycle:**
```
Creation
  │
  ├─▶ socket(AF_INET, SOCK_STREAM, 0)  → Creates TCP socket
  │
  ├─▶ setsockopt(SO_REUSEADDR)         → Allows address reuse
  │
  ├─▶ fcntl(F_SETFL, O_NONBLOCK)       → Non-blocking mode
  │
  ├─▶ fcntl(F_SETFD, FD_CLOEXEC)       → Close on exec
  │
  ├─▶ bind(host, port)                 → Binds to address
  │
  ├─▶ listen(backlog)                  → Starts listening
  │
  └─▶ Ready to accept connections
```

**Interaction with Server:**
```
Server::init()
  │
  ├─▶ new Socket()                     → Creates Socket object
  │
  ├─▶ Socket::bind(host, port)         → Binds to address
  │     │
  │     ├─▶ socket()                   → System call
  │     ├─▶ setsockopt()               → System call
  │     ├─▶ setNonBlocking()           → System call
  │     ├─▶ setCloseOnExec()           → System call
  │     └─▶ bind()                     → System call
  │
  ├─▶ Socket::listen()                 → Starts listening
  │     │
  │     └─▶ listen()                   → System call
  │
  └─▶ Socket stored in listening_sockets_

Server::acceptNewConnection()
  │
  └─▶ Socket::accept()                 → Accepts connection
        │
        └─▶ accept()                   → System call
              └─▶ Returns client_fd
```

---

### 4. Connection
**Role:** Represents an active client connection

**Responsibilities:**
- Store client socket file descriptor
- Track client IP address
- Monitor last activity timestamp
- Manage connection lifecycle
- Maintain per-connection HTTP parsing state

**Key Data Members:**
- `int fd_` - Client socket file descriptor
- `std::string client_ip_` - Client IP address
- `int client_port_` - Client port number
- `std::string server_host_` - Server host address
- `int server_port_` - Server port number
- `time_t last_activity_` - Timestamp of last activity
- `RequestParser parser_` - Stateful HTTP/1.x parser instance

**Key Methods:**
- `updateActivity()` - Updates last_activity_ timestamp
- `close()` - Closes connection file descriptor
- `getFd()`, `getClientIp()`, `getClientPort()`, `getServerHost()`, `getServerPort()`, `getLastActivity()` - Getters
- `RequestParser& getRequestParser()` - Accessor for parser instance
- `void resetRequestParser()` - Clears parser state after a completed request

**Connection Lifecycle:**
```
Creation (in Server::acceptNewConnection)
  │
  ├─▶ new Connection(client_fd, client_ip, client_port, server_host, server_port)
  │     │
  │     ├─▶ fd_ = client_fd
  │     ├─▶ client_ip_ = client_ip
  │     ├─▶ client_port_ = client_port
  │     ├─▶ server_host_ = server_host
  │     ├─▶ server_port_ = server_port
  │     └─▶ last_activity_ = time(NULL)
  │
  ├─▶ Stored in Server::connections_ map
  │
  ├─▶ Added to poll_fds_ in next setupPollFds()
  │
  └─▶ Monitored by event loop

Active State
  │
  ├─▶ Connection::updateActivity()     → Updates timestamp
  │
  ├─▶ Used in handleClientRead()
  │
  └─▶ Used in handleClientWrite()

Destruction (in Server::closeConnection)
  │
  ├─▶ Connection::~Connection()
  │     │
  │     └─▶ Connection::close()
  │           └─▶ close(fd_)           → System call
  │
  ├─▶ Removed from Server::connections_
  │
  └─▶ Removed from poll_fds_ in next setupPollFds()
```

**Interaction with Server:**
```
Server maintains: std::map<int, Connection*> connections_

Key:   client file descriptor (fd)
Value: Connection* object

Operations:
  - connections_[fd] = new Connection(...)  → Create
  - conn = connections_[fd]                 → Access
  - delete connections_[fd]                 → Destroy
  - connections_.erase(fd)                  → Remove
```

#### HTTP Parser Integration
```
Client socket fd
  │
  └─▶ Connection::getRequestParser()
         │
         ├─▶ RequestParser::consume(buffer, bytes)
         │     ├─▶ Parses start line, headers, body/chunks
         │     └─▶ Returns PARSE_INCOMPLETE / COMPLETE / ERROR
         │
         ├─▶ On PARSE_COMPLETE and keep-alive → Connection::resetRequestParser()
         └─▶ On PARSE_ERROR or Connection:close() → parser_ discarded with Connection
```
Each `Connection` owns its parser so pipeline state (partial headers, partially
received chunked bodies, etc.) remains isolated per client. The server never
shares parser instances across sockets; this guarantees concurrency safety and
enables keep-alive: once the response to a completed request is flushed, the
parser resets while the TCP connection stays open.

---

### 4.1 HTTP Request Parser

**Role:** Incrementally decode HTTP/1.0/1.1 requests from arbitrary-sized socket
reads while enforcing RFC-inspired validation and path normalization rules.

**Core states:**
1. `STATE_REQUEST_LINE` — parse method, raw target, and version; ensure method
   is a valid token, version is HTTP/1.0 or HTTP/1.1, and normalize the path
   (percent-decode segments, reject attempts to escape root via `..`).
2. `STATE_HEADERS` — accept header lines plus folded continuations, lowercase
   header names, trim values, and detect malformed syntax.
3. `STATE_BODY_CONTENT_LENGTH` — copy exactly `Content-Length` bytes into the
   request body buffer.
4. `STATE_BODY_CHUNK_*` — implement chunked transfer decoding (hex size line,
   chunk data, CRLF, optional trailers, terminal zero-sized chunk).
5. `STATE_COMPLETE` / `STATE_ERROR` — signal downstream logic to proceed or fail
   with a descriptive message.

**Key behaviors:**
- Stores headers in a `std::map<std::string, std::string>` with normalized keys
  so lookups are case-insensitive (`getHeader()` helper).
- Tracks `content_length`, `chunked`, `keep_alive`, and the fully decoded body.
- Determines persistent connections using HTTP version defaults plus explicit
  `Connection` header overrides.
- Exposes `const HttpRequest& getRequest()` so higher layers can inspect parsed
  data without copying.

**Interaction with Server event loop:**
```
handleClientRead(fd)
  │
  ├─▶ conn = connections_[fd]
  ├─▶ parser = conn->getRequestParser()
  ├─▶ result = parser.consume(buffer, bytes_read)
  │     ├─▶ PARSE_INCOMPLETE → wait for more data
  │     ├─▶ PARSE_COMPLETE  → use parser.getRequest(), build response
  │     └─▶ PARSE_ERROR     → sendErrorResponse(), close connection
  │
  └─▶ if request.keep_alive → conn->resetRequestParser()
        else → closeConnection(fd)
```

This layered approach keeps the networking code agnostic of HTTP syntax while
ensuring every connection remembers where it left off between poll events.

---

### 5. Server
**Role:** Core server orchestrator and event loop manager

**Responsibilities:**
- Initialize listening sockets from configuration
- Manage event loop using poll()
- Handle new connections
- Process client read/write events
- Drive HTTP parsing/response flow per connection
- Manage connection timeouts
- Coordinate all components

**Key Data Members:**
- `std::vector<Socket*> listening_sockets_` - Listening sockets
- `std::map<int, Connection*> connections_` - Active connections (fd → Connection*)
- `std::map<int, std::pair<std::string, int> > socket_to_address_` - Socket to address mapping
- `std::vector<struct pollfd> poll_fds_` - File descriptors for poll()
- `bool running_` - Server running flag
- `time_t connection_timeout_` - Connection timeout in seconds
- `ConfigParser config_` - Server configuration
- `RequestHandler request_handler_` - HTTP request handler

**Key Methods:**

#### `init(const ConfigParser& config)`
```
Flow:
  1. Store config in config_ member
  2. Get ServerConfig vector from ConfigParser
  3. For each ServerConfig:
     a. For each listen directive:
        - Create new Socket
        - Call Socket::bind(host, port)
        - Call Socket::listen()
        - Add to listening_sockets_
        - Store address mapping in socket_to_address_
        - Register in poll_fds_ with POLLIN
  4. Return success/failure
```

#### `run()` - Event Loop
```
Flow:
  1. Set running_ = true
  2. While running_:
     a. setupPollFds()              → Prepare poll structures
     b. poll(poll_fds_, timeout)    → Wait for events
     c. If timeout:
        - cleanupTimedOutConnections()
        - Continue
     d. If events:
        - handlePollEvents()        → Process events
        - cleanupTimedOutConnections()
  3. Cleanup on exit
```

#### `setupPollFds()`
```
Flow:
  1. Clear poll_fds_
  2. Add all listening sockets:
     - For each Socket* in listening_sockets_:
       - Add fd with POLLIN events
  3. Add all client connections:
     - For each Connection* in connections_:
       - Add fd with POLLIN | POLLOUT events
```

#### `handlePollEvents()`
```
Flow:
  For each pollfd in poll_fds_:
    1. Check for errors (POLLERR, POLLHUP)
       - If found: closeConnection(fd)
    2. Determine socket type:
       - If fd is in listening_sockets_:
         → is_listening = true
       - Else:
         → is_listening = false
    3. Handle events:
       - If is_listening && POLLIN:
         → acceptNewConnection(socket)
       - If !is_listening && POLLIN:
         → handleClientRead(fd)
       - If !is_listening && POLLOUT:
         → handleClientWrite(fd)
```

#### `acceptNewConnection(Socket* socket)`
```
Flow:
  1. Call Socket::accept()
     - Returns client_fd
  2. Set client_fd to non-blocking mode
  3. Get client IP address (getpeername)
  4. Create new Connection(client_fd, client_ip)
  5. Store in connections_ map
  6. Log connection
```

#### `handleClientRead(int fd)`
```
Flow:
  1. Find Connection in connections_ map
  2. Update connection activity timestamp
  3. Read data using recv(fd, buffer, size, 0)
  4. Handle read result:
     - If error (and not EAGAIN/EWOULDBLOCK):
       → closeConnection(fd)
     - If bytes_read == 0:
       → closeConnection(fd)  (client closed)
     - If bytes_read > 0:
       → parser = connection->getRequestParser()
       → result = parser.consume(buffer, bytes_read)
         • PARSE_INCOMPLETE → wait for more data
         • PARSE_ERROR → sendErrorResponse(fd, 400, parser.getError()); closeConnection(fd)
         • PARSE_COMPLETE:
            · request = parser.getRequest()
            · handleHttpRequest(fd, request)   (processes request, serves files)
            · if request.keep_alive:
                  connection->resetRequestParser()
              else:
                  closeConnection(fd)
```

#### `handleClientWrite(int fd)`
```
Flow:
  1. Find Connection in connections_ map
  2. Update connection activity timestamp
  3. (Currently minimal implementation)
```

#### Response helpers
- `sendParsedEcho(int fd, const HttpRequest& request)`  
  Builds a text/plain 200 response enumerating method, normalized path, query,
  protocol version, keep-alive flag, body lengths, chunked flag, and all parsed
  headers. Used as an interim handler until full Stage 4 response logic arrives.

- `sendErrorResponse(int fd, int status_code, const std::string& message)`  
  Generates a minimal error body (`<code> <reason>\n<details>\n`) and always
  appends `Connection: close`. Relies on `reasonPhrase(status_code)` helper for
  standard text (200, 400, 411, 413, 414, 431, 500).

- `sendAll(int fd, const std::string& data)`  
  Loops on `send()` to push the entire buffer, transparently retrying on EINTR
  and aborting only on fatal errors. Returns `false` if the socket stops
  accepting data so the caller can close the connection.

#### `closeConnection(int fd)`
```
Flow:
  1. Find Connection in connections_ map
  2. Delete Connection object
     - Destructor calls close(fd)
  3. Remove from connections_ map
  4. Log closure
```

#### `cleanupTimedOutConnections()`
```
Flow:
  1. Get current time
  2. For each Connection in connections_:
     - Calculate time since last_activity_
     - If time > connection_timeout_:
       → Add fd to to_close list
  3. For each fd in to_close:
     - closeConnection(fd)
```

#### `stop()`
```
Flow:
  1. Set running_ = false
  2. Delete all Connection objects
  3. Clear connections_ map
  4. Clear poll_fds_
  5. Delete all Socket objects
  6. Clear listening_sockets_
```

---

### 6. Logger
**Role:** Centralized logging system

**Responsibilities:**
- Provide logging interface with levels (DEBUG, INFO, WARNING, ERROR)
- Format log messages with timestamps and colors
- Singleton pattern for global access

**Key Methods:**
- `getInstance()` - Returns singleton instance
- `setLogLevel(LogLevel level)` - Sets minimum log level
- `debug()`, `info()`, `warning()`, `error()` - Log stream methods

**Usage:**
```cpp
LOG_DEBUG() << "Debug message" << std::endl;
LOG_INFO() << "Info message" << std::endl;
LOG_WARNING() << "Warning message" << std::endl;
LOG_ERROR() << "Error message" << std::endl;
```

---

### 7. HttpResponse

**Role:** HTTP response builder and formatter

**Responsibilities:**
- Build HTTP responses with status codes, headers, and body
- Format responses according to HTTP/1.1 specification
- Automatically add required headers (Date, Server, Content-Length)
- Support both text and binary body data

**Key Methods:**
- `setStatus(int code, const std::string& reason)` - Set HTTP status code
- `setHeader(const std::string& name, const std::string& value)` - Add/modify header
- `setBody(const std::string& body)` - Set response body
- `setKeepAlive(bool keep_alive)` - Set Connection header
- `toString() const` - Get complete HTTP response string

**Interaction Flow:**
```
RequestHandler
  │
  ├─▶ HttpResponse::setStatus(200, "OK")
  ├─▶ HttpResponse::setHeader("Content-Type", "text/html")
  ├─▶ HttpResponse::setBody(file_content)
  ├─▶ HttpResponse::setKeepAlive(true)
  │
  └─▶ HttpResponse::toString()
        │
        └─▶ "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n..."
```

---

### 8. RequestHandler

**Role:** HTTP request processor and response generator

**Responsibilities:**
- Match requests to server configuration
- Find appropriate location blocks
- Serve static files with proper content types
- Handle directory requests (index files, autoindex)
- Generate error pages
- Validate path safety

**Key Methods:**
- `handleRequest(...)` - Process request and return HttpResponse

**Request Processing Flow:**
```
HttpRequest
  │
  ├─▶ RequestHandler::findServerConfig()
  │     └─▶ Match by Host header and server address
  │
  ├─▶ RequestHandler::findLocation()
  │     └─▶ Find longest path prefix match
  │
  ├─▶ RequestHandler::buildFilePath()
  │     └─▶ Combine root + location root + request path
  │
  ├─▶ RequestHandler::isPathSafe()
  │     └─▶ Validate path (prevent directory traversal)
  │
  └─▶ RequestHandler::serveFile() or serveDirectory()
        │
        ├─▶ If file: read and serve with Content-Type
        ├─▶ If directory: serve index or generate autoindex
        └─▶ If error: generate error page
```

**File Serving:**
- Reads files in binary mode
- Determines Content-Type from file extension
- Returns appropriate HTTP status codes (200, 404, 403, 500)

**Directory Handling:**
- Checks for index file (from server or location config)
- If autoindex enabled: generates HTML directory listing
- If autoindex disabled and no index: returns 403 Forbidden

**Error Pages:**
- Checks for custom error page in config (error_page directive)
- If custom page exists: serves it
- Otherwise: generates default error page

**Path Safety:**
- Normalizes paths (removes //, /./, trailing slashes)
- Ensures requested path is within server root
- Prevents directory traversal attacks (../)
- Returns 403 Forbidden for unsafe paths

**Content Type Detection:**
Supports common file types: HTML, CSS, JavaScript, JSON, images (PNG, JPEG, GIF, SVG), text, PDF, XML, and default application/octet-stream.

**Integration with Server:**
```
Server::handleClientRead()
  │
  ├─▶ RequestParser::consume() → HttpRequest
  │
  └─▶ Server::handleHttpRequest()
        │
        └─▶ RequestHandler::handleRequest()
              │
              ├─▶ Find server config
              ├─▶ Find location block
              ├─▶ Build file path
              ├─▶ Serve file/directory
              └─▶ Return HttpResponse
                    │
                    └─▶ Server::sendAll() → Client
```

---

## Complete Execution Flow

### Phase 1: Initialization

```
┌─────────────────────────────────────────────────────────────┐
│ main()                                                      │
│                                                             │
│  1. Parse command-line arguments                            │
│  2. Set up signal handlers                                  │
│  3. Create ConfigParser                                     │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
        ┌────────────────────────-┐
        │ ConfigParser            │
        │                         │
        │ loadFromFile()          │
        │   ├─ Read config file   │
        │   ├─ Parse server blocks│
        │   └─ Build ServerConfig │
        │                         │
        │ validate()              │
        │   ├─ Check servers      │
        │   ├─ Validate ports     │
        │   └─ Check required     │
        └────────────┬───────────-┘
                     │
                     ▼
        ┌────────────────────────┐
        │ Server                 │
        │                        │
        │ init(ConfigParser)     │
        │   ├─ Get ServerConfig  │
        │   ├─ For each listen:  │
        │   │   ├─ new Socket()  │
        │   │   ├─ bind()        │
        │   │   ├─ listen()      │
        │   │   └─ Add to list   │
        │   └─ Register in poll  │
        └────────────┬───────────┘
                     │
                     ▼
              [Ready to run]
```

### Phase 2: Event Loop

```
┌─────────────────────────────────────────────────────────────┐
│ Server::run()                                               │
│                                                             │
│  while (running_) {                                         │
│                                                             │
│    ┌──────────────────────────────────────┐                 │
│    │ setupPollFds()                       │                 │
│    │   ├─ Clear poll_fds_                 │                 │
│    │   ├─ Add listening sockets (POLLIN)  │                 │
│    │   └─ Add client connections          │                 │
│    │      (POLLIN | POLLOUT)              │                 │
│    └──────────────────────────────────────┘                 │
│                     │                                       │
│                     ▼                                       │
│    ┌──────────────────────────────────────┐                 │
│    │ poll(poll_fds_, timeout)             │                 │
│    │   ├─ Wait for events (1 second)      │                 │
│    │   ├─ Returns:                        │                 │
│    │   │  -1: error                       │                 │
│    │   │   0: timeout                     │                 │
│    │   │  >0: number of events            │                 │
│    │   └─ Sets revents in poll_fds_       │                 │
│    └──────────────────────────────────────┘                 │
│                     │                                       │
│         ┌───────────┴───────────┐                           │
│         │                       │                           │
│         ▼                       ▼                           │
│    [Timeout]              [Events]                          │
│         │                       │                           │
│         │                       ▼                           │
│         │          ┌──────────────────────┐                 │
│         │          │ handlePollEvents()   │                 │
│         │          │                      │                 │
│         │          │ For each pollfd:     │                 │
│         │          │   ├─ Check errors    │                 │
│         │          │   ├─ If listening:   │                 │
│         │          │   │   └─ accept()    │                 │
│         │          │   └─ If client:      │                 │
│         │          │       ├─ POLLIN:     │                 │
│         │          │       │   recv()     │                 │
│         │          │       └─ POLLOUT:    │                 │
│         │          │           send()     │                 │
│         │          └──────────────────────┘                 │
│         │                       │                           │
│         └───────────┬───────────┘                           │
│                     │                                       │
│                     ▼                                       │
│    ┌──────────────────────────────────────┐                 │
│    │ cleanupTimedOutConnections()         │                 │
│    │   ├─ Check each connection           │                 │
│    │   └─ Close timed out connections     │                 │
│    └──────────────────────────────────────┘                 │
│                                                             │
│  }                                                          │
└─────────────────────────────────────────────────────────────┘
```

### Phase 3: New Connection Flow

```
Client connects
      │
      ▼
┌─────────────────────────────────────┐
│ poll() detects POLLIN on listening  │
│ socket                              │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ handlePollEvents()                  │
│   ├─ Identifies listening socket    │
│   └─ Calls acceptNewConnection()    │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ acceptNewConnection(Socket*)        │
│                                     │
│   1. Socket::accept()               │
│      └─ Returns client_fd           │
│                                     │
│   2. fcntl(client_fd, O_NONBLOCK)   │
│      └─ Set non-blocking mode       │
│                                     │
│   3. getpeername(client_fd)         │
│      └─ Get client IP address       │
│                                     │
│   4. new Connection(client_fd, ip)  │
│      └─ Create Connection object    │
│                                     │
│   5. connections_[client_fd] = conn │
│      └─ Store in map                │
└──────────────┬──────────────────────┘
               │
               ▼
      [Connection ready]
      (Will be monitored in
       next setupPollFds())
```

### Phase 4: Client Request Flow

```
Client sends HTTP request
      │
      ▼
┌─────────────────────────────────────┐
│ poll() detects POLLIN on client fd  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ handlePollEvents()                  │
│   ├─ Identifies client socket       │
│   └─ Calls handleClientRead()       │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ handleClientRead(int fd)            │
│                                     │
│   1. Find Connection in map         │
│      └─ conn = connections_[fd]     │
│                                     │
│   2. conn->updateActivity()         │
│      └─ Update timestamp            │
│                                     │
│   3. recv(fd, buffer, size, 0)      │
│      └─ Read request data           │
│                                     │
│   4. Process request                │
│      └─ (Currently simple response) │
│                                     │
│   5. send(fd, response, size, 0)    │
│      └─ Send HTTP response          │
│                                     │
│   6. closeConnection(fd)            │
│      └─ Clean up connection         │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ closeConnection(int fd)             │
│                                     │
│   1. Find Connection in map         │
│                                     │
│   2. delete connections_[fd]        │
│      └─ Connection destructor       │
│         └─ close(fd)                │
│                                     │
│   3. connections_.erase(fd)         │
│      └─ Remove from map             │
└─────────────────────────────────────┘
```

### Phase 5: Shutdown Flow

```
Signal (SIGINT/SIGTERM)
      │
      ▼
┌─────────────────────────────────────┐
│ signalHandler(int sig)              │
│   └─ g_server->stop()               │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ Server::stop()                      │
│                                     │
│   1. running_ = false               │
│      └─ Exit event loop             │
│                                     │
│   2. Delete all Connections         │
│      └─ For each in connections_:   │
│         └─ delete Connection*       │
│            └─ close(fd)             │
│                                     │
│   3. Clear connections_ map         │
│                                     │
│   4. Clear poll_fds_                │
│                                     │
│   5. Delete all Sockets             │
│      └─ For each in listening_:     │
│         └─ delete Socket*           │
│            └─ close(fd)             │
│                                     │
│   6. Clear listening_sockets_       │
└──────────────┬──────────────────────┘
               │
               ▼
         [Program exits]
```

---

## Data Flow Diagrams

### Configuration Flow

```
config/example.conf
      │
      ▼
┌─────────────────────────────────────┐
│ ConfigParser::loadFromFile()        │
│   ├─ Read file                      │
│   ├─ Parse directives               │
│   └─ Build ServerConfig structures  │
└──────────────┬──────────────────────┘
               │
               ▼
    std::vector<ServerConfig>
               │
               ▼
┌─────────────────────────────────────┐
│ Server::init(ConfigParser)          │
│   ├─ Get ServerConfig vector        │
│   ├─ Extract listen directives      │
│   └─ Create Socket objects          │
└──────────────┬──────────────────────┘
               │
               ▼
    std::vector<Socket*>
    (listening_sockets_)
```

### Connection Management Flow

```
New Connection Request
      │
      ▼
┌─────────────────────────────────────┐
│ Socket::accept()                    │
│   └─ Returns: client_fd             │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ new Connection(client_fd, client_ip)│
│   ├─ fd_ = client_fd                │
│   ├─ client_ip_ = client_ip         │
│   └─ last_activity_ = time(NULL)    │
└──────────────┬──────────────────────┘
               │
               ▼
┌────────────────────────────────────-─┐
│ connections_[client_fd] = Connection*│
│   └─ Stored in map                   │
└──────────────┬──────────────────────-┘
               │
               ▼
┌─────────────────────────────────────┐
│ setupPollFds()                      │
│   └─ Add client_fd to poll_fds_     │
│      with POLLIN | POLLOUT          │
└──────────────┬──────────────────────┘
               │
               ▼
      [Monitored by poll()]
```

### Request/Response Flow

```
HTTP Request arrives
      │
      ▼
┌─────────────────────────────────────┐
│ poll() → POLLIN event on client_fd  │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ handleClientRead(client_fd)         │
│                                     │
│   1. Find Connection                │
│      └─ conn = connections_[fd]     │
│                                     │
│   2. Update activity                │
│      └─ conn->updateActivity()      │
│                                     │
│   3. Read request                   │
│      └─ recv(fd, buffer, size)      │
│         └─ Returns: HTTP request    │
│            string                   │
│                                     │
│   4. Process request                │
│      └─ (Parse, route, etc.)        │
│                                     │
│   5. Build response                 │
│      └─ HTTP response string        │
│                                     │
│   6. Send response                  │
│      └─ send(fd, response, size)    │
│                                     │
│   7. Close connection               │
│      └─ closeConnection(fd)         │
└─────────────────────────────────────┘
```

---

## Component Interaction Matrix

| Component | Interacts With | Interaction Type | Purpose |
|-----------|---------------|-----------------|---------|
| main | ConfigParser | Uses | Parse configuration file |
| main | Server | Creates/Controls | Initialize and run server |
| main | Logger | Uses | Log messages |
| Server | ConfigParser | Reads | Get server configurations |
| Server | Socket | Creates/Manages | Create listening sockets |
| Server | Connection | Creates/Manages | Manage client connections |
| Server | Logger | Uses | Log server events |
| Socket | System calls | Uses | socket(), bind(), listen(), accept() |
| Connection | System calls | Uses | close() |
| ConfigParser | File I/O | Uses | Read configuration file |

---

## Event Loop State Machine

```
┌─────────────┐
│   IDLE      │
│             │
│ (No events) │
└──────┬──────┘
       │
       │ setupPollFds()
       ▼
┌─────────────┐
│   POLL      │
│             │
│ (Waiting for│
│  events)    │
└──────┬──────┘
       │
   ┌───┴───┐
   │       │
   ▼       ▼
┌─────-─┐ ┌──────────┐
│TIMEOUT│ │  EVENT   │
│       │ │          │
│(1 sec)│ │(POLLIN/  │
│       │ │ POLLOUT) │
└───┬──-┘ └────┬─────┘
    │          │
    │          │ handlePollEvents()
    │          ▼
    │    ┌──────────┐
    │    │ PROCESS  │
    │    │          │
    │    │(accept/  │
    │    │ read/    │
    │    │ write)   │
    │    └────┬─────┘
    │         │
    └─────────┘
         │
         │ cleanupTimedOutConnections()
         ▼
    ┌─────────────┐
    │   IDLE      │
    │             │
    │ (Loop back) │
    └─────────────┘
```

---

## File Descriptor Management

### Listening Sockets
```
Server::init()
  │
  ├─▶ For each listen directive:
  │     │
  │     ├─▶ Socket::bind() → socket() → fd = 3
  │     ├─▶ Socket::listen() → listen(fd)
  │     └─▶ listening_sockets_.push_back(Socket*)
  │
  └─▶ poll_fds_ contains:
        {fd: 3, events: POLLIN}
        {fd: 4, events: POLLIN}
        ...
```

### Client Connections
```
acceptNewConnection()
  │
  ├─▶ Socket::accept() → accept(fd) → client_fd = 5
  │
  ├─▶ new Connection(client_fd, ip)
  │
  ├─▶ connections_[5] = Connection*
  │
  └─▶ Next setupPollFds() adds:
        {fd: 5, events: POLLIN | POLLOUT}
```

### Poll File Descriptors Structure
```
poll_fds_ vector structure:

Index | fd  | events        | revents (after poll)
------|-----|---------------|-------------------
  0   |  3  | POLLIN        | POLLIN (if event)
  1   |  4  | POLLIN        | 0 (no event)
  2   |  5  | POLLIN|POLLOUT| POLLIN (if event)
  3   |  6  | POLLIN|POLLOUT| 0 (no event)
  ...

After each poll() call:
  - revents is set by kernel
  - Indicates which events occurred
  - Used in handlePollEvents()
```

---

## Memory Management

### Object Lifecycle

**Socket Objects:**
- Created: `Server::init()` → `new Socket()`
- Destroyed: `Server::stop()` → `delete Socket*`
- Lifetime: Entire server runtime

**Connection Objects:**
- Created: `Server::acceptNewConnection()` → `new Connection()`
- Destroyed: `Server::closeConnection()` → `delete Connection*`
- Lifetime: Single connection duration

**File Descriptors:**
- Opened: `socket()`, `accept()`
- Closed: `Connection::~Connection()`, `Socket::~Socket()`
- Managed: Automatically closed in destructors

### Resource Cleanup Order

```
Server::stop()
  │
  ├─▶ 1. Delete all Connection objects
  │     └─ Each destructor closes client_fd
  │
  ├─▶ 2. Clear connections_ map
  │
  ├─▶ 3. Clear poll_fds_ vector
  │
  └─▶ 4. Delete all Socket objects
        └─ Each destructor closes listening fd
```

---

## Error Handling Flow

```
┌─────────────────────────────────────┐
│ System Call Error                   │
│ (socket, bind, listen, accept, etc.)│
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ Check errno                         │
│                                     │
│   ├─ EAGAIN / EWOULDBLOCK           │
│   │   └─ Non-blocking operation     │
│   │      would block (normal)       │
│   │      → Continue                 │
│   │                                 │
│   ├─ EINTR                          │
│   │   └─ Interrupted by signal      │
│   │      → Retry                    │
│   │                                 │
│   └─ Other errors                   │
│       └─ Log error                  │
│          → Close connection         │
│          → Clean up resources       │
└─────────────────────────────────────┘
```

---

## Summary

The webserv architecture follows an **event-driven, single-threaded** model:

1. **ConfigParser** parses configuration files and provides structured data
2. **Socket** handles low-level socket operations
3. **Connection** represents individual client connections
4. **Server** orchestrates everything through an event loop using `poll()`
5. **Logger** provides centralized logging

**Key Design Patterns:**
- **Event-driven architecture**: Single thread handles multiple connections
- **Non-blocking I/O**: All sockets operate in non-blocking mode
- **Resource management**: RAII pattern for automatic cleanup
- **Singleton pattern**: Logger uses singleton for global access

**Data Flow:**
```
Config File → ConfigParser → ServerConfig → Server → Socket → Connection → Client
```

**Event Flow:**
```
poll() → Events → handlePollEvents() → accept/read/write → Response → Client
```

This architecture allows the server to handle multiple concurrent connections efficiently in a single thread, making it suitable for high-concurrency scenarios while maintaining simplicity and resource efficiency.
