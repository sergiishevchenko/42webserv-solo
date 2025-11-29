#!/bin/bash

# CGI Testing Script for webserv
# This script tests various CGI functionality

SERVER_URL="http://127.0.0.1:8080"
CONFIG_FILE="config/test_cgi.conf"
SERVER_PID=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

print_test() {
    echo -e "${BLUE}Testing:${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓ PASSED:${NC} $1"
    ((TESTS_PASSED++))
}

print_fail() {
    echo -e "${RED}✗ FAILED:${NC} $1"
    ((TESTS_FAILED++))
}

print_info() {
    echo -e "${YELLOW}INFO:${NC} $1"
}

# Check if server is running
check_server() {
    if ! curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL" > /dev/null 2>&1; then
        return 1
    fi
    return 0
}

# Start server
start_server() {
    print_info "Starting webserv server..."
    ./webserv "$CONFIG_FILE" > /dev/null 2>&1 &
    SERVER_PID=$!
    
    # Wait for server to start
    sleep 2
    
    if check_server; then
        print_success "Server started (PID: $SERVER_PID)"
        return 0
    else
        print_fail "Server failed to start"
        return 1
    fi
}

# Stop server
stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        print_info "Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null
        wait $SERVER_PID 2>/dev/null
    fi
}

# Test basic CGI script
test_hello() {
    print_test "Basic CGI script (hello.py)"
    
    response=$(curl -s -w "\n%{http_code}" "$SERVER_URL/cgi/hello.py")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')
    
    if [ "$http_code" = "200" ] && echo "$body" | grep -q "Hello from CGI"; then
        print_success "Hello CGI script"
    else
        print_fail "Hello CGI script (HTTP $http_code)"
    fi
}

# Test environment variables
test_env() {
    print_test "Environment variables (env.py)"
    
    response=$(curl -s -w "\n%{http_code}" "$SERVER_URL/cgi/env.py")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')
    
    if [ "$http_code" = "200" ] && echo "$body" | grep -q "REQUEST_METHOD"; then
        print_success "Environment variables script"
    else
        print_fail "Environment variables script (HTTP $http_code)"
    fi
}

# Test query string handling
test_query() {
    print_test "Query string parsing (query.py)"
    
    response=$(curl -s -w "\n%{http_code}" "$SERVER_URL/cgi/query.py?name=test&value=123")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')
    
    if [ "$http_code" = "200" ] && echo "$body" | grep -q "name" && echo "$body" | grep -q "test"; then
        print_success "Query string parsing"
    else
        print_fail "Query string parsing (HTTP $http_code)"
    fi
}

# Test POST request
test_post() {
    print_test "POST request handling (post.py)"
    
    response=$(curl -s -w "\n%{http_code}" -X POST -d "data=test_data" \
        -H "Content-Type: application/x-www-form-urlencoded" \
        "$SERVER_URL/cgi/post.py")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')
    
    if [ "$http_code" = "200" ] && echo "$body" | grep -q "test_data"; then
        print_success "POST request handling"
    else
        print_fail "POST request handling (HTTP $http_code)"
    fi
}

# Test status codes
test_status() {
    print_test "Custom status codes (status.py)"
    
    response=$(curl -s -w "\n%{http_code}" "$SERVER_URL/cgi/status.py?status=404")
    http_code=$(echo "$response" | tail -n1)
    
    if [ "$http_code" = "404" ]; then
        print_success "Custom status code (404)"
    else
        print_fail "Custom status code (expected 404, got $http_code)"
    fi
}

# Test JSON response
test_json() {
    print_test "JSON response (json_response.py)"
    
    response=$(curl -s -w "\n%{http_code}" "$SERVER_URL/cgi/json_response.py?test=value")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | sed '$d')
    
    if [ "$http_code" = "200" ] && echo "$body" | grep -q "\"method\""; then
        print_success "JSON response"
    else
        print_fail "JSON response (HTTP $http_code)"
    fi
}

# Test redirect
test_redirect() {
    print_test "CGI redirect (redirect.py)"
    
    response=$(curl -s -w "\n%{http_code}" -L "$SERVER_URL/cgi/redirect.py")
    http_code=$(echo "$response" | tail -n1)
    
    if [ "$http_code" = "200" ] || [ "$http_code" = "302" ]; then
        print_success "CGI redirect"
    else
        print_fail "CGI redirect (HTTP $http_code)"
    fi
}

# Test non-existent script
test_404() {
    print_test "Non-existent CGI script (404)"
    
    response=$(curl -s -w "\n%{http_code}" "$SERVER_URL/cgi/nonexistent.py")
    http_code=$(echo "$response" | tail -n1)
    
    if [ "$http_code" = "404" ]; then
        print_success "404 for non-existent script"
    else
        print_fail "404 handling (expected 404, got $http_code)"
    fi
}

# Make CGI scripts executable
make_scripts_executable() {
    print_info "Making CGI scripts executable..."
    chmod +x www/cgi/*.py 2>/dev/null
}

# Main execution
main() {
    echo "=========================================="
    echo "  CGI Testing Suite for webserv"
    echo "=========================================="
    echo ""
    
    # Check if webserv binary exists
    if [ ! -f "./webserv" ]; then
        print_fail "webserv binary not found. Please build the project first."
        exit 1
    fi
    
    # Check if config file exists
    if [ ! -f "$CONFIG_FILE" ]; then
        print_fail "Config file not found: $CONFIG_FILE"
        exit 1
    fi
    
    # Make scripts executable
    make_scripts_executable
    
    # Start server
    if ! start_server; then
        exit 1
    fi
    
    echo ""
    echo "Running tests..."
    echo ""
    
    # Run tests
    test_hello
    test_env
    test_query
    test_post
    test_status
    test_json
    test_redirect
    test_404
    
    # Stop server
    echo ""
    stop_server
    
    # Print summary
    echo ""
    echo "=========================================="
    echo "  Test Summary"
    echo "=========================================="
    echo -e "${GREEN}Passed:${NC} $TESTS_PASSED"
    echo -e "${RED}Failed:${NC} $TESTS_FAILED"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed.${NC}"
        exit 1
    fi
}

# Trap to ensure server is stopped on exit
trap stop_server EXIT INT TERM

# Run main function
main
