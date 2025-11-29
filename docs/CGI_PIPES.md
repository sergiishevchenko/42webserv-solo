# CGI Pipes and File Descriptors

## Overview

This document explains how pipes and file descriptors are used in the CGI implementation of webserv. Pipes are the primary mechanism for bidirectional communication between the web server (parent process) and CGI scripts (child processes).

## What are Pipes?

A pipe is a unidirectional inter-process communication (IPC) mechanism in Unix-like systems. It creates a communication channel where:
- One end is for writing (write end)
- One end is for reading (read end)
- Data written to one end can be read from the other end

In CGI implementation, we use two pipes:
1. **stdin_pipe**: For sending request body data to the CGI script
2. **stdout_pipe**: For receiving output from the CGI script

## Pipe Creation

### Creating Pipes

```cpp
int stdin_pipe[2];   // Pipe for CGI stdin
int stdout_pipe[2];  // Pipe for CGI stdout

if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    // Error handling
    return error_response;
}
```

### Understanding Pipe Arrays

Each pipe is represented by an array of two integers:
- `pipe[0]`: Read end of the pipe
- `pipe[1]`: Write end of the pipe

After creation:
- `stdin_pipe[0]`: Read end (server reads from here - not used in our case)
- `stdin_pipe[1]`: Write end (server writes request body here)
- `stdout_pipe[0]`: Read end (server reads CGI output from here)
- `stdout_pipe[1]`: Write end (CGI script writes output here)

## Process Forking and File Descriptor Inheritance

### Fork Operation

```cpp
pid_t pid = fork();
if (pid < 0) {
    // Fork failed - close all pipes and return error
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return error_response;
}
```

When `fork()` is called:
- The parent process receives the child's PID (positive number)
- The child process receives 0
- Both processes inherit all open file descriptors

### File Descriptor Inheritance

After forking, both parent and child processes have access to the same pipe file descriptors. This means:
- Both can read from and write to the pipes
- We need to close unused ends in each process to avoid deadlocks
- Proper cleanup is essential to prevent resource leaks

## Child Process Setup

### Closing Unused Ends

In the child process (CGI script), we close the ends we don't need:

```cpp
if (pid == 0) {  // Child process
    // Close write end of stdin pipe (child only reads from stdin)
    close(stdin_pipe[1]);
    
    // Close read end of stdout pipe (child only writes to stdout)
    close(stdout_pipe[0]);
    
    // ... rest of setup ...
}
```

### Redirecting Standard I/O

The child process needs to redirect its standard input and output to the pipes:

```cpp
// Redirect stdin to read from stdin_pipe[0]
if (dup2(stdin_pipe[0], STDIN_FILENO) < 0) {
    exit(1);
}

// Redirect stdout to write to stdout_pipe[1]
if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
    exit(1);
}
```

### Understanding dup2()

`dup2(old_fd, new_fd)` creates a copy of `old_fd` at `new_fd`:
- If `new_fd` is already open, it's closed first
- After `dup2()`, both file descriptors point to the same file/pipe
- We can then close the original pipe descriptor

### Final Cleanup in Child

After redirecting, we close the original pipe descriptors:

```cpp
close(stdin_pipe[0]);   // Original read end (now duplicated to STDIN)
close(stdout_pipe[1]);  // Original write end (now duplicated to STDOUT)
```

## Parent Process Setup

### Closing Unused Ends

In the parent process (web server), we close the ends we don't need:

```cpp
// Close read end of stdin pipe (parent only writes to it)
close(stdin_pipe[0]);

// Close write end of stdout pipe (parent only reads from it)
close(stdout_pipe[1]);
```

### Why Close Unused Ends?

Closing unused pipe ends is crucial:
1. **Prevents deadlocks**: If a process is waiting to read from a pipe, but no process has the write end open, it will hang forever
2. **Proper EOF signaling**: When the write end is closed, reading from the read end returns EOF (0 bytes)
3. **Resource management**: Prevents file descriptor leaks

## Writing Request Body to CGI

### Writing Data

