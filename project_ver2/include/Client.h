#ifndef CLIENT_H
#define CLIENT_H

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
#include <fcntl.h>              // set non-blocking
#include <errno.h>
#include <cstring>
#include <pthread.h>

// LOG LEVEL
//  LOG LEVEL 
// OFF  = tắt hết log
// INFO = chỉ log giá trị trung bình (50 mẫu)
// DEBUG = log tất cả dữ liệu nhận được
enum LogLevel { OFF, INFO, DEBUG };

struct DataLists {
    std::list<double> azimuth_list;
    std::list<double> elevation_list;
    std::list<double> temperature_list;
    std::list<double> humidity_list;
// dsách Lkết để lưu dữ liệu nhận được từ server
// Client parse số đó và push vào list, đồng thời duy trì tổng để tính trung bình
    double azimuth_sum = 0.0;
    double elevation_sum = 0.0;
    double temperature_sum = 0.0;
    double humidity_sum = 0.0;

    const int SAMPLE_SIZE = 50; // số mẫu để tính trung bình
};

class Client {
public:
    Client(const std::string& ip, int port, LogLevel level = INFO); 
    ~Client();
//di chuyển CURRENT_LOG_LEVEL vào class dưới dạng log_level
    bool connectToServer();
    void start();
    void stop();

private:
// Thread xử lý dữ liệu nhận được từ server
    static void* processDataThread(void* arg);
    void processData();

    int sock;       // chuyen sock thanh varible of class client 
                    // khi khoi tao truyen sock vao constructor or set sau khi connect
    std::string server_ip;
    int server_port;
    LogLevel log_level;

    pthread_t data_thread;
    bool running;
};

#endif
