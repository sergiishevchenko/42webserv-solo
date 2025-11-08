#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

ConfigParser::ConfigParser() : lastError_("") {}

ConfigParser::~ConfigParser() {}

std::string ConfigParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> ConfigParser::split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        token = trim(token);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

bool ConfigParser::isNumber(const std::string& str) {
    if (str.empty())
        return false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (!std::isdigit(str[i]))
            return false;
    }
    return true;
}

bool ConfigParser::parseListen(const std::string& line, ServerConfig& server) {
    std::string trimmed = trim(line);
    if (trimmed.find("listen") != 0) {
        lastError_ = "Parse error: Invalid 'listen' directive: " + line;
        return false;
    }

    trimmed = trimmed.substr(6);
    trimmed = trim(trimmed);
    if (!trimmed.empty() && trimmed[trimmed.length() - 1] == ';') {
        trimmed = trimmed.substr(0, trimmed.length() - 1);
        trimmed = trim(trimmed);
    }

    size_t colon = trimmed.find(':');
    if (colon == std::string::npos) {
        if (isNumber(trimmed)) {
            int port = std::atoi(trimmed.c_str());
            if (port > 0 && port < 65536) {
                server.listen.push_back(std::make_pair("0.0.0.0", port));
                return true;
            }
        }
        lastError_ = "Parse error: Invalid port in 'listen' directive: " + line;
        return false;
    }

    std::string interface = trimmed.substr(0, colon);
    std::string portStr = trimmed.substr(colon + 1);
    interface = trim(interface);
    portStr = trim(portStr);

    if (isNumber(portStr)) {
        int port = std::atoi(portStr.c_str());
        if (port > 0 && port < 65536) {
            server.listen.push_back(std::make_pair(interface, port));
            return true;
        }
    }
    lastError_ = "Parse error: Invalid port in 'listen' directive: " + line;
    return false;
}

bool ConfigParser::parseDirective(const std::string& line, ServerConfig& server) {
    std::string trimmed = trim(line);

    size_t commentPos = trimmed.find('#');
    if (commentPos != std::string::npos) {
        trimmed = trimmed.substr(0, commentPos);
        trimmed = trim(trimmed);
    }
    if (trimmed.empty())
        return true;

    if (!trimmed.empty() && trimmed[trimmed.length() - 1] == ';') {
        trimmed = trimmed.substr(0, trimmed.length() - 1);
        trimmed = trim(trimmed);
    }

    std::vector<std::string> tokens = split(trimmed, ' ');
    if (tokens.empty())
        return true;

    std::string directive = tokens[0];

    if (directive == "root" && tokens.size() >= 2) {
        server.root = tokens[1];
        return true;
    } else if (directive == "index" && tokens.size() >= 2) {
        server.index = tokens[1];
        return true;
    } else if (directive == "client_max_body_size" && tokens.size() >= 2) {
        if (isNumber(tokens[1])) {
            size_t size = std::atoi(tokens[1].c_str());
            if (size > 0) {
                server.client_max_body_size = size;
                return true;
            }
        }
        lastError_ =
            "Parse error: Invalid 'client_max_body_size' value: " + line;
        return false;
    } else if (directive == "error_page" && tokens.size() >= 3) {
        if (isNumber(tokens[1])) {
            int code = std::atoi(tokens[1].c_str());
            if (code >= 400 && code < 600) {
                server.error_pages[code] = tokens[2];
                return true;
            }
        }
        lastError_ = "Parse error: Invalid 'error_page' directive: " + line;
        return false;
    }

    return false;
}

bool ConfigParser::parseLocationDirective(const std::string& line,
                                    Location& location) {
    std::string trimmed = trim(line);

    size_t commentPos = trimmed.find('#');
    if (commentPos != std::string::npos) {
        trimmed = trimmed.substr(0, commentPos);
        trimmed = trim(trimmed);
    }
    if (trimmed.empty())
        return true;

    if (!trimmed.empty() && trimmed[trimmed.length() - 1] == ';') {
        trimmed = trimmed.substr(0, trimmed.length() - 1);
        trimmed = trim(trimmed);
    }

    std::vector<std::string> tokens = split(trimmed, ' ');
    if (tokens.empty())
        return true;

    std::string directive = tokens[0];

    if (directive == "methods" && tokens.size() >= 2) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            location.methods.insert(tokens[i]);
        }
        return true;
    } else if (directive == "root" && tokens.size() >= 2) {
        location.root = tokens[1];
        return true;
    } else if (directive == "index" && tokens.size() >= 2) {
        location.index = tokens[1];
        return true;
    } else if (directive == "autoindex" && tokens.size() >= 2) {
        location.autoindex = (tokens[1] == "on");
        return true;
    } else if (directive == "return" && tokens.size() >= 2) {
        location.redirect = tokens[1];
        return true;
    } else if (directive == "upload_store" && tokens.size() >= 2) {
        location.upload_store = tokens[1];
        return true;
    } else if (directive == "cgi_pass" && tokens.size() >= 3) {
        location.cgi_pass[tokens[1]] = tokens[2];
        return true;
    }

    return false;
}

