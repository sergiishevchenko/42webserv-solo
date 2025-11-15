# Socket Programming: Concept and Implementation

## Table of Contents
1. [What is a Socket?](#what-is-a-socket)
2. [Socket Lifecycle on Server](#socket-lifecycle-on-server)
3. [Listening vs Client Sockets](#listening-vs-client-sockets)
4. [File Descriptors](#file-descriptors)
5. [Data Transmission](#data-transmission)
6. [Non-blocking Mode](#non-blocking-mode)
7. [Socket Class in Project](#socket-class-in-project)

---

## What is a Socket?

**Socket** - a programming interface (endpoint) for network communication between programs. Think of a socket as a "door" or "window" through which programs can send and receive data over a network.

### Simple Diagram

```
SERVER                          CLIENT
┌─────────────┐                ┌─────────────┐
│  Program    │                │  Program    │
│   (Server)  │                │  (Browser)  │
└──────┬──────┘                └──────┬──────┘
       │                              │
       │  Socket (fd=3)               │  Socket (fd=8)
       │  127.0.0.1:8080              │  192.168.1.5:54321
       │  [Listening]                 │  [Client]
       │                              │
       │◄─────────────────────────────┤ connect()
       │                              │
       │  accept() → new fd=5         │
       │                              │
       │  Socket (fd=5)               │  Socket (fd=8)
       │  [Client]                    │  [Client]
       │                              │
       │◄─────────────────────────────┤ send("GET / HTTP/1.1")
       │  recv()                      │
       │                              │
       │─────────────────────────────►│ send("HTTP/1.1 200 OK")
       │  send()                      │  recv()
       │                              │
       │  close(fd=5)                 │  close(fd=8)
       │                              │
       │  Socket (fd=3)               │
       │  [Listening] ← continues listening
       │
```

### Key Concepts

1. **Socket** = file descriptor + address (IP:port)
2. **Listening socket** = waits for new connections (one per port)
3. **Client socket** = connection to a specific client (many simultaneously)
4. **File descriptor (fd)** = number to work with socket (3, 4, 5, ...)

### Phone Analogy

- **Socket** = telephone device
- **IP address** = phone number (e.g., 127.0.0.1)
- **Port** = extension number in office (e.g., 8080)
- **bind()** = configuring phone to specific number
- **listen()** = enabling call waiting mode
- **accept()** = accepting incoming call
- **recv()/send()** = conversation over phone

### What is a Socket at System Level?

1. **File Descriptor (fd):**

   **What it is:**
   - In Unix/Linux, a socket is represented as a file descriptor — an integer (e.g., 3, 4, 5)
   - File descriptor is a "handle" for accessing an operating system resource
   - This is an identifier that the OS kernel returns to the program when creating a resource

   **Why is it called "file descriptor"?**

   The name "file descriptor" comes from Unix history and philosophy:

   1. **Historical origin**: In early Unix systems, this mechanism was created primarily for working with files on disk. The `open()` system call returned a number (descriptor) that "described" or "identified" an opened file.

   2. **"Everything is a file" philosophy**: Unix treats many things as files:
      - Regular files on disk
      - Directories (special files)
      - Devices (represented as files in `/dev`)
      - Sockets (network connections)
      - Pipes (inter-process communication)
   
   3. **Unified interface**: Since all these resources use the same interface (`read()`, `write()`, `close()`), they all use the same mechanism — file descriptors.

   4. **Name stuck**: Even though descriptors are now used for sockets, pipes, devices, and other non-file resources, the original name "file descriptor" remained because:
      - It was the first use case
      - The interface is the same as for files
      - It reflects the Unix philosophy

   **In other words:**
   - "File" = follows the Unix "everything is a file" concept
   - "Descriptor" = describes/identifies a resource (not necessarily a file on disk)
   - The name is historical but still accurate because sockets, pipes, and devices are treated as "files" in Unix

   **What "identifier returned by the OS kernel" means:**

   When you call a system call (e.g., `socket()`, `open()`, `pipe()`), the following happens:

   1. **Your program** calls a function (e.g., `socket()`)
   2. **The function makes a system call** — switches to kernel mode
   3. **The OS kernel** creates an internal data structure for the resource in its memory:
      - For socket: creates a structure with buffers, state, address, etc.
      - For file: creates a structure with file information, read position, etc.
   4. **The kernel allocates a number** (identifier) from <span style="color:red;"><strong>the process's file descriptor table</strong></span>
   5. **The kernel links the number to the resource** — writes a pointer to the created structure in the process table
   6. **The kernel returns the number to the program** — the function returns this number (e.g., 3)

   **Why this is important:**

   - **Separation of concerns**: The resource lives in kernel memory (protected area), while the program only gets a "ticket number"
   - **Security**: The program cannot directly access kernel structures, only through system calls
   - **Management**: The kernel controls all resources and can check access rights
   - **Abstraction**: The program doesn't need to know implementation details, just the number

   **Analogy:**

   Think of a bank safe deposit box:
   - You ask the bank (kernel) to create a box (resource)
   - The bank creates a box in the vault (kernel memory)
   - The bank gives you a box number (file descriptor, e.g., 3)
   - You cannot enter the vault directly, but you can use the number for operations
   - When you say "open box 3", the bank knows which box to open

   **In code it looks like this:**

   ```cpp
   // Program calls socket()
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   
   // What happens inside:
   // 1. socket() → system call to kernel
   // 2. Kernel creates socket structure in its memory
   // 3. Kernel finds free slot in process descriptor table (e.g., index 3)
   // 4. Kernel writes: table[3] = pointer_to_socket_structure
   // 5. socket() returns 3
   // 6. Now fd = 3, and this number is the only link between program and resource
   
   // All subsequent operations use this number:
   bind(fd, ...);    // Kernel: "find resource by number 3 and execute bind"
   listen(fd, ...);   // Kernel: "find resource by number 3 and execute listen"
   close(fd);         // Kernel: "find resource by number 3 and delete it"
   ```

   **Unix Philosophy: "Everything is a file":**
   - In Unix/Linux, many resources are represented as files or file descriptors:
     - Regular files on disk
     - Directories
     - Devices (printers, disks)
     - Sockets (network connections)
     - Pipes
   - This allows using the same system calls (`read`, `write`, `close`) for working with different resources

   **How it works inside a process:**

   Every process in Unix/Linux has a file descriptor table. The first three descriptors (0, 1, 2) **are always present** in every process and are created automatically when the process starts:

   ```
   Process has a file descriptor table:
   
   Index | Resource
   ------|------------------
   0     | stdin (standard input)      ← ALWAYS present in every process
   1     | stdout (standard output)    ← ALWAYS present in every process
   2     | stderr (standard error)     ← ALWAYS present in every process
   3     | Socket (created by socket())  ← Example: created by program
   4     | Regular file (opened by open()) ← Example: created by program
   5     | Another socket (accepted by accept()) ← Example: created by program
   ...
   ```

   **Important:**
   - Descriptors **0, 1, 2** are **standard descriptors** that **every process** has by default
   - Descriptors **3, 4, 5 and beyond** are examples of additional descriptors created by the program when calling `socket()`, `open()`, `accept()`, etc.
   - When you create a new resource, the OS finds the first free slot in the table (usually starting from 3, since 0-2 are already occupied)

   **What happens when creating a socket:**

   ```cpp
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   // 1. Program calls socket()
   // 2. OS kernel creates internal data structure for socket in kernel memory
   // 3. Kernel allocates free file descriptor (e.g., 3)
   // 4. Kernel links this descriptor to the created socket
   // 5. Kernel returns number 3 to program
   // 6. Now fd = 3 - this is the "handle" for working with this socket
   ```

   **Internal structure in kernel:**

   When you create a socket, the OS kernel creates an internal data structure that contains:
   - Socket type (TCP, UDP)
   - Connection state
   - Buffers for incoming and outgoing data
   - Address and port (after bind())
   - Flags (non-blocking, close-on-exec, etc.)
   - Reference to linked socket (for TCP)

   A file descriptor is simply an index in the process table that points to this internal structure in the kernel.

2. **sockaddr_in Structure:**
   - Stores address and port (IP + port)
   - Example: 127.0.0.1:8080

3. **Socket Types:**
   - **Listening socket** - waits for incoming connections
   - **Client socket** - connection to server or accepted connection

---

## Socket Lifecycle on Server

### Step 1: Creating a Socket

```cpp
int fd = socket(AF_INET, SOCK_STREAM, 0);
// Creates an "empty" socket, not yet bound to any address
// AF_INET = IPv4 protocol
// SOCK_STREAM = TCP protocol (reliable, connection-oriented)
// Returns file descriptor (e.g., fd = 3)
```

**What happens:**
- OS creates data structure for socket
- Allocates file descriptor
- Socket exists but not yet used

### Step 2: Binding to Address (bind)

```cpp
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);        // Port 8080
addr.sin_addr.s_addr = INADDR_ANY;  // 0.0.0.0 (all interfaces)
bind(fd, (struct sockaddr*)&addr, sizeof(addr));
```

**What happens:**
- Socket is bound to specific IP address and port
- Now socket "listens" on this address
- Other programs can connect to this address

**Visualization:**
```
Before bind():
Socket fd=3: [not bound] ──┐
                            │ (listening to nothing)
                            │

After bind():
Socket fd=3: [127.0.0.1:8080] ──┐
                                 │ (listening on this address)
                                 │
                          Clients can connect
                          to 127.0.0.1:8080
```

### Step 3: Starting to Listen (listen)

```cpp
listen(fd, 128);
// Transitions socket to "passive" mode
// 128 = size of queue for pending connections
```

**What happens:**
- Socket enters waiting mode for incoming connections
- OS creates queue for pending connections
- Socket is ready to accept new connections via `accept()`

**Visualization:**
```
Listening socket fd=3:
┌─────────────────────────────────────┐
│ Socket: 127.0.0.1:8080             │
│ Status: LISTENING                   │
│ Queue: [empty]                      │
└─────────────────────────────────────┘
         │
         │ (waiting for connections)
         │
```

### Step 4: Accepting Connection (accept)

```cpp
int client_fd = accept(fd, NULL, NULL);
// When client connects, accept() creates a NEW socket
// for communication with this specific client
// Returns new file descriptor (e.g., client_fd = 5)
```

**What happens:**
- When client connects to 127.0.0.1:8080, OS notifies program
- `accept()` creates new socket specifically for this client
- Old socket (fd=3) continues listening for new connections
- New socket (fd=5) is used for communication with client

**Visualization:**
```
Before accept():
┌─────────────────────────────────────┐
│ Listening socket fd=3               │
│ 127.0.0.1:8080                     │
│ Queue: [client waiting...]          │
└─────────────────────────────────────┘

After accept():
┌─────────────────────────────────────┐
│ Listening socket fd=3               │ ← continues listening
│ 127.0.0.1:8080                     │
│ Queue: [empty]                      │
└─────────────────────────────────────┘
         │
         │ (connection accepted)
         │
┌─────────────────────────────────────┐
│ Client socket fd=5                  │ ← new socket for client
│ Client: 192.168.1.100:54321        │
│ Used for communication with client  │
└─────────────────────────────────────┘
```

---

## Listening vs Client Sockets

### Listening Socket

- **One per port** - created once when server starts
- **Waits for connections** - calls `accept()` to accept new clients
- **Doesn't transmit data directly** - only accepts connections
- **Lives permanently** - exists for entire server runtime

### Client Socket

- **One per client** - created for each new connection
- **Transmits data** - used for `recv()` and `send()`
- **Temporary** - closed after communication with client ends
- **Many simultaneously** - can have multiple client sockets

---

## File Descriptors

**File descriptor** - an integer that represents an open resource in the system:
- Regular file: `fd = open("file.txt")` → fd = 3
- Socket: `fd = socket(...)` → fd = 4
- Pipe: `fd = pipe(...)` → fd = 5

**Important to understand:**
- File descriptor is not the socket itself, but a "handle" to access it
- When you call `recv(fd, ...)`, system knows what to do with this fd
- Closing fd (`close(fd)`) releases resources

**Example:**
```cpp
int sock_fd = socket(AF_INET, SOCK_STREAM, 0);  // sock_fd = 3
bind(sock_fd, ...);                              // use fd=3
listen(sock_fd, 128);                            // use fd=3

int client_fd = accept(sock_fd, ...);           // client_fd = 5 (new!)
recv(client_fd, buffer, size, 0);               // read from client_fd=5
send(client_fd, data, size, 0);                 // write to client_fd=5
close(client_fd);                                // close client_fd=5
// sock_fd=3 is still open and continues listening!
```

---

## Data Transmission

### Sending Data (send)

```cpp
send(client_fd, "Hello", 5, 0);
```

**What happens:**
1. Data is copied to OS kernel buffer
2. OS sends data over network to client
3. Function returns number of bytes sent

**Visualization:**
```
Program:
  send(fd=5, "Hello", 5, 0)
         │
         │ (copying to OS buffer)
         ↓
OS Kernel:
  [Socket buffer fd=5]
  "Hello"
         │
         │ (sending over network)
         ↓
Network:
  TCP packets with data "Hello"
         │
         ↓
Client receives data
```

### Receiving Data (recv)

```cpp
char buffer[1024];
int bytes = recv(client_fd, buffer, 1024, 0);
```

**What happens:**
1. OS receives data from network
2. Data is copied to kernel buffer
3. `recv()` copies data from kernel buffer to program buffer
4. Function returns number of bytes read

**Visualization:**
```
Client sends data:
  "GET / HTTP/1.1\r\n..."
         │
         │ (over network)
         ↓
OS Kernel:
  [Socket buffer fd=5]
  "GET / HTTP/1.1\r\n..."
         │
         │ (recv() copies data)
         ↓
Program:
  char buffer[1024];
  recv(fd=5, buffer, 1024, 0)
  // buffer now contains "GET / HTTP/1.1\r\n..."
```

---

## Non-blocking Mode

### Blocking Mode (default)

```cpp
// Blocking socket
recv(fd, buffer, 1024, 0);
// Program FREEZES here until data arrives
// Can wait for seconds, minutes, or even infinitely
```

### Non-blocking Mode

```cpp
// Non-blocking socket
fcntl(fd, F_SETFL, O_NONBLOCK);
recv(fd, buffer, 1024, 0);
// If no data, function IMMEDIATELY returns -1
// errno = EAGAIN or EWOULDBLOCK
// Program does NOT wait and can do other work
```

**Why is this needed?**
- Allows handling multiple connections in a single thread
- Program doesn't block on one client
- Can use `poll()` or `select()` to monitor multiple sockets

---

## Socket Class in Project

### In Context of Our Project

In the `Socket` class:
- **Listening socket** is created in `Server::init()`
- Stores file descriptor in `fd_`
- Used to accept new connections
- Each `accept()` creates new file descriptor for client
- This new descriptor is wrapped in `Connection` object

**Data Flow:**
```
ConfigParser → ServerConfig (addresses and ports)
    ↓
Server::init() → creates Socket objects
    ↓
Socket::bind() → binds to address (creates listening socket)
Socket::listen() → starts listening
    ↓
Server::run() → event loop with poll()
    ↓
New connection → Socket::accept() → new client_fd
    ↓
Connection(client_fd) → wrapper around client socket
    ↓
recv()/send() → communication with client via client_fd
```

### Real Code Example

```cpp
// 1. Creating listening socket
Socket* socket = new Socket();
socket->bind("127.0.0.1", 8080);  // Creates socket, binds to address
socket->listen(128);               // Starts listening
// Now socket->getFd() = 3 (for example)

// 2. In event loop, when connection arrives
int client_fd = socket->accept();  // Returns new fd = 5
// socket->getFd() is still = 3 (continues listening)

// 3. Creating Connection for client
Connection* conn = new Connection(client_fd, "127.0.0.1");
// Connection stores client_fd = 5

// 4. Communication with client
recv(client_fd, buffer, size, 0);  // Read from fd=5
send(client_fd, response, size, 0); // Write to fd=5

// 5. Closing client connection
conn->close();  // Closes fd=5
// socket->getFd() = 3 is still open and listening!
```

**Key Point:**
- **Listening socket (fd=3)** lives for entire server runtime
- **Client sockets (fd=5, 6, 7...)** are created and destroyed for each client
- One listening socket can create many client sockets

### Socket Class Purpose

The `Socket` class encapsulates work with system sockets (socket API). Creates listening sockets for server.

### Key Methods

- `bind(host, port)` - binds socket to address and port
- `listen(backlog)` - transitions socket to listening mode
- `accept()` - accepts new connection (returns client file descriptor)
- `setNonBlocking()` - sets non-blocking mode
- `setCloseOnExec()` - sets FD_CLOEXEC flag

### Process of Creating Listening Socket

1. **socket()** - creates TCP socket (AF_INET, SOCK_STREAM)
2. **setsockopt(SO_REUSEADDR)** - allows address reuse
3. **setNonBlocking()** - makes socket non-blocking (important for event loop)
4. **bind()** - binds to host:port
5. **listen()** - starts listening for incoming connections

### Important Details

- Socket stores its file descriptor (`fd_`)
- After `bind()`, socket is ready to accept connections via `accept()`
- Each `accept()` returns new file descriptor for client

---

## Summary

1. **Socket** is a programming interface for network communication
2. **File descriptor** is a number used to work with socket
3. **Listening socket** waits for new connections (one per port)
4. **Client socket** is used for communication with specific client (many simultaneously)
5. **Non-blocking mode** allows handling multiple connections in single thread
6. One listening socket can create many client sockets
7. Data transmission happens through OS kernel buffers

For more details about socket implementation in this project, see:
- `include/Socket.hpp` - Socket class interface
- `src/Socket.cpp` - Socket class implementation
- `docs/ARCHITECTURE.md` - Overall architecture documentation
