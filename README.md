# 🏠 Home Surveillance System v2.0

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Raspberry%20Pi-C51A4A?logo=raspberry-pi&logoColor=white" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/Build-CMake%203.16%2B-064F8C?logo=cmake&logoColor=white" />
  <img src="https://img.shields.io/badge/OpenCV-4.x-5C3EE8?logo=opencv&logoColor=white" />
  <img src="https://img.shields.io/badge/License-MIT-green" />
</p>

<p align="center">
  A multithreaded, production-grade home security system built in C++17 for the Raspberry Pi.<br/>
  Captures live video via V4L2, detects motion with OpenCV MOG2 background subtraction,<br/>
  records MP4 clips, controls GPIO peripherals, and streams MJPEG over HTTP — all in one binary.
</p>

---

## 📸 Circuit Schematic

<img width="1024" height="1024" alt="circuit-diagram" src="https://github.com/user-attachments/assets/a3dc3103-f4fc-4af4-a93a-2e60c071d1dc" />

---

## ✨ Features

| Feature | Details |
|---|---|
| 🎥 **Live Capture** | V4L2 mmap kernel buffers from `/dev/video0` — YUYV or MJPEG pixel formats |
| 🔍 **Motion Detection** | OpenCV MOG2 background subtraction + contour filtering with configurable sensitivity |
| 💾 **Auto Recording** | Triggers a 15-second MP4 clip on any motion event; auto-deletes clips older than 30 days |
| 🌙 **Night Vision** | Reads per-frame brightness and switches IR LED via MOSFET gate automatically |
| 🚨 **PIR Sensor** | Hardware passive-infrared input on BCM 17, pulse-buzzes on detection |
| 📡 **MJPEG Stream** | Multi-client HTTP server on port 8080 — view live feed in any browser |
| 🔔 **Alert Queue** | Thread-safe alert manager triggers local buzzer (Telegram/email hooks ready to wire in) |
| 📋 **Async Logger** | Bounded queue, background writer thread — zero blocking on the hot capture path |
| ⚙️ **systemd Service** | Ships with a `surveillance.service` unit for auto-start on boot |

---

## 🗂️ Repository Structure

```
Home-Surveillance-System/
├── main.cpp            # Entry point, signal handling, system lifecycle
├── surveillance.hpp    # All class declarations + compile-time configuration
├── camera.cpp          # V4L2Camera — mmap capture, YUYV→BGR conversion
├── motion.cpp          # MotionDetector — MOG2 + contour analysis
├── recorder.cpp        # VideoRecorder — MP4 writer + retention cleanup
├── stream.cpp          # StreamServer — multi-client MJPEG HTTP server
├── gpio.cpp            # GPIOManager — libgpiod PIR monitor + IR/buzzer/LED control
├── alert.cpp           # AlertManager — queued alert dispatch
├── logger.cpp          # Logger — thread-safe async file logger
├── CMakeLists.txt      # CMake build definition (C++17, Release flags)
├── install.sh          # One-shot install: deps → build → systemd enable
└── hardware/
    ├── home-surveillance-schematic.png   # GPIO & peripheral wiring diagram
    └── home-surveillance.sch            # KiCad schematic source
```

---

## 🔌 Hardware Requirements

### Components

| Component | Description |
|---|---|
| Raspberry Pi | Any model with 40-pin GPIO header (Pi 3B+ / 4 / 5 recommended) |
| Camera | Pi CSI ribbon camera module **or** USB V4L2-compatible camera |
| PIR Sensor | Standard HC-SR501 (3-pin: VCC, GND, OUT) |
| IR LED Array | 850 nm IR LED array — driven via **logic N-MOSFET (Q1)** |
| Buzzer | 5 V active buzzer — driven via **NPN/N-MOSFET (Q2)** |
| Status LED | Any standard LED + **330 Ω resistor (R4)** |
| Resistors | R1: 100 Ω (IR gate), R2: 10 kΩ (buzzer base), R4: 330 Ω (LED) |

