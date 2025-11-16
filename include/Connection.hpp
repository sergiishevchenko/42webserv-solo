#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <ctime>

#include "RequestParser.hpp"

class Connection {
   public:
        Connection(int fd, const std::string& client_ip);
        ~Connection();

        int getFd() const { return fd_; }
        std::string getClientIp() const { return client_ip_; }
        time_t getLastActivity() const { return last_activity_; }

        void updateActivity() { last_activity_ = time(NULL); }

        void close();
        bool isValid() const { return fd_ >= 0; }

        RequestParser& getRequestParser() { return parser_; }
        void resetRequestParser() { parser_.reset(); }

   private:
        int fd_;
        std::string client_ip_;
        time_t last_activity_;
        RequestParser parser_;

        Connection(const Connection&);
        Connection& operator=(const Connection&);
};

#endif
