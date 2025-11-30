# Comprehensive Testing Documentation

This document describes the comprehensive testing suite for webserv, covering all test scenarios including static pages, CGI, uploads, stress tests, and automated test suites.

## Test Structure

### 1. Static Pages Test

**Location:** `www/`

**Test Files:**
- `index.html` - Main index page
- `test.html` - Test HTML page
- `test.txt` - Plain text file
- `test.json` - JSON file
- `404.html` - Custom 404 error page
- `500.html` - Custom 500 error page
- `large.bin` - 1MB binary file for large file testing

**Test Scenarios:**
- Basic static file serving
- Different content types (HTML, text, JSON, binary)
- Error pages (404, 500)
- Large file handling (1MB+)

### 2. CGI Samples

**Location:** `www/cgi/`

**Test Scripts:**
- `hello.py` - Basic "Hello World" CGI
- `env.py` - Environment variables display
- `query.py` - Query string parsing
- `post.py` - POST request handling
- `json_response.py` - JSON response
- `status.py` - Custom status codes
- `redirect.py` - CGI redirects
- `timeout.py` - Timeout testing
- `error.py` - Error handling

**Test Scenarios:**
- Basic CGI execution
- Environment variables
- Query string parsing
- POST body handling
- Custom status codes
- Redirects from CGI
- Error handling

### 3. Upload Scenarios

**Location:** `www/uploads/`

**Test Scenarios:**
- Small file upload (< 1KB)
- Medium file upload (100KB)
- Large file upload (1MB)
- Upload too large (11MB, should return 413)
- Multipart form upload
- Upload directory listing (autoindex)

**Test Script:** `scripts/test_upload.sh`

### 4. Configuration Tests

**Location:** `config/`

**Test Configurations:**
- `test_valid.conf` - Full featured configuration
- `test_cgi.conf` - CGI-specific configuration
- `test_multiport.conf` - Multi-port configuration
- `test_redirects.conf` - Redirect configuration
- `test_invalid1.conf`, `test_invalid2.conf`, `test_invalid3.conf` - Invalid configs for parser testing

**Test Scenarios:**
- Basic server configuration
- CGI routing
- Multiple ports
- Redirects
- Configuration parser validation

## Automated Test Suites

### 1. Comprehensive Test Suite

**Script:** `scripts/test_all.sh`

**Tests:**
- Static file serving
- CGI execution
- File uploads
- File deletion
- Redirects
- Large files
- Parallel connections
- Chunked encoding
- Error handling (404, 405, 413)

**Usage:**
```bash
./scripts/test_all.sh
```

### 2. Stress Test

**Script:** `scripts/test_stress.sh`

**Tests:**
- Long-running traffic (default 60 seconds)
- Concurrent connections (default 20)
- Request rate measurement
- Failure rate tracking

**Usage:**
```bash
# Default: 60 seconds, 20 concurrent connections
./scripts/test_stress.sh

# Custom: 120 seconds, 50 concurrent connections
./scripts/test_stress.sh 120 50
```

### 3. Upload Scenarios Test

**Script:** `scripts/test_upload.sh`

**Tests:**
- Small, medium, large file uploads
- Upload size limit enforcement (413)
- Multipart form uploads
- Upload directory listing

**Usage:**
```bash
./scripts/test_upload.sh
```

### 4. Python Comprehensive Test

**Script:** `scripts/test_comprehensive.py`

**Tests:**
- Basic GET requests
- Error handling (404, 405, 413)
- Keep-Alive connections
- Large body handling (1MB+)
- Chunked transfer encoding
- Parallel connections (10+ concurrent)
- CGI execution
- Connection timeouts
- Stress testing (100+ requests)

**Usage:**
```bash
# Start server first
./webserv config/test_valid.conf

# In another terminal
./scripts/test_comprehensive.py
```

## Running All Tests

### Quick Test (All Scenarios)

```bash
# Make sure server is not running
./scripts/test_all.sh
```

### Full Test Suite

```bash
# 1. Configuration tests
./scripts/test_config.sh

# 2. CGI tests
./scripts/test_cgi.sh

# 3. Redirect tests
./scripts/test_redirects.sh config/test_redirects.conf 8080

# 4. Upload tests
./scripts/test_upload.sh

# 5. Comprehensive tests
./scripts/test_all.sh

# 6. Stress test (run separately, takes time)
./scripts/test_stress.sh 60 20
```

## Test Coverage

### Test Coverage Requirements

✅ **Config set + static pages, CGI samples, upload scenarios**
- All test configurations created and tested
- Static pages for various content types
- Complete CGI sample scripts
- Comprehensive upload scenarios

✅ **Auto-tests (Python/Golang): parallel connections, large bodies, chunked, timeouts, drops**
- Python test suite with all required scenarios
- Parallel connection testing
- Large body handling
- Chunked encoding support
- Timeout testing
- Connection drop handling

✅ **Stress test: long-running traffic**
- Dedicated stress test script
- Configurable duration and concurrency
- Request rate measurement
- Failure rate tracking

## Expected Results

### Successful Test Run

All tests should:
- Start server successfully
- Complete all test scenarios
- Show PASS for all tests
- Clean up resources properly

### Failure Indicators

- Server fails to start
- HTTP status codes don't match expectations
- Timeouts or connection errors
- Resource leaks (check with `scripts/show_fds.sh`)

## Troubleshooting

### Server Won't Start

1. Check if port is already in use: `lsof -i :8080`
2. Check configuration file syntax
3. Check logs: `/tmp/webserv_test.log`

### Tests Timeout

1. Increase timeout values in test scripts
2. Check server performance
3. Verify system resources

### Upload Tests Fail

1. Check `www/uploads/` directory permissions
2. Verify `upload_store` configuration
3. Check `client_max_body_size` limit

### CGI Tests Fail

1. Verify Python interpreter path in config
2. Check CGI script permissions (should be executable)
3. Verify CGI script syntax

## Performance Benchmarks

### Expected Performance

- **Static files:** < 10ms response time
- **CGI scripts:** < 100ms response time
- **Uploads:** Depends on file size and network
- **Concurrent connections:** 100+ simultaneous connections
- **Request rate:** 1000+ requests/second (stress test)

### Monitoring

Use system tools to monitor:
- CPU usage: `top` or `htop`
- Memory usage: `free -m` or `vm_stat`
- File descriptors: `scripts/show_fds.sh`
- Network: `netstat -an | grep :8080`
