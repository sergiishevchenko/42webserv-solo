# CGI Support Documentation

## Overview

The webserv project implements CGI/1.1 support, allowing the execution of external scripts (Python, PHP, etc.) to generate dynamic content. The implementation follows the CGI specification and integrates seamlessly with the non-blocking event loop architecture.

## Configuration

CGI is configured at the location level using the `cgi_pass` directive. This directive maps file extensions to their respective interpreter executables.

### Configuration Syntax

```nginx
location /cgi {
    cgi_pass .py /usr/bin/python3;
    cgi_pass .php /usr/bin/php;
    methods GET POST;
}
```

### Configuration Parameters

- **`cgi_pass`**: Maps a file extension (e.g., `.py`, `.php`) to an interpreter executable path
- **`methods`**: HTTP methods allowed for CGI execution (typically GET and POST)
- Multiple `cgi_pass` directives can be specified for different extensions

### Example Configuration

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    
    location /cgi {
        cgi_pass .py /usr/bin/python3;
        cgi_pass .php /usr/bin/php;
        methods GET POST;
    }
}
```

## How CGI Works

### Request Flow

1. **Request Reception**: The server receives an HTTP request for a resource
2. **Path Resolution**: The server resolves the file path based on the location and root configuration
3. **CGI Detection**: The server checks if the file extension matches any configured CGI extension
4. **Script Validation**: The server verifies that:
   - The script file exists
   - The script is readable
   - The interpreter executable exists and is executable
5. **CGI Execution**: If all checks pass, the CGI script is executed
6. **Response Processing**: The CGI output is parsed and converted to an HTTP response

### Execution Model

The CGI execution uses a fork-exec model:

1. **Fork**: The server forks a child process
2. **Environment Setup**: CGI environment variables are set
3. **Pipe Creation**: Pipes are created for stdin (request body) and stdout (CGI output)
4. **Process Execution**: The interpreter is executed with `execve()` in the child process
5. **Communication**: 
   - Request body is written to the CGI process stdin
   - CGI output is read from the CGI process stdout
6. **Cleanup**: The server waits for the process to complete and cleans up

### Working Directory

The CGI script is executed with the working directory set to the server's root directory (as specified in the configuration). This ensures that relative paths in scripts resolve correctly.

## Environment Variables

The server sets the following CGI environment variables:

### Request Variables

- **`REQUEST_METHOD`**: HTTP method (GET, POST, etc.)
- **`REQUEST_URI`**: Original request URI including query string
- **`QUERY_STRING`**: Query string (after `?`)

### Script Variables

- **`SCRIPT_NAME`**: Path to the script as requested
- **`SCRIPT_FILENAME`**: Absolute path to the script file
- **`PATH_INFO`**: Additional path information after the script name
- **`PATH_TRANSLATED`**: Translated PATH_INFO to file system path

### Server Variables

- **`SERVER_PROTOCOL`**: HTTP protocol version (e.g., "HTTP/1.1")
- **`SERVER_SOFTWARE`**: Server software name and version ("webserv/1.0")
- **`SERVER_NAME`**: Server hostname from Host header
- **`SERVER_PORT`**: Server port number
- **`GATEWAY_INTERFACE`**: CGI interface version ("CGI/1.1")
- **`DOCUMENT_ROOT`**: Document root directory

### Content Variables

- **`CONTENT_LENGTH`**: Length of request body in bytes
- **`CONTENT_TYPE`**: Content type of request body (if present)

### Client Variables

- **`REMOTE_ADDR`**: Client IP address

### HTTP Headers

All HTTP request headers are converted to environment variables with the prefix `HTTP_`:
- Header names are converted to uppercase
- Hyphens (`-`) are replaced with underscores (`_`)
- Example: `Content-Type` → `HTTP_CONTENT_TYPE`

## Request Body Handling

### POST Requests

For POST requests, the request body is:
1. Read from the HTTP request
2. Validated against `client_max_body_size` limit
3. Written to the CGI process stdin via pipe
4. The CGI script can read the body from stdin

### Content Length

The `CONTENT_LENGTH` environment variable is always set, even if the body is empty (set to "0"). The `CONTENT_TYPE` variable is only set if the request includes a Content-Type header.

## CGI Response Format

### Header Parsing

The CGI script must output headers followed by a blank line, then the body. The server parses:

- **Status Header**: `Status: 200 OK` - Sets HTTP status code and reason phrase
- **Content-Type Header**: `Content-Type: text/html` - Sets response content type
- **Location Header**: `Location: /path` - Sets redirect location (automatically sets 302 status)
- **Content-Length Header**: `Content-Length: 1024` - Sets response body length
- **Other Headers**: All other headers are passed through to the HTTP response

### Response Parsing

The server:
1. Reads all output from the CGI process
2. Separates headers from body (by blank line: `\r\n\r\n` or `\n\n`)
3. Parses individual headers
4. Converts to HTTP response format
5. Returns the response to the client

### Default Status

If no `Status` header is present, the server defaults to HTTP 200 OK.

## Timeout and Process Management

### Timeout

- **Execution Timeout**: 30 seconds
- The timeout is checked during output reading
- If the timeout is exceeded, the CGI process is terminated

### Process Termination

If a CGI process exceeds the timeout or hangs:
1. **SIGTERM**: The server sends SIGTERM to allow graceful shutdown
2. **Wait**: The server waits 1 second for the process to exit
3. **SIGKILL**: If still running, the server sends SIGKILL to force termination
4. **Cleanup**: All file descriptors are closed and resources are freed

### Error Handling

The server handles various error conditions:

- **404 Not Found**: Script file doesn't exist
- **403 Forbidden**: Script file is not readable
- **500 Internal Server Error**: Interpreter not found or not executable
- **502 Bad Gateway**: CGI script execution failed or returned error
- **504 Gateway Timeout**: CGI script execution exceeded timeout

## Security Considerations

### Path Safety

- Scripts must be within the document root
- Path traversal attacks are prevented by path normalization
- Only configured extensions are processed as CGI

### File Permissions

- Scripts must be readable
- Interpreter executables must be executable
- The server validates permissions before execution

### Process Isolation

- CGI scripts run in separate processes
- Each request spawns a new process
- Processes are terminated after execution or timeout

## Example: Python CGI Script

### Script Location

Place the script in your document root under the CGI location:

```
www/
└── cgi/
    └── hello.py
