#include "RequestHandler.hpp"
#include "Logger.hpp"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <vector>

RequestHandler::RequestHandler() {}

RequestHandler::~RequestHandler() {}

HttpResponse RequestHandler::handleRequest(const HttpRequest& request, const ConfigParser& config,
                                            const std::string& server_host, int server_port) {
    HttpResponse response;

    const ServerConfig* server = findServerConfig(request, config, server_host, server_port);
    if (!server) {
        response.setStatus(500, "Internal Server Error");
        response.setBody("No matching server configuration");
        response.setKeepAlive(false);
        return response;
    }

    const Location* location = findLocation(request, *server);

    if (location && !location->redirect.empty()) {
        return handleRedirect(request, location);
    }

    if (!isMethodAllowed(request.method, location)) {
        response.setStatus(405, "Method Not Allowed");
        response.setBody("Method not allowed: " + request.method);
        response.setKeepAlive(request.keep_alive);
        return response;
    }

    if (request.method == "GET") {
        response = handleGet(request, *server, location);
    } else if (request.method == "POST") {
        response = handlePost(request, *server, location);
    } else if (request.method == "DELETE") {
        response = handleDelete(request, *server, location);
    } else {
        response.setStatus(501, "Not Implemented");
        response.setBody("Method not implemented: " + request.method);
    }

    response.setKeepAlive(request.keep_alive);
    return response;
}

const ServerConfig* RequestHandler::findServerConfig(const HttpRequest& request, const ConfigParser& config,
                                                     const std::string& server_host, int server_port) {
    const std::vector<ServerConfig>& servers = config.getServers();
    if (servers.empty()) {
        return NULL;
    }

    std::string host_header = request.getHeader("Host");
    std::string request_host;
    int request_port = server_port;

    if (!host_header.empty()) {
        size_t colon_pos = host_header.find(':');
        if (colon_pos != std::string::npos) {
            request_host = host_header.substr(0, colon_pos);
            std::string port_str = host_header.substr(colon_pos + 1);
            std::istringstream iss(port_str);
            iss >> request_port;
        } else {
            request_host = host_header;
        }
    }

    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig& server = servers[i];
        for (size_t j = 0; j < server.listen.size(); ++j) {
            const std::string& listen_host = server.listen[j].first;
            int listen_port = server.listen[j].second;

            bool host_matches = (listen_host == "0.0.0.0" || listen_host == server_host || listen_host == request_host || request_host.empty());
            bool port_matches = (listen_port == server_port || listen_port == request_port);

            if (host_matches && port_matches) {
                return &server;
            }
        }
    }

    return &servers[0];
}

const Location* RequestHandler::findLocation(const HttpRequest& request, const ServerConfig& server) {
    const Location* best_match = NULL;
    size_t best_match_len = 0;

    for (size_t i = 0; i < server.locations.size(); ++i) {
        const Location& loc = server.locations[i];
        if (request.path.find(loc.path) == 0) {
            if (loc.path.length() > best_match_len) {
                best_match = &loc;
                best_match_len = loc.path.length();
            }
        }
    }

    return best_match;
}

std::string RequestHandler::buildFilePath(const HttpRequest& request, const ServerConfig& server,
                                          const Location* location) {
    std::string root = server.root;
    if (location && !location->root.empty()) {
        root = location->root;
    }

    std::string path = request.path;
    if (location) {
        if (path.find(location->path) == 0) {
            path = path.substr(location->path.length());
            if (path.empty() || path[0] != '/') {
                path = "/" + path;
            }
        }
    }

    std::string full_path = normalizePath(root + path);
    return full_path;
}

HttpResponse RequestHandler::serveFile(const std::string& file_path) {
    HttpResponse response;

    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        response.setStatus(404, "Not Found");
        response.setBody("File not found");
        return response;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (!file.read(buffer.data(), size)) {
        response.setStatus(500, "Internal Server Error");
        response.setBody("Error reading file");
        return response;
    }

    response.setStatus(200, "OK");
    response.setHeader("Content-Type", getContentType(file_path));
    response.setBody(buffer.data(), static_cast<std::size_t>(size));
    return response;
}

HttpResponse RequestHandler::serveDirectory(const std::string& dir_path, const std::string& request_path,
                                             const ServerConfig& server, const Location* location) {
    std::string index_file = server.index;
    if (location && !location->index.empty()) {
        index_file = location->index;
    }

    if (!index_file.empty()) {
        std::string index_path = normalizePath(dir_path + "/" + index_file);
        if (isFile(index_path)) {
            return serveFile(index_path);
        }
    }

    bool autoindex_enabled = false;
    if (location && location->autoindex) {
        autoindex_enabled = true;
    }

    if (autoindex_enabled) {
        return generateAutoindex(dir_path, request_path);
    }

    HttpResponse response;
    response.setStatus(403, "Forbidden");
    response.setBody("Directory listing disabled");
    return response;
}

