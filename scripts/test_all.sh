#!/bin/bash

# Comprehensive test suite for webserv
# Tests all configurations, static pages, CGI, uploads, and stress scenarios

set -e

SERVER_HOST="127.0.0.1"
SERVER_PORT=8080
SERVER_PID=""
TEST_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_DIR="$TEST_DIR/config"
WWW_DIR="$TEST_DIR/www"
SCRIPTS_DIR="$TEST_DIR/scripts"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0

print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED++))
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAILED++))
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        print_info "Stopping server (PID: $SERVER_PID)"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

start_server() {
    local config="$1"
    print_info "Starting server with config: $config"
    cd "$TEST_DIR"
    ./webserv "$config" > /tmp/webserv_test.log 2>&1 &
    SERVER_PID=$!
    sleep 2
    
    # Check if server is running
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        print_fail "Server failed to start"
        cat /tmp/webserv_test.log
        return 1
    fi
    
    # Wait for server to be ready
    for i in {1..10}; do
        if curl -s "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    
    print_fail "Server not responding"
    cat /tmp/webserv_test.log
    return 1
}

test_static_file() {
    local path="$1"
    local expected_status="${2:-200}"
    
    print_test "Static file: $path"
    status=$(curl -s -o /dev/null -w "%{http_code}" "http://$SERVER_HOST:$SERVER_PORT$path")
    
    if [ "$status" = "$expected_status" ]; then
        print_pass "Static file $path returned $status"
        return 0
    else
        print_fail "Static file $path returned $status (expected $expected_status)"
        return 1
    fi
}

test_cgi() {
    local path="$1"
    local expected_status="${2:-200}"
    
    print_test "CGI: $path"
    status=$(curl -s -o /dev/null -w "%{http_code}" "http://$SERVER_HOST:$SERVER_PORT$path")
    
    if [ "$status" = "$expected_status" ]; then
        print_pass "CGI $path returned $status"
        return 0
    else
        print_fail "CGI $path returned $status (expected $expected_status)"
        return 1
    fi
}

test_upload() {
    print_test "File upload"
    local test_file="/tmp/test_upload_$$.txt"
    echo "Test upload content $(date)" > "$test_file"
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        -F "file=@$test_file" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/test_upload.txt")
    
    rm -f "$test_file"
    
    if [ "$status" = "201" ] || [ "$status" = "200" ]; then
        print_pass "Upload returned $status"
        return 0
    else
        print_fail "Upload returned $status (expected 201 or 200)"
        return 1
    fi
}

test_delete() {
    print_test "File delete"
    # First create a file
    echo "test" > "$WWW_DIR/uploads/test_delete.txt" 2>/dev/null || true
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X DELETE \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/test_delete.txt")
    
    if [ "$status" = "204" ] || [ "$status" = "200" ]; then
        print_pass "Delete returned $status"
        return 0
    else
        print_fail "Delete returned $status (expected 204 or 200)"
        return 1
    fi
}

test_redirect() {
    print_test "Redirect"
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -L "http://$SERVER_HOST:$SERVER_PORT/redirect")
    
    if [ "$status" = "200" ]; then
        print_pass "Redirect works"
        return 0
    else
        print_fail "Redirect returned $status (expected 200)"
        return 1
    fi
}

test_large_file() {
    print_test "Large file (1MB)"
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        "http://$SERVER_HOST:$SERVER_PORT/large.bin")
    
    if [ "$status" = "200" ]; then
        print_pass "Large file returned $status"
        return 0
    else
        print_fail "Large file returned $status (expected 200)"
        return 1
    fi
}

test_parallel() {
    print_test "Parallel connections (10)"
    local pids=()
    local success=0
    
    for i in {1..10}; do
        (
            if curl -s "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1; then
                exit 0
            else
                exit 1
            fi
        ) &
        pids+=($!)
    done
    
    for pid in "${pids[@]}"; do
        if wait $pid; then
            ((success++))
        fi
    done
    
    if [ $success -eq 10 ]; then
        print_pass "All 10 parallel connections succeeded"
        return 0
    else
        print_fail "Only $success/10 parallel connections succeeded"
        return 1
    fi
}

test_chunked() {
    print_test "Chunked transfer encoding"
    local test_data="Hello World Chunked"
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        -H "Transfer-Encoding: chunked" \
        -H "Content-Type: text/plain" \
        --data-binary "$(printf '%x\r\n%s\r\n0\r\n\r\n' ${#test_data} "$test_data")" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/chunked_test.txt")
    
    if [ "$status" = "201" ] || [ "$status" = "200" ]; then
        print_pass "Chunked upload returned $status"
        return 0
    else
        print_fail "Chunked upload returned $status"
        return 1
    fi
}

run_stress_test() {
    print_test "Stress test (100 requests)"
    local success=0
    local total=100
    
    for i in $(seq 1 $total); do
        if curl -s "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1; then
            ((success++))
        fi
        if [ $((i % 20)) -eq 0 ]; then
            print_info "Progress: $i/$total requests"
        fi
    done
    
    if [ $success -ge $((total * 95 / 100)) ]; then
        print_pass "Stress test: $success/$total succeeded (95%+ required)"
        return 0
    else
        print_fail "Stress test: $success/$total succeeded (expected 95%+)"
        return 1
    fi
}

main() {
    echo "=========================================="
    echo "webserv Comprehensive Test Suite"
    echo "=========================================="
    echo ""
    
    # Test 1: Basic configuration
    print_info "Test 1: Basic configuration (test_valid.conf)"
    if start_server "$CONFIG_DIR/test_valid.conf"; then
        test_static_file "/" || true
        test_static_file "/index.html" || true
        test_static_file "/test.html" || true
        test_static_file "/test.txt" || true
        test_static_file "/test.json" || true
        test_static_file "/nonexistent.html" "404" || true
        test_cgi "/cgi/hello.py" || true
        test_upload || true
        test_delete || true
        test_redirect || true
        test_large_file || true
        test_parallel || true
        test_chunked || true
        cleanup
        sleep 1
    fi
    
    # Test 2: CGI configuration
    print_info "Test 2: CGI configuration (test_cgi.conf)"
    if start_server "$CONFIG_DIR/test_cgi.conf"; then
        test_cgi "/cgi/hello.py" || true
        test_cgi "/cgi/env.py" || true
        test_cgi "/cgi/query.py?test=value" || true
        cleanup
        sleep 1
    fi
    
    # Test 3: Multi-port configuration
    print_info "Test 3: Multi-port configuration (test_multiport.conf)"
    if start_server "$CONFIG_DIR/test_multiport.conf"; then
        test_static_file "/" || true
        cleanup
        sleep 1
    fi
    
    # Test 4: Stress test
    print_info "Test 4: Stress test"
    if start_server "$CONFIG_DIR/test_valid.conf"; then
        run_stress_test || true
        cleanup
    fi
    
    # Summary
    echo ""
    echo "=========================================="
    echo "Test Summary"
    echo "=========================================="
    echo -e "${GREEN}Passed: $PASSED${NC}"
    echo -e "${RED}Failed: $FAILED${NC}"
    echo "Total: $((PASSED + FAILED))"
    
    if [ $FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed${NC}"
        exit 1
    fi
}

main "$@"
