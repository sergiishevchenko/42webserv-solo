#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include <cstddef>
#include <string>

#include "HttpRequest.hpp"

class RequestParser {
   public:
    enum ParseResult { PARSE_INCOMPLETE, PARSE_COMPLETE, PARSE_ERROR };

    RequestParser();

    ParseResult consume(const char* data, std::size_t length);
    const HttpRequest& getRequest() const { return request_; }
    const std::string& getError() const { return error_message_; }
    void reset();
    void setMaxBodySize(std::size_t max_size) { max_body_size_ = max_size; }

   private:
    enum State {
        STATE_REQUEST_LINE,
        STATE_HEADERS,
        STATE_BODY_CONTENT_LENGTH,
        STATE_BODY_CHUNK_SIZE,
        STATE_BODY_CHUNK_DATA,
        STATE_BODY_CHUNK_CRLF,
        STATE_BODY_CHUNK_TRAILERS,
        STATE_COMPLETE,
        STATE_ERROR
    };

    State state_;
    HttpRequest request_;
    std::string buffer_;
    std::string error_message_;
    std::string last_header_name_;
    std::size_t content_length_remaining_;
    std::size_t current_chunk_size_;
    std::size_t chunk_bytes_remaining_;
    std::size_t max_body_size_;

    bool parseRequestLine(const std::string& line);
    bool parseHeaderLine(const std::string& line);
    bool finalizeHeaders();
    bool parseChunkSizeLine(const std::string& line);
    void determineKeepAlive();
    void fail(const std::string& message);

    static std::string trim(const std::string& value);
    static std::string toLower(const std::string& value);
    static bool isToken(const std::string& value);
    static bool normalizePath(const std::string& raw_target,
                              std::string& normalized,
                              std::string& query);
};

#endif
