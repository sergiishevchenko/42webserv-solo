# File Descriptors

## Overview

This guide explains how to view and work with file descriptors (FDs) of the webserv process for debugging and monitoring purposes.

## Quick Methods

### Method 1: Using the Helper Script

```bash
# Auto-detect webserv process
./scripts/show_fds.sh

# Or specify PID manually
./scripts/show_fds.sh <PID>
```

### Method 2: Direct /proc Access

```bash
# Find webserv process ID
ps aux | grep webserv

# List all file descriptors
ls -lah /proc/<PID>/fd/

# Show detailed information
ls -l /proc/<PID>/fd/ | grep socket
```

### Method 3: Using lsof (if installed)

```bash
# Find PID
pgrep -f webserv

# Show all open files/descriptors
lsof -p <PID>

# Show only sockets
lsof -p <PID> | grep -E "IPv4|IPv6|TCP|UDP"

# Show with network information
lsof -i -p <PID>
```

### Method 4: Real-time Monitoring

```bash
# Watch file descriptors in real-time
watch -n 1 'ls -1 /proc/$(pgrep -f webserv)/fd/ | wc -l'

# Monitor specific process
watch -n 1 './scripts/show_fds.sh'
```

## Understanding the Output

### Standard Descriptors

Every process has three standard file descriptors:
- **FD 0**: stdin (standard input)
- **FD 1**: stdout (standard output)
- **FD 2**: stderr (standard error)

### Socket Descriptors

Socket descriptors appear as:
```
socket:[12345]
```

Where `12345` is the inode number of the socket in the kernel.

### Common Patterns

1. **Listening sockets**: Usually FD 3, 4, 5... (one per server block/port)
2. **Client sockets**: Created dynamically via `accept()`, higher FD numbers
3. **Epoll/poll file descriptors**: May appear as `anon_inode:[eventpoll]` or similar

## Example Output

```
==========================================
File Descriptors for PID: 12345
==========================================

Process: webserv
Command: ./webserv config/test_valid.conf

--- Method 1: /proc/12345/fd/ ---

FD 0: /dev/ttys001 (stdin)
FD 1: /dev/ttys001 (stdout)
FD 2: /dev/ttys001 (stderr)
FD 3: socket:[123456] (listening socket on 127.0.0.1:8080)
FD 4: socket:[123457] (listening socket on 0.0.0.0:8081)
FD 5: socket:[123458] (client socket)

--- Summary ---

Total file descriptors: 6
Socket descriptors: 3
```

## Troubleshooting

### Too Many Open File Descriptors

If you see many file descriptors, check for:
- Memory leaks (sockets not being closed)
- Connection handling issues
- File descriptor leaks

### Check System Limits

```bash
# Check current limits
ulimit -n

# Check process limits
cat /proc/<PID>/limits

# Check system-wide limits
cat /proc/sys/fs/file-max
```

### Finding Socket Information

```bash
# Show socket details
ss -tulpn | grep <PID>

# Or using netstat (older systems)
netstat -tulpn | grep <PID>
```

## Integration with Code

You can also add debugging output directly in the code:

```cpp
// In Server.cpp, add method to print FDs
void Server::printFileDescriptors() {
    LOG_DEBUG() << "File descriptors:" << std::endl;
    for (size_t i = 0; i < listening_sockets_.size(); ++i) {
        LOG_DEBUG() << "  Listening socket FD: " 
                    << listening_sockets_[i]->getFd() << std::endl;
    }
    // ... print client connections ...
}
```

## File I/O Architecture in webserv

### Key Design Decision: File Descriptors vs Socket Descriptors

**Critical Architecture Point:** The server uses **two different I/O models** for different resource types:

1. **Socket Descriptors (Network I/O)**: Non-blocking, event-driven via `poll()`
   - All socket file descriptors are tracked in `poll_fds_` vector
   - Managed through the event loop
   - Supports concurrent connections
   - Non-blocking mode (`O_NONBLOCK`)

2. **File Descriptors (Disk I/O)**: Synchronous, blocking, NOT in `poll()`
   - Files are **NOT** tracked in `poll_fds_`
   - Files are read completely into memory before sending
   - Uses C++ streams (`std::ifstream`, `std::ofstream`)
   - Blocking mode (default)

### Why Files Are NOT in poll()

**Architectural Reasons:**

1. **Performance**: File I/O is typically fast (disk cache, SSD)
   - Reading entire file into memory completes quickly
   - No need for complex async file handling

2. **Simplicity**: Synchronous file operations are easier to manage
   - No need to track file descriptors in poll()
   - Simpler error handling
   - Avoids file descriptor exhaustion

3. **Resource Management**: Files are opened, used, and closed immediately
   - File descriptors don't accumulate
   - No risk of FD leaks from file operations
   - Each file operation is self-contained

4. **Separation of Concerns**: Network I/O and file I/O have different characteristics
   - Network: slow, unpredictable, needs async handling
   - Files: fast, predictable, can be synchronous

