# ESP32-P4 ↔ ESP32-C6 SDIO Communication System

## Overview
This system enables communication between the ESP32-P4 (main processor) and ESP32-C6 (Wi-Fi/BT co-processor) on the M5Stack Tab5 using SDIO interface. Compatible with [ESP-Hosted MCU](https://github.com/espressif/esp-hosted-mcu) framework.

## Features
- **SDIO Communication**: 4-bit SDIO interface at up to 40MHz
- **UART Bridge Mode**: Firmware update capability for ESP32-C6
- **Queue-based Architecture**: Thread-safe message passing
- **Status Monitoring**: Real-time C6 status and connection tracking

## Pin Connections (Tab5)
| ESP32-P4 Pin | Function | ESP32-C6 Pin |
|--------------|----------|--------------|
| GPIO 12 | SDIO CLK | CLK |
| GPIO 13 | SDIO CMD | CMD |
| GPIO 11 | SDIO D0 | D0 |
| GPIO 10 | SDIO D1 | D1 |
| GPIO 9 | SDIO D2 | D2 |
| GPIO 8 | SDIO D3 | D3 |
| GPIO 15 | RESET | RESET |
| GPIO 14 | BOOT (IO2) | GPIO2 |
| GPIO 6 | UART TX | RX |
| GPIO 7 | UART RX | TX |

## File Structure
```
main/
├── sdio_communication.h/c   # SDIO host driver
├── c6_uart_bridge.c         # UART bridge for firmware updates  
├── tab5_c6_integration.c    # Integration and mode switching
└── main_simple.cpp          # Main application with C6 init
```

## Usage

### Normal Operation (SDIO Mode)
The system automatically attempts SDIO communication on boot:
```c
// In app_main()
ret = tab5_c6_system_init(false);  // false = SDIO mode
if (ret == ESP_OK) {
    // C6 ready for communication
    tab5_c6_demo();  // Run demo
}
```

### Firmware Update (UART Bridge Mode)
To flash new firmware to ESP32-C6:

1. **Enable bridge mode** in `c6_uart_bridge.c`:
```c
bool should_enter_bridge_mode(void) {
    return true;  // Enable bridge mode
}
```

2. **Build and flash P4 firmware**:
```bash
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash
```

3. **Flash C6 firmware** via bridge:
```bash
./flash_c6_firmware.sh esp32c6_sdio_slave.bin /dev/cu.usbmodem1101
```

4. **Restore normal mode** and rebuild:
```c
bool should_enter_bridge_mode(void) {
    return false;  // Back to SDIO mode
}
```

## API Functions

### Initialization
```c
esp_err_t tab5_c6_system_init(bool force_bridge_mode);
```

### Communication
```c
esp_err_t tab5_c6_send_message(const char *message);
esp_err_t tab5_sdio_send(tab5_sdio_handle_t *handle, const sdio_packet_t *packet);
esp_err_t tab5_sdio_receive(tab5_sdio_handle_t *handle, sdio_packet_t *packet, uint32_t timeout_ms);
```

### Status
```c
bool tab5_c6_is_ready(tab5_sdio_handle_t *handle);
esp_err_t tab5_c6_read_status(tab5_sdio_handle_t *handle, uint8_t *status);
```

### WiFi Control
```c
esp_err_t tab5_c6_wifi_connect(tab5_sdio_handle_t *handle, const char *ssid, const char *password);
```

## Troubleshooting

### SDIO Initialization Fails
- **Error**: `sdmmc_init_ocr: send_op_cond (1) returned 0x107`
- **Cause**: C6 doesn't have SDIO slave firmware
- **Solution**: Flash C6 with SDIO slave firmware using bridge mode

### No Response from C6
- Check GPIO 15 (RESET) and GPIO 14 (BOOT) connections
- Verify C6 has proper power supply
- Try manual reset sequence

### Bridge Mode Issues
- Ensure UART pins (GPIO 6/7) are connected properly
- Check baud rate matches (115200 default)
- Verify esptool.py is installed

## Development Notes
- SDIO max frequency: 20MHz (conservative), can increase to 40MHz
- Buffer size: 2048 bytes per packet
- Queue depth: 10 packets
- C6 ready signature: 0xAA

## Future Enhancements
- [ ] Interrupt-driven SDIO transfers
- [ ] DMA support for large transfers
- [ ] Automatic baud rate detection for bridge
- [ ] OTA update support for C6
- [ ] Power management integration