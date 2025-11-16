#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <cstddef>
#include <map>
#include <string>

struct HttpRequest {
    HttpRequest();

    void reset();
    std::string getHeader(const std::string& name) const;

    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    bool keep_alive;
    bool chunked;
    std::size_t content_length;
};

#endif