HttpResponse RequestHandler::generateAutoindex(const std::string& dir_path, const std::string& request_path) {
    HttpResponse response;
    response.setStatus(200, "OK");
    response.setHeader("Content-Type", "text/html");

    std::ostringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html><head><title>Index of " << request_path << "</title></head>\n";
    html << "<body><h1>Index of " << request_path << "</h1><hr><pre>\n";

    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        response.setStatus(500, "Internal Server Error");
        response.setBody("Error reading directory");
        return response;
    }

    struct dirent* entry;
    std::vector<std::string> entries;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        entries.push_back(name);
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end());

    for (size_t i = 0; i < entries.size(); ++i) {
        std::string entry_path = normalizePath(dir_path + "/" + entries[i]);
        struct stat st;
        if (stat(entry_path.c_str(), &st) == 0) {
            std::string link_path = request_path;
            if (link_path.empty() || link_path[link_path.length() - 1] != '/') {
                link_path += "/";
            }
            link_path += entries[i];

            if (S_ISDIR(st.st_mode)) {
                link_path += "/";
                entries[i] += "/";
            }

            html << "<a href=\"" << link_path << "\">" << entries[i] << "</a>\n";
        }
    }

    html << "</pre><hr></body></html>\n";
    response.setBody(html.str());
    return response;
}

HttpResponse RequestHandler::generateErrorPage(int status_code, const ServerConfig& server) {
    HttpResponse response;
    response.setStatus(status_code, "");

    std::map<int, std::string>::const_iterator it = server.error_pages.find(status_code);
    if (it != server.error_pages.end()) {
        std::string error_page_path = normalizePath(server.root + "/" + it->second);
        if (isFile(error_page_path)) {
            std::ifstream file(error_page_path.c_str(), std::ios::binary);
            if (file.is_open()) {
                file.seekg(0, std::ios::end);
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<char> buffer(static_cast<std::size_t>(size));
                if (file.read(buffer.data(), size)) {
                    response.setHeader("Content-Type", getContentType(error_page_path));
                    response.setBody(buffer.data(), static_cast<std::size_t>(size));
                    return response;
                }
            }
        }
    }

    std::ostringstream body;
    body << status_code << " " << HttpResponse::getReasonPhrase(status_code);
    response.setBody(body.str());
    return response;
}

bool RequestHandler::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

bool RequestHandler::isFile(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISREG(st.st_mode);
    }
    return false;
}

std::string RequestHandler::normalizePath(const std::string& path) {
    std::string result = path;
    size_t pos;

    while ((pos = result.find("//")) != std::string::npos) {
        result.replace(pos, 2, "/");
    }

    while ((pos = result.find("/./")) != std::string::npos) {
        result.replace(pos, 3, "/");
    }

    if (result.length() > 1 && result[result.length() - 1] == '/') {
        result = result.substr(0, result.length() - 1);
    }

    if (result.empty()) {
        result = "/";
    }

    return result;
}

bool RequestHandler::isPathSafe(const std::string& path, const std::string& root) {
    std::string normalized_path = normalizePath(path);
    std::string normalized_root = normalizePath(root);

    if (normalized_path.find(normalized_root) != 0) {
        return false;
    }

    if (normalized_path.find("../") != std::string::npos) {
        return false;
    }

    return true;
}

std::string RequestHandler::getContentType(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = file_path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "html" || ext == "htm") {
        return "text/html";
    } else if (ext == "css") {
        return "text/css";
    } else if (ext == "js") {
        return "application/javascript";
    } else if (ext == "json") {
        return "application/json";
    } else if (ext == "png") {
        return "image/png";
    } else if (ext == "jpg" || ext == "jpeg") {
        return "image/jpeg";
    } else if (ext == "gif") {
        return "image/gif";
    } else if (ext == "svg") {
        return "image/svg+xml";
    } else if (ext == "txt") {
        return "text/plain";
    } else if (ext == "pdf") {
        return "application/pdf";
    } else if (ext == "xml") {
        return "application/xml";
    } else {
        return "application/octet-stream";
    }
}

bool RequestHandler::isMethodAllowed(const std::string& method, const Location* location) {
    if (!location || location->methods.empty()) {
        return true;
    }
    return location->methods.find(method) != location->methods.end();
}

HttpResponse RequestHandler::handleGet(const HttpRequest& request, const ServerConfig& server, const Location* location) {
    std::string file_path = buildFilePath(request, server, location);

    if (!isPathSafe(file_path, server.root)) {
        return generateErrorPage(403, server);
    }

    if (CgiHandler::shouldHandleByCgi(request, location, file_path)) {
        if (isFile(file_path)) {
            return CgiHandler::executeCgi(request, server, location, file_path);
        } else {
            return generateErrorPage(404, server);
        }
    }

    if (isDirectory(file_path)) {
        return serveDirectory(file_path, request.path, server, location);
    } else if (isFile(file_path)) {
        return serveFile(file_path);
    } else {
        return generateErrorPage(404, server);
    }
}

