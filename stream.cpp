#include "surveillance.hpp"

StreamServer::StreamServer(int p, SystemStatus* s, Logger* l) : port(p), status(s), logger(l) {}
StreamServer::~StreamServer() { stop(); }

bool StreamServer::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { logger->error("Socket creation failed"); return false; }
    int option = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || listen(server_fd, config::MAX_CLIENTS) < 0) {
        logger->error("Unable to bind stream server");
        close(server_fd); server_fd = -1;
        return false;
    }
    running = true;
    server_thread = std::thread(&StreamServer::serverLoop, this);
    logger->info("Stream server started on port " + std::to_string(port));
    return true;
}

void StreamServer::stop() {
    running = false;
    if (server_fd >= 0) { shutdown(server_fd, SHUT_RDWR); close(server_fd); server_fd = -1; }
    if (server_thread.joinable()) server_thread.join();
    for (auto& thread : client_threads) if (thread.joinable()) thread.join();
    client_threads.clear();
}

void StreamServer::serverLoop() {
    while (running.load()) {
        sockaddr_in client_address{};
        socklen_t address_length = sizeof(client_address);
        const int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_address), &address_length);
        if (client_fd < 0) continue;
        if (active_clients.load() >= config::MAX_CLIENTS) { close(client_fd); continue; }
        timeval timeout{5, 0};
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        active_clients++;
        client_threads.emplace_back(&StreamServer::handleClient, this, client_fd);
    }
}

bool StreamServer::sendAll(int client_fd, const void* data, size_t length) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    size_t sent = 0;
    while (sent < length) {
        const ssize_t result = send(client_fd, bytes + sent, length - sent, MSG_NOSIGNAL);
        if (result <= 0) return false;
        sent += static_cast<size_t>(result);
    }
    return true;
}

void StreamServer::handleClient(int client_fd) {
    const char headers[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache\r\n\r\n";
    if (sendAll(client_fd, headers, sizeof(headers) - 1)) {
        while (running.load()) {
            cv::Mat frame;
            { std::lock_guard<std::mutex> lock(frame_mutex); frame = current_frame.clone(); }
            if (frame.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(33)); continue; }
            if (!sendMJPEG(client_fd, frame)) break;
        }
    }
    close(client_fd);
    active_clients--;
}

bool StreamServer::sendMJPEG(int client_fd, const cv::Mat& frame) {
    std::vector<uchar> jpeg;
    if (!cv::imencode(".jpg", frame, jpeg, {cv::IMWRITE_JPEG_QUALITY, config::STREAM_QUALITY})) return false;
    const std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(jpeg.size()) + "\r\n\r\n";
    static constexpr char ending[] = "\r\n";
    return sendAll(client_fd, header.data(), header.size()) && sendAll(client_fd, jpeg.data(), jpeg.size()) && sendAll(client_fd, ending, 2);
}

void StreamServer::updateFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(frame_mutex);
    current_frame = frame.clone();
}