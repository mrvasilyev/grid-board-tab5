#!/bin/bash

# Script to flash ESP32-C6 with ESP-Hosted slave firmware for M5Stack Tab5
# The ESP32-C6 on the Tab5 acts as a WiFi co-processor using SDIO communication

echo "==============================================="
echo "ESP32-C6 ESP-Hosted Slave Firmware Flash Tool"
echo "for M5Stack Tab5"
echo "==============================================="

# ESP32-C6 is connected via internal SDIO, but we need to flash it via USB/UART
# The Tab5 may have a way to access the C6's UART pins

echo ""
echo "This script will help you flash the ESP32-C6 with ESP-Hosted slave firmware."
echo ""
echo "Prerequisites:"
echo "1. Clone ESP-Hosted repository:"
echo "   git clone https://github.com/espressif/esp-hosted"
echo ""
echo "2. The ESP32-C6 needs to be accessible via UART/USB for flashing"
echo "   On M5Stack Tab5, this might require special hardware access"
echo ""

# Check if esp-hosted directory exists
if [ ! -d "esp-hosted" ]; then
    echo "ESP-Hosted repository not found. Cloning..."
    git clone https://github.com/espressif/esp-hosted
fi

cd esp-hosted/slave

echo ""
echo "Building ESP32-C6 slave firmware with SDIO support..."
echo ""

# Source ESP-IDF environment
source /Users/a.vasilyev/esp/esp-idf/export.sh

# Set target to ESP32-C6
idf.py set-target esp32c6

# Configure for SDIO transport
cat > sdkconfig.defaults << 'EOF'
# ESP32-C6 Slave configuration for M5Stack Tab5
CONFIG_IDF_TARGET="esp32c6"

# Transport: SDIO
CONFIG_ESP_HOSTED_TRANSPORT_SDIO=y

# SDIO Configuration
CONFIG_ESP_SDIO_CHECKSUM=y

# Pin Configuration for M5Stack Tab5
# These map to the ESP32-P4 host pins:
# P4 G11 -> C6 SDIO_D0
# P4 G10 -> C6 SDIO_D1
# P4 G9  -> C6 SDIO_D2
# P4 G8  -> C6 SDIO_D3
# P4 G13 -> C6 SDIO_CMD
# P4 G12 -> C6 SDIO_CLK
# Note: ESP32-C6 slave pins are typically fixed for SDIO

# Performance settings
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=64
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=64
CONFIG_ESP_WIFI_AMPDU_TX_ENABLED=y
CONFIG_ESP_WIFI_TX_BA_WIN=32
CONFIG_ESP_WIFI_AMPDU_RX_ENABLED=y
CONFIG_ESP_WIFI_RX_BA_WIN=32

# Enable console output for debugging
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
EOF

echo "Building firmware..."
idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo ""
    echo "Firmware binaries are located at:"
    echo "  build/bootloader/bootloader.bin"
    echo "  build/partition_table/partition-table.bin"
    echo "  build/network_adapter.bin"
    echo ""
    echo "To flash the ESP32-C6:"
    echo "1. Connect to the ESP32-C6's UART/programming interface"
    echo "2. Run: idf.py -p /dev/YOUR_PORT flash"
    echo ""
    echo "Note: The M5Stack Tab5's ESP32-C6 may require special access"
    echo "for programming. Check M5Stack documentation for details."
else
    echo "Build failed! Please check the error messages above."
    exit 1
fi

echo ""
echo "==============================================="
echo "After flashing the C6 slave firmware, the ESP32-P4"
echo "host should be able to communicate via SDIO."
echo "==============================================="