#!/bin/bash

echo "========================================"
echo "ESP32-C6 WiFi Firmware Flasher"
echo "for M5Stack Tab5"
echo "========================================"
echo ""
echo "IMPORTANT: Make sure the C6 flasher bridge is running on the ESP32-P4"
echo "The P4 should be showing 'Bridge active - Ready for flashing' in the serial output"
echo ""
echo "Press Enter to flash the C6 firmware, or Ctrl+C to cancel..."
read

C6_FW="/Users/a.vasilyev/Documents/my_code/IoT/m5stack/M5Tab5-UserDemo-fresh/platforms/tab5/wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin"

if [ ! -f "$C6_FW" ]; then
    echo "Error: Firmware file not found: $C6_FW"
    exit 1
fi

echo "Flashing ESP32-C6 with WiFi SDIO firmware..."
echo "Using firmware: $C6_FW"
echo ""

# Flash the C6 through the P4 bridge
esptool.py --chip esp32c6 \
    -p /dev/cu.usbmodem2101 \
    -b 115200 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_size 4MB \
    --flash_freq 40m \
    0x0 "$C6_FW"

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "SUCCESS! ESP32-C6 firmware flashed"
    echo "========================================"
    echo ""
    echo "Next steps:"
    echo "1. Reset the Tab5 device"
    echo "2. Flash the main Tab5 firmware back to the P4"
    echo "3. The WiFi should now work!"
else
    echo ""
    echo "ERROR: Failed to flash C6 firmware"
    echo "Make sure:"
    echo "1. The C6 flasher bridge is running on the P4"
    echo "2. The USB connection is stable"
    echo "3. Try reducing baud rate if needed"
fi