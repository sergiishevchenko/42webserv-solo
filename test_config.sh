#!/bin/bash

echo "=== Testing webserv configuration parser ==="
echo ""

echo "1. Valid config with multiple ports and locations:"
./webserv config/test_valid.conf
echo ""

echo "2. Original example.conf:"
./webserv config/example.conf
echo ""

echo "3. Test with invalid port (>65535):"
./webserv config/test_invalid1.conf
echo ""

echo "4. Test without listen directive:"
./webserv config/test_invalid2.conf
echo ""

echo "5. Test without root directive:"
./webserv config/test_invalid3.conf
echo ""

echo "6. Test with non-existent file:"
./webserv config/nonexistent.conf
echo ""

echo "=== Testing completed ==="
