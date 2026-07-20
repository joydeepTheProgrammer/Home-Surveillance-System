# Home Surveillance System v2.0

A Raspberry Pi home-surveillance application written in C++17. It captures a V4L2 camera, detects motion with OpenCV MOG2, records MP4 files, controls GPIO peripherals, and serves an MJPEG stream.

## What it does

- Captures YUYV or MJPEG frames from `/dev/video0` through V4L2 mmap buffers.
- Detects motion with background subtraction and contour filtering.
- Starts a 15-second MP4 recording after a vision motion event.
- Switches an IR LED based on image brightness; supports PIR input, buzzer, and status LED.
- Serves the current frame at `http://<pi-ip>:8080` as MJPEG.
- Deletes recordings older than 30 days.

## Important limitations

- The stream has no authentication or encryption. Restrict it to a trusted LAN or place it behind an authenticated reverse proxy.
- The requested codec is `mp4v`; hardware H.264 encoding is not implemented.
- Telegram and email delivery are not implemented. Alerts currently trigger the local buzzer and are written to the log.
- This project requires a camera exposed as `/dev/video0`. Modern Raspberry Pi camera setups may require a V4L2-compatible camera configuration.

## Hardware and GPIO

| Function | BCM GPIO |
| --- | --- |
| PIR input | 17 |
| IR LED | 18 |
| Buzzer | 27 |
| Status LED | 22 |

## Install on Raspberry Pi OS

Clone or copy this repository to the Pi, then run from the repository root:

```bash
chmod +x install.sh
./install.sh
```

The installer installs dependencies, builds the program, creates recording/log directories, installs the binary at `/usr/local/bin/surveillance`, and enables `surveillance.service`.

The service runs as the user who invoked `sudo` (`$SUDO_USER`) and adds that user to the `video` and `gpio` groups for the service. Ensure the camera is usable by that user before enabling the service.

## Manual build

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libopencv-dev libgpiod-dev libv4l-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build --prefix /usr/local
```

Before running manually, create writable storage directories:

```bash
sudo install -d -o "$USER" -g "$USER" /var/lib/surveillance/recordings /var/log/surveillance
/usr/local/bin/surveillance
```

## Configuration

Configuration constants, including camera path, dimensions, recording location, stream port, GPIO pins, and motion sensitivity, are in `surveillance.hpp`.

## Source layout

- `main.cpp` — system startup, processing loop, and shutdown.
- `camera.cpp` — V4L2 capture and mmap buffer management.
- `motion.cpp` — OpenCV motion detection.
- `recorder.cpp` — MP4 recording and retention cleanup.
- `stream.cpp` — MJPEG HTTP server.
- `gpio.cpp` — libgpiod input/output.
- `alert.cpp` — queued local alerts.

# License

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
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---
