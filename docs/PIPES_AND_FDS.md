# Pipes and File Descriptors

## Overview

This document provides a comprehensive explanation of pipes and their relationship with file descriptors in Unix-like systems. Understanding this relationship is crucial for implementing inter-process communication (IPC) mechanisms like CGI.

## What are File Descriptors?

### Definition

A **file descriptor** (FD) is a non-negative integer that serves as an abstract handle to an open file, socket, pipe, or other I/O resource. It's the primary interface between user-space programs and the kernel's I/O subsystem.

### Standard File Descriptors

Every process in Unix starts with three standard file descriptors:

| Descriptor | Name | Default | Purpose |
|------------|------|---------|---------|
| 0 | `STDIN_FILENO` | Terminal input | Standard input |
| 1 | `STDOUT_FILENO` | Terminal output | Standard output |
| 2 | `STDERR_FILENO` | Terminal output | Standard error |

### File Descriptor Properties

1. **Non-negative integers**: Start from 0, increment sequentially
2. **Process-local**: Each process has its own set of file descriptors
3. **Inherited on fork**: Child processes inherit parent's open file descriptors
4. **Kernel-managed**: The kernel maintains a per-process file descriptor table
5. **Resource limits**: System and process limits restrict the number of open FDs

### File Descriptor Table

The kernel maintains a **file descriptor table** for each process:

```
Process File Descriptor Table:
┌─────┬─────────────────────────────────┐
│ FD  │ Points to                       │
├─────┼─────────────────────────────────┤
│  0  │ /dev/tty (stdin)                │
│  1  │ /dev/tty (stdout)               │
│  2  │ /dev/tty (stderr)               │
│  3  │ socket:[12345]                  │
│  4  │ pipe:[67890]                    │
│  5  │ /path/to/file.txt               │
│ ... │ ...                             │
└─────┴─────────────────────────────────┘
```

Each entry points to an entry in the system-wide **open file table**, which contains:
- File status flags (read/write mode, append, etc.)
- File offset (current position)
- Reference count (how many FDs point to this entry)
- Pointer to the actual file/socket/pipe structure (inode)

## What are Pipes?

### Definition

A **pipe** is a unidirectional communication channel between two processes. It provides a way for one process to send data to another process, where:
- Data written to one end can be read from the other end
- Data flows in one direction only
- Pipes have a limited buffer size (typically 64KB on Linux)
- Reading from an empty pipe blocks until data is available
- Writing to a full pipe blocks until space is available

### Types of Pipes

1. **Anonymous Pipes** (created with `pipe()`)
   - No name in the filesystem
   - Only accessible to the creating process and its children
   - Destroyed when all references are closed

2. **Named Pipes (FIFOs)** (created with `mkfifo()`)
   - Have a name in the filesystem
   - Can be accessed by unrelated processes
   - Persist until explicitly removed

This document focuses on anonymous pipes created with `pipe()`.

## The pipe() System Call

### Function Signature

```c
int pipe(int pipefd[2]);
```

### Parameters

- `pipefd[2]`: Array of two integers that will be filled with file descriptors

### Return Value

- Returns `0` on success
- Returns `-1` on error and sets `errno`

### What pipe() Does

When `pipe()` is called:

1. **Creates two file descriptors** in the calling process
2. **Allocates kernel buffer** for data storage
3. **Connects the descriptors** so data written to one can be read from the other
4. **Returns the descriptors** in the array:
   - `pipefd[0]`: Read end (file descriptor for reading)
   - `pipefd[1]`: Write end (file descriptor for writing)

### Example

```c
int pipefd[2];

if (pipe(pipefd) == 0) {
    // pipefd[0] is the read end
    // pipefd[1] is the write end
    printf("Read FD: %d\n", pipefd[0]);
    printf("Write FD: %d\n", pipefd[1]);
}
```

## File Descriptors and Pipes: The Connection

### Pipes ARE File Descriptors

**Key insight**: Pipes are represented as file descriptors. When you create a pipe, you get two file descriptors that point to the same pipe object in the kernel.

### Internal Structure

