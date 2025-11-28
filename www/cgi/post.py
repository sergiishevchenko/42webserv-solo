#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html")
print("")

method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
content_length = os.environ.get('CONTENT_LENGTH', '0')
content_type = os.environ.get('CONTENT_TYPE', '')

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>POST Request Handler</title>")
print("<style>")
print("body { font-family: monospace; margin: 20px; }")
print("pre { background-color: #f4f4f4; padding: 10px; border: 1px solid #ddd; }")
print("</style>")
print("</head>")
print("<body>")
print("<h1>POST Request Handler</h1>")
print("<p><strong>Method:</strong> " + method + "</p>")
print("<p><strong>Content-Type:</strong> " + content_type + "</p>")
print("<p><strong>Content-Length:</strong> " + content_length + "</p>")

if method == 'POST':
    try:
        length = int(content_length)
        if length > 0:
            body = sys.stdin.read(length)
            print("<h2>Request Body:</h2>")
            print("<pre>" + body + "</pre>")
        else:
            print("<p>No body data received.</p>")
    except (ValueError, IOError) as e:
        print("<p>Error reading body: " + str(e) + "</p>")
else:
    print("<p>This script expects a POST request.</p>")
    print("<p>Use curl: curl -X POST -d 'data=test' http://localhost:8080/cgi/post.py</p>")

print("</body>")
print("</html>")
