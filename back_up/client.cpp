#include <iostream>
#include <list>
#include <string>           
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <iomanip>
#include <sys/epoll.h>
#include <fcntl.h>          // set non-blocking
#include <errno.h>
#include <cstring>
#include <pthread.h>

//  LOG LEVEL 
// OFF  = tắt hết log
// INFO = chỉ log giá trị trung bình (50 mẫu)
// DEBUG = log tất cả dữ liệu nhận được
enum LogLevel { OFF, INFO, DEBUG };
LogLevel CURRENT_LOG_LEVEL = INFO;  
// 


// dsách Lkết để lưu dữ liệu nhận được từ server
// Client parse số đó và push vào list, đồng thời duy trì tổng để tính trung bình
struct DataLists {
    std::list<double> azimuth_list;
    std::list<double> elevation_list;
    std::list<double> temperature_list;
    std::list<double> humidity_list;

    double azimuth_sum = 0.0;
    double elevation_sum = 0.0;
    double temperature_sum = 0.0;
    double humidity_sum = 0.0;

    const int SAMPLE_SIZE = 50;  // số mẫu để tính trung bình
};


// Thread xử lý dữ liệu nhận được từ server
void* process_data(void* arg) {
    int sock = *(int*)arg;  // Nhận sock trực tiếp từ arg
    std::string accumulated_data;   // buffer tích lũy dữ liệu khi chưa đủ 1 dòng
    char buffer[2048] = {0};
    DataLists data;

    while (true) {
        int epoll_fd = epoll_create1(0); // check data từ socket bằng epoll (non-blocking I/O)
        if (epoll_fd < 0) {
            perror("epoll_create1 failed");
            break;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN;  // chỉ quan tâm sự kiện có data để đọc
        ev.data.fd = sock;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) < 0) {
            perror("epoll_ctl failed");
            close(epoll_fd);
            break;
        }

        struct epoll_event events[1];
        int nfds = epoll_wait(epoll_fd, events, 1, 50);  // Timeout 50ms
        if (nfds < 0) {
            perror("epoll_wait failed");
            close(epoll_fd);
            break;
        } else if (nfds == 0) {
            // Không có dliệu từ server trong 50ms
            if (CURRENT_LOG_LEVEL == DEBUG)
                std::cout << "Timeout waiting for data" << std::endl;
            close(epoll_fd);
            continue;
        }

        int valread = read(sock, buffer, 2048);  // đọc dữ liệu từ socket
        if (valread > 0) {
            buffer[valread] = '\0';
            accumulated_data += std::string(buffer); // ghép dữ liệu nếu buffer chưa đầy

            size_t pos = 0;
            // Cắt chuỗi thành từng dòng (dữ liệu phân tách bằng '\n')
            while ((pos = accumulated_data.find('\n')) != std::string::npos) {
                std::string line = accumulated_data.substr(0, pos);  // cắt 1 line hoàn chỉnh
                accumulated_data.erase(0, pos + 1); // xóa dòng vừa lấy khỏi buffer, giữ lại phần còn lại

                double value;

                // Kiểm tra Dliệu thuộc loại nào
                if (line.find("AZ:") == 0) {
                    value = std::stod(line.substr(3));  // convert sang double
                    data.azimuth_list.push_back(value);
                    data.azimuth_sum += value;

                    if (CURRENT_LOG_LEVEL == DEBUG)
                        std::cout << "Received AZ: " << value << std::endl;

                    if (data.azimuth_list.size() > data.SAMPLE_SIZE) {
                        data.azimuth_sum -= data.azimuth_list.front();
                        data.azimuth_list.pop_front();
                    }
                    if (data.azimuth_list.size() == data.SAMPLE_SIZE && CURRENT_LOG_LEVEL == INFO) {
                        std::cout << "Azimuth average (50 samples): "
                                  << data.azimuth_sum / data.SAMPLE_SIZE << std::endl;
                    }
                } else if (line.find("EL:") == 0) {
                    value = std::stod(line.substr(3));
                    data.elevation_list.push_back(value);
                    data.elevation_sum += value;

                    if (CURRENT_LOG_LEVEL == DEBUG)
                        std::cout << "Received EL: " << value << std::endl;

                    if (data.elevation_list.size() > data.SAMPLE_SIZE) {
                        data.elevation_sum -= data.elevation_list.front();
                        data.elevation_list.pop_front();
                    }
                    if (data.elevation_list.size() == data.SAMPLE_SIZE && CURRENT_LOG_LEVEL == INFO) {
                        std::cout << "Elevation average (50 samples): "
                                  << data.elevation_sum / data.SAMPLE_SIZE << std::endl;
                    }
                } else if (line.find("TE:") == 0) {
                    value = std::stod(line.substr(3));
                    data.temperature_list.push_back(value);
                    data.temperature_sum += value;

                    if (CURRENT_LOG_LEVEL == DEBUG)
                        std::cout << "Received TE: " << value << std::endl;

                    if (data.temperature_list.size() > data.SAMPLE_SIZE) {
                        data.temperature_sum -= data.temperature_list.front();
                        data.temperature_list.pop_front();
                    }
                    if (data.temperature_list.size() == data.SAMPLE_SIZE && CURRENT_LOG_LEVEL == INFO) {
                        std::cout << "Temperature average (50 samples): "
                                  << data.temperature_sum / data.SAMPLE_SIZE << std::endl;
                    }
                } else if (line.find("HU:") == 0) {
                    value = std::stod(line.substr(3));
                    data.humidity_list.push_back(value);
                    data.humidity_sum += value;

                    if (CURRENT_LOG_LEVEL == DEBUG)
                        std::cout << "Received HU: " << value << std::endl;

                    if (data.humidity_list.size() > data.SAMPLE_SIZE) {
                        data.humidity_sum -= data.humidity_list.front();
                        data.humidity_list.pop_front();
                    }
                    if (data.humidity_list.size() == data.SAMPLE_SIZE && CURRENT_LOG_LEVEL == INFO) {
                        std::cout << "Humidity average (50 samples): "
                                  << data.humidity_sum / data.SAMPLE_SIZE << std::endl;
                    }
                } else {
                    if (CURRENT_LOG_LEVEL == DEBUG)
                        std::cout << "Unknown data: " << line << std::endl;
                }
            }
        } else if (valread <= 0) {
            // Nếu không đọc được dữ liệu
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // non-blocking: chưa có data
                close(epoll_fd);
                continue;
            }
            std::cout << "Connection issue" << std::endl;
            if (valread < 0) perror("Read failed");
            break;
        }
        close(epoll_fd);
    }
    return NULL;
}


