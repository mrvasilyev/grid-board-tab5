#!/bin/bash

# Script to prepare SD card with C6 firmware for Tab5
# Usage: ./prepare_sd_card.sh <sd_card_mount_point>

echo "======================================="
echo "Tab5 C6 Firmware SD Card Preparation"
echo "======================================="
echo ""

if [ $# -eq 0 ]; then
    echo "Usage: $0 <sd_card_mount_point>"
    echo "Example: $0 /Volumes/SDCARD"
    exit 1
fi

SD_MOUNT=$1
FIRMWARE_SRC="/Users/a.vasilyev/Documents/my_code/IoT/m5stack/tab5/M5Tab5-UserDemo-fresh/platforms/tab5/wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin"
FIRMWARE_DST="$SD_MOUNT/c6_firmware.bin"

# Check if SD card is mounted
if [ ! -d "$SD_MOUNT" ]; then
    echo "Error: SD card mount point not found: $SD_MOUNT"
    exit 1
fi

# Check if source firmware exists
if [ ! -f "$FIRMWARE_SRC" ]; then
    echo "Error: Source firmware not found: $FIRMWARE_SRC"
    exit 1
fi

echo "SD Card: $SD_MOUNT"
echo "Source firmware: $FIRMWARE_SRC"
echo "Destination: $FIRMWARE_DST"
echo ""

# Copy firmware to SD card
echo "Copying C6 firmware to SD card..."
cp "$FIRMWARE_SRC" "$FIRMWARE_DST"

if [ $? -eq 0 ]; then
    echo "✅ Firmware copied successfully!"
    ls -lh "$FIRMWARE_DST"
    echo ""
    echo "Instructions:"
    echo "1. Safely eject the SD card"
    echo "2. Insert SD card into Tab5"
    echo "3. Power on Tab5 - it will automatically update C6 firmware"
    echo "4. After update, firmware will be renamed to c6_firmware_backup.bin"
else
    echo "❌ Failed to copy firmware"
    exit 1
fi