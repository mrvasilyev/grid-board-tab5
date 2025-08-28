# ESP32-C6 WiFi Firmware Flashing Guide for M5Stack Tab5

## Problem
The M5Stack Tab5's ESP32-C6 co-processor needs ESP-Hosted slave firmware to enable WiFi functionality. Without this firmware, the ESP32-P4 cannot communicate with the C6 via SDIO, resulting in error 0x107.

## Solution Overview
We've created a UART bridge program that runs on the ESP32-P4, allowing you to flash the ESP32-C6 through the P4's USB connection.

## Files Created
1. **flash_c6_firmware.c** - UART bridge program for P4
2. **test_flash_c6/** - Project to build the flasher
3. **flash_c6.sh** - Script to flash C6 firmware
4. **wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin** - C6 firmware

## Step-by-Step Instructions

### Step 1: Build and Flash the Bridge to P4
```bash
cd test_flash_c6
source /Users/a.vasilyev/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash
```

### Step 2: Verify Bridge is Running
After flashing, the P4 will display:
```
ESP32-C6 Firmware Flasher for M5Stack Tab5
Bridge active - Ready for flashing
```

### Step 3: Flash C6 Firmware
In a new terminal:
```bash
cd ..
./flash_c6.sh
```

Or manually:
```bash
esptool.py --chip esp32c6 \
    -p /dev/cu.usbmodem2101 \
    -b 115200 \
    write_flash 0x0 wifi_c6_fw/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin
```

### Step 4: Restore Main Firmware
After successfully flashing the C6:
```bash
cd /Users/a.vasilyev/Documents/my_code/IoT/m5stack/M5Tab5-UserDemo-fresh/platforms/tab5
idf.py -p /dev/cu.usbmodem2101 flash
```

## Technical Details

### Pin Connections
- **UART**: P4 GPIO6/7 ↔ C6 UART
- **Reset**: P4 GPIO15 → C6 Reset
- **Boot Mode**: P4 GPIO14 → C6 GPIO2
- **SDIO**: P4 GPIO8-13 ↔ C6 SDIO

### C6 Boot Modes
- **GPIO2=0**: Download mode (UART flashing)
- **GPIO2=1**: Normal SPI boot

### Bridge Commands
While the bridge is running, you can type:
- `download` - Enter download mode
- `normal` - Enter normal mode  
- `info` - Show status

## Troubleshooting

1. **esptool sync fails**: 
   - Bridge auto-detects esptool sync pattern
   - Try manual `download` command first

2. **Flashing fails**:
   - Reduce baud rate to 115200
   - Check USB cable quality
   - Ensure stable power supply

3. **WiFi still doesn't work**:
   - Verify C6 firmware version matches your hardware
   - Check SDIO pin connections
   - Ensure proper pull-up resistors on SDIO lines

## Alternative Methods

If the UART bridge doesn't work, consider:
1. **Direct UART access**: If C6 UART pins are exposed
2. **USB flashing**: If C6 USB is connected
3. **M5Stack factory tool**: Official flashing utility
4. **JTAG**: If debug interface is available

## Verification
After successful flashing and restoring main firmware:
```
I (xxxx) wifi_remote: SDIO slave init done
I (xxxx) wifi: WiFi initialized successfully
```

## Notes
- The C6 firmware is a combined image flashed at offset 0x0
- Includes bootloader, partition table, and application
- SDIO configuration must match between host and slave
- Power cycle may be required after flashing