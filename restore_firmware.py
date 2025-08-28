#!/usr/bin/env python3
"""
Restore C6 firmware from backup for manual flashing
"""

import shutil
import os

# Check if backup exists
if os.path.exists("main/c6_firmware.bin"):
    print("✅ C6 firmware already exists")
else:
    print("❌ C6 firmware not found in main/")
    print("Please ensure c6_firmware.bin is in the main/ directory")

print("\nTo flash C6 manually:")
print("1. Connect USB-to-UART adapter to C6 (TX to RX, RX to TX)")
print("2. Hold GPIO9 low and reset C6 to enter bootloader mode")
print("3. Run: esptool.py --chip esp32c6 -p /dev/cu.usbserial-xxx -b 460800 write_flash 0x0 main/c6_firmware.bin")
print("\nOr use the official firmware from M5Stack:")
print("Path: /Users/a.vasilyev/Documents/my_code/IoT/m5stack/tab5/M5Tab5-UserDemo-fresh/platforms/tab5/wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin")