#!/bin/bash

# Stress test for webserv - long-running traffic test

set -e

SERVER_HOST="127.0.0.1"
SERVER_PORT=8080
SERVER_PID=""
TEST_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_DIR="$TEST_DIR/config"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        echo -e "${YELLOW}[INFO]${NC} Stopping server (PID: $SERVER_PID)"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

start_server() {
    local config="$1"
    echo -e "${BLUE}[INFO]${NC} Starting server with config: $config"
    cd "$TEST_DIR"
    ./webserv "$config" > /tmp/webserv_stress.log 2>&1 &
    SERVER_PID=$!
    sleep 2
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}[FAIL]${NC} Server failed to start"
        cat /tmp/webserv_stress.log
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

run_stress_test() {
    local duration="${1:-60}"  # Default 60 seconds
    local concurrency="${2:-20}"  # Default 20 concurrent connections
    local total_requests=0
    local successful=0
    local failed=0
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    echo -e "${BLUE}[TEST]${NC} Stress test: $duration seconds, $concurrency concurrent connections"
    echo ""
    
    while [ $(date +%s) -lt $end_time ]; do
        local pids=()
        
        # Launch concurrent requests
        for i in $(seq 1 $concurrency); do
            (
                if curl -s -m 5 "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1; then
                    exit 0
                else
                    exit 1
                fi
            ) &
            pids+=($!)
            ((total_requests++))
        done
        
        # Wait for all requests
        for pid in "${pids[@]}"; do
            if wait $pid; then
                ((successful++))
            else
                ((failed++))
            fi
        done
        
        # Progress update every 10 seconds
        local elapsed=$(($(date +%s) - start_time))
        if [ $((elapsed % 10)) -eq 0 ] && [ $elapsed -gt 0 ]; then
            echo -e "${YELLOW}[INFO]${NC} Elapsed: ${elapsed}s | Requests: $total_requests | Success: $successful | Failed: $failed"
        fi
        
        sleep 0.1
    done
    
    local total_time=$(($(date +%s) - start_time))
    local rps=$((total_requests / total_time))
    
    echo ""
    echo "=========================================="
    echo "Stress Test Results"
    echo "=========================================="
    echo "Duration: ${total_time}s"
    echo "Total requests: $total_requests"
    echo "Successful: $successful"
    echo "Failed: $failed"
    echo "Success rate: $((successful * 100 / total_requests))%"
    echo "Requests per second: $rps"
    echo ""
    
    if [ $failed -lt $((total_requests / 20)) ]; then
        echo -e "${GREEN}[PASS]${NC} Stress test passed (failure rate < 5%)"
        return 0
    else
        echo -e "${RED}[FAIL]${NC} Stress test failed (failure rate >= 5%)"
        return 1
    fi
}

main() {
    echo "=========================================="
    echo "webserv Stress Test"
    echo "=========================================="
    echo ""
    
    local duration="${1:-60}"
    local concurrency="${2:-20}"
    
    if start_server "$CONFIG_DIR/test_valid.conf"; then
        run_stress_test "$duration" "$concurrency"
        cleanup
    fi
}

main "$@"