bool ConfigParser::parseLocationBlock(std::istream& in, Location& location) {
    std::string line;
    int braceCount = 1;

    while (std::getline(in, line) && braceCount > 0) {
        std::string trimmed = trim(line);

        if (trimmed == "}") {
            braceCount--;
            if (braceCount == 0)
                break;
        } else if (trimmed.find('{') != std::string::npos) {
            braceCount++;
        } else {
            parseLocationDirective(line, location);
        }
    }

    return braceCount == 0;
}

bool ConfigParser::parseServerBlock(std::istream& in, ServerConfig& server) {
    std::string line;
    int braceCount = 1;

    while (std::getline(in, line) && braceCount > 0) {
        std::string trimmed = trim(line);

        if (trimmed == "}") {
            braceCount--;
            if (braceCount == 0)
                break;
        } else if (trimmed.find("location") == 0) {
            std::vector<std::string> tokens = split(trimmed, ' ');
            if (tokens.size() >= 2) {
                Location location;
                std::string pathToken = tokens[1];
                if (pathToken.find('{') != std::string::npos) {
                    location.path = pathToken.substr(0, pathToken.find('{'));
                    location.path = trim(location.path);
                } else if (tokens.size() >= 3 && tokens[2] == "{") {
                    location.path = pathToken;
                } else {
                    location.path = pathToken;
                    std::string nextLine;
                    if (std::getline(in, nextLine)) {
                        std::string nextTrimmed = trim(nextLine);
                        if (nextTrimmed != "{") {
                            lastError_ =
                                "Parse error: Expected '{' after location path";
                            return false;
                        }
                    }
                }
                if (!parseLocationBlock(in, location)) {
                    return false;
                }
                server.locations.push_back(location);
            }
        } else if (trimmed.find("listen") == 0) {
            if (!parseListen(trimmed, server)) {
                return false;
            }
        } else {
            parseDirective(trimmed, server);
        }
    }

    return braceCount == 0;
}

bool ConfigParser::loadFromFile(const std::string& filepath) {
    lastError_.clear();
    std::ifstream file(filepath.c_str());
    if (!file.is_open()) {
        lastError_ = "Failed to open configuration file: " + filepath;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);

        if (trimmed.find("server") == 0) {
            ServerConfig server;
            if (!parseServerBlock(file, server)) {
                if (lastError_.empty()) {
                    lastError_ = "Parse error: Failed to parse server block";
                }
                return false;
            }
            servers_.push_back(server);
        }
    }

    file.close();
    return true;
}

bool ConfigParser::validate() const {
    lastError_.clear();

    if (servers_.empty()) {
        lastError_ = "Configuration error: No server blocks found";
        return false;
    }

    for (size_t i = 0; i < servers_.size(); ++i) {
        const ServerConfig& server = servers_[i];
        std::stringstream ss;
        ss << "Server block " << (i + 1);

        if (server.listen.empty()) {
            lastError_ = ss.str() + ": Missing 'listen' directive";
            return false;
        }

        for (size_t j = 0; j < server.listen.size(); ++j) {
            int port = server.listen[j].second;
            if (port <= 0 || port >= 65536) {
                ss.str("");
                ss << "Server block " << (i + 1)
                   << ": Invalid port number: " << port;
                lastError_ = ss.str();
                return false;
            }
        }

        if (server.root.empty()) {
            lastError_ = ss.str() + ": Missing 'root' directive";
            return false;
        }

        if (server.client_max_body_size == 0) {
            lastError_ =
                ss.str() + ": 'client_max_body_size' must be greater than 0";
            return false;
        }

        for (size_t j = 0; j < server.locations.size(); ++j) {
            const Location& loc = server.locations[j];
            ss.str("");
            ss << "Server block " << (i + 1) << ", location '" << loc.path
               << "'";

            if (loc.redirect.empty() && loc.upload_store.empty() &&
                loc.cgi_pass.empty() && loc.root.empty()) {
                if (loc.methods.empty()) {
                    lastError_ =
                        ss.str() +
                        ": No methods specified and no special directives "
                        "(upload_store, cgi_pass, redirect)";
                    return false;
                }
            }

            if (!loc.upload_store.empty() &&
                loc.methods.find("POST") == loc.methods.end()) {
                lastError_ = ss.str() + ": 'upload_store' requires POST method";
                return false;
            }
        }
    }

    return true;
}