int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    const std::string server_ip = "192.168.1.3";
    const int server_port = 8080;

    // Tạo socket TCP, IPv4
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK); // non-blocking mode

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    // Chuyển IP dạng string -> binary
    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return -1;
    }

    // Thử connect
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("Connection failed");
            close(sock);
            return -1;
        }

        // Nếu đang connect non-blocking, dùng epoll để chờ kết nối
        int epoll_fd = epoll_create1(0);
        struct epoll_event ev;
        ev.events = EPOLLOUT;  // chờ khi socket sẵn sàng ghi
        ev.data.fd = sock;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);

        struct epoll_event events[1];
        if (epoll_wait(epoll_fd, events, 1, 5000) <= 0) {  // Timeout 5s
            perror("Connection timeout or error");
            close(epoll_fd);
            close(sock);
            return -1;
        }

        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            std::cerr << "Connection failed: " << strerror(so_error) << std::endl;
            close(epoll_fd);
            close(sock);
            return -1;
        }
        close(epoll_fd);
    }

    std::cout << "Connected to Z-turn Server at " << server_ip << ":" << server_port << std::endl;

    // Tạo thread xử lý dữ liệu nhận về
    pthread_t data_thread;
    if (pthread_create(&data_thread, NULL, process_data, &sock) < 0) {
        perror("Thread creation failed");
        close(sock);
        return -1;
    }

    // Gửi request "GET_DATA" với tần số 300Hz
    const auto frequency = 600;  // 300Hz
    const auto period = std::chrono::microseconds(static_cast<long>(1000000.0 / frequency));
    auto next_time = std::chrono::high_resolution_clock::now();
    std::string request = "GET_DATA\n";

    while (true) {
        // Gửi request tới server
        if (send(sock, request.c_str(), request.length(), 0) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // buffer gửi bị đầy -> chờ 100us rồi gửi lại
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            perror("Send failed");
            break;
        }

        // Điều chỉnh thời gian gửi để duy trì tần số 300Hz
        next_time += period;
        auto now = std::chrono::high_resolution_clock::now();
        if (now < next_time) {
            std::this_thread::sleep_until(next_time);
        } else { //(timeout)
            std::cout << "Warning: Cannot keep up with 300 Hz frequency" << std::endl;
        }
    }

    pthread_cancel(data_thread);
    close(sock);
    return 0;
}
