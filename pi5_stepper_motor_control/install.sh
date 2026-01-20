#!/bin/bash

# AllSky ESP32 Focuser - Combined Installer
# Installs:
# 1. Pi5 Focuser Service (Port 8080)
# 2. ZWO Camera Streamer Service (Port 5000)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
APP_DIR="$SCRIPT_DIR"
# Streamer runs from the same directory
STREAMER_DIR="$APP_DIR"

# Virtual Env (Shared)
VENV_DIR="$APP_DIR/venv"

# Services
FOCUSER_SERVICE="pi5-focuser"
STREAMER_SERVICE="pi5-streamer"
APP_USER=$(whoami)

echo -e "${GREEN}=== AllSky System Installer ===${NC}"
echo "Install Dir:  $APP_DIR"
echo "Venv Dir:     $VENV_DIR"
echo "User:         $APP_USER"
echo ""

# --- Pre-flight Checks ---

if ! grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
    echo -e "${YELLOW}Warning: Not running on a Raspberry Pi? GPIOs may fail.${NC}"
    # Non-blocking warning
fi

if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: Python3 missing.${NC}"
    exit 1
fi

# --- 1. System Dependencies ---
echo -e "${GREEN}[1/6] Installing system dependencies...${NC}"
sudo apt-get update -qq > /dev/null 2>&1
# Combined deps: venv, pip, gpio, opencv
sudo apt-get install -y -qq python3-venv python3-pip python3-gpiozero python3-lgpio libopencv-dev python3-opencv > /dev/null 2>&1
echo "✓ System packages installed"

# --- 2. Virtual Environment ---
echo -e "${GREEN}[2/6] Setting up shared virtual environment...${NC}"
if [ -d "$VENV_DIR" ]; then
    echo "Updating existing venv..."
else
    python3 -m venv --system-site-packages "$VENV_DIR"
fi

# --- 3. Python Packages ---
echo -e "${GREEN}[3/6] Installing Python libraries...${NC}"
source "$VENV_DIR/bin/activate"
pip install --upgrade pip -q > /dev/null 2>&1
# Combined requirements
pip install -q flask flask-cors gpiozero rpi-lgpio zwoasi opencv-python-headless numpy 2>&1 | grep -v "dependency conflicts" | grep -v "Flask-SQLAlchemy" || true
echo "✓ Python packages installed"

# --- 4. Streamer Checks ---
echo -e "${GREEN}[4/6] Verifying ZWO Streamer files...${NC}"

# Check for Library
if [ ! -f "$APP_DIR/libASICamera2.so" ]; then
    echo -e "${YELLOW}⚠ WARNING: libASICamera2.so is missing in $APP_DIR${NC}"
    echo -e "${YELLOW}  Streamer service will not function until you copy the ZWO SDK library.${NC}"
else
    echo "✓ Found libASICamera2.so"
fi

# Check for zwo.py
if [ ! -f "$APP_DIR/zwo.py" ]; then
    echo -e "${RED}Error: zwo.py not found in $APP_DIR${NC}"
    exit 1
fi
chmod +x "$APP_DIR/zwo.py"

# --- 5. Systemd Services ---
echo -e "${GREEN}[5/6] Creating Systemd Services...${NC}"

# Add user to gpio group
if ! groups $APP_USER | grep -q '\bgpio\b'; then
    sudo usermod -a -G gpio $APP_USER
fi

# Service 1: Focuser (Port 8080)
sudo tee "/etc/systemd/system/${FOCUSER_SERVICE}.service" > /dev/null <<EOF
[Unit]
Description=Pi5 Focuser Web Service
After=network.target

[Service]
Type=simple
User=$APP_USER
Group=gpio
WorkingDirectory=$APP_DIR
Environment="PATH=$VENV_DIR/bin"
ExecStart=$VENV_DIR/bin/python app.py
Restart=always
RestartSec=10
NoNewPrivileges=false

[Install]
WantedBy=multi-user.target
EOF

# Service 2: Streamer (Port 5000)
sudo tee "/etc/systemd/system/${STREAMER_SERVICE}.service" > /dev/null <<EOF
[Unit]
Description=ZWO Camera Streamer
After=network.target

[Service]
Type=simple
User=$APP_USER
Group=gpio
WorkingDirectory=$APP_DIR
Environment="PATH=$VENV_DIR/bin"
# Ensure library path finds .so in the app dir
Environment="LD_LIBRARY_PATH=$APP_DIR"
ExecStart=$VENV_DIR/bin/python zwo.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# --- 6. Start Services ---
echo -e "${GREEN}[6/6] Starting services...${NC}"
sudo systemctl daemon-reload
# Note: Services are NOT enabled for auto-start on boot
# To enable: sudo systemctl enable pi5-focuser pi5-streamer
sudo systemctl restart "$FOCUSER_SERVICE"
sudo systemctl restart "$STREAMER_SERVICE"

# --- 7. Status Check ---
echo -e "${GREEN}[7/7] Verifying status...${NC}"
sleep 3
FOCUSER_STATUS=$(sudo systemctl is-active "$FOCUSER_SERVICE")
STREAMER_STATUS=$(sudo systemctl is-active "$STREAMER_SERVICE")
IP_ADDR=$(hostname -I | awk '{print $1}')

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo -e "${GREEN}                    Installation Complete${NC}"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo -e "${GREEN}Service Status:${NC}"
echo ""

# Focuser Status
if [ "$FOCUSER_STATUS" = "active" ]; then
    echo -e "  ✓ ${GREEN}Focuser Service${NC} (Port 8080) - Running"
    echo "    → Motor control web interface"
    echo "    → URL: http://$IP_ADDR:8080"
else
    echo -e "  ✗ ${RED}Focuser Service${NC} (Port 8080) - Failed"
    echo "    → Check logs: sudo journalctl -u $FOCUSER_SERVICE -n 50"
fi

echo ""

# Streamer Status
if [ "$STREAMER_STATUS" = "active" ]; then
    echo -e "  ✓ ${GREEN}Streamer Service${NC} (Port 5000) - Running"
    echo "    → ZWO camera MJPEG stream"
    echo "    → Accessible via the focuser interface"
else
    echo -e "  ⚠ ${YELLOW}Streamer Service${NC} (Port 5000) - Not Active"
    echo "    → This is expected if:"
    echo "      • libASICamera2.so is missing"
    echo "      • No ZWO camera is connected"
    echo "    → Check logs: sudo journalctl -u $STREAMER_SERVICE -n 50"
fi

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo -e "${GREEN}Useful Commands:${NC}"
echo "  • View focuser logs:  sudo journalctl -u $FOCUSER_SERVICE -f"
echo "  • View streamer logs: sudo journalctl -u $STREAMER_SERVICE -f"
echo "  • Restart focuser:    sudo systemctl restart $FOCUSER_SERVICE"
echo "  • Restart streamer:   sudo systemctl restart $STREAMER_SERVICE"
echo "  • Check status:       sudo systemctl status $FOCUSER_SERVICE"
echo ""
echo "═══════════════════════════════════════════════════════════════"
echo ""
