#include <iostream>
#include <string>
#include "Config.hpp"

static void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " <configuration file>" << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string configPath = argv[1];
    Config config;

    if (!config.loadFromFile(configPath)) {
        std::cerr << "Error: " << config.getLastError() << std::endl;
        return 1;
    }

    if (!config.validate()) {
        std::cerr << "Error: " << config.getLastError() << std::endl;
        return 1;
    }

    std::cout << "webserv: Configuration loaded successfully" << std::endl;
    const std::vector<ServerConfig>& servers = config.getServers();
    std::cout << "Found " << servers.size() << " server block(s)" << std::endl;

    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig& s = servers[i];
        std::cout << "\n--- Server block " << (i + 1) << " ---" << std::endl;
        std::cout << "Listen: ";
        for (size_t j = 0; j < s.listen.size(); ++j) {
            if (j > 0)
                std::cout << ", ";
            std::cout << s.listen[j].first << ":" << s.listen[j].second;
        }
        std::cout << std::endl;
        std::cout << "Root: " << s.root << std::endl;
        std::cout << "Index: " << s.index << std::endl;
        std::cout << "Client max body size: " << s.client_max_body_size
                  << " bytes" << std::endl;
        if (!s.error_pages.empty()) {
            std::cout << "Error pages: ";
            for (std::map<int, std::string>::const_iterator it =
                     s.error_pages.begin();
                 it != s.error_pages.end(); ++it) {
                if (it != s.error_pages.begin())
                    std::cout << ", ";
                std::cout << it->first << " -> " << it->second;
            }
            std::cout << std::endl;
        }
        std::cout << "Locations: " << s.locations.size() << std::endl;
        for (size_t j = 0; j < s.locations.size(); ++j) {
            const Location& loc = s.locations[j];
            std::cout << "  [" << loc.path << "] ";
            if (!loc.methods.empty()) {
                std::cout << "methods: ";
                for (std::set<std::string>::const_iterator it =
                         loc.methods.begin();
                     it != loc.methods.end(); ++it) {
                    if (it != loc.methods.begin())
                        std::cout << " ";
                    std::cout << *it;
                }
                std::cout << " | ";
            }
            if (!loc.upload_store.empty())
                std::cout << "upload_store: " << loc.upload_store << " | ";
            if (!loc.redirect.empty())
                std::cout << "redirect: " << loc.redirect << " | ";
            if (loc.autoindex)
                std::cout << "autoindex: on | ";
            if (!loc.cgi_pass.empty()) {
                std::cout << "cgi_pass: ";
                for (std::map<std::string, std::string>::const_iterator it =
                         loc.cgi_pass.begin();
                     it != loc.cgi_pass.end(); ++it) {
                    if (it != loc.cgi_pass.begin())
                        std::cout << ", ";
                    std::cout << it->first << " -> " << it->second;
                }
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