```

### Python Script Example

```python
#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r")
print("\r")

method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
query = os.environ.get('QUERY_STRING', '')

print("<html>")
print("<head><title>CGI Test</title></head>")
print("<body>")
print("<h1>CGI Script Executed Successfully!</h1>")
print("<p>Method: " + method + "</p>")
print("<p>Query String: " + query + "</p>")
print("</body>")
print("</html>")
```

### Make Script Executable

```bash
chmod +x www/cgi/hello.py
```

### Request Example

```bash
curl http://127.0.0.1:8080/cgi/hello.py?name=world
```

## Implementation Details

### File Structure

- **Header**: `include/CgiHandler.hpp`
- **Implementation**: `src/CgiHandler.cpp`
- **Integration**: `src/RequestHandler.cpp`

### Key Classes and Functions

- **`CgiHandler`**: Main CGI handler class
  - `shouldHandleByCgi()`: Checks if request should be handled by CGI
  - `executeCgi()`: Executes the CGI script
  - `buildCgiEnv()`: Builds environment variables
  - `parseCgiResponse()`: Parses CGI output

### Pipes and File Descriptors

- **stdin_pipe[2]**: Pipe for sending request body to CGI
- **stdout_pipe[2]**: Pipe for reading CGI output
- Both pipes are properly closed after use
- Child process duplicates file descriptors to stdin/stdout

### Synchronous Execution

Currently, CGI execution is synchronous:
- The server waits for the CGI process to complete
- This is acceptable for most use cases
- Future improvements could add asynchronous CGI support

## Limitations

1. **One Request at a Time**: Each CGI request blocks until completion
2. **Timeout**: Maximum 30 seconds execution time
3. **No FastCGI**: Only standard CGI/1.1 is supported
4. **Memory**: Request body is loaded entirely into memory

## Testing

For detailed information about testing CGI scripts, including test scripts and automated testing, see the [CGI Test Scripts README](../../www/cgi/README.md).

## References

- [CGI/1.1 Specification (RFC 3875)](https://tools.ietf.org/html/rfc3875)
- [HTTP/1.1 Specification (RFC 7230-7237)](https://tools.ietf.org/html/rfc7230)
