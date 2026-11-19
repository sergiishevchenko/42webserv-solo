# webserv

HTTP/1.1 web server developed individually as part of the 42 project. Implements a non-blocking event-driven architecture with support for multiple clients, CGI scripts, redirects, and flexible configuration.

## Features

### HTTP Protocol
- ✅ **HTTP/1.1** - Full protocol support with proper headers
- ✅ **HTTP/1.0** - Backward compatibility
- ✅ **Keep-Alive** - Connection reuse for multiple requests
- ✅ **Chunked Encoding** - Transfer-Encoding: chunked support

### HTTP Methods
- ✅ **GET** - Static file serving with Range support
- ✅ **POST** - File uploads with size limits
- ✅ **DELETE** - File deletion with proper status codes

### Request Handling
- ✅ **Query Strings** - URL parameter parsing
- ✅ **Headers** - Full header parsing and validation
- ✅ **Body Parsing** - Content-Length and chunked body support
- ✅ **Path Normalization** - Security against directory traversal

### Response Features
- ✅ **Status Codes** - Complete HTTP status code support (2xx, 3xx, 4xx, 5xx)
- ✅ **Custom Error Pages** - Configurable error pages per status code
- ✅ **Content-Type** - Automatic MIME type detection

### File Serving
- ✅ **Static Files** - Binary and text file serving
- ✅ **Directory Index** - Configurable index files
- ✅ **Autoindex** - Automatic directory listing generation

### Advanced Features
- ✅ **CGI/1.1** - Python and other interpreters via fork/execve
- ✅ **Redirects** - HTTP 301/302 redirects at location level
- ✅ **File Uploads** - POST requests with upload store configuration
- ✅ **Multi-port** - Multiple server blocks on different ports
- ✅ **Virtual Hosts** - Host-based routing via Host header

### Configuration
- ✅ **Nginx Syntax** - Familiar configuration format
- ✅ **Location Blocks** - Path-based routing with method restrictions
- ✅ **Multiple Listen** - Bind to multiple interfaces/ports
- ✅ **Client Limits** - Configurable body size limits and timeouts

### Performance
- ✅ **Non-blocking I/O** - Single event loop using poll()
- ✅ **Concurrent Connections** - Handle multiple clients simultaneously
- ✅ **Connection Timeouts** - Automatic cleanup of idle connections
- ✅ **Request Timeouts** - Protection against slow requests

## Requirements

- C++98 compatible compiler (clang++ or g++)
- POSIX-compatible operating system (macOS, Linux)
- Python 3 (for CGI scripts, optional)

## Project Structure

```
.
├── src/           # Source files
├── include/       # Header files
├── config/        # Configuration examples
├── www/           # Document root
│   ├── cgi/       # CGI test scripts
│   └── uploads/   # Upload directory
├── docs/          # Documentation
├── scripts/       # Utility scripts
└── Makefile       # Build configuration
```

## Architecture

### Component Overview

```mermaid
flowchart TB
    subgraph Entry[" "]
        MAIN["main.cpp"]
    end

    MAIN --> CFG["ConfigParser"]
    MAIN --> SRV["Server"]

    CFG -->|"ServerConfig"| SRV

    SRV --> SOCK["Socket"]
    SRV --> CONN["Connection"]
    SRV --> LOG["Logger"]

    CONN --> RP["RequestParser"]
    RP --> RH["RequestHandler"]
    RH --> CGI["CgiHandler"]
    RH --> RESP["HttpResponse"]

    SOCK -.->|"bind / listen / accept"| NET["Network"]
    SRV -.->|"poll() event loop"| NET

    style MAIN fill:#1a1a2e,stroke:#e94560,color:#eee
    style SRV fill:#16213e,stroke:#0f3460,color:#eee
    style CFG fill:#0f3460,stroke:#533483,color:#eee
    style RH fill:#533483,stroke:#e94560,color:#eee
    style CGI fill:#e94560,stroke:#1a1a2e,color:#fff
    style RESP fill:#0f3460,stroke:#533483,color:#eee
```

### Event Loop

```mermaid
flowchart LR
    A["Start"] --> B["poll()"]
    B --> C{"Events?"}
    C -->|"none"| D["Check timeouts"]
    D --> B
    C -->|"POLLIN on listen"| E["accept()"]
    E --> B
    C -->|"POLLIN on client"| F["recv() + parse"]
    F --> G{"Request ready?"}
    G -->|no| B
    G -->|yes| H["RequestHandler"]
    H --> I["Queue response"]
    I --> B
    C -->|"POLLOUT"| J["send()"]
    J --> K{"Keep-Alive?"}
    K -->|yes| B
    K -->|no| L["close()"]
    L --> B

    style A fill:#1a1a2e,stroke:#e94560,color:#eee
    style B fill:#16213e,stroke:#0f3460,color:#eee
    style H fill:#533483,stroke:#e94560,color:#eee
    style J fill:#0f3460,stroke:#533483,color:#eee
```

