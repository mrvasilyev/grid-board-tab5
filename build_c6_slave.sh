#!/bin/bash

# ESP32-C6 Slave Firmware Builder for M5Stack Tab5
# Builds ESP-Hosted slave firmware with SDIO support

set -e

echo "==========================================="
echo "ESP32-C6 ESP-Hosted Slave Firmware Builder"
echo "for M5Stack Tab5"
echo "==========================================="

# Source ESP-IDF environment
echo "Sourcing ESP-IDF environment..."
source /Users/a.vasilyev/esp/esp-idf/export.sh

# Create a temporary directory for building
BUILD_DIR="/tmp/esp32c6_slave_build"
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR

echo ""
echo "Creating slave project from ESP-Hosted example..."
idf.py create-project-from-example "espressif/esp_hosted:slave"
cd slave

echo ""
echo "Setting target to ESP32-C6..."
idf.py set-target esp32c6

echo ""
echo "Creating sdkconfig with SDIO configuration..."
cat > sdkconfig.defaults << 'EOF'
# ESP32-C6 Slave configuration for M5Stack Tab5
CONFIG_IDF_TARGET="esp32c6"

# Transport: SDIO
CONFIG_ESP_HOSTED_TRANSPORT_SDIO=y

# SDIO Configuration
CONFIG_ESP_SDIO_CHECKSUM=y
CONFIG_ESP_SDIO_TX_BUFFER_NUM=20
CONFIG_ESP_SDIO_RX_BUFFER_NUM=20

# Reset pin configuration (if self-reset is needed)
# CONFIG_ESP_SDIO_RESET_SUPPORTED=y
# CONFIG_ESP_GPIO_TO_RESET_SLAVE=-1

# Performance settings
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=64
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=64
CONFIG_ESP_WIFI_AMPDU_TX_ENABLED=y
CONFIG_ESP_WIFI_TX_BA_WIN=32
CONFIG_ESP_WIFI_AMPDU_RX_ENABLED=y
CONFIG_ESP_WIFI_RX_BA_WIN=32

# Console output for debugging
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200
CONFIG_ESP_CONSOLE_UART_NUM=0

# Optimization
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT=y

# ESP32-C6 specific
CONFIG_ESP32C6_REV_MIN_0=y
CONFIG_ESP32C6_REV_MAX_99=y
EOF

echo ""
echo "Building slave firmware..."
idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================="
    echo "Build successful!"
    echo "========================================="
    
    # Copy firmware to Tab5 project directory
    FW_DIR="/Users/a.vasilyev/Documents/my_code/IoT/m5stack/M5Tab5-UserDemo-fresh/platforms/tab5/c6_slave_firmware"
    mkdir -p $FW_DIR
    
    cp build/bootloader/bootloader.bin $FW_DIR/
    cp build/partition_table/partition-table.bin $FW_DIR/
    cp build/slave.bin $FW_DIR/network_adapter.bin
    
    # Create combined firmware at offset 0x0
    echo "Creating combined firmware image..."
    cd $FW_DIR
    
    # Use esptool to merge binaries
    esptool.py --chip esp32c6 merge_bin \
        -o esp32c6_slave_sdio_combined.bin \
        --flash_mode dio \
        --flash_size 4MB \
        0x0 bootloader.bin \
        0x8000 partition-table.bin \
        0x10000 network_adapter.bin
    
    echo ""
    echo "Firmware files saved to: $FW_DIR"
    echo ""
    echo "Files created:"
    echo "  - bootloader.bin (flash at 0x0)"
    echo "  - partition-table.bin (flash at 0x8000)"
    echo "  - network_adapter.bin (flash at 0x10000)"
    echo "  - esp32c6_slave_sdio_combined.bin (combined, flash at 0x0)"
    echo ""
    echo "========================================="
    echo "Next steps:"
    echo "========================================="
    echo "1. Flash the ESP32-C6 with one of these methods:"
    echo ""
    echo "Method A - If C6 UART is accessible:"
    echo "  esptool.py --chip esp32c6 -p /dev/YOUR_PORT write_flash 0x0 $FW_DIR/esp32c6_slave_sdio_combined.bin"
    echo ""
    echo "Method B - Individual files:"
    echo "  esptool.py --chip esp32c6 -p /dev/YOUR_PORT write_flash \\"
    echo "    0x0 $FW_DIR/bootloader.bin \\"
    echo "    0x8000 $FW_DIR/partition-table.bin \\"
    echo "    0x10000 $FW_DIR/network_adapter.bin"
    echo ""
    echo "2. After flashing C6, the ESP32-P4 should be able to connect via SDIO"
    echo "========================================="
else
    echo "Build failed! Check the error messages above."
    exit 1
fi