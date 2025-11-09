# 42webserv-solo

HTTP web server developed individually as part of the 42 project. Implements HTTP/1.1 protocol, supports multiple clients, handles requests and responses, and includes configuration for virtual hosts and error handling.

## Building

```bash
make
```

## Running

```bash
./webserv config/example.conf
```

## Code Formatting

Format all source files:
```bash
make format
```

## Linting

Run static analysis (requires clang-tidy):
```bash
make lint
```

For more details, see [FORMATTING.md](FORMATTING.md)

## Testing

Run configuration parser tests:
```bash
./test_config.sh
```

Test the server:
```bash
# Start the server
./webserv config/test_valid.conf

# In another terminal, send a test request
curl http://127.0.0.1:8080/
```

For comprehensive testing instructions, see [TESTING.md](docs/TESTING.md)

## Documentation

- [STRUCTURES.md](docs/STRUCTURES.md) - Structures and classes reference
- [TESTING.md](docs/TESTING.md) - Testing guide
- [FORMATTING.md](docs/FORMATTING.md) - Code formatting guidelines
- [PARAMETER_PASSING.md](docs/PARAMETER_PASSING.md) - Parameter passing documentation