```
┌────────────────────────────────────────────────────────┐
│                    Kernel Space                        │
│                                                        │
│  ┌──────────────────────────────────────────────────┐  │
│  │         Pipe Buffer (64KB typical)               │  │
│  │  ┌──────────────────────────────────────────┐    │  │
│  │  │  [Data written here]                     │    │  │
│  │  └──────────────────────────────────────────┘    │  │
│  │         ▲                    ▲                   │  │
│  └─────────┼────────────────────┼──────────────────-┘  │
│            │                    │                      │
│    Read End              Write End                     │
│            │                    │                      │
└────────────┼────────────────────┼──────────────────────┘
             │                    │
             ▼                    ▼
    ┌──────────────┐      ┌──────────────┐
    │ File Desc 3  │      │ File Desc 4  │
    │ (read end)   │      │ (write end)  │
    └──────────────┘      └──────────────┘
             │                    │
             └────────┬───────────┘
                      │
                      ▼
            ┌─────────────────┐
            │ Process FD Table│
            └─────────────────┘
```

### File Descriptor Table Entry for Pipes

When a pipe is created, the kernel:

1. **Allocates a pipe object** in kernel memory
2. **Creates two file descriptor entries** in the process's FD table
3. **Links both FDs** to the same pipe object
4. **Sets different access modes**:
   - Read FD: read-only access to the pipe
   - Write FD: write-only access to the pipe

### Reading from a Pipe

```c
char buffer[1024];
ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
```

What happens:
1. Process calls `read()` on the read-end file descriptor
2. Kernel checks if data is available in the pipe buffer
3. If data exists: copies data from pipe buffer to user buffer
4. If pipe is empty: blocks (or returns EAGAIN if non-blocking)
5. If write end is closed: returns 0 (EOF)

### Writing to a Pipe

```c
const char *data = "Hello, pipe!";
ssize_t n = write(pipefd[1], data, strlen(data));
```

What happens:
1. Process calls `write()` on the write-end file descriptor
2. Kernel checks if space is available in the pipe buffer
3. If space exists: copies data from user buffer to pipe buffer
4. If pipe is full: blocks (or returns EAGAIN if non-blocking)
5. If read end is closed: process receives SIGPIPE signal (or EPIPE error)

## File Descriptor Inheritance

### Fork and File Descriptors

When a process calls `fork()`:

1. **Child process gets a copy** of parent's file descriptor table
2. **Both processes share** the same open file table entries
3. **Reference counts** are incremented for shared resources
4. **Both can read/write** the same pipes, files, sockets

### Example: Fork with Pipes

```c
int pipefd[2];
pipe(pipefd);  // Parent creates pipe

pid_t pid = fork();

if (pid == 0) {
    // Child process
    // Has access to pipefd[0] and pipefd[1]
    // Both point to the SAME pipe as parent
} else {
    // Parent process
    // Also has access to pipefd[0] and pipefd[1]
    // Both point to the SAME pipe as child
}
```

### Important: Shared Pipe Object

```
Before fork():
┌─────────────────────────────────────┐
│  Parent Process                     │
│  FD Table:                          │
│   3 → pipe read end                 │
│   4 → pipe write end                │
│       ↓                             │
│  ┌──────────┐                       │
│  │   Pipe   │                       │
│  └──────────┘                       │
└─────────────────────────────────────┘

After fork():
┌─────────────────────────────────────┐
│  Parent Process                     │
│  FD Table:                          │
│   3 → pipe read end ──┐             │
│   4 → pipe write end ─┼─┐           │
└───────────────────────┼─┼─────────-─┘
                        │ │
                    ┌───┘ └───┐
                    │         │
                    ▼         ▼
            ┌──────────────────┐
            │   Shared Pipe    │
            │    (kernel)      │
            └──────────────────┘
                    ▲         ▲
                    │         │
                    └───┐ ┌───┘
┌───────────────────────┼─┼───────-───┐
│  Child Process        │ │           │
│  FD Table:            │ │           │
│   3 → pipe read end ──┘ │           │
│   4 → pipe write end ───┘           │
└─────────────────────────────────────┘
```

## File Descriptor Duplication: dup2()

### The Problem

