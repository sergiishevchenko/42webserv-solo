#!/usr/bin/env python3
import os
import json

print("Content-Type: application/json")
print("")

method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
query = os.environ.get('QUERY_STRING', '')

data = {
    'method': method,
    'query_string': query,
    'server_protocol': os.environ.get('SERVER_PROTOCOL', ''),
    'server_name': os.environ.get('SERVER_NAME', ''),
    'server_port': os.environ.get('SERVER_PORT', ''),
    'script_name': os.environ.get('SCRIPT_NAME', ''),
    'message': 'CGI script executed successfully'
}

if query:
    from urllib.parse import parse_qs
    params = parse_qs(query)
    data['parsed_query'] = {k: v[0] if len(v) == 1 else v for k, v in params.items()}

print(json.dumps(data, indent=2))
