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

For more details, see [TESTING.md](TEST_CONFIG.md)
