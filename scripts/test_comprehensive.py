#!/usr/bin/env python3
"""
Comprehensive test suite for webserv
Tests: parallel connections, large bodies, chunked encoding, timeouts, drops
"""

import socket
import threading
import time
import sys
import os
import signal
import subprocess
import random
import string

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080
TEST_TIMEOUT = 5

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'

def print_test(name):
    print(f"{Colors.BLUE}[TEST]{Colors.RESET} {name}")

def print_pass(msg=""):
    print(f"{Colors.GREEN}[PASS]{Colors.RESET} {msg}")

def print_fail(msg=""):
    print(f"{Colors.RED}[FAIL]{Colors.RESET} {msg}")

def print_info(msg=""):
    print(f"{Colors.YELLOW}[INFO]{Colors.RESET} {msg}")

def create_socket():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TEST_TIMEOUT)
    return sock

def send_request(sock, request):
    try:
        sock.sendall(request.encode('utf-8'))
        response = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            if b"\r\n\r\n" in response:
                headers_end = response.find(b"\r\n\r\n")
                headers = response[:headers_end].decode('utf-8', errors='ignore')
                if "Content-Length:" in headers:
                    try:
                        cl_line = [l for l in headers.split('\r\n') if l.startswith('Content-Length:')][0]
                        cl_value = int(cl_line.split(':')[1].strip())
                        body_start = headers_end + 4
                        if len(response) >= body_start + cl_value:
                            break
                    except:
                        pass
                else:
                    break
        return response.decode('utf-8', errors='ignore')
    except Exception as e:
        print_fail(f"Error in send_request: {e}")
        return None

