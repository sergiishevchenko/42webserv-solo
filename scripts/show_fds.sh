#!/bin/bash

# Script to show file descriptors of webserv process

if [ $# -eq 0 ]; then
    # Find webserv process
    PID=$(pgrep -f "./webserv" | head -n 1)
    if [ -z "$PID" ]; then
        echo "Error: webserv process not found"
        echo "Usage: $0 [PID]"
        echo "   or: $0 (will auto-detect webserv process)"
        exit 1
    fi
else
    PID=$1
fi

echo "=========================================="
echo "File Descriptors for PID: $PID"
echo "=========================================="
echo ""

# Check if process exists
if ! kill -0 $PID 2>/dev/null; then
    echo "Error: Process $PID does not exist"
    exit 1
fi

# Show process info
echo "Process: $(ps -p $PID -o comm=)"
echo "Command: $(ps -p $PID -o args= | cut -c1-80)"
echo ""

# Method 1: Using /proc
echo "--- Method 1: /proc/$PID/fd/ ---"
echo ""
ls -lah /proc/$PID/fd/ 2>/dev/null | tail -n +2 | while read line; do
    FD=$(echo "$line" | awk '{print $9}')
    LINK=$(echo "$line" | awk '{print $10" "$11" "$12" "$13}')
    
    # Check if it's a socket
    if [ -L /proc/$PID/fd/$FD ]; then
        TARGET=$(readlink /proc/$PID/fd/$FD)
        if [[ "$TARGET" == socket:* ]]; then
            INODE=$(echo "$TARGET" | sed 's/socket:\[\(.*\)\]/\1/')
            echo "FD $FD: $TARGET (socket inode: $INODE)"
        elif [[ "$TARGET" == anon_inode:* ]]; then
            echo "FD $FD: $TARGET"
        else
            echo "FD $FD: $TARGET"
        fi
    fi
done

echo ""
echo "--- Method 2: lsof output ---"
echo ""
if command -v lsof >/dev/null 2>&1; then
    lsof -p $PID 2>/dev/null | grep -E "FD|CHR|REG|IPv4|IPv6" | head -20
else
    echo "lsof not installed. Install with: brew install lsof (macOS) or apt-get install lsof (Linux)"
fi

echo ""
echo "--- Summary ---"
echo ""
FD_COUNT=$(ls -1 /proc/$PID/fd/ 2>/dev/null | wc -l | tr -d ' ')
echo "Total file descriptors: $FD_COUNT"
echo ""
echo "Standard descriptors:"
echo "  FD 0 (stdin):  $(readlink /proc/$PID/fd/0 2>/dev/null || echo 'N/A')"
echo "  FD 1 (stdout): $(readlink /proc/$PID/fd/1 2>/dev/null || echo 'N/A')"
echo "  FD 2 (stderr): $(readlink /proc/$PID/fd/2 2>/dev/null || echo 'N/A')"
echo ""

# Count sockets
SOCKET_COUNT=$(ls -l /proc/$PID/fd/ 2>/dev/null | grep -c "socket:" || echo "0")
echo "Socket descriptors: $SOCKET_COUNT"
echo ""

