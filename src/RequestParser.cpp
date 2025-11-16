#include "RequestParser.hpp"

#include <cctype>
#include <sstream>
#include <vector>

static bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int fromHex(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return 0;
}

static std::string percentDecode(const std::string& input) {
    std::string result;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size() && isHexDigit(input[i + 1]) && isHexDigit(input[i + 2])) {
            char decoded = static_cast<char>(fromHex(input[i + 1]) * 16 + fromHex(input[i + 2]));
            result.push_back(decoded);
            i += 2;
        } else {
            result.push_back(input[i]);
        }
    }
    return result;
}

RequestParser::RequestParser()
    : state_(STATE_REQUEST_LINE),
      buffer_(),
      error_message_(),
      last_header_name_(),
      content_length_remaining_(0),
      current_chunk_size_(0),
      chunk_bytes_remaining_(0) {}

void RequestParser::reset() {
    state_ = STATE_REQUEST_LINE;
    request_.reset();
    buffer_.clear();
    error_message_.clear();
    last_header_name_.clear();
    content_length_remaining_ = 0;
    current_chunk_size_ = 0;
    chunk_bytes_remaining_ = 0;
}

void RequestParser::fail(const std::string& message) {
    state_ = STATE_ERROR;
    error_message_ = message;
}

RequestParser::ParseResult RequestParser::consume(const char* data, std::size_t length) {
    if (state_ == STATE_COMPLETE || state_ == STATE_ERROR) {
        return (state_ == STATE_COMPLETE) ? PARSE_COMPLETE : PARSE_ERROR;
    }

    if (length > 0) {
        buffer_.append(data, length);
    }

    while (state_ != STATE_COMPLETE && state_ != STATE_ERROR) {
        if (state_ == STATE_REQUEST_LINE) {
            std::size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                break;
            }
            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);
            if (!parseRequestLine(line)) {
                break;
            }
            state_ = STATE_HEADERS;
        } else if (state_ == STATE_HEADERS) {
            std::size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                break;
            }
            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);
            if (line.empty()) {
                if (!finalizeHeaders()) {
                    break;
                }
                if (request_.chunked) {
                    state_ = STATE_BODY_CHUNK_SIZE;
                } else if (content_length_remaining_ > 0) {
                    state_ = STATE_BODY_CONTENT_LENGTH;
                } else {
                    state_ = STATE_COMPLETE;
                }
            } else {
                if (!parseHeaderLine(line)) {
                    break;
                }
            }
        } else if (state_ == STATE_BODY_CONTENT_LENGTH) {
            if (buffer_.empty()) {
                break;
            }
            std::size_t to_copy = buffer_.size();
            if (to_copy > content_length_remaining_) {
                to_copy = content_length_remaining_;
            }
            request_.body.append(buffer_, 0, to_copy);
            buffer_.erase(0, to_copy);
            content_length_remaining_ -= to_copy;
            if (content_length_remaining_ == 0) {
                state_ = STATE_COMPLETE;
            } else {
                break;
            }
        } else if (state_ == STATE_BODY_CHUNK_SIZE) {
            std::size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                break;
            }
            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);
            if (!parseChunkSizeLine(line)) {
                break;
            }
            if (current_chunk_size_ == 0) {
                state_ = STATE_BODY_CHUNK_TRAILERS;
            } else {
                chunk_bytes_remaining_ = current_chunk_size_;
                state_ = STATE_BODY_CHUNK_DATA;
            }
        } else if (state_ == STATE_BODY_CHUNK_DATA) {
            if (buffer_.size() < chunk_bytes_remaining_) {
                request_.body.append(buffer_);
                chunk_bytes_remaining_ -= buffer_.size();
                buffer_.clear();
                break;
            } else {
                request_.body.append(buffer_, 0, chunk_bytes_remaining_);
                buffer_.erase(0, chunk_bytes_remaining_);
                chunk_bytes_remaining_ = 0;
                state_ = STATE_BODY_CHUNK_CRLF;
            }
        } else if (state_ == STATE_BODY_CHUNK_CRLF) {
            if (buffer_.size() < 2) {
                break;
            }
            if (buffer_.compare(0, 2, "\r\n") != 0) {
                fail("Invalid chunk delimiter");
                break;
            }
            buffer_.erase(0, 2);
            state_ = STATE_BODY_CHUNK_SIZE;
        } else if (state_ == STATE_BODY_CHUNK_TRAILERS) {
            std::size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                break;
            }
            if (pos == 0) {
                buffer_.erase(0, 2);
                state_ = STATE_COMPLETE;
            } else {
                std::string line = buffer_.substr(0, pos);
                buffer_.erase(0, pos + 2);
                parseHeaderLine(line);
            }
        }
    }

    if (state_ == STATE_COMPLETE) {
        return PARSE_COMPLETE;
    }
    if (state_ == STATE_ERROR) {
        return PARSE_ERROR;
    }
    return PARSE_INCOMPLETE;
}

