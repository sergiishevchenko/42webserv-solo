#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>

struct Location {
    std::string path;
    std::set<std::string> methods;
    std::string root;
    std::string index;
    bool autoindex;
    std::string redirect;
    std::string upload_store;
    std::map<std::string, std::string> cgi_pass;

    Location() : autoindex(false) {}
};

struct ServerConfig {
    std::vector<std::pair<std::string, int> > listen;
    std::string root;
    std::string index;
    size_t client_max_body_size;
    std::map<int, std::string> error_pages;
    std::vector<Location> locations;

    ServerConfig() : client_max_body_size(1048576) {}
};

class ConfigParser {
   public:
    ConfigParser();
    ~ConfigParser();

    bool loadFromFile(const std::string& filepath);
    std::string getLastError() const { return lastError_; }

    const std::vector<ServerConfig>& getServers() const { return servers_; }

    bool validate() const;

   private:
    std::vector<ServerConfig> servers_;
    mutable std::string lastError_;

    bool parseServerBlock(std::istream& in, ServerConfig& server);
    bool parseLocationBlock(std::istream& in, Location& location);
    bool parseListen(const std::string& line, ServerConfig& server);
    bool parseDirective(const std::string& line, ServerConfig& server);
    bool parseLocationDirective(const std::string& line, Location& location);

    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delim);
    bool isNumber(const std::string& str);
};

#endif
