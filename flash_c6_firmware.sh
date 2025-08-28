#!/bin/bash

# Script to flash ESP32-C6 firmware via UART bridge mode
# The Tab5 P4 must be running with bridge mode enabled

echo "========================================"
echo "ESP32-C6 Firmware Flash Script for Tab5"
echo "========================================"
echo ""
echo "Prerequisites:"
echo "1. Tab5 must be in UART bridge mode"
echo "2. ESP32-C6 SDIO slave firmware binary required"
echo ""

# Check if firmware file is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <c6_firmware.bin> [port]"
    echo "Example: $0 esp32c6_sdio_slave.bin /dev/cu.usbmodem1101"
    exit 1
fi

FIRMWARE=$1
PORT=${2:-/dev/cu.usbmodem1101}

if [ ! -f "$FIRMWARE" ]; then
    echo "Error: Firmware file '$FIRMWARE' not found"
    exit 1
fi

echo "Firmware: $FIRMWARE"
echo "Port: $PORT"
echo ""
echo "Flashing ESP32-C6..."
echo ""

# Flash the C6 via the P4 UART bridge
esptool.py --chip esp32c6 -p $PORT -b 460800 \
    --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_freq 40m \
    0x0 $FIRMWARE

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ ESP32-C6 firmware flashed successfully!"
    echo "Reset the Tab5 to exit bridge mode and use SDIO communication"
else
    echo ""
    echo "❌ Flashing failed. Check connections and try again."
    exit 1
fi