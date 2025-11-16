#include "HttpRequest.hpp"

#include <algorithm>
#include <cctype>

static std::string toLowerLocal(const std::string& value) {
    std::string result(value);
    for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
        *it = static_cast<char>(std::tolower(*it));
    }
    return result;
}

HttpRequest::HttpRequest() { reset(); }

void HttpRequest::reset() {
    method.clear();
    target.clear();
    path.clear();
    query.clear();
    version.clear();
    headers.clear();
    body.clear();
    keep_alive = false;
    chunked = false;
    content_length = 0;
}

std::string HttpRequest::getHeader(const std::string& name) const {
    std::string key = toLowerLocal(name);
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it == headers.end()) {
        return "";
    }
    return it->second;
}
