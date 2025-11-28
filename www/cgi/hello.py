#!/usr/bin/env python3
import os

print("Content-Type: text/html")
print("")

method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
query = os.environ.get('QUERY_STRING', '')

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>CGI Hello World</title>")
print("</head>")
print("<body>")
print("<h1>Hello from CGI!</h1>")
print("<p><strong>Method:</strong> " + method + "</p>")
if query:
    print("<p><strong>Query String:</strong> " + query + "</p>")
print("<p>CGI script executed successfully!</p>")
print("</body>")
print("</html>")
