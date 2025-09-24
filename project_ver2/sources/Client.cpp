#include "Client.h"

Client::Client(const std::string& ip, int port, LogLevel level)
    : server_ip(ip), server_port(port), log_level(level), sock(-1), running(false) {}

Client::~Client() {
    stop();
}

bool Client::connectToServer() {
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return false;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("Connection failed");
            close(sock);
            return false;
        }

        int epoll_fd = epoll_create1(0);
        struct epoll_event ev;
        ev.events = EPOLLOUT;
        ev.data.fd = sock;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);

        struct epoll_event events[1];
        if (epoll_wait(epoll_fd, events, 1, 5000) <= 0) {
            perror("Connection timeout or error");
            close(epoll_fd);
            close(sock);
            return false;
        }

        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            std::cerr << "Connection failed: " << strerror(so_error) << std::endl;
            close(epoll_fd);
            close(sock);
            return false;
        }
        close(epoll_fd);
    }

    std::cout << "Connected to Z-turn Server at " << server_ip << ":" << server_port << std::endl;
    return true;
}

void Client::start() {
    running = true;
    if (pthread_create(&data_thread, NULL, processDataThread, this) < 0) {
        perror("Thread creation failed");
        close(sock);
        return;
    }

    const auto frequency = 600;
    const auto period = std::chrono::microseconds(static_cast<long>(1000000.0 / frequency));
    auto next_time = std::chrono::high_resolution_clock::now();
    std::string request = "GET_DATA\n";

    while (running) {
        if (send(sock, request.c_str(), request.length(), 0) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            perror("Send failed");
            break;
        }

        next_time += period;
        auto now = std::chrono::high_resolution_clock::now();
        if (now < next_time) {
            std::this_thread::sleep_until(next_time);
        } else {
            std::cout << "Warning: Cannot keep up with 300 Hz frequency" << std::endl;
        }
    }
}

void Client::stop() {
    running = false;
    if (sock >= 0) {
        pthread_cancel(data_thread);
        close(sock);
        sock = -1;
    }
}

void* Client::processDataThread(void* arg) {
    Client* client = static_cast<Client*>(arg);
    client->processData();
    return NULL;
}

void Client::processData() {
    std::string accumulated_data;
    char buffer[2048] = {0};
    DataLists data;

    while (running) {
        int epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            perror("epoll_create1 failed");
            break;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sock;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) < 0) {
            perror("epoll_ctl failed");
            close(epoll_fd);
            break;
        }

        struct epoll_event events[1];
        int nfds = epoll_wait(epoll_fd, events, 1, 50);
        if (nfds <= 0) {
            close(epoll_fd);
            continue;
        }

        int valread = read(sock, buffer, 2048);
        if (valread > 0) {
            buffer[valread] = '\0';
            accumulated_data += std::string(buffer);

            size_t pos = 0;
            while ((pos = accumulated_data.find('\n')) != std::string::npos) {
                std::string line = accumulated_data.substr(0, pos);
                accumulated_data.erase(0, pos + 1);

                double value;
                if (line.find("AZ:") == 0) {
                    value = std::stod(line.substr(3));
                    data.azimuth_list.push_back(value);
                    data.azimuth_sum += value;
                    if (log_level == DEBUG) std::cout << "Received AZ: " << value << std::endl;
                    if (data.azimuth_list.size() > data.SAMPLE_SIZE) {
                        data.azimuth_sum -= data.azimuth_list.front();
                        data.azimuth_list.pop_front();
                    }
                    if (data.azimuth_list.size() == data.SAMPLE_SIZE && log_level == INFO) {
                        std::cout << "Azimuth average (50 samples): "
                                  << data.azimuth_sum / data.SAMPLE_SIZE << std::endl;
                    }
                }
                else if (line.find("EL:") == 0) {
                    value = std::stod(line.substr(3));
                    data.elevation_list.push_back(value);
                    data.elevation_sum += value;
                    if (log_level == DEBUG) std::cout << "Received EL: " << value << std::endl;
                    if (data.elevation_list.size() > data.SAMPLE_SIZE) {
                        data.elevation_sum -= data.elevation_list.front();
                        data.elevation_list.pop_front();
                    }
                    if (data.elevation_list.size() == data.SAMPLE_SIZE && log_level == INFO) {
                        std::cout << "Elevation average (50 samples): "
                                  << data.elevation_sum / data.SAMPLE_SIZE << std::endl;
                    }
                }
                else if (line.find("TE:") == 0) {
                    value = std::stod(line.substr(3));
                    data.temperature_list.push_back(value);
                    data.temperature_sum += value;
                    if (log_level == DEBUG) std::cout << "Received TE: " << value << std::endl;
                    if (data.temperature_list.size() > data.SAMPLE_SIZE) {
                        data.temperature_sum -= data.temperature_list.front();
                        data.temperature_list.pop_front();
                    }
                    if (data.temperature_list.size() == data.SAMPLE_SIZE && log_level == INFO) {
                        std::cout << "Temperature average (50 samples): "
                                  << data.temperature_sum / data.SAMPLE_SIZE << std::endl;
                    }
                }
                else if (line.find("HU:") == 0) {
                    value = std::stod(line.substr(3));
                    data.humidity_list.push_back(value);
                    data.humidity_sum += value;
                    if (log_level == DEBUG) std::cout << "Received HU: " << value << std::endl;
                    if (data.humidity_list.size() > data.SAMPLE_SIZE) {
                        data.humidity_sum -= data.humidity_list.front();
                        data.humidity_list.pop_front();
                    }
                    if (data.humidity_list.size() == data.SAMPLE_SIZE && log_level == INFO) {
                        std::cout << "Humidity average (50 samples): "
                                  << data.humidity_sum / data.SAMPLE_SIZE << std::endl;
                    }
                }
            }
        }
        close(epoll_fd);
    }
}
