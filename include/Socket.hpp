#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>

class Socket {
   public:
    Socket();
    ~Socket();

    bool bind(const std::string& host, int port);
    bool listen(int backlog = 128);
    int accept();
    void close();
    bool setNonBlocking();
    bool setCloseOnExec();

    int getFd() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }

   private:
    int fd_;
    std::string host_;
    int port_;

    Socket(const Socket&);
    Socket& operator=(const Socket&);
};

#endif