The parent process writes the HTTP request body to the CGI script's stdin:

```cpp
std::string request_body = request.body;
size_t written = 0;

while (written < request_body.size()) {
    ssize_t n = write(stdin_pipe[1], 
                      request_body.c_str() + written, 
                      request_body.size() - written);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;  // Non-blocking, try again
        }
        break;  // Error occurred
    }
    written += n;
}
```

### Closing Write End

After writing all data, we close the write end:

```cpp
close(stdin_pipe[1]);
```

This signals EOF to the CGI script, so it knows no more data is coming.

## Reading CGI Output

### Reading Data

The parent process reads the CGI script's output:

```cpp
std::string cgi_output;
char buffer[4096];
ssize_t n;

while (true) {
    n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
    
    if (n > 0) {
        buffer[n] = '\0';
        cgi_output.append(buffer, n);
    } else if (n == 0) {
        break;  // EOF - CGI script closed stdout
    } else {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;  // Interrupted or non-blocking, try again
        }
        break;  // Error occurred
    }
}
```

### EOF Detection

When `read()` returns 0, it means:
- The CGI script has closed its stdout (stdout_pipe[1])
- No more data will be available
- We've received all the output

### Closing Read End

After reading all data:

```cpp
close(stdout_pipe[0]);
```

## Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Parent Process (Server)                   │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ fork()
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Child Process (CGI)                      │
└─────────────────────────────────────────────────────────────┘

Pipe Setup:
┌──────────────┐         ┌──────────────┐
│ stdin_pipe   │         │ stdout_pipe  │
│ [0]  [1]     │         │ [0]  [1]     │
│ read  write  │         │ read  write  │
└──────┬───────┘         └──────┬───────┘
       │                        │
       │                        │
       │ dup2(stdin_pipe[0],    │ dup2(stdout_pipe[1],
       │      STDIN_FILENO)     │      STDOUT_FILENO)
       │                        │
       ▼                        ▼
┌──────────────┐         ┌──────────────┐
│   STDIN      │         │   STDOUT     │
│  (CGI reads) │         │ (CGI writes) │
└──────────────┘         └──────────────┘

Data Flow:
┌─────────────────┐                    ┌─────────────────┐
│  Parent writes  │ ───stdin_pipe[1]──▶│  Child reads    │
│  request body   │                    │  from STDIN     │
└─────────────────┘                    └─────────────────┘

┌─────────────────┐                    ┌─────────────────┐
│  Parent reads   │ ◀──stdout_pipe[0]──│  Child writes   │
│  CGI output     │                    │  to STDOUT      │
└─────────────────┘                    └─────────────────┘
```

## File Descriptor States

### After Pipe Creation

| Descriptor | Parent | Child | Purpose |
|------------|--------|-------|---------|
| stdin_pipe[0] | Open | Open | Read end (unused by parent) |
| stdin_pipe[1] | Open | Open | Write end (parent writes here) |
| stdout_pipe[0] | Open | Open | Read end (parent reads here) |
| stdout_pipe[1] | Open | Open | Write end (unused by parent) |

### After Closing Unused Ends

| Descriptor | Parent | Child | Purpose |
|------------|--------|-------|---------|
| stdin_pipe[0] | Closed | Open | Will be dup2'd to STDIN |
| stdin_pipe[1] | Open | Closed | Parent writes request body |
| stdout_pipe[0] | Open | Closed | Parent reads CGI output |
| stdout_pipe[1] | Closed | Open | Will be dup2'd to STDOUT |

### After dup2() in Child

| Descriptor | Parent | Child | Purpose |
|------------|--------|-------|---------|
| STDIN_FILENO (0) | - | Points to stdin_pipe[0] | CGI reads from here |
| STDOUT_FILENO (1) | - | Points to stdout_pipe[1] | CGI writes to here |
| stdin_pipe[0] | Closed | Can be closed | Duplicated to STDIN |
| stdin_pipe[1] | Open | Closed | Parent writes request body |
| stdout_pipe[0] | Open | Closed | Parent reads CGI output |
| stdout_pipe[1] | Closed | Can be closed | Duplicated to STDOUT |

### Final State (After All Setup)

| Descriptor | Parent | Child | Purpose |
|------------|--------|-------|---------|
| STDIN_FILENO (0) | - | Points to stdin_pipe | CGI reads request body |
| STDOUT_FILENO (1) | - | Points to stdout_pipe | CGI writes output |
| stdin_pipe[1] | Open | Closed | Parent writes request body |
| stdout_pipe[0] | Open | Closed | Parent reads CGI output |

## Error Handling

### Pipe Creation Failure

```cpp
if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    freeEnvArray(env_array);
    error_response.setStatus(500, "Internal Server Error");
    error_response.setBody("Failed to create pipes for CGI");
    return error_response;
}
```

### Fork Failure

```cpp
if (pid < 0) {
    // Close all pipes before returning
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    freeEnvArray(env_array);
    error_response.setStatus(500, "Internal Server Error");
    error_response.setBody("Failed to fork process for CGI");
    return error_response;
}
```

### dup2() Failure in Child

```cpp
if (dup2(stdin_pipe[0], STDIN_FILENO) < 0) {
    exit(1);  // Child process exits on error
}
if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
    exit(1);
}
```

## Timeout Handling

### Reading with Timeout

```cpp
time_t start_time = time(NULL);
const time_t timeout = 30;

