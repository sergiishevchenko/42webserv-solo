#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"
#include "CgiHandler.hpp"
#include <string>

class RequestHandler {
   public:
    RequestHandler();
    ~RequestHandler();

    HttpResponse handleRequest(const HttpRequest& request, const ConfigParser& config,
                               const std::string& server_host, int server_port);

   private:
    const ServerConfig* findServerConfig(const HttpRequest& request, const ConfigParser& config,
                                         const std::string& server_host, int server_port);
    const Location* findLocation(const HttpRequest& request, const ServerConfig& server);
    std::string buildFilePath(const HttpRequest& request, const ServerConfig& server, const Location* location);
    HttpResponse serveFile(const std::string& file_path);
    HttpResponse serveDirectory(const std::string& dir_path, const std::string& request_path,
                                const ServerConfig& server, const Location* location);
    HttpResponse generateAutoindex(const std::string& dir_path, const std::string& request_path);
    HttpResponse generateErrorPage(int status_code, const ServerConfig& server);
    bool isDirectory(const std::string& path);
    bool isFile(const std::string& path);
    std::string normalizePath(const std::string& path);
    bool isPathSafe(const std::string& path, const std::string& root);
    std::string getContentType(const std::string& file_path);
    bool isMethodAllowed(const std::string& method, const Location* location);
    HttpResponse handleGet(const HttpRequest& request, const ServerConfig& server, const Location* location);
    HttpResponse handlePost(const HttpRequest& request, const ServerConfig& server, const Location* location);
    HttpResponse handleDelete(const HttpRequest& request, const ServerConfig& server, const Location* location);
    HttpResponse handleRedirect(const HttpRequest& request, const Location* location);

    RequestHandler(const RequestHandler&);
    RequestHandler& operator=(const RequestHandler&);
};

#endif
