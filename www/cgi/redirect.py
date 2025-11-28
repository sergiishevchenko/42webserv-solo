#!/usr/bin/env python3

print("Status: 302 Found")
print("Location: /index.html")
print("Content-Type: text/html")
print("")

print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>Redirecting...</title>")
print("</head>")
print("<body>")
print("<h1>Redirecting...</h1>")
print("<p>You should be redirected to /index.html</p>")
print("<p>If not, <a href='/index.html'>click here</a></p>")
print("</body>")
print("</html>")
