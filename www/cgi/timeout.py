#!/usr/bin/env python3
import time

print("Content-Type: text/html")
print("")

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>Timeout Test</title>")
print("</head>")
print("<body>")
print("<h1>Timeout Test Script</h1>")
print("<p>This script sleeps for 35 seconds to test timeout handling...</p>")

time.sleep(35)

print("<p>This should not be reached due to timeout.</p>")
print("</body>")
print("</html>")
