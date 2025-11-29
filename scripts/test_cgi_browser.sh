#!/bin/bash

# Simple script to test CGI and optionally open in browser

SERVER_URL="http://127.0.0.1:8080"
SCRIPT="${1:-env.py}"

if [ -z "$1" ]; then
    echo "Usage: $0 <cgi_script> [open_in_browser]"
    echo "Example: $0 env.py"
    echo "Example: $0 env.py open"
    echo ""
    echo "Available scripts:"
    ls -1 www/cgi/*.py 2>/dev/null | xargs -n1 basename
    exit 1
fi

URL="${SERVER_URL}/cgi/${SCRIPT}"

echo "Testing: ${URL}"
echo ""

# Fetch and display
response=$(curl -s -w "\n%{http_code}" "${URL}")
http_code=$(echo "$response" | tail -n1)
body=$(echo "$response" | sed '$d')

echo "HTTP Status: ${http_code}"
echo ""

if [ "$2" = "open" ]; then
    echo "$body" > /tmp/cgi_test.html
    echo "Opening in browser..."
    open /tmp/cgi_test.html
else
    echo "$body"
fi
