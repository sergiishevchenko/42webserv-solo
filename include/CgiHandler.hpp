#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"
#include <string>
#include <map>

class CgiHandler {
   public:
    CgiHandler();
    ~CgiHandler();

    static bool shouldHandleByCgi(const HttpRequest& request, const Location* location, const std::string& file_path);
    static HttpResponse executeCgi(const HttpRequest& request, const ServerConfig& server, const Location* location,
                                   const std::string& script_path);

   private:
    static std::string getFileExtension(const std::string& file_path);
    static std::string findCgiInterpreter(const std::string& extension, const Location* location);
    static std::map<std::string, std::string> buildCgiEnv(const HttpRequest& request, const ServerConfig& server,
                                                          const Location* location, const std::string& script_path);
    static char** envToArray(const std::map<std::string, std::string>& env);
    static void freeEnvArray(char** env_array);
    static std::string parsePathInfo(const std::string& request_path, const Location* location,
                                     const std::string& script_path);
    static HttpResponse parseCgiResponse(const std::string& cgi_output);
    static void killProcess(pid_t pid);

    CgiHandler(const CgiHandler&);
    CgiHandler& operator=(const CgiHandler&);
};

#endif
