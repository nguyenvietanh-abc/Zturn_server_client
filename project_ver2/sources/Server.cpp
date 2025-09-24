#include "Server.h"

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
std::uniform_real_distribution<double> azimuth_dist(0.0, 360.0);
std::uniform_real_distribution<double> elevation_dist(0.0, 90.0);
std::uniform_real_distribution<double> temp_dist(20.0, 30.0);
std::uniform_real_distribution<double> humidity_dist(40.0, 80.0);

Server::Server(int port) : port(port), server_fd(-1) {}

Server::~Server() {
    if (server_fd >= 0) close(server_fd);
}

void Server::start() {
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Z-turn Server listening on port " << port << "..." << std::endl;

    while (true) {
        int* new_socket = (int*)malloc(sizeof(int));
        if ((*new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                free(new_socket);
                continue;
            }
            perror("Accept failed");
            free(new_socket);
            continue;
        }

        std::cout << "New client connected from "
                  << inet_ntoa(((struct sockaddr_in*)&address)->sin_addr) << std::endl;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleClient, (void*)new_socket) < 0) {
            perror("Thread creation failed");
            close(*new_socket);
            free(new_socket);
            continue;
        }
        pthread_detach(thread_id);
    }
}

void* Server::handleClient(void* arg) {
    int new_socket = *(int*)arg;
    free(arg);

    fcntl(new_socket, F_SETFL, O_NONBLOCK);

    char buffer[1024] = {0};
    while (true) {
        int valread = read(new_socket, buffer, 1024);
        if (valread <= 0) {
            if (valread < 0 && errno == EAGAIN) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            break;
        }

        std::string request(buffer, valread);

        if (request.find("GET_DATA") != std::string::npos) {
            double azimuth = azimuth_dist(rng);
            double elevation = elevation_dist(rng);
            double temperature = temp_dist(rng);
            double humidity = humidity_dist(rng);

            std::string data = "AZ:" + std::to_string(azimuth) + "\n" +
                               "EL:" + std::to_string(elevation) + "\n" +
                               "TE:" + std::to_string(temperature) + "\n" +
                               "HU:" + std::to_string(humidity) + "\n";

            send(new_socket, data.c_str(), data.length(), 0);
            std::cout << "Sent AZ: " << azimuth << std::endl;
            std::cout << "Sent EL: " << elevation << std::endl;
            std::cout << "Sent TE: " << temperature << std::endl;
            std::cout << "Sent HU: " << humidity << std::endl;
        }
    }
    close(new_socket);
    return NULL;
}
