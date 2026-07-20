#include "surveillance.hpp"

namespace {
volatile std::sig_atomic_t g_stop_requested = 0;
}

SurveillanceSystem::SurveillanceSystem() = default;

SurveillanceSystem::~SurveillanceSystem() {
    stop();
}

bool SurveillanceSystem::init() {
    status.running = true;
    logger = new Logger(config::LOG_FILE);
    logger->info("=== Surveillance System Starting ===");

    buffer_pool = new FrameBuffer[config::BUFFER_POOL_SIZE];
    gpio = new GPIOManager(&status, logger);
    if (!gpio->init()) {
        logger->error("GPIO init failed");
        return false;
    }

    camera = new V4L2Camera(&status, logger, buffer_pool, config::BUFFER_POOL_SIZE);
    detector = new MotionDetector(&status, logger);
    recorder = new VideoRecorder(&status, logger);
    streamer = new StreamServer(config::STREAM_PORT, &status, logger);
    alerter = new AlertManager(gpio, logger);

    if (!camera->start() || !streamer->start()) {
        logger->error("Camera or stream server initialization failed");
        return false;
    }

    gpio->start();
    alerter->start();
    processing_thread = std::thread(&SurveillanceSystem::processingLoop, this);
    logger->info("System initialized successfully");
    return true;
}

void SurveillanceSystem::processingLoop() {
    while (status.running.load()) {
        FrameBuffer* fb = camera->acquireFrame();
        if (!fb) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (fb->brightness < config::NIGHT_THRESHOLD && !status.night_mode.load()) {
            gpio->setIR(true);
        } else if (fb->brightness > config::NIGHT_THRESHOLD + config::NIGHT_HYSTERESIS && status.night_mode.load()) {
            gpio->setIR(false);
        }

        if (detector->detect(fb)) {
            alerter->trigger("MOTION", "Frame " + std::to_string(fb->frame_number));
            if (!recorder->isRecording()) {
                recorder->startRecording(fb->frame.size(), config::CAM_FPS);
            }
        }

        if (!fb->motion_regions.empty()) {
            detector->drawRegions(fb->frame, fb->motion_regions);
        }

        streamer->updateFrame(fb->frame);
        if (recorder->isRecording()) {
            recorder->writeFrame(fb->frame);
            if (recorder->shouldStop()) {
                recorder->stopRecording();
                recorder->cleanupOldRecordings();
            }
        }

        static auto last_fps_time = std::chrono::steady_clock::now();
        static int frame_count = 0;
        ++frame_count;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_fps_time).count();
        if (elapsed >= 1) {
            status.fps = frame_count / elapsed;
            frame_count = 0;
            last_fps_time = now;
        }

        camera->releaseFrame(fb);
    }
}

void SurveillanceSystem::start() {
    if (!init()) {
        std::cerr << "Failed to initialize system" << std::endl;
        stop();
        return;
    }

    while (status.running.load() && !g_stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    stop();
}

void SurveillanceSystem::stop() {
    if (stopped.exchange(true)) return;
    status.running = false;

    if (processing_thread.joinable()) processing_thread.join();
    if (recorder) recorder->stopRecording();
    if (alerter) alerter->stop();
    if (streamer) streamer->stop();
    if (camera) camera->stop();
    if (gpio) gpio->stop();

    delete alerter; alerter = nullptr;
    delete streamer; streamer = nullptr;
    delete recorder; recorder = nullptr;
    delete detector; detector = nullptr;
    delete camera; camera = nullptr;
    delete gpio; gpio = nullptr;
    delete[] buffer_pool; buffer_pool = nullptr;
    delete logger; logger = nullptr;
}

void signal_handler(int) {
    g_stop_requested = 1;
}

int main() {
    std::cout << "Home Surveillance System v2.0" << std::endl;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SurveillanceSystem system;
    system.start();
    return 0;
}