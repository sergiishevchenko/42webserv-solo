#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <ctime>

#include "RequestParser.hpp"

class Connection {
   public:
        Connection(int fd, const std::string& client_ip, int client_port,
                   const std::string& server_host, int server_port);
        ~Connection();

        int getFd() const { return fd_; }
        std::string getClientIp() const { return client_ip_; }
        int getClientPort() const { return client_port_; }
        std::string getServerHost() const { return server_host_; }
        int getServerPort() const { return server_port_; }
        time_t getLastActivity() const { return last_activity_; }
        time_t getRequestStartTime() const { return request_start_time_; }

        void updateActivity() { last_activity_ = time(NULL); }
        void startRequest() { request_start_time_ = time(NULL); }
        void resetRequest() { request_start_time_ = 0; }

        void close();
        bool isValid() const { return fd_ >= 0; }

        RequestParser& getRequestParser() { return parser_; }
        void resetRequestParser() { parser_.reset(); }

   private:
        int fd_;
        std::string client_ip_;
        int client_port_;
        std::string server_host_;
        int server_port_;
        time_t last_activity_;
        time_t request_start_time_;
        RequestParser parser_;

        Connection(const Connection&);
        Connection& operator=(const Connection&);
};

#endif