Sometimes you need to redirect a file descriptor to point to a different resource. For example, redirecting `STDOUT_FILENO` (1) to point to a pipe instead of the terminal.

### The Solution: dup2()

```c
int dup2(int oldfd, int newfd);
```

### What dup2() Does

1. **Closes `newfd`** if it's already open
2. **Creates a copy** of `oldfd` at `newfd`
3. **Both descriptors** now point to the same resource
4. **Returns `newfd`** on success, or `-1` on error

### Example: Redirecting stdout to a pipe

```c
int pipefd[2];
pipe(pipefd);

// Redirect stdout (FD 1) to point to pipe write end
dup2(pipefd[1], STDOUT_FILENO);

// Now both pipefd[1] and STDOUT_FILENO (1) point to the same pipe
// Anything written to stdout goes to the pipe

// Close the original pipe descriptor (we have a copy at FD 1)
close(pipefd[1]);
```

### Visual Representation

```
Before dup2():
┌─────────────────────────────────────┐
│  File Descriptor Table              │
│   0 → stdin (terminal)              │
│   1 → stdout (terminal)             │
│   2 → stderr (terminal)             │
│   3 → pipe read end                 │
│   4 → pipe write end                │
└─────────────────────────────────────┘

After dup2(pipefd[1], STDOUT_FILENO):
┌─────────────────────────────────────┐
│  File Descriptor Table              │
│   0 → stdin (terminal)              │
│   1 → pipe write end ────┐          │
│   2 → stderr (terminal)  │          │
│   3 → pipe read end      │          │
│   4 → pipe write end ────┘          │
│       (both point to same pipe)     │
└─────────────────────────────────────┘
```

## Pipe Communication Patterns

### Pattern 1: Parent-to-Child Communication

```c
int pipefd[2];
pipe(pipefd);

pid_t pid = fork();

if (pid == 0) {
    // Child: close write end, read from read end
    close(pipefd[1]);
    char buffer[1024];
    read(pipefd[0], buffer, sizeof(buffer));
    close(pipefd[0]);
} else {
    // Parent: close read end, write to write end
    close(pipefd[0]);
    write(pipefd[1], "Hello child!", 12);
    close(pipefd[1]);
    wait(NULL);
}
```

### Pattern 2: Child-to-Parent Communication

```c
int pipefd[2];
pipe(pipefd);

pid_t pid = fork();

if (pid == 0) {
    // Child: close read end, write to write end
    close(pipefd[0]);
    write(pipefd[1], "Hello parent!", 14);
    close(pipefd[1]);
    exit(0);
} else {
    // Parent: close write end, read from read end
    close(pipefd[1]);
    char buffer[1024];
    read(pipefd[0], buffer, sizeof(buffer));
    close(pipefd[0]);
    wait(NULL);
}
```

### Pattern 3: Bidirectional Communication

Requires two pipes:

```c
int parent_to_child[2];
int child_to_parent[2];

pipe(parent_to_child);
pipe(child_to_parent);

pid_t pid = fork();

if (pid == 0) {
    // Child
    close(parent_to_child[1]);  // Close write end
    close(child_to_parent[0]);  // Close read end
    
    // Read from parent
    read(parent_to_child[0], ...);
    
    // Write to parent
    write(child_to_parent[1], ...);
    
    close(parent_to_child[0]);
    close(child_to_parent[1]);
} else {
    // Parent
    close(parent_to_child[0]);  // Close read end
    close(child_to_parent[1]);  // Close write end
    
    // Write to child
    write(parent_to_child[1], ...);
    
    // Read from child
    read(child_to_parent[0], ...);
    
    close(parent_to_child[1]);
    close(child_to_parent[0]);
}
```

## Why Close Unused Pipe Ends?

### The EOF Problem

When you read from a pipe:
- If the **write end is still open**, `read()` will block waiting for data
- If the **write end is closed**, `read()` returns 0 (EOF) when all data is consumed

### Example: Deadlock Prevention

