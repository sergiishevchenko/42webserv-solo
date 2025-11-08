# How to Test the Configuration Parser

## Quick Test

Run the automated testing script:

```bash
./test_config.sh
```

## Manual Testing

### 1. Testing Valid Configuration

```bash
./webserv config/test_valid.conf
```

Expected result: successful loading with output of all parsed data

### 2. Testing Error Handling

```bash
# Invalid port
./webserv config/test_invalid1.conf

# Missing listen directive
./webserv config/test_invalid2.conf

# Missing root directive
./webserv config/test_invalid3.conf

# Non-existent file
./webserv config/nonexistent.conf
```

Expected result: detailed error messages

### 3. Creating Your Own Test Configuration

Create a file `config/my_test.conf` and test it:

```bash
./webserv config/my_test.conf
```

## What to Check

1. **Directive Parsing:**
   - `listen` with single and multiple ports
   - `root`, `index`
   - `client_max_body_size`
   - `error_page` with error codes

2. **Location Block Parsing:**
   - `methods` (GET, POST, DELETE)
   - `upload_store`
   - `cgi_pass` with extensions
   - `redirect`
   - `autoindex on/off`

3. **Validation:**
   - Required directives (listen, root)
   - Port validity (1-65535)
   - Error code validity (400-599)

4. **Error Handling:**
   - Detailed error messages
   - Correct handling of non-existent files

## Test Configuration Examples

All test configurations are located in the `config/` directory:
- `example.conf` - basic example
- `test_valid.conf` - complete valid configuration
- `test_invalid1.conf` - invalid port
- `test_invalid2.conf` - missing listen
- `test_invalid3.conf` - missing root
