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

# Method 1: Using /proc (Linux only)
echo "--- Method 1: /proc/$PID/fd/ ---"
echo ""
if [ -d "/proc/$PID/fd" ]; then
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
else
    echo "(/proc not available on this system - using lsof instead)"
fi

echo ""
echo "--- Method 2: lsof output ---"
echo ""
if command -v lsof >/dev/null 2>&1; then
    LSOF_OUTPUT=$(lsof -p $PID 2>/dev/null)
    echo "$LSOF_OUTPUT" | head -20
    
    # Parse lsof output for summary
    echo ""
    echo "--- Summary ---"
    echo ""
    
    # Count total file descriptors (exclude header and txt entries that are just loaded libraries)
    # Extract FD numbers (format: "3u" -> "3") and count unique ones
    FD_COUNT=$(echo "$LSOF_OUTPUT" | awk 'NR>1 && $4 != "txt" && $4 ~ /^[0-9]/ {
        # Extract numeric part of FD (e.g., "3u" -> "3")
        gsub(/[^0-9].*/, "", $4)
        if ($4 != "") print $4
    }' | sort -u | wc -l | tr -d ' ')
    echo "Total file descriptors: $FD_COUNT"
    echo ""
    
    # Show standard descriptors
    echo "Standard descriptors:"
    STDIN=$(echo "$LSOF_OUTPUT" | awk 'NR>1 && $4 ~ /^0/ {
        name = ""
        for (i=9; i<=NF; i++) name = name (i>9 ? " " : "") $i
        print name
        exit
    }')
    STDOUT=$(echo "$LSOF_OUTPUT" | awk 'NR>1 && $4 ~ /^1/ {
        name = ""
        for (i=9; i<=NF; i++) name = name (i>9 ? " " : "") $i
        print name
        exit
    }')
    STDERR=$(echo "$LSOF_OUTPUT" | awk 'NR>1 && $4 ~ /^2/ {
        name = ""
        for (i=9; i<=NF; i++) name = name (i>9 ? " " : "") $i
        print name
        exit
    }')
    echo "  FD 0 (stdin):  ${STDIN:-N/A}"
    echo "  FD 1 (stdout): ${STDOUT:-N/A}"
    echo "  FD 2 (stderr): ${STDERR:-N/A}"
    echo ""
    
    # Count sockets (IPv4 and IPv6)
    SOCKET_COUNT=$(echo "$LSOF_OUTPUT" | awk 'NR>1 && ($5 == "IPv4" || $5 == "IPv6") {
        # Extract numeric part of FD
        gsub(/[^0-9].*/, "", $4)
        if ($4 != "") print $4
    }' | sort -u | wc -l | tr -d ' ')
    echo "Socket descriptors: $SOCKET_COUNT"
    
    # List socket descriptors
    if [ "$SOCKET_COUNT" -gt 0 ]; then
        echo ""
        echo "Socket details:"
        echo "$LSOF_OUTPUT" | awk 'NR>1 && ($5 == "IPv4" || $5 == "IPv6") {
            fd = $4
            type = $5
            name = ""
            for (i=9; i<=NF; i++) name = name (i>9 ? " " : "") $i
            printf "  FD %s: %s%s\n", fd, type, name
        }'
    fi
    echo ""
else
    echo "lsof not installed. Install with: brew install lsof (macOS) or apt-get install lsof (Linux)"
    echo ""
    echo "--- Summary ---"
    echo ""
    echo "Total file descriptors: N/A (lsof not available)"
    echo ""
fi

