# Test Scripts Overview

This directory contains comprehensive test suites for webserv, covering all Stage 10 requirements.

## Test Scripts

### 1. `test_all.sh` - Comprehensive Test Suite

**Purpose:** Run all test scenarios automatically

**Features:**
- Starts and stops server automatically
- Tests all configurations
- Static file serving
- CGI execution
- File uploads and deletions
- Redirects
- Large files
- Parallel connections
- Chunked encoding
- Error handling

**Usage:**
```bash
./scripts/test_all.sh
```

**Output:**
- Shows PASS/FAIL for each test
- Summary with total passed/failed tests
- Exit code 0 if all tests pass, 1 otherwise

### 2. `test_stress.sh` - Stress Test

**Purpose:** Long-running traffic test with concurrent connections

**Features:**
- Configurable duration (default: 60 seconds)
- Configurable concurrency (default: 20 connections)
- Request rate measurement
- Failure rate tracking
- Real-time progress updates

**Usage:**
```bash
# Default: 60 seconds, 20 concurrent connections
./scripts/test_stress.sh

# Custom: 120 seconds, 50 concurrent connections
./scripts/test_stress.sh 120 50
```

**Output:**
- Progress updates every 10 seconds
- Final statistics (duration, total requests, success rate, RPS)
- Exit code 0 if failure rate < 5%, 1 otherwise

### 3. `test_upload.sh` - Upload Scenarios Test

**Purpose:** Test file upload functionality

**Features:**
- Small file uploads (< 1KB)
- Medium file uploads (100KB)
- Large file uploads (1MB)
- Upload size limit enforcement (413)
- Multipart form uploads
- Upload directory listing

**Usage:**
```bash
./scripts/test_upload.sh
```

**Output:**
- PASS/FAIL for each upload scenario
- Summary with total passed/failed tests
- Automatic cleanup of test files

### 4. `test_comprehensive.py` - Python Test Suite

**Purpose:** Python-based comprehensive testing

**Features:**
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

**Requirements:**
- Python 3
- Server must be running before test execution

### 5. `test_config.sh` - Configuration Parser Tests

**Purpose:** Test configuration file parsing

**Usage:**
```bash
./scripts/test_config.sh
```

### 6. `test_cgi.sh` - CGI Tests

**Purpose:** Test CGI script execution

**Usage:**
```bash
# Start server first
./webserv config/test_cgi.conf

# In another terminal
./scripts/test_cgi.sh
```

### 7. `test_redirects.sh` - Redirect Tests

**Purpose:** Test HTTP redirects

**Usage:**
```bash
./scripts/test_redirects.sh config/test_redirects.conf 8080
```

## Test Data

### Static Pages (`www/`)

- `index.html` - Main index page
- `test.html` - Test HTML page
- `test.txt` - Plain text file
- `test.json` - JSON file
- `404.html` - Custom 404 error page
- `500.html` - Custom 500 error page
- `large.bin` - 1MB binary file for large file testing

### CGI Scripts (`www/cgi/`)

- `hello.py` - Basic "Hello World" CGI
- `env.py` - Environment variables display
- `query.py` - Query string parsing
- `post.py` - POST request handling
- `json_response.py` - JSON response
- `status.py` - Custom status codes
- `redirect.py` - CGI redirects
- `timeout.py` - Timeout testing
- `error.py` - Error handling

### Test Configurations (`config/`)

- `test_valid.conf` - Full featured configuration
- `test_cgi.conf` - CGI-specific configuration
- `test_multiport.conf` - Multi-port configuration
- `test_redirects.conf` - Redirect configuration
- `test_invalid*.conf` - Invalid configs for parser testing

## Running All Tests

### Quick Test (Recommended)

```bash
# Run all tests automatically
./scripts/test_all.sh
```

### Full Test Suite

```bash
# 1. Configuration tests
./scripts/test_config.sh

# 2. CGI tests (server must be running)
./webserv config/test_cgi.conf &
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

## Expected Results

All tests should:
- Start server successfully (if required)
- Complete all test scenarios
- Show PASS for all tests
- Clean up resources properly
- Exit with code 0

## Troubleshooting

### Server Won't Start

1. Check if port is already in use: `lsof -i :8080`
2. Check configuration file syntax
3. Check logs: `/tmp/webserv_test.log` or `/tmp/webserv_stress.log`

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

Expected performance:
- **Static files:** < 10ms response time
- **CGI scripts:** < 100ms response time
- **Uploads:** Depends on file size and network
- **Concurrent connections:** 100+ simultaneous connections
- **Request rate:** 1000+ requests/second (stress test)
