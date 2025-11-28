#!/usr/bin/env python3
import sys

print("Content-Type: text/html")
print("")

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>CGI Error Test</title>")
print("</head>")
print("<body>")
print("<h1>This script will exit with error code</h1>")
print("<p>Exiting with status code 1...</p>")
print("</body>")
print("</html>")

sys.exit(1)