def test_basic_get():
    """Test basic GET request"""
    print_test("Basic GET request")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        response = send_request(sock, request)
        sock.close()
        
        if response and "200" in response:
            print_pass("Basic GET works")
            return True
        else:
            print_fail(f"Expected 200, got: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_404():
    """Test 404 error"""
    print_test("404 Not Found")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        request = "GET /nonexistent.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        response = send_request(sock, request)
        sock.close()
        
        if response and "404" in response:
            print_pass("404 error works")
            return True
        else:
            print_fail(f"Expected 404, got: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_405():
    """Test 405 Method Not Allowed"""
    print_test("405 Method Not Allowed")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        request = "DELETE / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        response = send_request(sock, request)
        sock.close()
        
        if response and "405" in response:
            print_pass("405 error works")
            return True
        else:
            print_fail(f"Expected 405, got: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_keep_alive():
    """Test keep-alive connections"""
    print_test("Keep-Alive connection")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        # First request
        request1 = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
        response1 = send_request(sock, request1)
        
        if not response1 or "200" not in response1:
            sock.close()
            print_fail("First request failed")
            return False
        
        # Second request on same connection
        time.sleep(0.1)
        request2 = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
        response2 = send_request(sock, request2)
        sock.close()
        
        if response2 and "200" in response2:
            print_pass("Keep-alive works")
            return True
        else:
            print_fail("Second request on keep-alive failed")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_large_body():
    """Test POST with large body"""
    print_test("Large body POST (1MB)")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        body_size = 1024 * 1024  # 1MB
        body = "A" * body_size
        request = f"POST /uploads/large.txt HTTP/1.1\r\n"
        request += f"Host: localhost\r\n"
        request += f"Content-Length: {body_size}\r\n"
        request += f"Content-Type: text/plain\r\n"
        request += f"\r\n{body}"
        
        response = send_request(sock, request)
        sock.close()
        
        if response and ("201" in response or "200" in response):
            print_pass("Large body POST works")
            return True
        else:
            print_fail(f"Expected 201/200, got: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_chunked_encoding():
    """Test chunked transfer encoding"""
    print_test("Chunked Transfer-Encoding")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        chunks = ["Hello", " World", " Chunked"]
        request = "POST /uploads/chunked.txt HTTP/1.1\r\n"
        request += "Host: localhost\r\n"
        request += "Transfer-Encoding: chunked\r\n"
        request += "Content-Type: text/plain\r\n"
        request += "\r\n"
        
        for chunk in chunks:
            request += f"{len(chunk):x}\r\n{chunk}\r\n"
        request += "0\r\n\r\n"
        
        response = send_request(sock, request)
        sock.close()
        
        if response and ("201" in response or "200" in response):
            print_pass("Chunked encoding works")
            return True
        else:
            print_fail(f"Expected 201/200, got: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_parallel_connections(num_connections=10):
    """Test parallel connections"""
    print_test(f"Parallel connections ({num_connections})")
    results = []
    errors = []
    
    def worker(worker_id):
        try:
            sock = create_socket()
            sock.connect((SERVER_HOST, SERVER_PORT))
            request = f"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
            response = send_request(sock, request)
            sock.close()
            
            if response and "200" in response:
                results.append(worker_id)
            else:
                errors.append(f"Worker {worker_id}: Invalid response")
        except Exception as e:
            errors.append(f"Worker {worker_id}: {e}")
    
    threads = []
    for i in range(num_connections):
        t = threading.Thread(target=worker, args=(i,))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()
    
    if len(results) == num_connections:
        print_pass(f"All {num_connections} parallel connections succeeded")
        return True
    else:
        print_fail(f"Only {len(results)}/{num_connections} succeeded. Errors: {errors[:3]}")
        return False

def test_cgi():
    """Test CGI execution"""
    print_test("CGI execution")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        request = "GET /cgi/hello.py HTTP/1.1\r\nHost: localhost\r\n\r\n"
        response = send_request(sock, request)
        sock.close()
        
        if response and "200" in response and ("Hello" in response or "Content-Type" in response):
            print_pass("CGI execution works")
            return True
        else:
            print_fail(f"CGI failed: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_timeout():
    """Test connection timeout (idle connection)"""
    print_test("Connection timeout (idle)")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        # Don't send anything, just wait
        sock.settimeout(70)  # Wait for timeout
        try:
            data = sock.recv(1024)
            sock.close()
            print_info("Connection closed by server (timeout)")
            print_pass("Timeout handling works")
            return True
        except socket.timeout:
            sock.close()
            print_fail("Connection did not timeout")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def test_stress(num_requests=100):
    """Stress test: many requests"""
    print_test(f"Stress test ({num_requests} requests)")
    success = 0
    failed = 0
    
    for i in range(num_requests):
        try:
            sock = create_socket()
            sock.connect((SERVER_HOST, SERVER_PORT))
            request = f"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
            response = send_request(sock, request)
            sock.close()
            
            if response and "200" in response:
                success += 1
            else:
                failed += 1
        except Exception as e:
            failed += 1
        
        if (i + 1) % 20 == 0:
            print_info(f"Progress: {i + 1}/{num_requests} requests")
    
    if success >= num_requests * 0.95:  # 95% success rate
        print_pass(f"Stress test passed: {success}/{num_requests} succeeded")
        return True
    else:
        print_fail(f"Stress test failed: {success}/{num_requests} succeeded")
        return False

def test_413_payload_too_large():
    """Test 413 Payload Too Large"""
    print_test("413 Payload Too Large")
    try:
        sock = create_socket()
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        # Assuming max body size is 10MB, send 11MB
        body_size = 11 * 1024 * 1024
        body = "A" * body_size
        request = f"POST /uploads/large.txt HTTP/1.1\r\n"
        request += f"Host: localhost\r\n"
        request += f"Content-Length: {body_size}\r\n"
        request += f"Content-Type: text/plain\r\n"
        request += f"\r\n{body[:1024]}"  # Send only first chunk to test
        
        response = send_request(sock, request)
        sock.close()
        
        if response and "413" in response:
            print_pass("413 error works")
            return True
        else:
            print_fail(f"Expected 413, got: {response[:200] if response else 'None'}")
            return False
    except Exception as e:
        print_fail(f"Exception: {e}")
        return False

def main():
    print(f"{Colors.BLUE}{'='*60}{Colors.RESET}")
    print(f"{Colors.BLUE}Comprehensive webserv Test Suite{Colors.RESET}")
    print(f"{Colors.BLUE}{'='*60}{Colors.RESET}\n")
    
    # Check if server is running
    try:
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(1)
        test_sock.connect((SERVER_HOST, SERVER_PORT))
        test_sock.close()
        print_info(f"Server detected at {SERVER_HOST}:{SERVER_PORT}\n")
    except:
        print_fail(f"Server not running at {SERVER_HOST}:{SERVER_PORT}")
        print_info("Please start the server first: ./webserv config/test_valid.conf")
        sys.exit(1)
    
    tests = [
        ("Basic GET", test_basic_get),
        ("404 Not Found", test_404),
        ("405 Method Not Allowed", test_405),
        ("Keep-Alive", test_keep_alive),
        ("Large Body", test_large_body),
        ("Chunked Encoding", test_chunked_encoding),
        ("Parallel Connections", lambda: test_parallel_connections(10)),
        ("CGI Execution", test_cgi),
        ("413 Payload Too Large", test_413_payload_too_large),
        ("Stress Test", lambda: test_stress(100)),
    ]
    
    results = []
    for name, test_func in tests:
        try:
            result = test_func()
            results.append((name, result))
        except Exception as e:
            print_fail(f"{name} crashed: {e}")
            results.append((name, False))
        print()
    
    # Summary
    print(f"{Colors.BLUE}{'='*60}{Colors.RESET}")
    print(f"{Colors.BLUE}Test Summary{Colors.RESET}")
    print(f"{Colors.BLUE}{'='*60}{Colors.RESET}")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = f"{Colors.GREEN}PASS{Colors.RESET}" if result else f"{Colors.RED}FAIL{Colors.RESET}"
        print(f"{status} - {name}")
    
    print(f"\n{Colors.BLUE}Total: {passed}/{total} tests passed{Colors.RESET}")
    
    if passed == total:
        print(f"{Colors.GREEN}All tests passed!{Colors.RESET}")
        sys.exit(0)
    else:
        print(f"{Colors.RED}Some tests failed{Colors.RESET}")
        sys.exit(1)

if __name__ == "__main__":
    main()
