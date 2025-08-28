/*
 * ESP32-C6 UART Bridge for M5Stack Tab5
 * Creates a UART bridge between USB and ESP32-C6 for firmware flashing
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_vfs_dev.h"
#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "sd_card_helper.h"

static const char *TAG = "C6_UART_BRIDGE";

// Pin definitions for Tab5 - C6 might be connected to UART1
#define C6_UART_NUM     UART_NUM_1
#define C6_TX_PIN       GPIO_NUM_6   // P4 TX to C6 RX (PC_TX)
#define C6_RX_PIN       GPIO_NUM_7   // P4 RX from C6 TX (PC_RX)
#define C6_RESET_GPIO   GPIO_NUM_15  // Reset control
#define C6_IO2_GPIO     GPIO_NUM_14  // Boot mode control

#define BUF_SIZE 1024

static uint8_t usb_to_uart_buf[BUF_SIZE];
static uint8_t uart_to_usb_buf[BUF_SIZE];

// Configure UART for C6 communication
static void configure_c6_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(C6_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(C6_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(C6_UART_NUM, C6_TX_PIN, C6_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "UART%d configured: TX=%d, RX=%d, Baud=115200", C6_UART_NUM, C6_TX_PIN, C6_RX_PIN);
}

// Put C6 into download mode
static void c6_enter_download_mode(void)
{
    ESP_LOGI(TAG, "Putting ESP32-C6 into download mode...");
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << C6_RESET_GPIO) | (1ULL << C6_IO2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Set IO2 low for download mode
    gpio_set_level(C6_IO2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset sequence
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "ESP32-C6 should be in download mode");
}

// Reset C6 to normal mode
static void c6_normal_mode(void)
{
    ESP_LOGI(TAG, "Resetting ESP32-C6 to normal mode...");
    
    // Set IO2 high for normal boot
    gpio_set_level(C6_IO2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    
    // Release IO2
    gpio_set_direction(C6_IO2_GPIO, GPIO_MODE_INPUT);
    
    ESP_LOGI(TAG, "ESP32-C6 reset to normal mode");
}

// Bridge task: USB -> UART
static void usb_to_uart_task(void *arg)
{
    while (1) {
        int len = usb_serial_jtag_read_bytes(usb_to_uart_buf, BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (len > 0) {
            uart_write_bytes(C6_UART_NUM, (const char *)usb_to_uart_buf, len);
        }
    }
}

// Bridge task: UART -> USB
static void uart_to_usb_task(void *arg)
{
    while (1) {
        int len = uart_read_bytes(C6_UART_NUM, uart_to_usb_buf, BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (len > 0) {
            usb_serial_jtag_write_bytes((const char *)uart_to_usb_buf, len, 100 / portTICK_PERIOD_MS);
        }
    }
}

// Main UART bridge function
void start_c6_uart_bridge(void)
{
    ESP_LOGI(TAG, "Starting ESP32-C6 UART Bridge");
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "This creates a bridge between USB and C6 UART");
    ESP_LOGI(TAG, "Use esptool.py on this port to flash the C6");
    ESP_LOGI(TAG, "=================================");
    
    // Configure USB serial
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .rx_buffer_size = BUF_SIZE,
        .tx_buffer_size = BUF_SIZE,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    
    // Uninstall VFS console to prevent interference
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
    
    // Configure UART
    configure_c6_uart();
    
    // Put C6 in download mode initially
    c6_enter_download_mode();
    
    // Small delay to ensure mode change
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create bridge tasks
    xTaskCreate(usb_to_uart_task, "usb_to_uart", 4096, NULL, 10, NULL);
    xTaskCreate(uart_to_usb_task, "uart_to_usb", 4096, NULL, 10, NULL);
    
    // Bridge is now active - no more logging
}

// Function to check if we should enter bridge mode
bool should_enter_bridge_mode(void)
{
    // Check for a specific boot flag or GPIO state
    // ENABLE FOR C6 FIRMWARE UPDATE - SET TO true
    
    // Check if backup firmware exists but C6 isn't responding to SDIO
    // This means we transferred firmware but it needs proper flashing
    struct stat file_stat;
    if (stat("/sdcard/c6_firmware_backup.bin", &file_stat) == 0) {
        ESP_LOGI(TAG, "C6 firmware backup found - entering bridge mode for proper flashing");
        ESP_LOGI(TAG, "After flashing, delete /sdcard/c6_firmware_backup.bin to exit bridge mode");
        return true;
    }
    
    return false;  // DISABLED - C6 firmware update via UART bridge
}

// Main entry point
void c6_uart_bridge_main(void)
{
    if (should_enter_bridge_mode()) {
        ESP_LOGI(TAG, "Entering C6 UART bridge mode");
        start_c6_uart_bridge();
        
        // Keep the main task alive
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}