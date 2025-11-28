#!/usr/bin/env python3
import os

print("Content-Type: text/html")
print("")

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>CGI Environment Variables</title>")
print("<style>")
print("body { font-family: monospace; margin: 20px; }")
print("table { border-collapse: collapse; width: 100%; }")
print("th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }")
print("th { background-color: #4CAF50; color: white; }")
print("tr:nth-child(even) { background-color: #f2f2f2; }")
print("</style>")
print("</head>")
print("<body>")
print("<h1>CGI Environment Variables</h1>")
print("<table>")
print("<tr><th>Variable</th><th>Value</th></tr>")

cgi_vars = [
    'REQUEST_METHOD',
    'SERVER_PROTOCOL',
    'SERVER_SOFTWARE',
    'SERVER_NAME',
    'SERVER_PORT',
    'GATEWAY_INTERFACE',
    'SCRIPT_NAME',
    'SCRIPT_FILENAME',
    'PATH_INFO',
    'PATH_TRANSLATED',
    'QUERY_STRING',
    'REQUEST_URI',
    'DOCUMENT_ROOT',
    'CONTENT_LENGTH',
    'CONTENT_TYPE',
    'REMOTE_ADDR'
]

for var in cgi_vars:
    value = os.environ.get(var, '')
    print("<tr><td>" + var + "</td><td>" + value + "</td></tr>")

print("<tr><th colspan='2'>HTTP Headers</th></tr>")
for key in sorted(os.environ.keys()):
    if key.startswith('HTTP_'):
        header_name = key[5:].replace('_', '-').title()
        value = os.environ.get(key, '')
        print("<tr><td>" + key + "</td><td>" + value + "</td></tr>")

print("</table>")
print("</body>")
print("</html>")
