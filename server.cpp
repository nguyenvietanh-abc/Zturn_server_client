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

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
std::uniform_real_distribution<double> azimuth_dist(0.0, 360.0);
std::uniform_real_distribution<double> elevation_dist(0.0, 90.0);
std::uniform_real_distribution<double> temp_dist(20.0, 30.0);
std::uniform_real_distribution<double> humidity_dist(40.0, 80.0);


void* handle_client(void* arg) {
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

        //Xử lý request
        if (request.find("GET_DATA") != std::string::npos) { //sinh giá trị ngẫu nhiên
            double azimuth = azimuth_dist(rng);
            double elevation = elevation_dist(rng);
            double temperature = temp_dist(rng);
            double humidity = humidity_dist(rng);

            // Gom 4 giá trị vào một chuỗi
            std::string data = "AZ:" + std::to_string(azimuth) + "\n" +
                               "EL:" + std::to_string(elevation) + "\n" +
                               "TE:" + std::to_string(temperature) + "\n" +
                               "HU:" + std::to_string(humidity) + "\n";

            //Gom thành chuỗi nhiều dòng
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


int main() {
    int server_fd, *new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { //Tạo socket tcp
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    fcntl(server_fd, F_SETFL, O_NONBLOCK); //set non-blocking

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) { //Set việc tránh reuse old address
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    //bind địa chỉ
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  //nhạn kết nối từ mọi ip
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    //nghe
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Z-turn Server listening on port 8080..." << std::endl;

    //Vòng lặp chấn nhận client
    while (true) {
        new_socket = (int*)malloc(sizeof(int));
        if ((*(new_socket) = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));    //accpt chưa có ai chấp nhận, sleep 1 chút rồ lặp lại
                free(new_socket);
                continue;
            }
            perror("Accept failed");
            free(new_socket);
            continue;
        }

        //Phát hiện new client
        std::cout << "New client connected from " << inet_ntoa(((struct sockaddr_in*)&address)->sin_addr) << std::endl;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_socket) < 0) {
            perror("Thread creation failed");
            close(*new_socket);
            free(new_socket);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}