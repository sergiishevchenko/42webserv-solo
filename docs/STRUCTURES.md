# Structures and Classes Reference

This document describes all structures and classes used in the webserv project.

---

## Structures

### Location

Represents a location block in the server configuration. Defines how requests to specific URL paths should be handled.

**Definition:**
```cpp
struct Location {
    std::string path;                                    // URL path pattern
    std::set<std::string> methods;                      // Allowed HTTP methods
    std::string root;                                    // Root directory for this location
    std::string index;                                   // Default index file
    bool autoindex;                                      // Enable directory listing
    std::string redirect;                                // Redirect URL
    std::string upload_store;                           // Upload directory path
    std::map<std::string, std::string> cgi_pass;       // CGI handler mappings (extension -> program)

    Location() : autoindex(false) {}
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `path` | `std::string` | URL path pattern (e.g., `/uploads`, `/cgi`) |
| `methods` | `std::set<std::string>` | Allowed HTTP methods (GET, POST, DELETE, etc.) |
| `root` | `std::string` | Root directory path for serving files from this location |
| `index` | `std::string` | Default file to serve when directory is requested |
| `autoindex` | `bool` | Enable directory listing (default: `false`) |
| `redirect` | `std::string` | URL to redirect requests to this location |
| `upload_store` | `std::string` | Directory path for file uploads |
| `cgi_pass` | `std::map<std::string, std::string>` | Maps file extensions to CGI program paths (e.g., `{".py": "/usr/bin/python"}`) |

**Usage Example:**
```cpp
Location location;
location.path = "/uploads";
location.methods.insert("GET");
location.methods.insert("POST");
location.upload_store = "www/uploads";
location.autoindex = false;
```

---

### ServerConfig

Represents a server block in the configuration. Defines server settings, listening addresses/ports, and location blocks.

**Definition:**
```cpp
struct ServerConfig {
    std::vector<std::pair<std::string, int> > listen;  // Listening interfaces and ports
    std::string root;                                   // Root directory for the server
    std::string index;                                  // Default index file
    size_t client_max_body_size;                        // Maximum request body size (bytes)
    std::map<int, std::string> error_pages;             // Custom error page mappings
    std::vector<Location> locations;                    // Location blocks

    ServerConfig() : client_max_body_size(1048576) {}  // Default: 1 MB
};
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `listen` | `std::vector<std::pair<std::string, int> >` | List of interface:port pairs to listen on (e.g., `("127.0.0.1", 8080)`) |
| `root` | `std::string` | Root directory path for serving files |
| `index` | `std::string` | Default index file name |
| `client_max_body_size` | `size_t` | Maximum allowed size of request body in bytes (default: 1048576 = 1 MB) |
| `error_pages` | `std::map<int, std::string>` | Maps HTTP error codes to custom error page paths (e.g., `{404: "/errors/404.html"}`) |
| `locations` | `std::vector<Location>` | List of location blocks for path-specific configuration |

**Usage Example:**
```cpp
ServerConfig server;
server.listen.push_back(std::make_pair("127.0.0.1", 8080));
server.listen.push_back(std::make_pair("0.0.0.0", 8080));
server.root = "www";
server.index = "index.html";
server.client_max_body_size = 10485760;  // 10 MB
server.error_pages[404] = "/errors/404.html";
```

---

## Classes

### ConfigParser

Parses and validates server configuration files. Supports nginx-like configuration syntax with server and location blocks.

**Definition:**
```cpp
class ConfigParser {
   public:
    ConfigParser();
    ~ConfigParser();

    bool loadFromFile(const std::string& filepath);
    std::string getLastError() const;
    const std::vector<ServerConfig>& getServers() const;
    bool validate() const;

   private:
    // ... implementation details
};
```

**Public Methods:**

| Method | Return Type | Description |
|--------|-------------|-------------|
| `ConfigParser()` | - | Default constructor |
| `~ConfigParser()` | - | Destructor |
| `loadFromFile(const std::string& filepath)` | `bool` | Loads and parses configuration from file. Returns `true` on success, `false` on error. |
| `getLastError() const` | `std::string` | Returns the last error message encountered during parsing or validation |
| `getServers() const` | `const std::vector<ServerConfig>&` | Returns a const reference to the parsed server configurations |
| `validate() const` | `bool` | Validates the parsed configuration. Returns `true` if valid, `false` otherwise. |

**Usage Example:**
```cpp
ConfigParser parser;

if (!parser.loadFromFile("config/example.conf")) {
    std::cerr << "Error: " << parser.getLastError() << std::endl;
    return 1;
}

if (!parser.validate()) {
    std::cerr << "Validation error: " << parser.getLastError() << std::endl;
    return 1;
}

const std::vector<ServerConfig>& servers = parser.getServers();
for (size_t i = 0; i < servers.size(); ++i) {
    const ServerConfig& server = servers[i];
    // Use server configuration...
}
```

**Configuration File Format:**

The parser supports nginx-like configuration syntax:

```nginx
server {
    listen 127.0.0.1:8080;
    root www;
    index index.html;
    client_max_body_size 10485760;
    
    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;
    
    location /uploads {
        methods GET POST;
        upload_store www/uploads;
        autoindex on;
    }
    
    location /cgi {
        methods GET POST;
        cgi_pass .py /usr/bin/python;
        cgi_pass .php /usr/bin/php;
    }
    
    location / {
        methods GET;
        root www/public;
        index index.html;
    }
}
```

**Error Handling:**

- If `loadFromFile()` returns `false`, use `getLastError()` to get the error message
- If `validate()` returns `false`, use `getLastError()` to get validation error details
- Common errors include:
  - File not found
  - Invalid syntax
  - Missing required directives
  - Invalid port numbers
  - Invalid configuration values

---

## Data Structures Reference

### std::set<std::string> methods

Stores allowed HTTP methods for a location. Automatically sorts and removes duplicates.

**Example:**
```cpp
std::set<std::string> methods;
methods.insert("GET");
methods.insert("POST");
methods.insert("DELETE");
// Result: {"DELETE", "GET", "POST"} (sorted)
```

### std::map<std::string, std::string> cgi_pass

Maps file extensions to CGI program paths.

**Example:**
```cpp
std::map<std::string, std::string> cgi_pass;
cgi_pass[".py"] = "/usr/bin/python";
cgi_pass[".php"] = "/usr/bin/php";
```

### std::vector<std::pair<std::string, int> > listen

Stores listening interface:port pairs.

**Example:**
```cpp
std::vector<std::pair<std::string, int> > listen;
listen.push_back(std::make_pair("127.0.0.1", 8080));
listen.push_back(std::make_pair("0.0.0.0", 8080));
// Access: listen[0].first = "127.0.0.1", listen[0].second = 8080
```

### std::map<int, std::string> error_pages

Maps HTTP error codes to custom error page paths.

**Example:**
```cpp
std::map<int, std::string> error_pages;
error_pages[404] = "/errors/404.html";
error_pages[500] = "/errors/500.html";
```

---

## Notes

- All string fields are empty by default (except `client_max_body_size` which defaults to 1 MB)
- Collections (vectors, maps, sets) are empty by default
- The `autoindex` field in `Location` defaults to `false`
- Configuration files support comments starting with `#`
- Directives must end with `;` (semicolon)
- Block structures use `{` and `}` braces

---

## Future Additions

This document will be updated as new structures and classes are added to the project.