### Request Lifecycle

```mermaid
sequenceDiagram
    participant C as Client
    participant S as Server
    participant P as RequestParser
    participant H as RequestHandler
    participant R as HttpResponse

    C->>S: TCP connect
    S->>S: accept() + add to poll
    C->>S: HTTP request bytes
    S->>P: feed buffer
    P->>P: parse start-line, headers, body
    P-->>S: HttpRequest ready
    S->>H: handle(request)
    alt redirect
        H->>R: 301 / 302 + Location
    else CGI
        H->>H: fork / execve CGI
        H->>R: CGI stdout as response
    else static / upload / delete
        H->>R: file or status body
    end
    R-->>S: serialized response
    S->>C: send()
    alt Keep-Alive
        S->>S: wait for next request
    else
        S->>C: close connection
    end
```

### Request Routing

```mermaid
flowchart TD
    REQ["Incoming HttpRequest"] --> MATCH["Match server + location"]
    MATCH --> METH{"Method allowed?"}
    METH -->|no| E405["405 Method Not Allowed"]
    METH -->|yes| REDIR{"return / redirect?"}
    REDIR -->|yes| R3xx["301 / 302 Location"]
    REDIR -->|no| CGIQ{"cgi_pass match?"}
    CGIQ -->|yes| CGIR["CgiHandler"]
    CGIQ -->|no| METH2{"Method"}
    METH2 -->|GET| GET["Static file / index / autoindex"]
    METH2 -->|POST| POST["Upload to upload_store"]
    METH2 -->|DELETE| DEL["Delete file"]
    GET --> OK["HttpResponse"]
    POST --> OK
    DEL --> OK
    CGIR --> OK
    R3xx --> OK
    E405 --> OK

    style REQ fill:#1a1a2e,stroke:#e94560,color:#eee
    style OK fill:#0f3460,stroke:#533483,color:#eee
    style CGIR fill:#e94560,stroke:#1a1a2e,color:#fff
    style R3xx fill:#533483,stroke:#e94560,color:#eee
```

### CGI Pipeline

```mermaid
flowchart LR
    subgraph Parent["Server process"]
        H["CgiHandler"]
        ENV["Build CGI env"]
        IN["stdin pipe"]
        OUT["stdout pipe"]
    end

    subgraph Child["CGI process"]
        EXEC["execve interpreter + script"]
    end

    H --> ENV
    H -->|"fork()"| Child
    H -->|"request body"| IN
    IN --> EXEC
    EXEC --> OUT
    OUT -->|"parse CGI headers + body"| RESP["HttpResponse"]

    style H fill:#16213e,stroke:#0f3460,color:#eee
    style EXEC fill:#e94560,stroke:#1a1a2e,color:#fff
    style RESP fill:#0f3460,stroke:#533483,color:#eee
```

## Building

```bash
make
```

The build system creates the `webserv` executable with the following flags:
- `-Wall -Wextra -Werror`
- `-std=c++98`

### Build Targets

- `make` or `make all` - Build the project
- `make clean` - Remove object files
- `make fclean` - Remove object files and binary
- `make re` - Rebuild from scratch
- `make format` - Format source code (requires clang-format)
- `make lint` - Run static analysis (requires clang-tidy)

## Running

### Basic Usage

```bash
./webserv <configuration_file>
```

### Launch Examples

**Basic server:**
```bash
./webserv config/example.conf
```

**Full-featured server:**
```bash
./webserv config/test_valid.conf
```

**Multi-port server:**
```bash
./webserv config/test_multiport.conf
```

**CGI-enabled server:**
```bash
./webserv config/test_cgi.conf
```

The server will start listening on the configured ports and log startup information. Press `Ctrl+C` to stop the server gracefully.

### Interactive Demo

Once the server is running, access the interactive demonstration page:

```bash
# Start server
./webserv config/test_valid.conf

# Open in browser
# http://127.0.0.1:8080/demo.html
```

Or simply visit the main page and click the "🚀 Try Interactive Demo" link.

The demo page provides an interactive interface to test all features:
- Static file serving (HTML, text, JSON, binary)
- CGI script execution with query parameters
- File uploads via web form
- File deletion
- HTTP redirects
- Error handling (404, 405)
- Directory listings

#### Demo Screenshots

<div align="center">

![Demo Screenshot 1](images/1.png)

![Demo Screenshot 2](images/2.png)

![Demo Screenshot 3](images/3.png)

