#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SOURCE_DIR/build"
SERVICE_USER="${SUDO_USER:-$USER}"

sudo apt update
sudo apt install -y build-essential cmake pkg-config libopencv-dev libgpiod-dev libv4l-dev v4l-utils

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(nproc)"
sudo cmake --install "$BUILD_DIR" --prefix /usr/local

sudo install -d -o "$SERVICE_USER" -g "$SERVICE_USER" /var/lib/surveillance/recordings /var/log/surveillance
sudo tee /etc/systemd/system/surveillance.service >/dev/null <<EOF
[Unit]
Description=Home Surveillance System
After=network.target

[Service]
Type=simple
User=$SERVICE_USER
SupplementaryGroups=video gpio
ExecStart=/usr/local/bin/surveillance
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now surveillance.service
echo "Stream: http://<raspberry-pi-ip>:8080"