> ⚠️ **Do NOT** connect the IR LED array or 5 V buzzer directly to a GPIO pin.  
> GPIO pins source/sink ≤ 16 mA at 3.3 V — always use a driver transistor or MOSFET.

### GPIO Pin Map (BCM Numbering)

| Function | BCM GPIO | Physical Pin | Direction |
|---|---|---|---|
| PIR Sensor Output | **17** | Pin 11 | Input |
| IR LED (via MOSFET) | **18** | Pin 12 | Output |
| Buzzer (via transistor) | **27** | Pin 13 | Output |
| Status LED | **22** | Pin 15 | Output |
| Power (PIR VCC) | — | Pin 2 (+5 V) | Supply |
| Common Ground | — | Pin 6 (GND) | Return |

---

## ⚙️ Configuration

All tunable parameters live in the `config` namespace inside [`surveillance.hpp`](surveillance.hpp):

```cpp
namespace config {
    // Camera
    constexpr int         CAM_WIDTH            = 1280;
    constexpr int         CAM_HEIGHT           = 720;
    constexpr int         CAM_FPS              = 30;
    constexpr const char* CAM_DEVICE           = "/dev/video0";

    // Motion Detection
    constexpr int         MOTION_THRESHOLD     = 25;   // pixel diff threshold
    constexpr int         MOTION_MIN_AREA      = 500;  // px² — smaller blobs ignored
    constexpr int         MOTION_HISTORY       = 500;  // MOG2 history frames
    constexpr int         MOTION_VAR_THRESHOLD = 16;   // MOG2 variance threshold
    constexpr int         MOTION_COOLDOWN_SEC  = 5;    // seconds between triggers
    constexpr int         RECORD_DURATION_SEC  = 15;   // clip length after motion

    // Recording
    constexpr const char* RECORD_DIR           = "/var/lib/surveillance/recordings";
    constexpr const char* LOG_FILE             = "/var/log/surveillance/system.log";
    constexpr int         MAX_RECORD_AGE_DAYS  = 30;   // auto-delete older clips
    constexpr const char* VIDEO_CODEC          = "mp4v";
    constexpr const char* VIDEO_EXT            = ".mp4";

    // Streaming
    constexpr int         STREAM_PORT          = 8080;
    constexpr int         STREAM_QUALITY       = 80;   // JPEG quality 0–100
    constexpr int         MAX_CLIENTS          = 10;

    // GPIO (BCM numbering)
    constexpr int         PIR_PIN              = 17;
    constexpr int         IR_LED_PIN           = 18;
    constexpr int         BUZZER_PIN           = 27;
    constexpr int         STATUS_LED_PIN       = 22;

    // Night Vision
    constexpr int         NIGHT_THRESHOLD      = 50;   // brightness < this → IR on
    constexpr int         NIGHT_HYSTERESIS     = 20;   // prevents rapid toggling
}
```

No config file or environment variables needed — rebuild after editing constants.

---

## 🚀 Quick Install (Raspberry Pi OS)

Clone the repository onto your Pi and run the installer:

```bash
git clone https://github.com/<your-username>/Home-Surveillance-System.git
cd Home-Surveillance-System
chmod +x install.sh
sudo ./install.sh
```

The script will:
1. `apt install` all dependencies
2. Build the binary in Release mode
3. Install to `/usr/local/bin/surveillance`
4. Create `/var/lib/surveillance/recordings` and `/var/log/surveillance`
5. Write and enable `surveillance.service` via systemd

Once complete, the live stream is available at:
```
http://<raspberry-pi-ip>:8080
```

---

## 🔧 Manual Build

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
    libopencv-dev libgpiod-dev libv4l-dev v4l-utils

# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# Install binary
sudo cmake --install build --prefix /usr/local

# Create storage directories
sudo install -d -o "$USER" -g "$USER" \
    /var/lib/surveillance/recordings \
    /var/log/surveillance

# Run
/usr/local/bin/surveillance
```

---

## 🔒 systemd Service Management

```bash
# Check status
sudo systemctl status surveillance

# View live logs
journalctl -u surveillance -f

