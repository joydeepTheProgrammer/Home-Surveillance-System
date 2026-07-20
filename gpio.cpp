#include "surveillance.hpp"

GPIOManager::GPIOManager(SystemStatus* s, Logger* l) : status(s), logger(l) {}
GPIOManager::~GPIOManager() { stop(); }

bool GPIOManager::init() {
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) { logger->error("Failed to open GPIO chip"); return false; }
    pir_line = gpiod_chip_get_line(chip, config::PIR_PIN);
    ir_line = gpiod_chip_get_line(chip, config::IR_LED_PIN);
    buzzer_line = gpiod_chip_get_line(chip, config::BUZZER_PIN);
    status_line = gpiod_chip_get_line(chip, config::STATUS_LED_PIN);
    if (!pir_line || !ir_line || !buzzer_line || !status_line ||
        gpiod_line_request_input(pir_line, "surveillance") < 0 ||
        gpiod_line_request_output(ir_line, "surveillance", 0) < 0 ||
        gpiod_line_request_output(buzzer_line, "surveillance", 0) < 0 ||
        gpiod_line_request_output(status_line, "surveillance", 0) < 0) {
        logger->error("Failed to request one or more GPIO lines");
        stop();
        return false;
    }
    logger->info("GPIO initialized");
    return true;
}

void GPIOManager::start() {
    if (running.exchange(true)) return;
    monitor_thread = std::thread(&GPIOManager::monitorLoop, this);
}

void GPIOManager::stop() {
    running = false;
    if (monitor_thread.joinable()) monitor_thread.join();
    if (ir_line) { gpiod_line_set_value(ir_line, 0); gpiod_line_release(ir_line); ir_line = nullptr; }
    if (buzzer_line) { gpiod_line_set_value(buzzer_line, 0); gpiod_line_release(buzzer_line); buzzer_line = nullptr; }
    if (status_line) { gpiod_line_set_value(status_line, 0); gpiod_line_release(status_line); status_line = nullptr; }
    if (pir_line) { gpiod_line_release(pir_line); pir_line = nullptr; }
    if (chip) { gpiod_chip_close(chip); chip = nullptr; }
}

void GPIOManager::monitorLoop() {
    while (running.load()) {
        const int value = pir_line ? gpiod_line_get_value(pir_line) : 0;
        if (value == 1 && !status->pir_triggered.exchange(true)) {
            logger->info("PIR motion detected");
            status->motion_events++;
            pulseBuzzer(200);
        } else if (value == 0) {
            status->pir_triggered = false;
        }
        if (status_line) { std::lock_guard<std::mutex> lock(line_mutex); gpiod_line_set_value(status_line, (status->is_recording || status->motion_detected) ? 1 : 0); }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void GPIOManager::setIR(bool state) { std::lock_guard<std::mutex> lock(line_mutex); if (ir_line) { gpiod_line_set_value(ir_line, state ? 1 : 0); status->night_mode = state; } }
void GPIOManager::setBuzzer(bool state) { std::lock_guard<std::mutex> lock(line_mutex); if (buzzer_line) gpiod_line_set_value(buzzer_line, state ? 1 : 0); }
void GPIOManager::setStatusLED(bool state) { std::lock_guard<std::mutex> lock(line_mutex); if (status_line) gpiod_line_set_value(status_line, state ? 1 : 0); }
void GPIOManager::pulseBuzzer(int ms) { setBuzzer(true); std::this_thread::sleep_for(std::chrono::milliseconds(ms)); setBuzzer(false); }
bool GPIOManager::readPIR() { std::lock_guard<std::mutex> lock(line_mutex); return pir_line && gpiod_line_get_value(pir_line) == 1; }