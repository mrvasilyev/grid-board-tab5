# Grid Board Tab5 - Interactive Touch Display for M5Stack Tab5

An interactive touch-enabled grid display application for the M5Stack Tab5 ESP32-P4 development board. Features animated text messages with emoji support, creating an engaging visual experience on the 5-inch display.

![Grid Board Tab5](docs/images/grid_board_demo.jpg)

## Features

- **Interactive Touch Grid**: 5x9 grid of touch-responsive cells
- **Animated Messages**: Smooth text animations with customizable messages
- **Emoji Support**: Full Unicode emoji rendering
- **Landscape Orientation**: Optimized for horizontal viewing
- **ESP32-C6 Integration**: Optional Wi-Fi module support via SDIO
- **Customizable Messages**: Easy to modify display messages

## Hardware Requirements

- **M5Stack Tab5** development board
  - ESP32-P4 main processor (RISC-V 32-bit dual-core 400MHz)
  - 5" IPS TFT Display (1280×720) via MIPI-DSI
  - GT911 multi-touch controller
  - 16MB Flash + 32MB PSRAM
  - ESP32-C6 wireless module (optional)

## Software Requirements

- **ESP-IDF v5.4+** (ESP-IDF v5.5 recommended)
- **Python 3.8+**
- **esptool.py v4.10+**

## Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/grid-board-tab5.git
cd grid-board-tab5
```

### 2. Set Up ESP-IDF Environment

```bash
# Source ESP-IDF environment
source /path/to/esp-idf/export.sh

# Or on macOS with default installation
source ~/esp/esp-idf/export.sh
```

### 3. Build the Project

```bash
idf.py build
```

### 4. Flash to Device

```bash
# Flash all components
idf.py -p /dev/cu.usbmodem212301 flash

# Or flash with specific parameters
python -m esptool --chip esp32p4 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x2000 build/bootloader/bootloader.bin \
  0x10000 build/partition_table/partition-table.bin \
  0x20000 build/grid_board_tab5.bin
```

### 5. Monitor Output

```bash
idf.py -p /dev/cu.usbmodem212301 monitor

# Or use screen
screen /dev/cu.usbmodem212301 115200
```

Press `Ctrl+]` to exit the monitor.

## Customizing Messages

Edit the messages array in `main/main_simple.cpp`:

```cpp
const char* messages[] = {
    "HELLO WORLD! 👋🌍",
    "M5STACK TAB5 🚀✨",
    "TOUCH ME! 👆😊",
    "ESP32-P4 POWER! 💪🔥",
    "FAMILY TOGETHER ❤😊❤"
};
```

Messages cycle every 30 seconds automatically.

## Project Structure

```
grid-board-tab5/
├── main/
│   ├── main_simple.cpp          # Main application entry
│   ├── grid_board.cpp           # Grid board UI implementation
│   ├── grid_board.h             # Grid board header
│   └── CMakeLists.txt           # Build configuration
├── components/
│   ├── m5stack_tab5/            # BSP for M5Stack Tab5
│   └── imlib/                   # Image and font libraries
├── build/                        # Build artifacts (generated)
├── partitions.csv               # Partition table configuration
├── sdkconfig                    # Project configuration
├── CMakeLists.txt              # Root build configuration
└── README.md                   # This file
```

## Display Features

### Grid System
- **Grid Size**: 5 rows × 9 columns (45 cells total)
- **Cell States**: Empty, filled, or animated
- **Touch Response**: Visual feedback on touch events
- **Animation**: Smooth transitions between states

### Message Display
- **Text Processing**: Automatic character-to-grid mapping
- **Emoji Rendering**: Full Unicode support
- **Animation Speed**: Configurable delay between characters
- **Cycle Time**: 30-second intervals between messages

## Advanced Features

### ESP32-C6 Wi-Fi Module
The project includes support for the ESP32-C6 wireless module:

```cpp
// Initialize C6 communication
esp_err_t ret = tab5_c6_system_init(false);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "C6 communication initialized");
}
```

### Touch Calibration
Touch events are processed through the GT911 controller:

```cpp
// Touch callback in grid_board.cpp
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    // Touch processing logic
}
```

## Troubleshooting

### Common Issues

1. **Partition Table MD5 Error**
   ```
   E (1115) partition: No MD5 found in partition table
   ```
   Solution: Use the provided partition table or disable MD5 check in menuconfig.

2. **Display Not Initializing**
   - Ensure proper connection to the device
   - Check if bootloader is flashed correctly
   - Verify flash addresses match the partition table

3. **Touch Not Working**
   - Check I2C connections (GPIO 31/32)
   - Verify GT911 initialization in logs
   - Ensure display backlight is enabled

### Debug Output

Enable verbose logging:
```bash
idf.py -p /dev/cu.usbmodem212301 monitor -b 115200
```

Check for initialization messages:
```
I (1109) GridBoard_Tab5: Grid Board Demo for M5Stack Tab5 starting...
I (1144) GridBoard_Tab5: Initializing display with landscape orientation
I (1168) GridBoard_Tab5: Starting LVGL task
I (1186) GridBoard_Tab5: Grid board initialized successfully!
```

## Performance Specifications

- **Display Refresh Rate**: 60 Hz
- **Touch Sampling Rate**: 100 Hz
- **Message Cycle Time**: 30 seconds
- **Animation Frame Rate**: 100 fps (10ms per frame)
- **Memory Usage**: ~200KB RAM + PSRAM

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests and verify functionality
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **M5Stack** for the Tab5 hardware platform
- **Espressif** for ESP-IDF framework
- **LVGL** for the graphics library
- **Community** for testing and feedback

## Contact

- **Author**: Alexander Vasilyev
- **GitHub**: [@mrvasilyev](https://github.com/mrvasilyev)
- **Project**: [Grid Board Tab5](https://github.com/yourusername/grid-board-tab5)

## Version History

- **v1.0.0** (2025-08-28)
  - Initial release
  - Full touch grid implementation
  - Emoji support
  - Message cycling
  - ESP32-C6 integration

## Demo Video

[Watch Demo Video](https://youtube.com/watch?v=demo_link)

---

Made with ❤️ for the M5Stack community