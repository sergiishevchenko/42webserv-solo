#!/bin/bash

# Test script for redirects
# Usage: ./test_redirects.sh [config_file] [port]

CONFIG="${1:-config/test_valid.conf}"
PORT="${2:-8080}"
HOST="127.0.0.1"
BASE_URL="http://${HOST}:${PORT}"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "Testing Redirects"
echo "=========================================="
echo "Config: $CONFIG"
echo "Server: $BASE_URL"
echo ""

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to test redirect
test_redirect() {
    local path=$1
    local expected_status=$2
    local expected_location=$3
    local test_name=$4
    
    echo -n "Testing: $test_name ... "
    
    # Get response with headers
    response=$(curl -s -w "\n%{http_code}\n%{redirect_url}" -L --max-redirs 0 "${BASE_URL}${path}" 2>&1)
    status_code=$(echo "$response" | tail -n 2 | head -n 1)
    location=$(curl -s -I "${BASE_URL}${path}" | grep -i "location:" | cut -d' ' -f2 | tr -d '\r')
    
    # Check status code
    if [ "$status_code" = "$expected_status" ]; then
        status_ok=true
    else
        status_ok=false
    fi
    
    # Check location header (if expected)
    if [ -n "$expected_location" ]; then
        if echo "$location" | grep -q "$expected_location"; then
            location_ok=true
        else
            location_ok=false
        fi
    else
        location_ok=true
    fi
    
    if [ "$status_ok" = true ] && [ "$location_ok" = true ]; then
        echo -e "${GREEN}PASS${NC}"
        echo "  Status: $status_code (expected: $expected_status)"
        if [ -n "$expected_location" ]; then
            echo "  Location: $location"
        fi
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected status: $expected_status, got: $status_code"
        if [ -n "$expected_location" ]; then
            echo "  Expected location: $expected_location"
            echo "  Got location: $location"
        fi
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    echo ""
}

# Test 1: Basic redirect (302 default)
echo "Test 1: Basic redirect (302 default)"
test_redirect "/redirect" "302" "/index.html" "Basic redirect to /index.html"

# Test 2: Check Location header
echo "Test 2: Location header check"
echo -n "Testing: Location header present ... "
location=$(curl -s -I "${BASE_URL}/redirect" | grep -i "location:" | cut -d' ' -f2 | tr -d '\r')
if [ -n "$location" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "  Location: $location"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "  Location header not found"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
echo ""

# Test 3: Check Content-Length is 0
echo "Test 3: Content-Length check"
echo -n "Testing: Content-Length is 0 ... "
content_length=$(curl -s -I "${BASE_URL}/redirect" | grep -i "content-length:" | cut -d' ' -f2 | tr -d '\r')
if [ "$content_length" = "0" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "  Content-Length: $content_length"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "  Expected: 0, got: $content_length"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
echo ""

# Test 4: Verbose output
echo "Test 4: Verbose redirect response"
echo "Full response headers:"
curl -v "${BASE_URL}/redirect" 2>&1 | grep -E "< HTTP|< Location|< Content-Length" || true
echo ""

# Test 5: Follow redirect (should get final content)
echo "Test 5: Follow redirect automatically"
echo -n "Testing: Following redirect gets final content ... "
final_content=$(curl -s -L "${BASE_URL}/redirect" | head -c 20)
if [ -n "$final_content" ]; then
    echo -e "${GREEN}PASS${NC}"
    echo "  Got content: ${final_content}..."
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "  No content received"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
echo ""

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