while (true) {
    n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
    
    // ... handle read result ...
    
    if (time(NULL) - start_time > timeout) {
        killProcess(pid);
        close(stdout_pipe[0]);
        freeEnvArray(env_array);
        // ... cleanup and return timeout error ...
    }
}
```

If the CGI script takes too long, we:
1. Kill the child process
2. Close the read end of the pipe
3. Clean up resources
4. Return a 504 Gateway Timeout error

## Cleanup and Resource Management

### Proper Cleanup Order

1. **Close write end of stdin_pipe** (after writing request body)
2. **Close read end of stdout_pipe** (after reading all output)
3. **Wait for child process** to complete
4. **Free environment array**
5. **Handle any remaining cleanup**

### Preventing File Descriptor Leaks

Always ensure:
- All pipe ends are closed in both processes
- Original pipe descriptors are closed after dup2()
- Error paths also close all descriptors
- Child process exits properly (doesn't continue parent code)

## Common Pitfalls

### 1. Not Closing Unused Ends

**Problem**: If you don't close unused ends, the pipe won't signal EOF properly.

**Solution**: Always close the ends you don't use in each process.

### 2. Closing Ends Too Early

**Problem**: Closing a pipe end before you're done using it.

**Solution**: Close write end only after writing all data, read end only after reading all data.

### 3. Forgetting to Close After dup2()

**Problem**: After dup2(), the original file descriptor is still open, causing leaks.

**Solution**: Always close the original descriptor after successful dup2().

### 4. Not Handling EINTR

**Problem**: System calls can be interrupted by signals, returning EINTR.

**Solution**: Retry the operation when errno is EINTR.

### 5. Blocking Operations

**Problem**: Pipes can block if the buffer is full (write) or empty (read).

**Solution**: Handle EAGAIN/EWOULDBLOCK appropriately, or use non-blocking I/O with select/poll.

## Best Practices

1. **Always close unused pipe ends** in both parent and child
2. **Close original descriptors** after dup2()
3. **Handle all error cases** and clean up resources
4. **Check return values** of all system calls
5. **Use proper error handling** for interrupted system calls
6. **Implement timeouts** for long-running CGI scripts
7. **Clean up on all exit paths** (normal and error)

## References

- `man 2 pipe` - Pipe system call documentation
- `man 2 dup2` - Duplicate file descriptor documentation
- `man 2 fork` - Fork system call documentation
- `man 2 read` - Read system call documentation
- `man 2 write` - Write system call documentation
- [POSIX Pipes](https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html)
- [File Descriptors in Unix](https://en.wikipedia.org/wiki/File_descriptor)
