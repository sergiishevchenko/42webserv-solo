#!/usr/bin/env python3
import os
import urllib.parse

print("Content-Type: text/html")
print("")

query_string = os.environ.get('QUERY_STRING', '')
method = os.environ.get('REQUEST_METHOD', 'GET')

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>Query String Parser</title>")
print("<style>")
print("body { font-family: monospace; margin: 20px; }")
print("table { border-collapse: collapse; width: 100%; }")
print("th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }")
print("th { background-color: #2196F3; color: white; }")
print("</style>")
print("</head>")
print("<body>")
print("<h1>Query String Parser</h1>")
print("<p><strong>Method:</strong> " + method + "</p>")
print("<p><strong>Raw Query String:</strong> " + query_string + "</p>")

if query_string:
    params = urllib.parse.parse_qs(query_string)
    print("<h2>Parsed Parameters:</h2>")
    print("<table>")
    print("<tr><th>Parameter</th><th>Value(s)</th></tr>")
    for key, values in params.items():
        print("<tr><td>" + key + "</td><td>" + ", ".join(values) + "</td></tr>")
    print("</table>")
else:
    print("<p>No query string provided. Try: /cgi/query.py?name=value&foo=bar</p>")

print("</body>")
print("</html>")