### File Descriptor Lifecycle

**Static File Serving:**

```
1. Request arrives → Socket FD (in poll())
2. Parse request → Still using socket FD
3. Open file → File FD created (NOT in poll())
4. Read entire file → Synchronous, blocking
5. Close file → File FD closed immediately
6. Send response → Socket FD (in poll(), non-blocking write)
```

**File Upload:**

```
1. Request arrives → Socket FD (in poll())
2. Parse request body → Socket FD (in poll())
3. Open upload file → File FD created (NOT in poll())
4. Write entire body → Synchronous, blocking
5. Close file → File FD closed immediately
6. Send response → Socket FD (in poll(), non-blocking write)
```

### What IS Tracked in poll()

**Tracked File Descriptors:**
- ✅ Listening sockets (one per server block/port)
- ✅ Client connection sockets (one per active connection)
- ✅ CGI pipes (stdin/stdout) - handled by CgiHandler

**NOT Tracked File Descriptors:**
- ❌ Static file descriptors (`std::ifstream` for reading files)
- ❌ Upload file descriptors (`std::ofstream` for writing files)
- ❌ Configuration file descriptors
- ❌ Directory descriptors (`opendir()` for directory listing)
- ❌ Error page file descriptors

### Code Example

**File Reading (NOT in poll()):**

```cpp
// RequestHandler::serveFile()
std::ifstream file(file_path.c_str(), std::ios::binary);
// File FD opened here, but NOT added to poll_fds_

file.seekg(0, std::ios::end);
std::streamsize size = file.tellg();
file.seekg(0, std::ios::beg);

std::vector<char> buffer(static_cast<std::size_t>(size));
file.read(buffer.data(), size);  // Blocking read
file.close();  // File FD closed immediately

// Response sent via socket (which IS in poll())
response.setBody(buffer.data(), static_cast<std::size_t>(size));
```

**Socket I/O (IS in poll()):**

```cpp
// Server::acceptNewConnection()
int client_fd = socket->accept();
fcntl(client_fd, F_SETFL, O_NONBLOCK);  // Non-blocking

Connection* conn = new Connection(client_fd, ...);
connections_[client_fd] = conn;
addPollFd(client_fd, POLLIN | POLLOUT);  // Added to poll()
```

### Implications

**Advantages:**
- ✅ Simpler code (no file FD management in poll())
- ✅ Predictable behavior (file operations complete quickly)
- ✅ Easier error handling (synchronous operations)
- ✅ No file descriptor leaks (files closed immediately)
- ✅ Lower FD count (only sockets tracked)

**Limitations:**
- ⚠️ Large files consume memory (entire file in RAM)
- ⚠️ File I/O blocks the event loop briefly (but files read fast)
- ⚠️ Not suitable for very large files (>100MB) without optimization
- ⚠️ No streaming for large file transfers

### Monitoring File Descriptors

When monitoring webserv file descriptors, you'll see:

**Typical FD Distribution:**
- FD 0-2: Standard I/O (stdin, stdout, stderr)
- FD 3+: Listening sockets (one per port)
- FD 10+: Client sockets (dynamic, one per connection)
- **No file FDs visible** (they're opened and closed too quickly)

**Why You Don't See File FDs:**
- Files are opened, read/written, and closed in the same function call
- File operations complete before the next poll() iteration
- File descriptors are ephemeral (exist for milliseconds)

### For Very Large Files

**Current Implementation:**
- Loads entire file into memory
- Suitable for files up to ~100MB (depending on `client_max_body_size`)

**Future Optimizations:**
- Use `sendfile()` on Linux (zero-copy, kernel-to-kernel transfer)
- Stream file reads in chunks (would require adding file FDs to poll())
- Implement range requests (HTTP Range header)

### Comparison Table

| Aspect | Socket FDs | File FDs |
|--------|-----------|----------|
| **Tracked in poll()** | ✅ Yes | ❌ No |
| **Blocking Mode** | Non-blocking | Blocking |
| **I/O Model** | Event-driven | Synchronous |
| **Lifetime** | Long (connection duration) | Short (operation duration) |
| **Concurrency** | Multiple simultaneous | One at a time |
| **Memory Usage** | Small buffers | Entire file |
| **Error Handling** | Async (via poll events) | Sync (immediate) |

## Related Documentation

- [Pipes and File Descriptors](PIPES_AND_FDS.md) - Detailed explanation of pipes and their relationship with file descriptors
- [CGI Pipes and File Descriptors](CGI_PIPES.md) - CGI-specific pipe implementation details
- [Structures](STRUCTURES.md) - Data structures and classes reference

## References

- `/proc/<PID>/fd/` - Directory containing file descriptor symlinks
- `lsof` - List open files utility
- `ss` - Socket statistics utility
- `strace` - System call tracer (advanced debugging)

