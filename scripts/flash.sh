#!/bin/bash

# Grid Board Tab5 - Flash Script
# This script builds and flashes the Grid Board Tab5 firmware to your M5Stack Tab5 device

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Grid Board Tab5 - Flash Script${NC}"
echo "================================"

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo -e "${YELLOW}ESP-IDF environment not found. Trying to source it...${NC}"
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        source $HOME/esp/esp-idf/export.sh
    elif [ -f "/Users/a.vasilyev/esp/esp-idf/export.sh" ]; then
        source /Users/a.vasilyev/esp/esp-idf/export.sh
    else
        echo -e "${RED}Error: ESP-IDF not found. Please source it manually:${NC}"
        echo "source /path/to/esp-idf/export.sh"
        exit 1
    fi
fi

# Default serial port
PORT=${1:-/dev/cu.usbmodem212301}

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo -e "${YELLOW}Warning: Port $PORT not found${NC}"
    echo "Available USB ports:"
    ls /dev/cu.usb* 2>/dev/null || echo "No USB ports found"
    echo ""
    echo "Usage: $0 [PORT]"
    echo "Example: $0 /dev/cu.usbmodem1101"
    exit 1
fi

echo -e "${GREEN}Using port: $PORT${NC}"

# Build the project
echo -e "${YELLOW}Building project...${NC}"
idf.py build

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# Flash the device
echo -e "${YELLOW}Flashing to device...${NC}"
idf.py -p $PORT flash

if [ $? -ne 0 ]; then
    echo -e "${RED}Flash failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Flash completed successfully!${NC}"
echo ""
echo "To monitor the device output, run:"
echo "  idf.py -p $PORT monitor"
echo "Or:"
echo "  screen $PORT 115200"