HttpResponse RequestHandler::handlePost(const HttpRequest& request, const ServerConfig& server, const Location* location) {
    HttpResponse response;

    if (request.body.size() > server.client_max_body_size) {
        response = generateErrorPage(413, server);
        return response;
    }

    std::string file_path = buildFilePath(request, server, location);

    if (!isPathSafe(file_path, server.root)) {
        response = generateErrorPage(403, server);
        return response;
    }

    if (CgiHandler::shouldHandleByCgi(request, location, file_path)) {
        if (isFile(file_path)) {
            return CgiHandler::executeCgi(request, server, location, file_path);
        } else {
            return generateErrorPage(404, server);
        }
    }

    std::string upload_dir;
    if (location && !location->upload_store.empty()) {
        upload_dir = normalizePath(location->upload_store);
    } else {
        upload_dir = normalizePath(server.root);
    }

    if (!isPathSafe(upload_dir, server.root)) {
        response = generateErrorPage(403, server);
        return response;
    }

    if (!isDirectory(upload_dir)) {
        if (mkdir(upload_dir.c_str(), 0755) != 0) {
            response.setStatus(500, "Internal Server Error");
            response.setBody("Failed to create upload directory");
            return response;
        }
    }

    std::string filename;
    std::string content_disposition = request.getHeader("Content-Disposition");
    if (!content_disposition.empty()) {
        size_t filename_pos = content_disposition.find("filename=");
        if (filename_pos != std::string::npos) {
            filename = content_disposition.substr(filename_pos + 9);
            size_t end_pos = filename.find_first_of(";\"");
            if (end_pos != std::string::npos) {
                filename = filename.substr(0, end_pos);
            }
            if (filename[0] == '"' && filename[filename.length() - 1] == '"') {
                filename = filename.substr(1, filename.length() - 2);
            }
        }
    }

    if (filename.empty()) {
        std::string path_part = request.path;
        size_t last_slash = path_part.find_last_of('/');
        if (last_slash != std::string::npos && last_slash < path_part.length() - 1) {
            filename = path_part.substr(last_slash + 1);
        }
        if (filename.empty()) {
            filename = "upload_file";
        }
    }

    size_t invalid_char = filename.find_first_of("/\\");
    while (invalid_char != std::string::npos) {
        filename[invalid_char] = '_';
        invalid_char = filename.find_first_of("/\\", invalid_char + 1);
    }

    std::string upload_path = normalizePath(upload_dir + "/" + filename);
    if (!isPathSafe(upload_path, server.root)) {
        response = generateErrorPage(403, server);
        return response;
    }

    std::ofstream file(upload_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        response.setStatus(500, "Internal Server Error");
        response.setBody("Failed to create upload file");
        return response;
    }

    file.write(request.body.c_str(), request.body.size());
    file.close();

    if (!file.good()) {
        response.setStatus(500, "Internal Server Error");
        response.setBody("Failed to write upload file");
        return response;
    }

    response.setStatus(201, "Created");
    response.setHeader("Location", request.path + "/" + filename);
    response.setBody("File uploaded successfully: " + filename);
    return response;
}

HttpResponse RequestHandler::handleDelete(const HttpRequest& request, const ServerConfig& server, const Location* location) {
    HttpResponse response;

    std::string file_path = buildFilePath(request, server, location);

    if (!isPathSafe(file_path, server.root)) {
        response = generateErrorPage(403, server);
        return response;
    }

    if (isDirectory(file_path)) {
        response = generateErrorPage(403, server);
        return response;
    }

    if (!isFile(file_path)) {
        response = generateErrorPage(404, server);
        return response;
    }

    if (unlink(file_path.c_str()) != 0) {
        if (errno == EACCES || errno == EPERM) {
            response = generateErrorPage(403, server);
        } else {
            response.setStatus(500, "Internal Server Error");
            response.setBody("Failed to delete file");
        }
        return response;
    }

    response.setStatus(204, "No Content");
    response.setHeader("Content-Length", "0");
    response.setBody("");
    return response;
}

HttpResponse RequestHandler::handleRedirect(const HttpRequest& request, const Location* location) {
    HttpResponse response;
    
    if (!location || location->redirect.empty()) {
        response.setStatus(500, "Internal Server Error");
        response.setBody("Redirect configuration error");
        return response;
    }

    std::string redirect_url = location->redirect;
    int redirect_code = location->redirect_code;
    
    if (redirect_url.find("http://") == 0 || redirect_url.find("https://") == 0) {
        response.setHeader("Location", redirect_url);
    } else {
        if (redirect_url[0] != '/') {
            redirect_url = "/" + redirect_url;
        }
        response.setHeader("Location", redirect_url);
    }
    
    if (redirect_code == 301) {
        response.setStatus(301, "Moved Permanently");
    } else {
        response.setStatus(302, "Found");
    }
    
    response.setHeader("Content-Length", "0");
    response.setBody("");
    response.setKeepAlive(request.keep_alive);
    return response;
}