```c
// WRONG: Not closing unused ends
int pipefd[2];
pipe(pipefd);
fork();

if (pid == 0) {
    // Child reads
    char buffer[1024];
    read(pipefd[0], buffer, sizeof(buffer));  // Blocks forever!
    // Parent still has write end open, so read() never returns EOF
}

// CORRECT: Close unused ends
int pipefd[2];
pipe(pipefd);
fork();

if (pid == 0) {
    close(pipefd[1]);  // Close write end in child
    char buffer[1024];
    read(pipefd[0], buffer, sizeof(buffer));  // Returns EOF when done
    close(pipefd[0]);
}
```

### Reference Counting

The kernel uses reference counting for pipes:
- Each open file descriptor increments the reference count
- When a descriptor is closed, the count decrements
- When the count reaches zero, the pipe is destroyed

Closing unused ends ensures:
1. Proper EOF signaling
2. Resource cleanup
3. No deadlocks

## File Descriptor Limits

### System Limits

```bash
# Maximum file descriptors per process
ulimit -n

# System-wide maximum
cat /proc/sys/fs/file-max
```

### Checking Current Usage

```bash
# Count open file descriptors for a process
ls -1 /proc/<PID>/fd/ | wc -l

# List all file descriptors
ls -l /proc/<PID>/fd/
```

### Common Limits

- **Default per-process**: 1024 (can be increased)
- **System-wide**: Typically 100,000+ on modern systems
- **Hard limit**: Set by system administrator

## Error Handling

### Common Errors

1. **EMFILE**: Too many open file descriptors
   ```c
   if (pipe(pipefd) == -1 && errno == EMFILE) {
       // Process has reached FD limit
   }
   ```

2. **EPIPE**: Writing to a pipe with closed read end
   ```c
   if (write(pipefd[1], data, len) == -1 && errno == EPIPE) {
       // Read end is closed, process receives SIGPIPE
   }
   ```

3. **EAGAIN/EWOULDBLOCK**: Non-blocking operation would block
   ```c
   if (read(pipefd[0], buffer, size) == -1) {
       if (errno == EAGAIN || errno == EWOULDBLOCK) {
           // Would block, try again later
       }
   }
   ```

4. **EINTR**: System call interrupted by signal
   ```c
   ssize_t n;
   do {
       n = read(pipefd[0], buffer, size);
   } while (n == -1 && errno == EINTR);
   ```

## Non-Blocking Pipes

### Setting Non-Blocking Mode

```c
#include <fcntl.h>

int flags = fcntl(pipefd[0], F_GETFL);
fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

// Now read() won't block
ssize_t n = read(pipefd[0], buffer, size);
if (n == -1 && errno == EAGAIN) {
    // No data available, try again later
}
```

### Use Cases

- Event-driven programming
- Avoiding deadlocks
- Timeout handling
- Integration with select()/poll()/epoll()

## Pipes vs Other IPC Mechanisms

### Pipes vs Sockets

| Feature | Pipes | Sockets |
|---------|-------|---------|
| Scope | Related processes | Any processes (network) |
| Direction | Unidirectional | Bidirectional |
| Type | Byte stream | Byte stream or datagram |
| Naming | Anonymous or named | Network address |
| Performance | Faster (kernel) | Slower (network stack) |

### Pipes vs Shared Memory

| Feature | Pipes | Shared Memory |
|---------|-------|---------------|
| Synchronization | Automatic | Manual (semaphores, etc.) |
| Speed | Slower (copying) | Faster (direct access) |
| Complexity | Simple | More complex |
| Use Case | Streaming data | Large data structures |

## Practical Example: CGI Implementation

### Complete Flow

```c
// 1. Create pipes
int stdin_pipe[2], stdout_pipe[2];
pipe(stdin_pipe);
pipe(stdout_pipe);

// 2. Fork process
pid_t pid = fork();

if (pid == 0) {
    // Child process (CGI script)
    
    // 3. Close unused ends
    close(stdin_pipe[1]);   // Don't write to stdin pipe
    close(stdout_pipe[0]);  // Don't read from stdout pipe
    
    // 4. Redirect standard I/O
    dup2(stdin_pipe[0], STDIN_FILENO);   // stdin reads from pipe
    dup2(stdout_pipe[1], STDOUT_FILENO); // stdout writes to pipe
    
    // 5. Close original descriptors
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    
    // 6. Execute CGI script
    execve(interpreter, argv, env);
    exit(1);
} else {
    // Parent process (web server)
    
    // 7. Close unused ends
    close(stdin_pipe[0]);   // Don't read from stdin pipe
    close(stdout_pipe[1]);  // Don't write to stdout pipe
    
    // 8. Write request body
    write(stdin_pipe[1], request_body, body_len);
    close(stdin_pipe[1]);   // Signal EOF to child
    
    // 9. Read CGI output
    char buffer[4096];
    ssize_t n;
    while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
        // Process CGI output
    }
    close(stdout_pipe[0]);
    
    // 10. Wait for child
    waitpid(pid, &status, 0);
}
```

