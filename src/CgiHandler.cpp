#include "CgiHandler.hpp"
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <ctime>

CgiHandler::CgiHandler() {}

CgiHandler::~CgiHandler() {}

std::string CgiHandler::getFileExtension(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == file_path.length() - 1) {
        return "";
    }
    return file_path.substr(dot_pos);
}

bool CgiHandler::shouldHandleByCgi(const HttpRequest& request, const Location* location, const std::string& file_path) {
    (void)request;
    if (!location || location->cgi_pass.empty()) {
        return false;
    }

    std::string extension = getFileExtension(file_path);
    if (extension.empty()) {
        return false;
    }

    return location->cgi_pass.find(extension) != location->cgi_pass.end();
}

std::string CgiHandler::findCgiInterpreter(const std::string& extension, const Location* location) {
    if (!location) {
        return "";
    }

    std::map<std::string, std::string>::const_iterator it = location->cgi_pass.find(extension);
    if (it != location->cgi_pass.end()) {
        return it->second;
    }

    return "";
}

std::string CgiHandler::parsePathInfo(const std::string& request_path, const Location* location,
                                      const std::string& script_path) {
    (void)location;
    (void)script_path;
    
    size_t dot_pos = request_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    
    size_t slash_after_script = request_path.find('/', dot_pos);
    if (slash_after_script == std::string::npos) {
        return "";
    }
    
    return request_path.substr(slash_after_script);
}

std::map<std::string, std::string> CgiHandler::buildCgiEnv(const HttpRequest& request, const ServerConfig& server,
                                                           const Location* location, const std::string& script_path) {
    std::map<std::string, std::string> env;

    env["REQUEST_METHOD"] = request.method;
    env["SERVER_PROTOCOL"] = request.version;
    env["SERVER_SOFTWARE"] = "webserv/1.0";
    env["GATEWAY_INTERFACE"] = "CGI/1.1";

    std::string path_info = parsePathInfo(request.path, location, script_path);
    env["PATH_INFO"] = path_info;
    
    if (!path_info.empty()) {
        env["PATH_TRANSLATED"] = server.root + path_info;
    } else {
        env["PATH_TRANSLATED"] = "";
    }

    env["QUERY_STRING"] = request.query;
    
    std::string script_name = request.path;
    if (!path_info.empty()) {
        script_name = request.path.substr(0, request.path.length() - path_info.length());
    }
    env["SCRIPT_NAME"] = script_name;
    env["SCRIPT_FILENAME"] = script_path;

    std::ostringstream oss;
    oss << request.body.size();
    env["CONTENT_LENGTH"] = oss.str();
    
    std::string content_type = request.getHeader("Content-Type");
    if (!content_type.empty()) {
        env["CONTENT_TYPE"] = content_type;
    }

    std::string server_host = request.getHeader("Host");
    if (!server_host.empty()) {
        size_t colon_pos = server_host.find(':');
        if (colon_pos != std::string::npos) {
            env["SERVER_NAME"] = server_host.substr(0, colon_pos);
            env["SERVER_PORT"] = server_host.substr(colon_pos + 1);
        } else {
            env["SERVER_NAME"] = server_host;
            env["SERVER_PORT"] = "80";
        }
    } else {
        env["SERVER_NAME"] = "localhost";
        env["SERVER_PORT"] = "8080";
    }

    env["REQUEST_URI"] = request.target;
    env["DOCUMENT_ROOT"] = server.root;

    for (std::map<std::string, std::string>::const_iterator it = request.headers.begin(); it != request.headers.end(); ++it) {
        std::string header_name = it->first;
        std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::toupper);
        std::replace(header_name.begin(), header_name.end(), '-', '_');
        env["HTTP_" + header_name] = it->second;
    }

    std::string remote_addr = request.getHeader("X-Forwarded-For");
    if (!remote_addr.empty()) {
        env["REMOTE_ADDR"] = remote_addr;
    } else {
        env["REMOTE_ADDR"] = "127.0.0.1";
    }

    return env;
}

