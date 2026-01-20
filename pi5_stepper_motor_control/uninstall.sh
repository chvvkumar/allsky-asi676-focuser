#!/bin/bash

# AllSky ESP32 Focuser - Combined Uninstaller

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SERVICE_FOCUSER="pi5-focuser"
SERVICE_STREAMER="pi5-streamer"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="$SCRIPT_DIR/venv"

echo -e "${YELLOW}=== AllSky System Uninstaller ===${NC}"
echo ""
echo -e "${YELLOW}This will remove:${NC}"
echo "  - Service: $SERVICE_FOCUSER"
echo "  - Service: $SERVICE_STREAMER"
echo "  - Virtual Environment: $VENV_DIR"
echo ""
read -p "Are you sure you want to continue? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

# Function to remove a service
remove_service() {
    local svc=$1
    echo -e "${GREEN}Removing $svc...${NC}"
    
    if sudo systemctl is-active --quiet "$svc"; then
        sudo systemctl stop "$svc"
        echo "  - Stopped"
    fi

    if sudo systemctl is-enabled --quiet "$svc" 2>/dev/null; then
        sudo systemctl disable "$svc"
        echo "  - Disabled"
    fi

    if [ -f "/etc/systemd/system/${svc}.service" ]; then
        sudo rm "/etc/systemd/system/${svc}.service"
        echo "  - Service file removed"
    else
        echo "  - Service file not found"
    fi
}

remove_service "$SERVICE_FOCUSER"
remove_service "$SERVICE_STREAMER"

echo -e "${GREEN}Reloading systemd...${NC}"
sudo systemctl daemon-reload

echo -e "${GREEN}Removing virtual environment...${NC}"
if [ -d "$VENV_DIR" ]; then
    rm -rf "$VENV_DIR"
    echo "  - Venv removed"
else
    echo "  - Venv not found"
fi

echo ""
echo -e "${GREEN}=== Uninstallation Complete! ===${NC}"
echo "To reinstall, run: ./install.sh"