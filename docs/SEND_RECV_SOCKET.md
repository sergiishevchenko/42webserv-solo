# Detailed Explanation of Sending and Receiving Data Through Sockets

## Table of Contents
1. [Sending Data (send) - Detailed Breakdown](#sending-data-send)
2. [Receiving Data (recv) - Detailed Breakdown](#receiving-data-recv)
3. [Buffering at Different Levels](#buffering-at-different-levels)
4. [TCP Protocol and Segmentation](#tcp-protocol-and-segmentation)
5. [Partial Send and Receive](#partial-send-and-receive)
6. [Blocking vs Non-blocking Operations](#blocking-vs-non-blocking-operations)
7. [Examples from Real Code](#examples-from-real-code)

---

## Sending Data (send)

### Basic Call

```cpp
ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
```

### Step-by-Step Process

#### Step 1: Calling the send() Function

```cpp
// Program calls send()
send(fd=5, "HTTP/1.1 200 OK\r\n...", 100, 0);
```

**What happens:**
- Program transfers control to OS kernel (system call)
- Kernel receives parameters: file descriptor, data pointer, size, flags
- Kernel finds socket structure by file descriptor (fd=5)

#### Step 2: Copying Data to Kernel Buffer

```
Program (user space):
┌─────────────────────────────────────┐
│ char* data = "HTTP/1.1 200 OK...";  │
│ size = 100 bytes                    │
└──────────────┬──────────────────────┘
               │
               │ send(fd=5, data, 100, 0)
               │ (system call)
               ↓
OS Kernel (kernel space):
┌─────────────────────────────────────┐
│ Socket fd=5 structure:              │
│ ┌───────────────────────────────┐   │
│ │ Send Buffer (kernel buffer)   │   │
│ │ [empty]                       │   │
│ └───────────────────────────────┘   │
└─────────────────────────────────────┘
```

**What happens:**
- Kernel copies data from user space to kernel buffer
- This copying is necessary for security (program cannot directly write to network stack)
- Data is placed in the **send buffer** (send buffer) of the socket

#### Step 3: Data in Kernel Buffer

```
OS Kernel (kernel space):
┌─────────────────────────────────────┐
│ Socket fd=5 structure:              │
│ ┌───────────────────────────────┐   │
│ │ Send Buffer                   │   │
│ │ "HTTP/1.1 200 OK\r\n..."      │   │ ← 100 bytes
│ │ [100 bytes copied]            │   │
│ └───────────────────────────────┘   │
│                                     │
│ State: ESTABLISHED                  │
│ Remote: 192.168.1.100:54321         │
└─────────────────────────────────────┘
```

**Important:**
- `send()` does **NOT wait** for data to be sent over the network
- `send()` returns control to the program **immediately after copying to kernel buffer**
- Actual network transmission happens **asynchronously** by the OS kernel

#### Step 4: TCP Processing and Segmentation

```
OS Kernel:
┌─────────────────────────────────────┐
│ Send Buffer: "HTTP/1.1 200 OK..."   │
└──────────────┬──────────────────────┘
               │
               │ TCP layer processing
               ↓
┌─────────────────────────────────────┐
│ TCP Layer:                          │
│ 1. Adds TCP header                  │
│ 2. Splits into segments (MSS)       │
│ 3. Adds sequence numbers            │
│ 4. Adds checksums                   │
└──────────────┬──────────────────────┘
               │
               │ IP layer processing
               ↓
┌─────────────────────────────────────┐
│ IP Layer:                           │
│ 1. Adds IP header                   │
│ 2. Routing                          │
└──────────────┬──────────────────────┘
               │
               │ Network interface
               ↓
Network: TCP/IP packets
```

**TCP Segmentation:**
- If data is large (e.g., 100 KB), TCP splits it into segments
- Segment size (MSS - Maximum Segment Size) is usually 1460 bytes for Ethernet
- Each segment gets its own sequence number

#### Step 5: Sending Over Network

```
OS Kernel → Network Interface → Network → Client
```

**What happens:**
- Kernel sends TCP segments through network interface
- Transmission happens **asynchronously** (does not block the program)
- Kernel manages retransmission on packet loss (TCP reliability)

#### Step 6: Return from send()

```cpp
ssize_t bytes_sent = send(fd=5, data, 100, 0);
// bytes_sent = 100 (or less, see partial send)
```

**Possible return values:**
- `> 0`: number of bytes copied to kernel buffer (usually equals requested size)
- `-1`: error (errno contains error code)
- `0`: socket closed (rare for send)

### Full Process Visualization

```
┌─────────────────────────────────────────────────────────────┐
│ PROGRAM (User Space)                                        │
│                                                             │
│  std::string response = "HTTP/1.1 200 OK\r\n...";           │
│  send(fd=5, response.c_str(), 100, 0);                      │
│         │                                                   │
│         │ (1) System call                                   │
│         ↓                                                   │
└─────────┼───────────────────────────────────────────────────┘
          │
          │ (switch to kernel mode)
          ↓
┌─────────────────────────────────────────────────────────────┐
│ OS KERNEL (Kernel Space)                                    │
│                                                             │
│  (2) Finds socket by fd=5                                   │
│  (3) Copies 100 bytes to send buffer                        │
│      ┌──────────────────────────────┐                       │
│      │ Send Buffer (fd=5)           │                       │
│      │ "HTTP/1.1 200 OK\r\n..."     │                       │
│      └──────────┬───────────────────┘                       │
│                 │                                           │
│                 │ (4) TCP processing                        │
│                 ↓                                           │
│      ┌──────────────────────────────┐                       │
│      │ TCP Layer                    │                       │
│      │ - Adds headers               │                       │
│      │ - Segmentation (if needed)   │                       │
│      └──────────┬───────────────────┘                       │
│                 │                                           │
│                 │ (5) IP processing                         │
│                 ↓                                           │
│      ┌──────────────────────────────┐                       │
│      │ IP Layer                     │                       │
│      │ - Routing                    │                       │
│      └──────────┬───────────────────┘                       │
│                 │                                           │
│                 │ (6) Send over network (asynchronously)    │
│                 ↓                                           │
│  (7) Return to program: bytes_sent = 100                    │
└─────────┼───────────────────────────────────────────────────┘
          │
          │ (return to user mode)
          ↓
┌─────────────────────────────────────────────────────────────┐
│ PROGRAM (User Space)                                        │
│                                                             │
│  // send() returned 100                                     │
│  // Data is already in kernel buffer, transmission continues│
│  // Program can continue working                            │
└─────────────────────────────────────────────────────────────┘
```

---

## Receiving Data (recv)

### Basic Call

```cpp
char buffer[4096];
ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
```

### Step-by-Step Process

#### Step 1: Client Sends Data

```
Client (browser):
┌─────────────────────────────────────┐
│ "GET / HTTP/1.1\r\n"                │
│ "Host: 127.0.0.1:8080\r\n"          │
│ "User-Agent: curl/7.68.0\r\n"       │
│ "\r\n"                              │
│ [sends over network]                │
└──────────────┬──────────────────────┘
               │
               │ TCP/IP packets
               ↓
```

#### Step 2: Data Arrives in OS Kernel

```
Network → Network Interface → OS Kernel
```

**What happens:**
- Network interface receives TCP/IP packets
- Kernel processes IP headers
- Kernel processes TCP headers
- Kernel assembles TCP segments in correct order
- Data is placed in the **receive buffer** (receive buffer) of the socket

```
OS Kernel (kernel space):
┌─────────────────────────────────────┐
│ Socket fd=5 structure:              │
│ ┌───────────────────────────────┐   │
│ │ Receive Buffer                │   │
│ │ "GET / HTTP/1.1\r\n"          │   │
│ │ "Host: 127.0.0.1:8080\r\n"    │   │
│ │ "User-Agent: curl/7.68.0\r\n" │   │
│ │ "\r\n"                        │   │
│ │ [~80 bytes received]          │   │
│ └───────────────────────────────┘   │
│                                     │
│ State: ESTABLISHED                  │
│ Remote: 192.168.1.100:54321         │
└─────────────────────────────────────┘
```

**Important:**
- Data is already in kernel buffer **before recv() is called**
- Kernel automatically assembles TCP segments
- Kernel acknowledges receipt (ACK) to client

#### Step 3: Program Calls recv()

```cpp
// Program calls recv()
char buffer[4096];
ssize_t bytes_read = recv(fd=5, buffer, 4095, 0);
```

**What happens:**
- Program transfers control to OS kernel (system call)
- Kernel finds socket structure by file descriptor (fd=5)
- Kernel checks if there is data in receive buffer

#### Step 4: Copying Data from Kernel Buffer

```
OS Kernel (kernel space):
┌─────────────────────────────────────┐
│ Receive Buffer:                     │
│ "GET / HTTP/1.1\r\n..."             │
│ [80 bytes available]                │
└──────────────┬──────────────────────┘
               │
               │ recv() copies data
               ↓
Program (user space):
┌─────────────────────────────────────┐
│ char buffer[4096];                  │
│ buffer = "GET / HTTP/1.1\r\n..."    │
│ [80 bytes copied]                   │
└─────────────────────────────────────┘
```

**What happens:**
- Kernel copies data from receive buffer to program buffer
- **As many bytes as available** are copied (but not more than requested size)
- Data remains in kernel receive buffer until next recv() call

#### Step 5: Return from recv()

```cpp
ssize_t bytes_read = recv(fd=5, buffer, 4095, 0);
// bytes_read = 80 (number of bytes read)
buffer[bytes_read] = '\0';  // null-terminate for string
```

**Possible return values:**
- `> 0`: number of bytes read from kernel buffer
- `0`: connection closed by client (EOF)
- `-1`: error (errno contains error code)

### Full Process Visualization

```
┌─────────────────────────────────────────────────────────────┐
│ CLIENT (browser)                                            │
│                                                             │
│  Sends: "GET / HTTP/1.1\r\n..."                             │
│         │                                                   │
│         │ (1) TCP/IP packets over network                   │
│         ↓                                                   │
└─────────┼───────────────────────────────────────────────────┘
          │
          │
          ↓
┌─────────────────────────────────────────────────────────────┐
│ OS KERNEL (Kernel Space)                                    │
│                                                             │
│  (2) Network interface receives packets                     │
│  (3) TCP layer assembles segments                           │
│  (4) Data is placed in receive buffer                       │
│      ┌──────────────────────────────┐                       │
│      │ Receive Buffer (fd=5)        │                       │
│      │ "GET / HTTP/1.1\r\n..."      │                       │
│      │ [80 bytes]                    │                      │
│      └──────────┬───────────────────┘                       │
│                 │                                           │
│                 │ (waits for recv() call from program)      │
└─────────┼───────────────────────────────────────────────────┘
          │
          │ (program calls recv())
          ↓
┌─────────────────────────────────────────────────────────────┐
│ PROGRAM (User Space)                                        │
│                                                             │
│  char buffer[4096];                                         │
│  recv(fd=5, buffer, 4095, 0);                               │
│         │                                                   │
│         │ (5) System call                                   │
│         ↓                                                   │
└─────────┼───────────────────────────────────────────────────┘
          │
          │ (switch to kernel mode)
          ↓
┌─────────────────────────────────────────────────────────────┐
│ OS KERNEL (Kernel Space)                                    │
│                                                             │
│  (6) Finds socket by fd=5                                   │
│  (7) Copies data from receive buffer to program buffer      │
│      ┌──────────────────────────────┐                       │
│      │ Receive Buffer (fd=5)        │                       │
│      │ [empty after copy]           │                       │
│      └──────────────────────────────┘                       │
│  (8) Return: bytes_read = 80                                │
└─────────┼───────────────────────────────────────────────────┘
          │
          │ (return to user mode)
          ↓
┌─────────────────────────────────────────────────────────────┐
│ PROGRAM (User Space)                                        │
│                                                             │
│  // recv() returned 80                                      │
│  // buffer contains "GET / HTTP/1.1\r\n..."                 │
│  // Program can process the data                            │
└─────────────────────────────────────────────────────────────┘
```

---

## Buffering at Different Levels

### Level 1: Program Buffer (User Space Buffer)

```cpp
char buffer[4096];  // Buffer in program memory
```

**Characteristics:**
- Located in process memory (user space)
- Managed by program
- Size determined by programmer
- Used for temporary data storage

### Level 2: Kernel Socket Buffer

**Send Buffer (SO_SNDBUF):**
- Default size: usually 8-64 KB (depends on OS)
- Stores data ready to send
- Cleared after successful transmission

**Receive Buffer (SO_RCVBUF):**
- Default size: usually 8-64 KB (depends on OS)
- Stores incoming data until recv() is called
- Filled by kernel when packets arrive

### Level 3: Network Stack Buffer

- TCP buffers for segmentation
- IP buffers for routing
- Network interface buffers

### Visualization of All Levels

```
┌─────────────────────────────────────────────────────────────┐
│ PROGRAM                                                     │
│                                                             │
│  char user_buffer[4096];  ← Level 1: User Space Buffer      │
│                                                             │
└──────────────┬──────────────────────────────────────────────┘
               │
               │ send() / recv() system calls
               ↓
┌────────────────────────────────────────────────────────────--┐
│ OS KERNEL                                                    │
│                                                              │
│  Socket fd=5:                                                │
│  ┌──────────────────────────────┐                            │
│  │ Send Buffer (8-64 KB)        │ ← Level 2: Socket Buffer   │
│  │ Receive Buffer (8-64 KB)     │                            │
│  └──────────┬───────────────────┘                            │
│             │                                                │
│             │ TCP/IP processing                              │
│             ↓                                                │
│  ┌──────────────────────────────┐                            │
│  │ TCP/IP Stack Buffers         │ ← Level 3: Network Stack   │
│  └──────────┬───────────────────┘                            │
│             │                                                │
│             │ Network interface                              │
│             ↓                                                │
└─────────────┼────────────────────────────────────────────────┘
              │
              │ Network (Ethernet, Wi-Fi, etc.)
              ↓
```

---

## TCP Protocol and Segmentation

### What is TCP Segmentation

TCP automatically splits large data into segments for network transmission.

### Segmentation Example

```
Program sends:
  "HTTP/1.1 200 OK\r\nContent-Length: 100000\r\n\r\n" + [100 KB data]

TCP splits into segments:
  Segment 1: [TCP header] + "HTTP/1.1 200 OK\r\n..." (1460 bytes)
  Segment 2: [TCP header] + [data] (1460 bytes)
  Segment 3: [TCP header] + [data] (1460 bytes)
  ...
  Segment N: [TCP header] + [remaining data] (< 1460 bytes)
```

### What TCP Does Automatically

1. **Segmentation**: splits data into segments (MSS)
2. **Numbering**: assigns sequence number to each segment
3. **Ordering**: guarantees correct segment order
4. **Reliability**: retransmission on packet loss
5. **Flow Control**: manages transmission speed
6. **Assembly**: assembles segments in correct order on receiving side

### TCP Segmentation Visualization

```
Program:
  send(fd, large_data, 100000, 0);
         │
         │ (copying to send buffer)
         ↓
Send Buffer (kernel):
  [100000 bytes of data]
         │
         │ (TCP segmentation)
         ↓
TCP Layer:
  Segment 1: seq=1,    data[0:1460]     → network
  Segment 2: seq=1461, data[1460:2920]  → network
  Segment 3: seq=2921, data[2920:4380]  → network
  ...
         │
         │ (network)
         ↓
Client TCP Layer:
  Receives segments (possibly out of order)
  Assembles by sequence numbers
         │
         │ (assembly into receive buffer)
         ↓
Receive Buffer (client kernel):
  [100000 bytes of data in correct order]
         │
         │ (recv() copies to program)
         ↓
Client Program:
  recv() receives all 100000 bytes
```

---

## Partial Send and Receive

### Partial Send

**When it happens:**
- Send buffer is full (no space)
- Socket in non-blocking mode

```cpp
// Attempting to send 1000 bytes
ssize_t sent = send(fd, data, 1000, 0);

// Possible results:
// - sent = 1000  (all sent)
// - sent = 500   (only part sent, buffer full)
// - sent = -1    (error, errno = EAGAIN in non-blocking mode)
```

**Solution:**
```cpp
std::string response = "HTTP/1.1 200 OK\r\n...";
size_t total = response.length();
size_t sent_total = 0;

while (sent_total < total) {
    ssize_t sent = send(fd, response.c_str() + sent_total, 
                       total - sent_total, 0);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Buffer full, need to wait
            // Use poll() with POLLOUT
            break;
        } else {
            // Error
            break;
        }
    }
    sent_total += sent;
}
```

### Partial Receive

**When it happens:**
- Data arrives in parts over network
- Not all data has been received yet

```cpp
char buffer[4096];
ssize_t bytes_read = recv(fd, buffer, 4095, 0);

// Possible results:
// - bytes_read = 4095  (buffer full, but more data may exist)
// - bytes_read = 100   (only 100 bytes received, rest still in transit)
// - bytes_read = 0     (connection closed)
// - bytes_read = -1    (error or no data in non-blocking mode)
```

**Solution for HTTP:**
```cpp
std::string request;
char buffer[4096];

while (true) {
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data, need to wait
            break;
        } else {
            // Error
            break;
        }
    }
    
    if (bytes_read == 0) {
        // Connection closed
        break;
    }
    
    buffer[bytes_read] = '\0';
    request += std::string(buffer, bytes_read);
    
    // Check if full HTTP request received
    if (request.find("\r\n\r\n") != std::string::npos) {
        // Full request received
        break;
    }
}
```

---

## Blocking vs Non-blocking Operations

### Blocking Mode (default)

```cpp
// Socket in blocking mode
recv(fd, buffer, 1024, 0);
// Program FREEZES here
// Waits until data arrives
// Can wait for seconds, minutes, or infinitely
```

**Characteristics:**
- Program blocks until data is received
- Cannot handle multiple connections in single thread
- Simple to use

### Non-blocking Mode

```cpp
// Setting non-blocking mode
fcntl(fd, F_SETFL, O_NONBLOCK);

// Now recv() does not block
ssize_t bytes = recv(fd, buffer, 1024, 0);
if (bytes < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No data, but this is not an error
        // Program can do other work
    }
}
```

**Characteristics:**
- Function returns immediately
- If no data, returns -1 with errno = EAGAIN/EWOULDBLOCK
- Allows handling multiple connections in single thread
- Requires use of poll()/select()/epoll()

### Comparison

| Characteristic | Blocking | Non-blocking |
|---------------|----------|--------------|
| Behavior when no data | Waits | Returns immediately |
| Handling multiple connections | Need threads | Single thread with poll() |
| Complexity | Simple | More complex |
| Performance | Lower | Higher |

### In Your Project

```108:113:src/Server.cpp
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        LOG_ERROR() << "Failed to set non-blocking mode for client: "
                    << strerror(errno) << std::endl;
        ::close(client_fd);
        return;
    }
```

All client sockets are set to non-blocking mode to handle multiple connections through `poll()`.

---

## Examples from Real Code

### Example 1: Receiving Data (from Server.cpp)

```138:159:src/Server.cpp
    char buffer[4096];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR() << "recv() failed for fd " << fd << ": "
                        << strerror(errno) << std::endl;
            closeConnection(fd);
        }
        return;
    }

    if (bytes_read == 0) {
        LOG_INFO() << "Connection closed by client (fd: " << fd << ")"
                   << std::endl;
        closeConnection(fd);
        return;
    }

    buffer[bytes_read] = '\0';
    LOG_DEBUG() << "Received " << bytes_read << " bytes from fd " << fd
                << std::endl;
```

**What happens:**
1. `recv()` call attempts to read up to 4095 bytes
2. If `bytes_read < 0` and `errno == EAGAIN/EWOULDBLOCK` - this is normal for non-blocking socket (no data)
3. If `bytes_read == 0` - client closed connection
4. If `bytes_read > 0` - data received

### Example 2: Sending Data (from Server.cpp)

```161:179:src/Server.cpp
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Content-Length: 13\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += "Hello, World!";

    ssize_t bytes_sent = send(fd, response.c_str(), response.length(), 0);
    if (bytes_sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR() << "send() failed for fd " << fd << ": "
                        << strerror(errno) << std::endl;
            closeConnection(fd);
        }
    } else {
        LOG_DEBUG() << "Sent " << bytes_sent << " bytes to fd " << fd
                    << std::endl;
        closeConnection(fd);
    }
```

**What happens:**
1. HTTP response is formed
2. `send()` call copies data to kernel buffer
3. If `bytes_sent < 0` and `errno == EAGAIN/EWOULDBLOCK` - buffer is full (need to wait)
4. If `bytes_sent >= 0` - data copied to kernel buffer (transmission happens asynchronously)

**Important:** Current implementation does not handle partial send. If `send()` returns less than `response.length()`, data will not be fully sent.

### Improved Version with Partial Send

```cpp
bool sendAll(int fd, const std::string& data) {
    size_t total_sent = 0;
    size_t total = data.length();
    
    while (total_sent < total) {
        ssize_t bytes_sent = send(fd, data.c_str() + total_sent, 
                                  total - total_sent, 0);
        
        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Buffer full, need to wait
                // In real implementation need to use poll() with POLLOUT
                return false;  // Need to retry later
            } else {
                // Real error
                return false;
            }
        }
        
        total_sent += bytes_sent;
    }
    
    return true;  // All sent
}
```

---

## Key Points for Understanding

### 1. send() Does Not Send Directly to Network

- `send()` copies data to kernel buffer
- Actual transmission happens asynchronously by kernel
- `send()` returns immediately after copying

### 2. recv() Reads from Kernel Buffer

- Data is already in kernel buffer before `recv()` is called
- `recv()` copies data from kernel buffer to program
- If buffer is empty, `recv()` blocks (blocking mode) or returns error (non-blocking)

### 3. Buffering at Three Levels

- **User space buffer**: program buffer
- **Kernel socket buffer**: socket buffer in kernel (send/receive buffers)
- **Network stack buffer**: network stack buffers

### 4. TCP Automatically Manages Segmentation

- TCP splits large data into segments
- TCP guarantees order and reliability
- Program should not worry about segmentation

### 5. Partial Send/Receive

- `send()` may not send all data (if buffer is full)
- `recv()` may not receive all data (if it's still in transit)
- Need to handle partial send/receive in loops

### 6. Non-blocking Mode for Multiple Connections

- Non-blocking mode allows handling multiple connections
- Used with `poll()`/`select()`/`epoll()` to monitor readiness
- In your project, all client sockets are in non-blocking mode

---

## Summary

1. **send()**: copies data to kernel buffer → kernel sends asynchronously
2. **recv()**: copies data from kernel buffer to program
3. **Buffering**: three levels (program, kernel socket, network stack)
4. **TCP**: automatically manages segmentation and reliability
5. **Partial send/receive**: need to handle in loops
6. **Non-blocking mode**: necessary for handling multiple connections

For more detailed information see:
- `docs/SOCKETS.md` - general information about sockets
- `src/Server.cpp` - connection handling implementation
- `src/Connection.cpp` - connection management class