char** CgiHandler::envToArray(const std::map<std::string, std::string>& env) {
    char** env_array = new char*[env.size() + 1];
    size_t i = 0;

    for (std::map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
        std::string env_var = it->first + "=" + it->second;
        env_array[i] = new char[env_var.length() + 1];
        std::strcpy(env_array[i], env_var.c_str());
        ++i;
    }

    env_array[i] = NULL;
    return env_array;
}

void CgiHandler::freeEnvArray(char** env_array) {
    if (!env_array) {
        return;
    }

    for (size_t i = 0; env_array[i] != NULL; ++i) {
        delete[] env_array[i];
    }
    delete[] env_array;
}

void CgiHandler::killProcess(pid_t pid) {
    if (pid <= 0) {
        return;
    }

    kill(pid, SIGTERM);
    sleep(1);

    int status;
    if (waitpid(pid, &status, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
}

HttpResponse CgiHandler::parseCgiResponse(const std::string& cgi_output) {
    HttpResponse response;
    
    size_t header_end = cgi_output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = cgi_output.find("\n\n");
        if (header_end == std::string::npos) {
            response.setStatus(502, "Bad Gateway");
            response.setBody("Invalid CGI response format");
            return response;
        }
        header_end += 2;
    } else {
        header_end += 4;
    }

    std::string headers_str = cgi_output.substr(0, header_end);
    std::string body = cgi_output.substr(header_end);

    std::istringstream headers_stream(headers_str);
    std::string line;
    bool status_set = false;

    while (std::getline(headers_stream, line)) {
        if (line.empty() || line == "\r") {
            break;
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        std::string header_name = line.substr(0, colon_pos);
        std::string header_value = line.substr(colon_pos + 1);
        
        while (!header_value.empty() && (header_value[0] == ' ' || header_value[0] == '\t')) {
            header_value = header_value.substr(1);
        }

        while (!header_value.empty() && header_value[header_value.length() - 1] == '\r') {
            header_value = header_value.substr(0, header_value.length() - 1);
        }

        if (header_name == "Status") {
            std::istringstream iss(header_value);
            int status_code;
            std::string reason;
            iss >> status_code;
            std::getline(iss, reason);
            while (!reason.empty() && reason[0] == ' ') {
                reason = reason.substr(1);
            }
            if (reason.empty()) {
                reason = HttpResponse::getReasonPhrase(status_code);
            }
            response.setStatus(status_code, reason);
            status_set = true;
        } else if (header_name == "Content-Type") {
            response.setHeader("Content-Type", header_value);
        } else if (header_name == "Location") {
            response.setHeader("Location", header_value);
            if (!status_set) {
                response.setStatus(302, "Found");
            }
        } else if (header_name == "Content-Length") {
            response.setHeader("Content-Length", header_value);
        } else {
            response.setHeader(header_name, header_value);
        }
    }

    if (!status_set) {
        response.setStatus(200, "OK");
    }

    response.setBody(body);
    return response;
}

HttpResponse CgiHandler::executeCgi(const HttpRequest& request, const ServerConfig& server, const Location* location,
                                    const std::string& script_path) {
    HttpResponse error_response;

    if (script_path.empty()) {
        error_response.setStatus(404, "Not Found");
        error_response.setBody("CGI script not found");
        return error_response;
    }

    struct stat st;
    if (stat(script_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        error_response.setStatus(404, "Not Found");
        error_response.setBody("CGI script not found");
        return error_response;
    }

    if (access(script_path.c_str(), R_OK) != 0) {
        error_response.setStatus(403, "Forbidden");
        error_response.setBody("CGI script is not readable");
        return error_response;
    }

    std::string extension = getFileExtension(script_path);
    std::string interpreter = findCgiInterpreter(extension, location);
    if (interpreter.empty()) {
        error_response.setStatus(500, "Internal Server Error");
        error_response.setBody("CGI interpreter not found for extension: " + extension);
        return error_response;
    }

    if (access(interpreter.c_str(), X_OK) != 0) {
        error_response.setStatus(500, "Internal Server Error");
        error_response.setBody("CGI interpreter is not executable: " + interpreter);
        return error_response;
    }

    std::map<std::string, std::string> env = buildCgiEnv(request, server, location, script_path);
    char** env_array = envToArray(env);

    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
        freeEnvArray(env_array);
        error_response.setStatus(500, "Internal Server Error");
        error_response.setBody("Failed to create pipes for CGI");
        return error_response;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        freeEnvArray(env_array);
        error_response.setStatus(500, "Internal Server Error");
        error_response.setBody("Failed to fork process for CGI");
        return error_response;
    }

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        if (dup2(stdin_pipe[0], STDIN_FILENO) < 0) {
            exit(1);
        }
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            exit(1);
        }
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        std::string cwd = server.root;
        if (chdir(cwd.c_str()) != 0) {
            exit(1);
        }

        std::string relative_script_path = script_path;
        if (script_path.find(server.root) == 0) {
            relative_script_path = script_path.substr(server.root.length());
            if (relative_script_path.empty() || relative_script_path[0] != '/') {
                relative_script_path = "/" + relative_script_path;
            }
            if (relative_script_path[0] == '/') {
                relative_script_path = relative_script_path.substr(1);
            }
        }

        char* argv[3];
        argv[0] = const_cast<char*>(interpreter.c_str());
        argv[1] = const_cast<char*>(relative_script_path.c_str());
        argv[2] = NULL;

        execve(interpreter.c_str(), argv, env_array);
        exit(1);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    std::string request_body = request.body;
    size_t written = 0;
    while (written < request_body.size()) {
        ssize_t n = write(stdin_pipe[1], request_body.c_str() + written, request_body.size() - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }
        written += n;
    }
    close(stdin_pipe[1]);

    std::string cgi_output;
    char buffer[4096];
    ssize_t n;

    time_t start_time = time(NULL);
    const time_t timeout = 30;

    while (true) {
        n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
        
        if (n > 0) {
            buffer[n] = '\0';
            cgi_output.append(buffer, n);
        } else if (n == 0) {
            break;
        } else {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }

        if (time(NULL) - start_time > timeout) {
            killProcess(pid);
            close(stdout_pipe[0]);
            freeEnvArray(env_array);

            int status;
            waitpid(pid, &status, 0);

            error_response.setStatus(504, "Gateway Timeout");
            error_response.setBody("CGI script execution timeout");
            return error_response;
        }
    }
    close(stdout_pipe[0]);

    int status;
    time_t wait_start = time(NULL);
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            if (time(NULL) - wait_start > 5) {
                killProcess(pid);
                freeEnvArray(env_array);
                error_response.setStatus(500, "Internal Server Error");
                error_response.setBody("Failed to wait for CGI process");
                return error_response;
            }
            continue;
        }
        killProcess(pid);
        freeEnvArray(env_array);
        error_response.setStatus(500, "Internal Server Error");
        error_response.setBody("Failed to wait for CGI process");
        return error_response;
    }

    freeEnvArray(env_array);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        error_response.setStatus(502, "Bad Gateway");
        error_response.setBody("CGI script exited with error");
        return error_response;
    }

    if (WIFSIGNALED(status)) {
        error_response.setStatus(502, "Bad Gateway");
        error_response.setBody("CGI script was terminated");
        return error_response;
    }

    if (cgi_output.empty()) {
        error_response.setStatus(502, "Bad Gateway");
        error_response.setBody("Empty response from CGI script");
        return error_response;
    }

    HttpResponse response = parseCgiResponse(cgi_output);
    return response;
}
