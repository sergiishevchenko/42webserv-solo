# Examples of ConfigParser.hpp Data Structures Usage

## 1. `std::set<std::string> methods`

**Purpose:** Stores allowed HTTP methods for a location (GET, POST, DELETE, etc.)

**Example:**
```cpp
Location location;
location.methods.insert("GET");
location.methods.insert("POST");
location.methods.insert("DELETE");

**Result:** `{"DELETE", "GET", "POST"}` (set automatically sorts)

---

## 2. `std::map<std::string, std::string> cgi_pass`

**Purpose:** Maps file extensions to programs for CGI processing

**Example:**
```cpp
Location location;
location.cgi_pass[".py"] = "/usr/bin/python";
location.cgi_pass[".php"] = "/usr/bin/php";
location.cgi_pass[".pl"] = "/usr/bin/perl";
```

**Result:**
```
Extension: .pl -> Program: /usr/bin/perl
Extension: .php -> Program: /usr/bin/php
Extension: .py -> Program: /usr/bin/python
```

---

## 3. `std::vector<std::pair<std::string, int> > listen`

**Purpose:** Stores a list of interfaces and ports to listen on

**Example:**
```cpp
ServerConfig server;
server.listen.push_back(std::make_pair("127.0.0.1", 8080));
server.listen.push_back(std::make_pair("0.0.0.0", 8080));
server.listen.push_back(std::make_pair("192.168.1.100", 3000));
```

**Result:**
```
127.0.0.1:8080
0.0.0.0:8080
192.168.1.100:3000
```

---

## 4. `std::map<int, std::string> error_pages`

**Purpose:** Maps HTTP error codes to custom error page paths

**Example:**
```cpp
ServerConfig server;
server.error_pages[404] = "/errors/404.html";
server.error_pages[500] = "/errors/500.html";
server.error_pages[403] = "/errors/forbidden.html";
server.error_pages[502] = "/errors/bad_gateway.html";
```

**Result:**
```
Error 403 -> Page: /errors/forbidden.html
Error 404 -> Page: /errors/404.html
Error 500 -> Page: /errors/500.html
Error 502 -> Page: /errors/bad_gateway.html
```

---

## 5. `std::vector<Location> locations`

**Purpose:** Stores a list of all location blocks for the server

**Example:**
```cpp
ServerConfig server;

// Location 1: file upload
Location loc1;
loc1.path = "/uploads";
loc1.methods.insert("GET");
loc1.methods.insert("POST");
loc1.upload_store = "www/uploads";
server.locations.push_back(loc1);

// Location 2: CGI
Location loc2;
loc2.path = "/cgi";
loc2.cgi_pass[".py"] = "/usr/bin/python";
loc2.methods.insert("GET");
loc2.methods.insert("POST");
server.locations.push_back(loc2);

// Location 3: root directory
Location loc3;
loc3.path = "/";
loc3.root = "www/public";
loc3.index = "index.html";
loc3.autoindex = false;
loc3.methods.insert("GET");
server.locations.push_back(loc3);
```

**Result:**
```
Location: /uploads
  Methods: GET POST
Location: /cgi
  Methods: GET POST
Location: /
  Methods: GET
```

---

## Constructor Explanation

```cpp
ServerConfig() : client_max_body_size(1048576) {}
```

**Syntax:**
- `ServerConfig()` - default constructor
- `: client_max_body_size(1048576)` - member initializer list
- `{}` - empty constructor body

**What happens:**
1. Initializes `client_max_body_size = 1048576` (1 MB)
2. Other fields are initialized with default values:
   - `listen` → empty `std::vector`
   - `root`, `index` → empty `std::string`
   - `error_pages` → empty `std::map`
   - `locations` → empty `std::vector`

**Why use member initializer list:**
- More efficient (direct initialization instead of assignment)
- Only way to initialize const fields and references
- Recommended C++ style

---

## getServers() Method Explanation

```cpp
const std::vector<ServerConfig>& getServers() const { return servers_; }
```

**Breakdown:**
- `const std::vector<ServerConfig>&` - returns const reference (not a copy!)
- `getServers()` - method name
- `const` - method does not modify the ConfigParser object
- `return servers_;` - returns reference to private field

**Advantages:**
1. ✅ Does not create a copy (efficient)
2. ✅ Protection against modification (const reference)
3. ✅ Method does not modify object (const method)

**Usage:**
```cpp
ConfigParser parser;
parser.loadFromFile("config.conf");

// Get reference (not a copy!)
const std::vector<ServerConfig>& servers = parser.getServers();

// Read data
std::cout << "Number of servers: " << servers.size() << std::endl;

// servers.push_back(...);  // ❌ Error - const reference
```
