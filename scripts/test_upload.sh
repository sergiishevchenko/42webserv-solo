#!/bin/bash

# Upload scenarios test for webserv

set -e

SERVER_HOST="127.0.0.1"
SERVER_PORT=8080
SERVER_PID=""
TEST_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_DIR="$TEST_DIR/config"
UPLOAD_DIR="$TEST_DIR/www/uploads"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASSED=0
FAILED=0

cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        echo -e "${YELLOW}[INFO]${NC} Stopping server (PID: $SERVER_PID)"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    # Cleanup test files
    rm -f "$UPLOAD_DIR"/test_*.txt "$UPLOAD_DIR"/test_*.bin 2>/dev/null || true
}

trap cleanup EXIT

start_server() {
    local config="$1"
    echo -e "${BLUE}[INFO]${NC} Starting server with config: $config"
    cd "$TEST_DIR"
    mkdir -p "$UPLOAD_DIR"
    ./webserv "$config" > /tmp/webserv_upload_test.log 2>&1 &
    SERVER_PID=$!
    sleep 2
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}[FAIL]${NC} Server failed to start"
        cat /tmp/webserv_upload_test.log
        exit 1
    fi
    
    for i in {1..10}; do
        if curl -s "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    
    echo -e "${RED}[FAIL]${NC} Server not responding"
    exit 1
}

test_small_upload() {
    echo -e "${BLUE}[TEST]${NC} Small file upload (< 1KB)"
    local test_file="/tmp/test_small_$$.txt"
    echo "Small test file content" > "$test_file"
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        --data-binary "@$test_file" \
        -H "Content-Type: text/plain" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/small.txt")
    
    rm -f "$test_file"
    
    if [ "$status" = "201" ] || [ "$status" = "200" ]; then
        echo -e "${GREEN}[PASS]${NC} Small upload returned $status"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Small upload returned $status"
        ((FAILED++))
        return 1
    fi
}

test_medium_upload() {
    echo -e "${BLUE}[TEST]${NC} Medium file upload (100KB)"
    local test_file="/tmp/test_medium_$$.txt"
    dd if=/dev/zero of="$test_file" bs=1024 count=100 2>/dev/null
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        --data-binary "@$test_file" \
        -H "Content-Type: application/octet-stream" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/medium.bin")
    
    rm -f "$test_file"
    
    if [ "$status" = "201" ] || [ "$status" = "200" ]; then
        echo -e "${GREEN}[PASS]${NC} Medium upload returned $status"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Medium upload returned $status"
        ((FAILED++))
        return 1
    fi
}

test_large_upload() {
    echo -e "${BLUE}[TEST]${NC} Large file upload (1MB)"
    local test_file="/tmp/test_large_$$.bin"
    dd if=/dev/zero of="$test_file" bs=1024 count=1024 2>/dev/null
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        --data-binary "@$test_file" \
        -H "Content-Type: application/octet-stream" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/large.bin")
    
    rm -f "$test_file"
    
    if [ "$status" = "201" ] || [ "$status" = "200" ]; then
        echo -e "${GREEN}[PASS]${NC} Large upload returned $status"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Large upload returned $status"
        ((FAILED++))
        return 1
    fi
}

test_upload_too_large() {
    echo -e "${BLUE}[TEST]${NC} Upload too large (11MB, should fail with 413)"
    local test_file="/tmp/test_toolarge_$$.bin"
    dd if=/dev/zero of="$test_file" bs=1024 count=11264 2>/dev/null
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        --data-binary "@$test_file" \
        -H "Content-Type: application/octet-stream" \
        --max-time 5 \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/toolarge.bin" 2>/dev/null || echo "000")
    
    rm -f "$test_file"
    
    if [ "$status" = "413" ]; then
        echo -e "${GREEN}[PASS]${NC} Too large upload correctly returned 413"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Too large upload returned $status (expected 413)"
        ((FAILED++))
        return 1
    fi
}

test_multipart_upload() {
    echo -e "${BLUE}[TEST]${NC} Multipart form upload"
    local test_file="/tmp/test_multipart_$$.txt"
    echo "Multipart test content" > "$test_file"
    
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        -X POST \
        -F "file=@$test_file" \
        -F "name=test" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/multipart.txt")
    
    rm -f "$test_file"
    
    if [ "$status" = "201" ] || [ "$status" = "200" ]; then
        echo -e "${GREEN}[PASS]${NC} Multipart upload returned $status"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Multipart upload returned $status"
        ((FAILED++))
        return 1
    fi
}

test_upload_list() {
    echo -e "${BLUE}[TEST]${NC} Upload directory listing"
    status=$(curl -s -o /dev/null -w "%{http_code}" \
        "http://$SERVER_HOST:$SERVER_PORT/uploads/")
    
    if [ "$status" = "200" ]; then
        echo -e "${GREEN}[PASS]${NC} Upload directory listing returned $status"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Upload directory listing returned $status"
        ((FAILED++))
        return 1
    fi
}

main() {
    echo "=========================================="
    echo "webserv Upload Scenarios Test"
    echo "=========================================="
    echo ""
    
    if start_server "$CONFIG_DIR/test_valid.conf"; then
        test_small_upload
        test_medium_upload
        test_large_upload
        test_upload_too_large
        test_multipart_upload
        test_upload_list
        cleanup
    fi
    
    echo ""
    echo "=========================================="
    echo "Test Summary"
    echo "=========================================="
    echo -e "${GREEN}Passed: $PASSED${NC}"
    echo -e "${RED}Failed: $FAILED${NC}"
    echo "Total: $((PASSED + FAILED))"
    
    if [ $FAILED -eq 0 ]; then
        echo -e "${GREEN}All upload tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some upload tests failed${NC}"
        exit 1
    fi
}

main "$@"
