# Testing Redirects

## Quick Start

### 1. Start the server

```bash
./webserv config/test_redirects.conf
```

Or use existing config:

```bash
./webserv config/test_valid.conf
```

### 2. Run the test script

```bash
./scripts/test_redirects.sh config/test_redirects.conf 8080
```

## Manual Testing with curl

### Basic redirect test (302)

```bash
curl -v http://127.0.0.1:8080/redirect
```

Expected result:
```
< HTTP/1.1 302 Found
< Location: /index.html
< Content-Length: 0
```

### Check status code

```bash
curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8080/redirect
# Should return: 302
```

### Check Location header

```bash
curl -s -I http://127.0.0.1:8080/redirect | grep -i location
# Should return: Location: /index.html
```

### Follow redirect automatically

```bash
curl -L http://127.0.0.1:8080/redirect
# Will get final content after redirect
```

### Without following redirect

```bash
curl -L --max-redirs 0 http://127.0.0.1:8080/redirect
# Will show only redirect response, won't follow
```

## Testing Different Redirect Types

### 1. Permanent redirect (301)

```bash
curl -v http://127.0.0.1:8080/permanent
```

Expected result:
```
< HTTP/1.1 301 Moved Permanently
< Location: /index.html
< Content-Length: 0
```

### 2. Temporary redirect (302)

```bash
curl -v http://127.0.0.1:8080/temporary
```

Expected result:
```
< HTTP/1.1 302 Found
< Location: /index.html
< Content-Length: 0
```

### 3. External redirect

```bash
curl -v http://127.0.0.1:8080/external
```

Expected result:
```
< HTTP/1.1 302 Found
< Location: http://example.com
< Content-Length: 0
```

## Check All Headers

```bash
curl -I http://127.0.0.1:8080/redirect
```

Full verbose output:

```bash
curl -v http://127.0.0.1:8080/redirect 2>&1
```

## Browser Testing

1. Start the server:
   ```bash
   ./webserv config/test_redirects.conf
   ```

2. Open browser and navigate to:
   ```
   http://127.0.0.1:8080/redirect
   ```

3. Browser will automatically follow redirect and show final page

4. Check in developer tools (F12):
   - Network tab will show redirect
   - Status code will be 302 or 301
   - Response Headers will show Location header

## Automated Testing

### Using the test script

```bash
# With default config
./scripts/test_redirects.sh

# With specified config and port
./scripts/test_redirects.sh config/test_redirects.conf 8080
```

The script checks:
- ✅ Redirect status code
- ✅ Location header presence
- ✅ Content-Length is 0
- ✅ Automatic redirect following

## Testing Various Scenarios

### Test 1: Basic redirect
```bash
curl -v http://127.0.0.1:8080/redirect
```

### Test 2: Check Content-Length
```bash
curl -I http://127.0.0.1:8080/redirect | grep -i content-length
# Should be: Content-Length: 0
```

### Test 3: Check Connection header
```bash
curl -I http://127.0.0.1:8080/redirect | grep -i connection
# Should preserve keep-alive if present in request
```

### Test 4: Follow redirect
```bash
curl -L http://127.0.0.1:8080/redirect
# Should return content of /index.html
```

## Expected Results

### A successful redirect should contain:

1. **Correct status code**: 301 or 302
2. **Location header**: with target URL
3. **Content-Length: 0**: empty response body
4. **Connection header**: preserved from request

### Example of correct response:

```
HTTP/1.1 302 Found
Location: /index.html
Content-Length: 0
Connection: keep-alive
```

## Troubleshooting

### Problem: Redirect not working

1. Check configuration:
   ```bash
   ./webserv config/test_redirects.conf
   ```

2. Check server logs for errors

3. Make sure location block is configured correctly:
   ```nginx
   location /redirect {
       return /index.html;
   }
   ```

### Problem: Wrong status code

Check syntax in config:
- `return /path` → 302 (default)
- `return 301 /path` → 301
- `return 302 /path` → 302

### Problem: Location header missing

Make sure:
- `return` directive is specified in location block
- Path is specified correctly
- Server is running with correct config

## Additional Commands

### Test all redirects at once

```bash
for path in redirect permanent temporary external; do
    echo "Testing /$path:"
    curl -I http://127.0.0.1:8080/$path | grep -E "HTTP|Location"
    echo ""
done
```

### Save response to file

```bash
curl -v http://127.0.0.1:8080/redirect > redirect_response.txt 2>&1
```

### Test with timeout

```bash
curl --max-time 5 -v http://127.0.0.1:8080/redirect
```
