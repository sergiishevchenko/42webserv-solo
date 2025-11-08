#include <string>
#include <sstream>
#include "Config.hpp"
#include "Logger.hpp"

static void printUsage(const char* progName) {
    LOG_ERROR() << "Usage: " << progName << " <configuration file>"
                << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string configPath = argv[1];
    Config config;

    if (!config.loadFromFile(configPath)) {
        LOG_ERROR() << config.getLastError() << std::endl;
        return 1;
    }

    if (!config.validate()) {
        LOG_ERROR() << config.getLastError() << std::endl;
        return 1;
    }

    LOG_INFO() << "webserv: Configuration loaded successfully" << std::endl;
    const std::vector<ServerConfig>& servers = config.getServers();
    LOG_INFO() << "Found " << servers.size() << " server block(s)"
               << std::endl;

    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig& s = servers[i];
        std::ostringstream oss;

        oss << "\n--- Server block " << (i + 1) << " ---\n";
        oss << "Listen: ";
        for (size_t j = 0; j < s.listen.size(); ++j) {
            if (j > 0)
                oss << ", ";
            oss << s.listen[j].first << ":" << s.listen[j].second;
        }
        oss << "\n";
        oss << "Root: " << s.root << "\n";
        oss << "Index: " << s.index << "\n";
        oss << "Client max body size: " << s.client_max_body_size
            << " bytes\n";
        if (!s.error_pages.empty()) {
            oss << "Error pages: ";
            for (std::map<int, std::string>::const_iterator it =
                     s.error_pages.begin();
                 it != s.error_pages.end(); ++it) {
                if (it != s.error_pages.begin())
                    oss << ", ";
                oss << it->first << " -> " << it->second;
            }
            oss << "\n";
        }
        oss << "Locations: " << s.locations.size() << "\n";
        for (size_t j = 0; j < s.locations.size(); ++j) {
            const Location& loc = s.locations[j];
            oss << "  [" << loc.path << "] ";
            if (!loc.methods.empty()) {
                oss << "methods: ";
                for (std::set<std::string>::const_iterator it =
                         loc.methods.begin();
                     it != loc.methods.end(); ++it) {
                    if (it != loc.methods.begin())
                        oss << " ";
                    oss << *it;
                }
                oss << " | ";
            }
            if (!loc.upload_store.empty())
                oss << "upload_store: " << loc.upload_store << " | ";
            if (!loc.redirect.empty())
                oss << "redirect: " << loc.redirect << " | ";
            if (loc.autoindex)
                oss << "autoindex: on | ";
            if (!loc.cgi_pass.empty()) {
                oss << "cgi_pass: ";
                for (std::map<std::string, std::string>::const_iterator it =
                         loc.cgi_pass.begin();
                     it != loc.cgi_pass.end(); ++it) {
                    if (it != loc.cgi_pass.begin())
                        oss << ", ";
                    oss << it->first << " -> " << it->second;
                }
            }
            oss << "\n";
        }
        LOG_INFO() << oss.str();
    }

    return 0;
}
