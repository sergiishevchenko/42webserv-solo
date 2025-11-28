#!/usr/bin/env python3
import os

query_string = os.environ.get('QUERY_STRING', '')
status = query_string.split('=')[1] if 'status=' in query_string else '200'

status_codes = {
    '200': ('200', 'OK'),
    '201': ('201', 'Created'),
    '301': ('301', 'Moved Permanently'),
    '302': ('302', 'Found'),
    '400': ('400', 'Bad Request'),
    '404': ('404', 'Not Found'),
    '500': ('500', 'Internal Server Error')
}

code, reason = status_codes.get(status, ('200', 'OK'))

print("Status: " + code + " " + reason)
print("Content-Type: text/html")
print("")

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>Status Code Test</title>")
print("</head>")
print("<body>")
print("<h1>HTTP Status Code: " + code + " " + reason + "</h1>")
print("<p>This response was generated with status code " + code + "</p>")
print("<p>Try different status codes: /cgi/status.py?status=404</p>")
print("</body>")
print("</html>")
