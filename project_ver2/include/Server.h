#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>

class Server {
public:
    Server(int port);
    ~Server();

    void start();

private:
    static void* handleClient(void* arg);

    int server_fd;
    int port;
};

#endif