![Demo Screenshot 4](images/4.png)

![Demo Screenshot 5](images/5.png)

</div>

## Configuration

### Basic Server Configuration

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    index index.html;
    client_max_body_size 10485760;
    
    error_page 404 /404.html;
    
    location / {
        methods GET;
    }
}
```

### Location Blocks

```nginx
location /uploads {
    methods GET POST;
    upload_store www/uploads;
    autoindex on;
}

location /cgi {
    cgi_pass .py /usr/bin/python3;
    methods GET POST;
}

location /redirect {
    return 302 /index.html;
}
```

### Multiple Server Blocks

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
}

server {
    listen 127.0.0.1:8081;
    root www2;
}
```

### Example Configurations

See example configurations in the `config/` directory:
- `config/example.conf` - Basic configuration
- `config/test_valid.conf` - Full featured example
- `config/test_cgi.conf` - CGI configuration
- `config/test_multiport.conf` - Multi-port setup
- `config/test_redirects.conf` - Redirect examples

## Testing

### Automated Test Suites

**Full test suite:**
```bash
# Starts server, runs all tests, stops server
./scripts/test_all.sh
```

This includes:
- Static file serving tests
- CGI execution tests
- File upload scenarios
- File deletion tests
- Redirect tests
- Large file handling
- Parallel connections
- Chunked encoding
- Error handling (404, 405, 413)

**Python comprehensive test:**
```bash
# Start the server in one terminal
./webserv config/test_valid.conf

# Run comprehensive tests in another terminal
./scripts/test_comprehensive.py
```

The Python test suite includes:
- Basic GET requests
- Error handling (404, 405, 413)
- Keep-Alive connections
- Large body handling (1MB+)
- Chunked transfer encoding
- Parallel connections (10+ concurrent)
- CGI execution
- Connection timeouts
- Stress testing (100+ requests)

### Specialized Tests

**Upload scenarios:**
```bash
./scripts/test_upload.sh
```

Tests:
- Small file uploads (< 1KB)
- Medium file uploads (100KB)
- Large file uploads (1MB)
- Upload size limit enforcement (413)
- Multipart form uploads
- Upload directory listing

**Stress test:**
```bash
# Default: 60 seconds, 20 concurrent connections
./scripts/test_stress.sh

# Custom: 120 seconds, 50 concurrent connections
./scripts/test_stress.sh 120 50
```

**Configuration parser:**
```bash
./scripts/test_config.sh
```

**CGI testing:**
```bash
# Start server with CGI config
./webserv config/test_cgi.conf

# Run automated CGI tests
./scripts/test_cgi.sh
```

**Redirect testing:**
```bash
# Start server with redirect config
./webserv config/test_redirects.conf

# Run automated redirect tests
./scripts/test_redirects.sh config/test_redirects.conf 8080

# Manual redirect test
curl -v http://127.0.0.1:8080/redirect
```

### Manual Testing

```bash
# Start the server
./webserv config/test_valid.conf

# Test static files
curl http://127.0.0.1:8080/

# Test demo page
curl http://127.0.0.1:8080/demo.html

# Test CGI
curl http://127.0.0.1:8080/cgi/hello.py

# Test redirect
curl -v http://127.0.0.1:8080/redirect

# Test POST upload
curl -X POST -d "test=data" http://127.0.0.1:8080/uploads/test.txt

# Test DELETE
curl -X DELETE http://127.0.0.1:8080/uploads/test.txt

# Test keep-alive
curl -v --keepalive-time 2 http://127.0.0.1:8080/

# Test chunked encoding
echo -e "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n" | nc 127.0.0.1 8080
```

### Testing Documentation

For detailed testing instructions, see:
- [TESTING.md](docs/TESTING.md) - General testing guide
- [TESTING_REDIRECTS.md](docs/TESTING_REDIRECTS.md) - Redirect testing guide
- [COMPREHENSIVE_TESTING.md](docs/COMPREHENSIVE_TESTING.md) - Comprehensive testing documentation

## CGI Scripts

The project includes test CGI scripts in `www/cgi/`:

- `hello.py` - Basic "Hello World" example
- `env.py` - Environment variables display
- `query.py` - Query string parser
- `post.py` - POST request handler
- `json_response.py` - JSON response example
- `status.py` - Custom status codes
- `redirect.py` - CGI redirect example

See [www/cgi/README.md](www/cgi/README.md) for details on using and testing CGI scripts.

## Code Quality

### Formatting

Format all source files:
```bash
make format
```

### Linting

Run static analysis:
```bash
make lint
```

For more details, see [FORMATTING.md](docs/FORMATTING.md)

## License

This project is part of the 42 Lausanne curriculum.
