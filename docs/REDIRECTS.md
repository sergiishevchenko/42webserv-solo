# Redirects Documentation

## Overview

The webserv project supports HTTP redirects at the location level using the `return` directive. Redirects allow you to automatically send clients to a different URL or path when they request a specific location.

## Configuration

Redirects are configured within location blocks using the `return` directive. This directive can specify both the redirect status code and the target URL.

### Configuration Syntax

```nginx
location /path {
    return [status_code] target_url;
}
```

### Configuration Parameters

- **`return`**: Directive that triggers a redirect
- **`status_code`**: Optional HTTP status code (301 or 302). Defaults to 302 if omitted
- **`target_url`**: The URL or path to redirect to

### Supported Status Codes

- **301 Moved Permanently**: Indicates that the resource has been permanently moved to a new location. Browsers and search engines should update their links.
- **302 Found** (default): Indicates a temporary redirect. The client should continue to use the original URL for future requests.

## Redirect Syntax Variations

### Basic Redirect (Default 302)

```nginx
location /old-path {
    return /new-path;
}
```

This creates a temporary redirect (302) to `/new-path`.

### Explicit Status Code

```nginx
location /old-path {
    return 301 /new-path;
}
```

This creates a permanent redirect (301) to `/new-path`.

```nginx
location /old-path {
    return 302 /new-path;
}
```

This explicitly creates a temporary redirect (302) to `/new-path`.

## Redirect Target Formats

### Relative Paths

Relative paths are automatically converted to absolute paths by prefixing with `/`:

```nginx
location /redirect {
    return index.html;    # Becomes /index.html
    return /index.html;   # Explicitly /index.html
}
```

### Absolute URLs

You can redirect to external URLs using absolute URLs:

```nginx
location /external {
    return 301 http://example.com/new-page;
}

location /secure {
    return 302 https://secure.example.com;
}
```

## How Redirects Work

### Request Flow

1. **Request Reception**: The server receives an HTTP request for a resource
2. **Location Matching**: The server finds the matching location block
3. **Redirect Detection**: If the location has a `return` directive, redirect handling is triggered
4. **Response Generation**: The server generates an HTTP response with:
   - Appropriate status code (301 or 302)
   - `Location` header with the target URL
   - Empty body with `Content-Length: 0`
5. **Client Redirect**: The client follows the redirect and makes a new request

### Processing Order

Redirects are checked early in the request processing pipeline:

1. Server configuration matching
2. Location matching
3. **Redirect check** (if present, redirect is returned immediately)
4. Method validation
5. Normal request handling (GET, POST, DELETE)

This means redirects take precedence over other location directives.

## Examples

### Basic Redirect

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    
    location /redirect {
        return /index.html;
    }
}
```

Request: `GET /redirect`
Response: `302 Found` with `Location: /index.html`

### Permanent Redirect

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    
    location /old-page {
        return 301 /new-page.html;
    }
}
```

Request: `GET /old-page`
Response: `301 Moved Permanently` with `Location: /new-page.html`

### Temporary Redirect

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    
    location /temp {
        return 302 /temporary-page.html;
    }
}
```

Request: `GET /temp`
Response: `302 Found` with `Location: /temporary-page.html`

### External Redirect

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    
    location /external {
        return 301 http://example.com;
    }
}
```

Request: `GET /external`
Response: `301 Moved Permanently` with `Location: http://example.com`

