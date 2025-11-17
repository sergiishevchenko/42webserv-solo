#include "HttpResponse.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

HttpResponse::HttpResponse() : status_code_(200), reason_phrase_("OK") {
    setHeader("Server", "webserv/1.0");
    setHeader("Date", getCurrentDate());
}

HttpResponse::~HttpResponse() {}

void HttpResponse::setStatus(int code, const std::string& reason) {
    status_code_ = code;
    reason_phrase_ = reason.empty() ? getReasonPhrase(code) : reason;
}

void HttpResponse::setHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

void HttpResponse::setBody(const std::string& body) {
    body_ = body;
    std::ostringstream oss;
    oss << body_.size();
    setHeader("Content-Length", oss.str());
}

void HttpResponse::setBody(const char* data, std::size_t size) {
    body_.assign(data, size);
    std::ostringstream oss;
    oss << size;
    setHeader("Content-Length", oss.str());
}

void HttpResponse::setKeepAlive(bool keep_alive) {
    setHeader("Connection", keep_alive ? "keep-alive" : "close");
}

std::string HttpResponse::toString() const {
    std::ostringstream response;
    response << getStatusLine();
    response << getHeaders();
    response << "\r\n";
    response << body_;
    return response.str();
}

std::string HttpResponse::getStatusLine() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code_ << " " << reason_phrase_ << "\r\n";
    return oss.str();
}

std::string HttpResponse::getHeaders() const {
    std::ostringstream oss;
    for (std::map<std::string, std::string>::const_iterator it = headers_.begin();
         it != headers_.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
    }
    return oss.str();
}

std::string HttpResponse::getBody() const {
    return body_;
}

std::string HttpResponse::getReasonPhrase(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 400:
            return "Bad Request";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 411:
            return "Length Required";
        case 413:
            return "Payload Too Large";
        case 414:
            return "URI Too Long";
        case 431:
            return "Request Header Fields Too Large";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        case 505:
            return "HTTP Version Not Supported";
        default:
            return "Unknown Status";
    }
}

std::string HttpResponse::getCurrentDate() {
    time_t now = time(NULL);
    struct tm* gmt = gmtime(&now);
    char buffer[128];
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buffer);
}