bool RequestParser::parseRequestLine(const std::string& line) {
    std::size_t first_space = line.find(' ');
    std::size_t second_space =
        (first_space == std::string::npos) ? std::string::npos
                                           : line.find(' ', first_space + 1);

    if (first_space == std::string::npos || second_space == std::string::npos) {
        fail("Malformed request line");
        return false;
    }

    std::string method = line.substr(0, first_space);
    std::string target =
        line.substr(first_space + 1, second_space - first_space - 1);
    std::string version = line.substr(second_space + 1);

    if (!isToken(method)) {
        fail("Invalid method");
        return false;
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        fail("Unsupported HTTP version");
        return false;
    }

    std::string normalized_path;
    std::string query;
    if (!normalizePath(target, normalized_path, query)) {
        fail("Invalid request target");
        return false;
    }

    request_.method = method;
    request_.target = target;
    request_.path = normalized_path;
    request_.query = query;
    request_.version = version;

    return true;
}

bool RequestParser::parseHeaderLine(const std::string& line) {
    if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
        if (last_header_name_.empty()) {
            fail("Header continuation without name");
            return false;
        }
        std::string value = trim(line);
        request_.headers[last_header_name_] += " " + value;
        return true;
    }

    std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
        fail("Malformed header");
        return false;
    }

    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    name = toLower(trim(name));
    value = trim(value);

    if (name.empty() || !isToken(name)) {
        fail("Invalid header name");
        return false;
    }

    request_.headers[name] = value;
    last_header_name_ = name;

    return true;
}

bool RequestParser::finalizeHeaders() {
    std::string content_length_value = request_.getHeader("content-length");
    if (!content_length_value.empty()) {
        std::istringstream iss(content_length_value);
        long long length = -1;
        iss >> length;
        if (iss.fail() || length < 0) {
            fail("Invalid Content-Length");
            return false;
        }
        request_.content_length = static_cast<std::size_t>(length);
        content_length_remaining_ = request_.content_length;
    } else {
        request_.content_length = 0;
        content_length_remaining_ = 0;
    }

    std::string transfer_encoding = request_.getHeader("transfer-encoding");
    if (!transfer_encoding.empty()) {
        std::string lower = toLower(transfer_encoding);
        if (lower.find("chunked") != std::string::npos) {
            request_.chunked = true;
        }
    }

    if (request_.chunked) {
        content_length_remaining_ = 0;
    }

    determineKeepAlive();
    return true;
}

void RequestParser::determineKeepAlive() {
    bool keep_alive_default = (request_.version == "HTTP/1.1");
    std::string connection = toLower(request_.getHeader("connection"));

    if (!connection.empty()) {
        if (connection.find("close") != std::string::npos) {
            request_.keep_alive = false;
            return;
        }
        if (connection.find("keep-alive") != std::string::npos) {
            request_.keep_alive = true;
            return;
        }
    }

    request_.keep_alive = keep_alive_default;
}

bool RequestParser::parseChunkSizeLine(const std::string& line) {
    std::string trimmed = line;
    std::size_t semicolon = trimmed.find(';');
    if (semicolon != std::string::npos) {
        trimmed = trimmed.substr(0, semicolon);
    }
    trimmed = trim(trimmed);
    if (trimmed.empty()) {
        fail("Empty chunk size");
        return false;
    }

    std::size_t size = 0;
    for (std::size_t i = 0; i < trimmed.size(); ++i) {
        if (!isHexDigit(trimmed[i])) {
            fail("Invalid chunk size");
            return false;
        }
        size = (size << 4) + fromHex(trimmed[i]);
    }
    current_chunk_size_ = size;
    return true;
}

std::string RequestParser::trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() &&
           (value[start] == ' ' || value[start] == '\t' ||
            value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' ||
            value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string RequestParser::toLower(const std::string& value) {
    std::string result(value);
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>(std::tolower(result[i]));
    }
    return result;
}

bool RequestParser::isToken(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const std::string separators = "()<>@,;:\\\"/[]?={} \t";
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c <= 31 || c == 127 || separators.find(c) != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool RequestParser::normalizePath(const std::string& raw_target, std::string& normalized, std::string& query) {
    std::string target = raw_target;
    std::size_t query_pos = target.find('?');
    std::string path_part = (query_pos == std::string::npos) ? target : target.substr(0, query_pos);
    query = (query_pos == std::string::npos) ? "" : target.substr(query_pos + 1);

    if (path_part.empty()) {
        path_part = "/";
    }

    if (!path_part.empty() && path_part[0] != '/') {
        return false;
    }

    std::vector<std::string> segments;
    std::size_t i = 0;
    while (i < path_part.size()) {
        while (i < path_part.size() && path_part[i] == '/') {
            ++i;
        }
        std::size_t start = i;
        while (i < path_part.size() && path_part[i] != '/') {
            ++i;
        }
        if (start == i) {
            continue;
        }
        std::string segment =
            percentDecode(path_part.substr(start, i - start));
        if (segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (segments.empty()) {
                return false;
            }
            segments.pop_back();
            continue;
        }
        segments.push_back(segment);
    }

    normalized = "/";
    for (std::size_t j = 0; j < segments.size(); ++j) {
        normalized += segments[j];
        if (j + 1 < segments.size()) {
            normalized += "/";
        }
    }

    if (path_part[path_part.size() - 1] == '/' && normalized.size() > 1 &&
        normalized[normalized.size() - 1] != '/') {
        normalized += "/";
    }

    return true;
}
