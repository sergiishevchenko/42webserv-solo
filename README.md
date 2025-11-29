# webserv

HTTP/1.1 web server developed individually as part of the 42 project. Implements a non-blocking event-driven architecture with support for multiple clients, CGI scripts, redirects, and flexible configuration.

## Features

### Core Functionality

- ✅ **HTTP/1.1 Protocol** - Full request/response handling
- ✅ **Non-blocking I/O** - Single event loop using `poll()` for efficient concurrent connections
- ✅ **Multiple Server Blocks** - Listen on multiple ports with different configurations
- ✅ **Virtual Hosts** - Support for multiple server configurations

### HTTP Methods

- ✅ **GET** - Serve static files and handle directory listings
- ✅ **POST** - File uploads with size limits
- ✅ **DELETE** - File deletion with proper status codes

### Static File Serving

- ✅ **Static Files** - Serve files from document root
- ✅ **Directory Index** - Configurable index files
- ✅ **Autoindex** - Automatic directory listing generation
- ✅ **Error Pages** - Custom error pages for 4xx and 5xx errors

### Advanced Features

- ✅ **CGI Support** - Execute Python scripts and other interpreters via CGI/1.1
- ✅ **Redirects** - HTTP 301/302 redirects at location level
- ✅ **File Uploads** - POST requests with upload store configuration
- ✅ **Keep-Alive** - HTTP connection reuse
- ✅ **Chunked Encoding** - Support for Transfer-Encoding: chunked

### Configuration

- ✅ **Nginx-inspired Syntax** - Familiar configuration format
- ✅ **Flexible Location Blocks** - Path-based routing with method restrictions
- ✅ **Multiple Listen Directives** - Bind to multiple interfaces/ports
- ✅ **Client Limits** - Configurable body size limits and timeouts

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

### Example

```bash
./webserv config/example.conf
```

The server will start listening on the configured ports and log startup information.

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

See example configurations in the `config/` directory:
- `config/example.conf` - Basic configuration
- `config/test_valid.conf` - Full featured example
- `config/test_cgi.conf` - CGI configuration
- `config/test_multiport.conf` - Multi-port setup
- `config/test_redirects.conf` - Redirect examples

## Supported Features

| Feature | Status | Description |
|---------|--------|-------------|
| HTTP/1.1 | ✅ | Full protocol support |
| GET Method | ✅ | Static file serving |
| POST Method | ✅ | File uploads |
| DELETE Method | ✅ | File deletion |
| Keep-Alive | ✅ | Connection reuse |
| Chunked Encoding | ✅ | Transfer-Encoding support |
| CGI/1.1 | ✅ | Python and other interpreters |
| Redirects | ✅ | 301/302 redirects |
| Autoindex | ✅ | Directory listings |
| Error Pages | ✅ | Custom error pages |
| Multi-port | ✅ | Multiple server blocks |
| Virtual Hosts | ✅ | Host-based routing |

## Testing

### Configuration Parser Tests

```bash
./scripts/test_config.sh
```

### CGI Testing

```bash
# Start server with CGI config
./webserv config/test_cgi.conf

# Run automated CGI tests
./scripts/test_cgi.sh
```

### Redirect Testing

```bash
# Start server with redirect config
./webserv config/test_redirects.conf

# Run automated redirect tests
./scripts/test_redirects.sh config/test_redirects.conf 8080

# Manual redirect test
curl -v http://127.0.0.1:8080/redirect
```

For detailed redirect testing instructions, see [TESTING_REDIRECTS.md](docs/TESTING_REDIRECTS.md)

### Manual Testing

```bash
# Start the server
./webserv config/test_valid.conf

# Test static files
curl http://127.0.0.1:8080/

# Test CGI
curl http://127.0.0.1:8080/cgi/hello.py

# Test redirect
curl -v http://127.0.0.1:8080/redirect

# Test POST upload
curl -X POST -d "test=data" http://127.0.0.1:8080/uploads/test.txt
```

For comprehensive testing instructions, see [TESTING.md](docs/TESTING.md)

## CGI Scripts

The project includes test CGI scripts in `www/cgi/`:

- `hello.py` - Basic "Hello World" example
- `env.py` - Environment variables display
- `query.py` - Query string parser
- `post.py` - POST request handler
- `json.py` - JSON response example
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

## Documentation

### Feature Documentation

- [CGI.md](docs/CGI.md) - CGI/1.1 implementation details
- [REDIRECTS.md](docs/REDIRECTS.md) - HTTP redirects documentation
- [TESTING_REDIRECTS.md](docs/TESTING_REDIRECTS.md) - Redirect testing guide
- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - System architecture overview

### Reference Documentation

- [STRUCTURES.md](docs/STRUCTURES.md) - Data structures and classes
- [TESTING.md](docs/TESTING.md) - Testing guide
- [FORMATTING.md](docs/FORMATTING.md) - Code formatting guidelines

### Technical Documentation

- [SOCKETS.md](docs/SOCKETS.md) - Socket implementation
- [POLL.md](docs/POLL.md) - Event loop implementation
- [FILE_DESCRIPTORS.md](docs/FILE_DESCRIPTORS.md) - File descriptor management

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

## Requirements

- C++98 compatible compiler (clang++ or g++)
- POSIX-compatible operating system (macOS, Linux)
- Python 3 (for CGI scripts, optional)

## Known Limitations

1. **Single-threaded** - All connections handled in one event loop
2. **CGI Timeout** - Maximum 30 seconds execution time for CGI scripts
3. **Synchronous CGI** - CGI requests block until completion
4. **No FastCGI** - Only standard CGI/1.1 supported
5. **Limited Redirect Codes** - Only 301 and 302 supported

## License

This project is part of the 42 Lausanne curriculum.

## Author

Developed as an individual project for 42 School.
