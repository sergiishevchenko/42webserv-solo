# Testing Guide

This document describes how to test the webserv project, including configuration parsing, server functionality, and network operations.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building the Project](#building-the-project)
3. [Configuration Testing](#configuration-testing)
4. [Server Testing](#server-testing)
5. [Network Testing](#network-testing)
6. [Connection Testing](#connection-testing)
7. [Timeout Testing](#timeout-testing)
8. [Multiple Ports Testing](#multiple-ports-testing)
9. [Troubleshooting](#troubleshooting)

---

## Prerequisites

Before testing, ensure you have:

- **Compiler**: C++ compiler supporting C++98 (g++, clang++)
- **Build tools**: `make`
- **Testing tools**: `curl`, `telnet`, or `nc` (netcat)
- **Permissions**: Ability to bind to ports (may require `sudo` for ports < 1024)

---

## Building the Project

### Clean Build

```bash
# Clean previous build
make fclean

# Build the project
make

# Verify the binary was created
ls -la webserv
```

**Expected Output:**
```
╔═══════════════════════════════════════════════════════╗
║                                                       ║
║          42webserv - Building Project                 ║
║                                                       ║
╚═══════════════════════════════════════════════════════╝

Platform: Darwin
Compiler: c++
Flags: -Wall -Wextra -Werror -std=c++98
Source files: 6
Target: webserv

→ Compiling main.cpp...
✓ main.cpp → main.o
→ Compiling ConfigParser.cpp...
✓ ConfigParser.cpp → ConfigParser.o
...

╔══════════════════════════════════════════╗
║  Build completed successfully!           ║
╚══════════════════════════════════════════╝
Binary: webserv
```

### Rebuild

```bash
make re
```

---

## Configuration Testing

### Test Valid Configuration

```bash
# Test with valid configuration
./webserv config/test_valid.conf
```

**Expected Output:**
```
[2025-11-09 21:30:05] [INFO] webserv: Configuration loaded successfully
[2025-11-09 21:30:05] [INFO] Found 1 server block(s)

--- Server block 1 ---
Listen: 127.0.0.1:8080, 0.0.0.0:8081
Root: www
Index: index.html
Client max body size: 10485760 bytes
Error pages: 404 -> /404.html, 500 -> /500.html
Locations: 3
  [/uploads] methods: GET POST | upload_store: www/uploads | autoindex: on | 
  [/cgi] methods: GET POST | cgi_pass: .php -> /usr/bin/php, .py -> /usr/bin/python
  [/redirect] redirect: /index.html | 

[2025-11-09 21:30:05] [INFO] Listening on 127.0.0.1:8080
[2025-11-09 21:30:05] [INFO] Listening on 0.0.0.0:8081
[2025-11-09 21:30:05] [INFO] Server started. Waiting for connections...
```

### Test Invalid Configurations

```bash
# Test invalid configuration files
./webserv config/test_invalid1.conf
./webserv config/test_invalid2.conf
./webserv config/test_invalid3.conf
```

**Expected Behavior:**
- Server should exit with error message
- Error should be descriptive and indicate the problem

### Run Configuration Test Script

```bash
# Run the configuration test script
./test_config.sh
```

---

## Server Testing

### Start the Server

```bash
# Start server with valid configuration
./webserv config/test_valid.conf
```

The server will:
1. Parse the configuration file
2. Display parsed configuration
3. Create listening sockets for each `listen` directive
4. Enter the event loop and wait for connections

### Stop the Server

Press `Ctrl+C` to send `SIGINT` signal, which will:
1. Call the signal handler
2. Stop the event loop
3. Close all connections
4. Clean up resources
5. Exit gracefully

---

## Network Testing

### Test Basic Connection with curl

```bash
# In terminal 1: Start the server
./webserv config/test_valid.conf

# In terminal 2: Send HTTP request
curl -v http://127.0.0.1:8080/
```

**Expected Output:**
```
* Connected to 127.0.0.1 (127.0.0.1) port 8080
> GET / HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/8.7.1
> Accept: */*
> 
< HTTP/1.1 200 OK
< Content-Type: text/plain
< Content-Length: 13
< Connection: close
< 
Hello, World!
```

**Server Logs:**
```
[2025-11-09 21:30:30] [INFO] New connection from 127.0.0.1 (fd: 4)
[2025-11-09 21:30:30] [DEBUG] Received 78 bytes from fd 4
[2025-11-09 21:30:30] [DEBUG] Sent 102 bytes to fd 4
[2025-11-09 21:30:30] [DEBUG] Connection closed (fd: 4)
```

### Test Multiple Ports

```bash
# Test port 8080 (127.0.0.1)
curl -v http://127.0.0.1:8080/

# Test port 8081 (0.0.0.0 - all interfaces)
curl -v http://127.0.0.1:8081/
curl -v http://localhost:8081/
```

**Expected Behavior:**
- Both ports should accept connections
- Both should respond with "Hello, World!"

### Test with netcat (nc)

```bash
# Connect to server
nc 127.0.0.1 8080

# Send HTTP request manually
GET / HTTP/1.1
Host: 127.0.0.1:8080


```

**Expected Output:**
```
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 13
Connection: close

Hello, World!
```

### Test with telnet

```bash
# Connect to server
telnet 127.0.0.1 8080

# Send HTTP request
GET / HTTP/1.1
Host: 127.0.0.1:8080


```

---

## Connection Testing

### Test Multiple Simultaneous Connections

```bash
# Start server
./webserv config/test_valid.conf

# In another terminal: Send multiple requests
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8081/ &
wait
```

**Expected Behavior:**
- Server should handle multiple connections simultaneously
- Each connection should receive a response
- Connections should be closed after response

### Test Connection Refusal

```bash
# Stop the server, then try to connect
curl http://127.0.0.1:8080/

# Expected: Connection refused error
```

### Test Invalid Port

```bash
# Try to connect to non-existent port
curl http://127.0.0.1:9999/

# Expected: Connection refused error
```

---

## Timeout Testing

### Test Connection Timeout

The server has a default connection timeout of 60 seconds. To test:

```bash
# Start server
./webserv config/test_valid.conf

# Connect but don't send data
nc 127.0.0.1 8080

# Wait 60+ seconds without sending data
# Expected: Connection should be closed by server
```

**Server Logs:**
```
[2025-11-09 21:35:00] [INFO] Closing timed out connection (fd: 4)
[2025-11-09 21:35:00] [DEBUG] Connection closed (fd: 4)
```

### Test Activity Update

```bash
# Connect and send data periodically
nc 127.0.0.1 8080

# Send data every 30 seconds to keep connection alive
GET / HTTP/1.1
Host: 127.0.0.1:8080


# Wait 30 seconds, then send again
GET / HTTP/1.1
Host: 127.0.0.1:8080


```

**Expected Behavior:**
- Connection should remain alive as long as data is sent regularly
- Activity timestamp is updated on each read/write operation

---

## Multiple Ports Testing

### Test Different Interfaces

```bash
# Start server with configuration that listens on multiple interfaces
./webserv config/test_valid.conf

# Test localhost interface
curl http://127.0.0.1:8080/

# Test all interfaces (0.0.0.0)
curl http://127.0.0.1:8081/
curl http://localhost:8081/

# If accessible from network, test with external IP
curl http://<external-ip>:8081/
```

### Test Port Availability

```bash
# Check if ports are listening
netstat -an | grep LISTEN | grep -E "8080|8081"

# On macOS
lsof -i -P | grep LISTEN | grep -E "8080|8081"

# On Linux
ss -tlnp | grep -E "8080|8081"
```

**Expected Output:**
```
tcp4       0      0  127.0.0.1.8080         *.*                    LISTEN
tcp4       0      0  *.8081                 *.*                    LISTEN
```

---

## Stress Testing

### Test Multiple Rapid Connections

```bash
# Send 10 rapid requests
for i in {1..10}; do
    curl -s http://127.0.0.1:8080/ > /dev/null &
done
wait

# Check server logs for all connections
```

### Test Concurrent Connections

```bash
# Use Apache Bench (if available)
ab -n 100 -c 10 http://127.0.0.1:8080/

# Or use a simple loop
for i in {1..50}; do
    curl -s http://127.0.0.1:8080/ &
done
wait
```

**Expected Behavior:**
- Server should handle all connections
- No crashes or errors
- All connections should receive responses

---

## Error Testing

### Test Invalid Configuration File

```bash
# Test with non-existent file
./webserv config/nonexistent.conf

# Expected: Error message about file not found
```

### Test Missing Configuration Argument

```bash
# Run without configuration file
./webserv

# Expected: Usage message
```

### Test Port Already in Use

```bash
# Start server
./webserv config/test_valid.conf

# In another terminal, try to start another server on same port
./webserv config/test_valid.conf

# Expected: Error about address already in use
```

**Expected Output:**
```
[2025-11-09 21:30:20] [ERROR] bind() failed for 127.0.0.1:8080: Address already in use
[2025-11-09 21:30:20] [ERROR] Failed to bind 127.0.0.1:8080
[2025-11-09 21:30:20] [ERROR] No listening sockets created
[2025-11-09 21:30:20] [ERROR] Failed to initialize server
```

---

## Logging Testing

### Check Log Levels

The server uses different log levels:
- `DEBUG`: Detailed information (connection details, data transfer)
- `INFO`: General information (server start, new connections)
- `WARNING`: Warning messages (unexpected events)
- `ERROR`: Error messages (failures, errors)

### Verify Log Output

```bash
# Start server and observe logs
./webserv config/test_valid.conf

# Send a request
curl http://127.0.0.1:8080/

# Check logs for:
# - Server startup messages
# - New connection messages
# - Data received/sent messages
# - Connection closed messages
```

---

## Integration Testing

### Full Workflow Test

```bash
# 1. Clean build
make fclean
make

# 2. Start server
./webserv config/test_valid.conf &

# 3. Wait for server to start
sleep 2

# 4. Send multiple requests
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8081/
curl http://127.0.0.1:8080/test

# 5. Check server is still running
ps aux | grep webserv

# 6. Stop server
pkill -f webserv
```

---

## Troubleshooting

### Port Already in Use

**Problem:** `bind() failed: Address already in use`

**Solution:**
```bash
# Find process using the port
lsof -i :8080

# Kill the process
kill -9 <PID>

# Or use a different port in configuration
```

### Connection Refused

**Problem:** `curl: (7) Failed to connect`

**Solution:**
1. Check if server is running: `ps aux | grep webserv`
2. Check if port is listening: `lsof -i :8080`
3. Verify configuration file is correct
4. Check firewall settings

### Server Not Responding

**Problem:** Server starts but doesn't respond to requests

**Solution:**
1. Check server logs for errors
2. Verify socket creation was successful
3. Check if event loop is running (should see "Server started" message)
4. Test with `nc` or `telnet` for raw connection

### Permission Denied

**Problem:** Cannot bind to port < 1024

**Solution:**
```bash
# Use ports >= 1024, or
# Run with sudo (not recommended for testing)
sudo ./webserv config/test_valid.conf
```

### Compilation Errors

**Problem:** Build fails with errors

**Solution:**
```bash
# Clean and rebuild
make fclean
make

# Check for syntax errors
make lint  # If clang-tidy is available

# Verify C++98 compatibility
g++ -std=c++98 -Wall -Wextra -Werror ...
```

---

## Testing Checklist

Use this checklist to verify all functionality:

- [ ] Project builds successfully
- [ ] Configuration parsing works
- [ ] Invalid configurations are rejected
- [ ] Server starts and listens on configured ports
- [ ] Server accepts connections
- [ ] Server responds to HTTP requests
- [ ] Multiple ports work simultaneously
- [ ] Multiple connections work concurrently
- [ ] Connection timeouts work correctly
- [ ] Server handles connection errors gracefully
- [ ] Server logs are informative
- [ ] Server stops gracefully on SIGINT/SIGTERM
- [ ] Server cleans up resources on exit

---

## Advanced Testing

### Test with Different HTTP Methods

```bash
# GET request
curl -X GET http://127.0.0.1:8080/

# POST request (will be handled in future stages)
curl -X POST http://127.0.0.1:8080/ -d "test=data"

# DELETE request (will be handled in future stages)
curl -X DELETE http://127.0.0.1:8080/test
```

### Test with Headers

```bash
# Custom headers
curl -H "User-Agent: TestClient" http://127.0.0.1:8080/
curl -H "Accept: text/html" http://127.0.0.1:8080/
```

### Test Network Interfaces

```bash
# Test localhost
curl http://127.0.0.1:8080/

# Test all interfaces (0.0.0.0)
curl http://localhost:8081/

# Test with IP address
curl http://<your-ip>:8081/
```

---

## Performance Testing

### Measure Response Time

```bash
# Time a single request
time curl -s http://127.0.0.1:8080/ > /dev/null

# Measure multiple requests
for i in {1..100}; do
    time curl -s http://127.0.0.1:8080/ > /dev/null
done
```

### Monitor Resource Usage

```bash
# Monitor CPU and memory
top -p $(pgrep webserv)

# Or use htop
htop -p $(pgrep webserv)
```

---

## References

- [cURL Documentation](https://curl.se/docs/)
- [netcat (nc) Manual](https://linux.die.net/man/1/nc)
- [telnet Manual](https://linux.die.net/man/1/telnet)
- [poll() System Call](https://man7.org/linux/man-pages/man2/poll.2.html)