# Stop
sudo systemctl stop surveillance

# Disable autostart
sudo systemctl disable surveillance
```

The service runs as the invoking `$SUDO_USER` and automatically adds that user to the `video` and `gpio` groups.

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    SurveillanceSystem                   │
│                                                         │
│  ┌───────────┐   frames   ┌───────────────────────────┐ │
│  │ V4L2Camera│──────────►│     processingLoop()       │ │
│  │  (thread) │           │                            │ │
│  └───────────┘           │  brightness → GPIOManager  │ │
│                          │  MotionDetector.detect()   │ │
│  ┌───────────┐           │  VideoRecorder.write()     │ │
│  │GPIOManager│◄──────────│  StreamServer.update()     │ │
│  │  (thread) │           │  AlertManager.trigger()    │ │
│  └───────────┘           └───────────────────────────-┘ │
│                                                         │
│  ┌─────────────┐  ┌────────────┐  ┌──────────────────┐  │
│  │StreamServer │  │VideoRecorder│ │  AlertManager    │  │
│  │(HTTP/MJPEG) │  │  (MP4/disk) │ │(buzzer + queue)  │  │
│  └─────────────┘  └────────────┘  └──────────────────┘  │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Logger  — async bounded queue, background writer │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Thread Model

| Thread | Owner | Role |
|---|---|---|
| `capture_thread` | `V4L2Camera` | Dequeues V4L2 mmap buffers, converts YUYV→BGR |
| `processing_thread` | `SurveillanceSystem` | Motion detect, record, stream, GPIO logic |
| `monitor_thread` | `GPIOManager` | Polls PIR pin every 50 ms |
| `server_thread` | `StreamServer` | Accept loop for new HTTP clients |
| `client_threads` | `StreamServer` | One thread per MJPEG subscriber (max 10) |
| `alert_thread` | `AlertManager` | Drains alert queue, pulses buzzer |
| `writer_thread` | `Logger` | Drains log queue to file |

A lock-free **frame buffer pool** (`FrameBuffer[4]`) is used between the capture and processing threads to avoid memory allocation on the hot path.

---

## ⚠️ Known Limitations

- **No stream authentication.** The MJPEG endpoint on port 8080 has no password or TLS. Keep it on a trusted LAN or put it behind an authenticated reverse proxy (e.g. nginx + HTTP Basic Auth).
- **Software encoding only.** The codec is `mp4v` via OpenCV `VideoWriter`. Hardware H.264 (e.g. `v4l2_codec`) is not implemented.
- **Telegram / email alerts not wired.** The `AlertManager` has `sendTelegram()` and `sendEmail()` stubs ready to implement — currently only the local buzzer fires.
- **Camera device path is hardcoded.** Modern Pi camera setups using `libcamera` may need `v4l2-compat` or a dedicated V4L2 device node (`/dev/video0`). Verify with `v4l2-ctl --list-devices`.

---

## 📦 Dependencies

| Library | Version | Purpose |
|---|---|---|
| **OpenCV** | 4.x | MOG2 background subtraction, contour analysis, `VideoWriter`, JPEG encode |
| **libgpiod** | ≥ 1.6 | Modern kernel GPIO access (replaces deprecated `wiringPi`) |
| **libv4l** / kernel headers | — | V4L2 mmap camera capture |
| **CMake** | ≥ 3.16 | Build system |
| **GCC / Clang** | C++17 | Compiler |

---

## 🤝 Contributing

Pull requests are welcome! Please:
1. Fork the repository and create a feature branch.
2. Follow the existing code style (clang-format compatible).
3. Keep each PR focused — one feature or fix per PR.
4. Add a brief description of changes and test on real hardware where possible.

---

## 📄 License

Unless otherwise specified, all content in this repository—including, but not
limited to, software source code, firmware, hardware design files (schematics,
PCB layouts, Gerber files, BOMs, CAD files), documentation, configuration
files, examples, and supporting materials—is made available under the MIT
License.

---

## MIT License

Copyright (c) 2026 Joydeep Majumdar

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
---