### Multiple Redirects

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    
    location /redirect {
        return 302 /index.html;
    }
    
    location /permanent {
        return 301 /index.html;
    }
    
    location /external {
        return 302 http://example.com;
    }
}
```

## HTTP Response Format

### Redirect Response Structure

When a redirect is triggered, the server generates the following response:

```
HTTP/1.1 302 Found
Location: /target-path
Content-Length: 0
Connection: keep-alive
[other headers]
```

### Response Headers

- **Status Line**: Contains the redirect status code and reason phrase
  - `301 Moved Permanently`
  - `302 Found`
- **Location Header**: Contains the target URL or path
- **Content-Length**: Always set to `0` (empty body)
- **Connection**: Preserves keep-alive setting from request

### Response Body

Redirect responses always have an empty body with `Content-Length: 0`.

## Use Cases

### URL Migration

When moving content to a new location:

```nginx
location /old-url {
    return 301 /new-url;
}
```

### Temporary Maintenance

Redirecting to a maintenance page:

```nginx
location / {
    return 302 /maintenance.html;
}
```

### Domain Redirects

Redirecting to a different domain:

```nginx
location / {
    return 301 https://www.example.com;
}
```

### Path Normalization

Redirecting trailing slashes or alternative paths:

```nginx
location /page/ {
    return 301 /page;
}
```

## Implementation Details

### File Structure

- **Header**: `include/RequestHandler.hpp`
- **Implementation**: `src/RequestHandler.cpp`
- **Configuration**: `src/ConfigParser.cpp`

### Key Classes and Functions

- **`RequestHandler::handleRedirect()`**: Processes redirect requests
- **`Location::redirect`**: Stores the redirect target URL
- **`Location::redirect_code`**: Stores the redirect status code (301 or 302)

### Processing Logic

1. **URL Format Detection**: The handler checks if the redirect URL starts with `http://` or `https://`
2. **Path Normalization**: Relative paths are normalized to absolute paths
3. **Status Code Selection**: Uses configured code or defaults to 302
4. **Response Generation**: Creates HTTP response with appropriate headers

### Code Flow

```
Request → findServerConfig() → findLocation() → 
    handleRedirect() → HttpResponse with 301/302
```

## Behavior Details

### Keep-Alive

Redirect responses preserve the `keep-alive` setting from the original request. If the client requested keep-alive, the connection remains open for subsequent requests.

### Query Strings

Query strings in the original request are **not** automatically preserved in the redirect. To preserve them, you would need to handle this in your application or use a different approach.

### Request Method

Redirects work with all HTTP methods (GET, POST, DELETE, etc.). However, redirects typically only make sense for GET and HEAD requests, as the HTTP specification recommends that clients should not automatically follow redirects for POST requests.

### Status Code Selection

- If no status code is specified: Defaults to `302 Found`
- If `301` is specified: Returns `301 Moved Permanently`
- If `302` is specified: Returns `302 Found`
- Other status codes: Not supported (falls back to 302)

## Testing

### Manual Testing with curl

```bash
# Test temporary redirect (302)
curl -v http://127.0.0.1:8080/redirect

# Test permanent redirect (301)
curl -v http://127.0.0.1:8080/permanent

# Follow redirects automatically
curl -L http://127.0.0.1:8080/redirect

# Show redirect response only (no following)
curl -v -L --max-redirs 0 http://127.0.0.1:8080/redirect
```

### Expected Response

```
< HTTP/1.1 302 Found
< Location: /index.html
< Content-Length: 0
< Connection: keep-alive
```

### Browser Testing

1. Start the server with redirect configuration
2. Navigate to the redirect location in a browser
3. Observe that the browser automatically follows the redirect
4. For 301 redirects, check that the browser caches the redirect

## Limitations

1. **Status Codes**: Only 301 and 302 are supported. Other redirect codes (303, 307, 308) are not implemented
2. **Query String Preservation**: Query strings from the original request are not automatically appended to redirect targets
3. **Conditional Redirects**: No support for conditional redirects based on headers or request parameters
4. **Regex Patterns**: Location matching does not support regex patterns for redirects

## Best Practices

### When to Use 301 vs 302

- **Use 301** when:
  - Content has permanently moved
  - You want search engines to update their indexes
  - The old URL will never be valid again
  
- **Use 302** when:
  - Content is temporarily moved
  - You want to preserve SEO for the original URL
  - Maintenance or temporary relocation

### Redirect Targets

- Use absolute paths (`/path`) for internal redirects
- Use absolute URLs (`http://example.com/path`) for external redirects
- Avoid relative paths without leading slash (they will be normalized, but explicit is better)

## References

- [HTTP/1.1 Specification - Status Code Definitions (RFC 7231)](https://tools.ietf.org/html/rfc7231#section-6.4)
- [HTTP Redirection (MDN)](https://developer.mozilla.org/en-US/docs/Web/HTTP/Redirections)