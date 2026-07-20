#include "surveillance.hpp"

Logger::Logger(const std::string& filename) {
    log_file.open(filename, std::ios::app);
    writer_thread = std::thread(&Logger::writerLoop, this);
}

Logger::~Logger() {
    running = false;
    cv.notify_all();
    if (writer_thread.joinable()) writer_thread.join();
    if (log_file.is_open()) log_file.close();
}

void Logger::log(const std::string& level, const std::string& msg) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
    localtime_r(&time, &local_time);

    std::stringstream ss;
    ss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    ss << " [" << level << "] " << msg;

    std::lock_guard<std::mutex> lock(mtx);
    if (buffer.size() >= config::LOG_BUFFER_SIZE) buffer.pop();
    buffer.push(ss.str());
    cv.notify_one();
}

void Logger::writerLoop() {
    for (;;) {
        std::string message;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return !buffer.empty() || !running; });
            if (!running && buffer.empty()) break;
            message = std::move(buffer.front());
            buffer.pop();
        }
        std::cout << message << std::endl;
        if (log_file.is_open()) {
            log_file << message << std::endl;
            log_file.flush();
        }
    }
}