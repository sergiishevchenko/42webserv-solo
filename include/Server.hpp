#ifndef SERVER_HPP
#define SERVER_HPP

#include "ConfigParser.hpp"
#include "Socket.hpp"
#include "Connection.hpp"
#include <vector>
#include <map>
#include <poll.h>

class Server {
   public:
    Server();
    ~Server();

    bool init(const ConfigParser& config);
    void run();
    void stop();

   private:
    std::vector<Socket*> listening_sockets_;
    std::map<int, Connection*> connections_;
    std::vector<struct pollfd> poll_fds_;
    bool running_;
    time_t connection_timeout_;

    void setupPollFds();
    void handlePollEvents();
    void acceptNewConnection(Socket* socket);
    void handleClientRead(int fd);
    void handleClientWrite(int fd);
    void closeConnection(int fd);
    void cleanupTimedOutConnections();
    void addPollFd(int fd, short events);
    void removePollFd(int fd);
    void updatePollFd(int fd, short events);

    Server(const Server&);
    Server& operator=(const Server&);
};

#endif
