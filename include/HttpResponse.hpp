#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <ctime>

class HttpResponse {
   public:
    HttpResponse();
    ~HttpResponse();

    void setStatus(int code, const std::string& reason);
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);
    void setBody(const char* data, std::size_t size);
    void setKeepAlive(bool keep_alive);

    std::string toString() const;
    std::string getStatusLine() const;
    std::string getHeaders() const;
    std::string getBody() const;
    int getStatusCode() const { return status_code_; }

    static std::string getReasonPhrase(int status_code);
    static std::string getCurrentDate();

   private:
    int status_code_;
    std::string reason_phrase_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};

#endif

