# CGI Test Scripts

This directory contains test CGI scripts for webserv.

## Scripts

### Basic Tests

- **hello.py** - Simple "Hello World" CGI script
  - Tests basic CGI execution
  - Displays request method and query string
  - Usage: `GET /cgi/hello.py`

- **env.py** - Environment variables display
  - Shows all CGI environment variables
  - Displays HTTP headers
  - Usage: `GET /cgi/env.py`

### Request Handling

- **query.py** - Query string parser
  - Parses and displays query string parameters
  - Usage: `GET /cgi/query.py?name=value&foo=bar`

- **post.py** - POST request handler
  - Reads and displays POST body data
  - Shows Content-Type and Content-Length
  - Usage: `POST /cgi/post.py` with body data

### Advanced Features

- **status.py** - Custom status codes
  - Returns custom HTTP status codes
  - Usage: `GET /cgi/status.py?status=404`

- **json_response.py** - JSON response
  - Returns JSON formatted data
  - Includes request information
  - Usage: `GET /cgi/json_response.py?key=value`
  - Note: Renamed from `json.py` to avoid conflict with Python's `json` module

- **redirect.py** - CGI redirect
  - Demonstrates redirect from CGI script
  - Usage: `GET /cgi/redirect.py`

### Error Handling Tests

- **error.py** - Error handling test
  - Tests error handling when CGI script exits with error code
  - Usage: `GET /cgi/error.py`

- **timeout.py** - Timeout test
  - Tests timeout handling (sleeps for 35 seconds)
  - Usage: `GET /cgi/timeout.py`

## Testing

### Manual Testing

1. Start the server:
   ```bash
   ./webserv config/test_cgi.conf
   ```

2. Test scripts in browser or with curl:
   ```bash
   curl http://127.0.0.1:8080/cgi/hello.py
   curl http://127.0.0.1:8080/cgi/env.py
   curl "http://127.0.0.1:8080/cgi/query.py?name=test&value=123"
   curl -X POST -d "data=test" http://127.0.0.1:8080/cgi/post.py
   ```

#### Viewing HTML Output in Browser

**Important:** When testing CGI scripts that return HTML (like `env.py`), use a browser to see properly formatted output. The `curl` command outputs raw HTML text, so tables and styling won't be visible.

**Option 1: Open directly in browser:**
```bash
# macOS
open http://127.0.0.1:8080/cgi/env.py

# Linux
xdg-open http://127.0.0.1:8080/cgi/env.py
```

**Option 2: Use the browser test script:**
```bash
./scripts/test_cgi_browser.sh env.py open
```

**Option 3: Save and open HTML file:**
```bash
curl -s http://127.0.0.1:8080/cgi/env.py > /tmp/cgi_output.html
open /tmp/cgi_output.html  # macOS
```

**Why use a browser?**
- `curl` displays HTML as plain text
- Browsers render HTML with proper formatting, CSS styling, and table layout
- Scripts like `env.py` output formatted HTML tables that are best viewed in a browser

### Automated Testing

Run the test suite:
```bash
./scripts/test_cgi.sh
```

## Requirements

- Python 3 (scripts use `#!/usr/bin/env python3`)
- Executable permissions on all scripts (`chmod +x www/cgi/*.py`)
- Server configured with CGI location pointing to this directory

## Configuration

Make sure your config file includes:
```nginx
location /cgi {
    cgi_pass .py /usr/bin/python3;
    methods GET POST;
}
```

Adjust the Python interpreter path if needed (e.g., `/usr/bin/python`).
