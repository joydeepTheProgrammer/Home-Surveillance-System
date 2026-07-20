#include "surveillance.hpp"

V4L2Camera::V4L2Camera(SystemStatus* s, Logger* l, FrameBuffer* p, int ps)
    : status(s), logger(l), pool(p), pool_size(ps) {}

V4L2Camera::~V4L2Camera() { stop(); }

bool V4L2Camera::initDevice() {
    fd = open(config::CAM_DEVICE, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
        logger->error(std::string("Cannot open device: ") + config::CAM_DEVICE);
        return false;
    }

    v4l2_capability cap{};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        logger->error("Camera does not provide V4L2 video capture");
        return false;
    }

    fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config::CAM_WIDTH;
    fmt.fmt.pix.height = config::CAM_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        logger->error("VIDIOC_S_FMT failed");
        return false;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0 || fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
            logger->error("Camera supports neither requested YUYV nor MJPEG format");
            return false;
        }
    }
    logger->info("Camera format: " + std::to_string(fmt.fmt.pix.width) + "x" + std::to_string(fmt.fmt.pix.height));
    return true;
}

bool V4L2Camera::initBuffers() {
    req = {};
    req.count = config::BUFFER_POOL_SIZE;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count == 0) {
        logger->error("VIDIOC_REQBUFS failed");
        return false;
    }

    buffers.resize(req.count, {nullptr, 0});
    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) < 0) return false;
        buffers[i].length = buffer.length;
        buffers[i].start = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            buffers[i].start = nullptr;
            logger->error("mmap failed");
            return false;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0) return false;
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        logger->error("VIDIOC_STREAMON failed");
        return false;
    }
    return true;
}

void V4L2Camera::convertYUYVtoBGR(const uint8_t* yuyv, cv::Mat& bgr, int width, int height) {
    cv::Mat source(height, width, CV_8UC2, const_cast<uint8_t*>(yuyv));
    cv::cvtColor(source, bgr, cv::COLOR_YUV2BGR_YUYV);
}

void V4L2Camera::captureLoop() {
    while (running.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval timeout{2, 0};
        if (select(fd + 1, &fds, nullptr, nullptr, &timeout) <= 0) continue;

        buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN) continue;
            logger->error("VIDIOC_DQBUF failed");
            break;
        }

        FrameBuffer* fb = nullptr;
        for (int i = 0; i < pool_size; ++i) {
            bool expected = false;
            if (pool[i].in_use.compare_exchange_strong(expected, true)) {
                fb = &pool[i];
                break;
            }
        }

        if (fb) {
            fb->ready = false;
            fb->has_motion = false;
            fb->motion_regions.clear();
            fb->timestamp = std::chrono::steady_clock::now();
            fb->frame_number = status->total_frames.fetch_add(1);
            if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
                convertYUYVtoBGR(static_cast<const uint8_t*>(buffers[buf.index].start), fb->frame, fmt.fmt.pix.width, fmt.fmt.pix.height);
            } else {
                std::vector<uint8_t> jpeg(static_cast<uint8_t*>(buffers[buf.index].start), static_cast<uint8_t*>(buffers[buf.index].start) + buf.bytesused);
                fb->frame = cv::imdecode(jpeg, cv::IMREAD_COLOR);
            }
            if (!fb->frame.empty()) {
                cv::Mat gray;
                cv::cvtColor(fb->frame, gray, cv::COLOR_BGR2GRAY);
                fb->brightness = cv::mean(gray)[0];
                fb->ready = true;
            } else {
                fb->in_use = false;
            }
        }
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) logger->error("VIDIOC_QBUF failed");
    }
}

bool V4L2Camera::start() {
    if (!initDevice() || !initBuffers()) { stop(); return false; }
    running = true;
    capture_thread = std::thread(&V4L2Camera::captureLoop, this);
    logger->info("Camera capture started");
    return true;
}

void V4L2Camera::stop() {
    running = false;
    if (capture_thread.joinable()) capture_thread.join();
    if (fd >= 0) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    for (auto& buffer : buffers) {
        if (buffer.start && buffer.start != MAP_FAILED) munmap(buffer.start, buffer.length);
    }
    buffers.clear();
    if (fd >= 0) { close(fd); fd = -1; }
}

FrameBuffer* V4L2Camera::acquireFrame() {
    for (int i = 0; i < pool_size; ++i) {
        if (pool[i].ready.exchange(false)) return &pool[i];
    }
    return nullptr;
}

void V4L2Camera::releaseFrame(FrameBuffer* fb) {
    if (fb) { fb->ready = false; fb->in_use = false; }
}