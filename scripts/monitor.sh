#!/bin/bash

# Grid Board Tab5 - Monitor Script
# This script monitors the serial output from your M5Stack Tab5 device

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Grid Board Tab5 - Serial Monitor${NC}"
echo "=================================="

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

echo -e "${GREEN}Monitoring port: $PORT${NC}"
echo "Press Ctrl+] to exit (or Ctrl+A then K if using screen)"
echo ""

# Check if IDF is available
if [ -n "$IDF_PATH" ]; then
    echo "Using ESP-IDF monitor..."
    idf.py -p $PORT monitor
else
    echo "Using screen for monitoring..."
    screen $PORT 115200
fi