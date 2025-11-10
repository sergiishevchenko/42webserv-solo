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

## References

- `/proc/<PID>/fd/` - Directory containing file descriptor symlinks
- `lsof` - List open files utility
- `ss` - Socket statistics utility
- `strace` - System call tracer (advanced debugging)