### File Descriptor States Throughout Execution

```
Step 1: After pipe() creation
Parent: stdin_pipe[0]=3, stdin_pipe[1]=4, stdout_pipe[0]=5, stdout_pipe[1]=6
Child:  stdin_pipe[0]=3, stdin_pipe[1]=4, stdout_pipe[0]=5, stdout_pipe[1]=6

Step 2: After closing unused ends in child
Parent: stdin_pipe[0]=3, stdin_pipe[1]=4, stdout_pipe[0]=5, stdout_pipe[1]=6
Child:  stdin_pipe[0]=3, stdin_pipe[1]=CLOSED, stdout_pipe[0]=CLOSED, stdout_pipe[1]=6

Step 3: After dup2() in child
Parent: stdin_pipe[0]=3, stdin_pipe[1]=4, stdout_pipe[0]=5, stdout_pipe[1]=6
Child:  STDIN=0→stdin_pipe, STDOUT=1→stdout_pipe, 
        stdin_pipe[0]=3 (can close), stdout_pipe[1]=6 (can close)

Step 4: After closing original descriptors in child
Parent: stdin_pipe[0]=3, stdin_pipe[1]=4, stdout_pipe[0]=5, stdout_pipe[1]=6
Child:  STDIN=0→stdin_pipe, STDOUT=1→stdout_pipe

Step 5: After closing unused ends in parent
Parent: stdin_pipe[0]=CLOSED, stdin_pipe[1]=4, stdout_pipe[0]=5, stdout_pipe[1]=CLOSED
Child:  STDIN=0→stdin_pipe, STDOUT=1→stdout_pipe

Step 6: After writing and closing write end in parent
Parent: stdin_pipe[1]=CLOSED, stdout_pipe[0]=5
Child:  STDIN=0→stdin_pipe (read end still open in parent, but parent closed it)

Step 7: After reading and closing read end in parent
Parent: stdout_pipe[0]=CLOSED
Child:  STDOUT=1→stdout_pipe (write end still open in child)
```

## Debugging Tips

### Inspecting File Descriptors

```bash
# List all file descriptors for a process
ls -l /proc/<PID>/fd/

# Check for pipe descriptors
ls -l /proc/<PID>/fd/ | grep pipe

# Count open descriptors
ls -1 /proc/<PID>/fd/ | wc -l
```

### Using strace

```bash
# Trace pipe-related system calls
strace -e pipe,dup2,read,write,close ./program

# Show file descriptor operations
strace -e trace=file ./program
```

### Common Issues

1. **Too many open FDs**: Check with `lsof -p <PID>`
2. **Deadlock**: Ensure unused pipe ends are closed
3. **EPIPE errors**: Check if read end is closed before writing
4. **Blocking forever**: Verify EOF is signaled by closing write end

## References

- `man 2 pipe` - Pipe system call
- `man 2 dup2` - Duplicate file descriptor
- `man 2 fork` - Create child process
- `man 2 read` - Read from file descriptor
- `man 2 write` - Write to file descriptor
- `man 2 fcntl` - File control operations
- [POSIX Pipes](https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html)
- [Advanced Programming in the UNIX Environment](https://www.amazon.com/Advanced-Programming-UNIX-Environment-3rd/dp/0321637739)

## Related Documentation

- [CGI Pipes and File Descriptors](CGI_PIPES.md) - CGI-specific implementation details
- [File Descriptors](FILE_DESCRIPTORS.md) - General file descriptor management in webserv
