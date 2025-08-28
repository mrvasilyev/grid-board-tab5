/*
 * ESP32-C6 Firmware Flasher for M5Stack Tab5
 * This program creates a UART bridge to flash the C6 co-processor
 * 
 * Usage:
 * 1. Build and flash this to ESP32-P4
 * 2. Use esptool to flash C6 through the bridge
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"

static const char *TAG = "C6_FLASHER";

// Pin definitions for Tab5
#define C6_UART_NUM     UART_NUM_1
#define C6_TX_PIN       GPIO_NUM_6   // P4 TX to C6 RX
#define C6_RX_PIN       GPIO_NUM_7   // P4 RX from C6 TX
#define C6_RESET_GPIO   GPIO_NUM_15  // Reset control
#define C6_IO2_GPIO     GPIO_NUM_14  // Boot mode control (GPIO2)

#define BUF_SIZE 4096
#define UART_BAUD 115200

static uint8_t usb_rx_buf[BUF_SIZE];
static uint8_t uart_rx_buf[BUF_SIZE];
static bool download_mode_active = false;

// Configure GPIO for C6 control
static void configure_control_pins(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << C6_RESET_GPIO) | (1ULL << C6_IO2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Set initial states
    gpio_set_level(C6_RESET_GPIO, 1);  // Not in reset
    gpio_set_level(C6_IO2_GPIO, 1);    // Normal boot mode
}

// Put C6 into download mode
static void enter_download_mode(void)
{
    ESP_LOGI(TAG, "Entering ESP32-C6 download mode...");
    
    // Set IO2 (GPIO2) low for download mode
    gpio_set_level(C6_IO2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset sequence
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    download_mode_active = true;
    ESP_LOGI(TAG, "ESP32-C6 in download mode");
}

// Reset C6 to normal mode
static void exit_download_mode(void)
{
    ESP_LOGI(TAG, "Resetting ESP32-C6 to normal mode...");
    
    // Set IO2 high for normal boot
    gpio_set_level(C6_IO2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    
    download_mode_active = false;
    ESP_LOGI(TAG, "ESP32-C6 in normal mode");
}

// Configure UART for C6 communication
static void configure_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(C6_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(C6_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(C6_UART_NUM, C6_TX_PIN, C6_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "UART%d configured: TX=GPIO%d, RX=GPIO%d, Baud=%d", 
             C6_UART_NUM, C6_TX_PIN, C6_RX_PIN, UART_BAUD);
}

// Configure USB serial
static void configure_usb_serial(void)
{
    usb_serial_jtag_driver_config_t usb_serial_config = {
        .rx_buffer_size = BUF_SIZE,
        .tx_buffer_size = BUF_SIZE,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_config));
}

// USB to UART bridge task
static void usb_to_uart_task(void *arg)
{
    while (1) {
        int len = usb_serial_jtag_read_bytes(usb_rx_buf, BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (len > 0) {
            // Forward to UART
            uart_write_bytes(C6_UART_NUM, (const char *)usb_rx_buf, len);
            
            // Auto-detect esptool sync pattern
            if (len >= 36 && memcmp(usb_rx_buf, "\xc0\x00", 2) == 0) {
                if (!download_mode_active) {
                    ESP_LOGI(TAG, "Detected esptool sync, entering download mode");
                    enter_download_mode();
                }
            }
        }
        taskYIELD();
    }
}

// UART to USB bridge task
static void uart_to_usb_task(void *arg)
{
    while (1) {
        int len = uart_read_bytes(C6_UART_NUM, uart_rx_buf, BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (len > 0) {
            usb_serial_jtag_write_bytes((const char *)uart_rx_buf, len, 100 / portTICK_PERIOD_MS);
        }
        taskYIELD();
    }
}

// Command handler task
static void command_handler_task(void *arg)
{
    char cmd_buf[64];
    int cmd_idx = 0;
    
    while (1) {
        uint8_t byte;
        int len = usb_serial_jtag_read_bytes(&byte, 1, portMAX_DELAY);
        
        if (len > 0) {
            if (byte == '\n' || byte == '\r') {
                if (cmd_idx > 0) {
                    cmd_buf[cmd_idx] = '\0';
                    
                    // Process command
                    if (strcmp(cmd_buf, "download") == 0) {
                        enter_download_mode();
                        ESP_LOGI(TAG, "Manual download mode activated");
                    } else if (strcmp(cmd_buf, "normal") == 0) {
                        exit_download_mode();
                        ESP_LOGI(TAG, "Manual normal mode activated");
                    } else if (strcmp(cmd_buf, "info") == 0) {
                        ESP_LOGI(TAG, "C6 Flasher Status:");
                        ESP_LOGI(TAG, "  Mode: %s", download_mode_active ? "Download" : "Normal");
                        ESP_LOGI(TAG, "  UART: %d baud", UART_BAUD);
                    }
                    
                    cmd_idx = 0;
                }
            } else if (cmd_idx < sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_idx++] = byte;
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "ESP32-C6 Firmware Flasher for M5Stack Tab5");
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "This creates a UART bridge to flash the C6");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Instructions:");
    ESP_LOGI(TAG, "1. This ESP32-P4 acts as a bridge");
    ESP_LOGI(TAG, "2. Use esptool on your computer:");
    ESP_LOGI(TAG, "   esptool.py --chip esp32c6 -p /dev/cu.usbmodem2101 \\");
    ESP_LOGI(TAG, "     write_flash 0x0 ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Commands (type and press Enter):");
    ESP_LOGI(TAG, "  download - Enter download mode");
    ESP_LOGI(TAG, "  normal   - Enter normal mode");
    ESP_LOGI(TAG, "  info     - Show status");
    ESP_LOGI(TAG, "=================================================");
    
    // Configure hardware
    configure_control_pins();
    configure_uart();
    configure_usb_serial();
    
    // Start in download mode
    enter_download_mode();
    
    // Create bridge tasks
    xTaskCreate(usb_to_uart_task, "usb_to_uart", 4096, NULL, 10, NULL);
    xTaskCreate(uart_to_usb_task, "uart_to_usb", 4096, NULL, 10, NULL);
    xTaskCreate(command_handler_task, "cmd_handler", 2048, NULL, 5, NULL);
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (download_mode_active) {
            ESP_LOGI(TAG, "Bridge active - Ready for flashing");
        } else {
            ESP_LOGI(TAG, "Bridge active - Normal mode");
        }
    }
}