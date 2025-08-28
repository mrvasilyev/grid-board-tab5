/*
 * ESP32-C6 Flasher Firmware for M5Stack Tab5
 * 
 * This firmware turns the ESP32-P4 into a dedicated C6 flasher.
 * It provides UART bridge functionality to flash the C6 co-processor
 * with the ESP-Hosted slave firmware.
 * 
 * Tab5 Pin Configuration (different from Function-EV-Board):
 * - C6 UART: P4 GPIO6(TX) <-> C6 RX, P4 GPIO7(RX) <-> C6 TX
 * - C6 Reset: P4 GPIO15 -> C6 RST
 * - C6 Boot Mode: P4 GPIO14 -> C6 GPIO2 (boot mode control)
 * 
 * SDIO Pins (for reference when flashing main firmware later):
 * CLK=12, CMD=13, D0=11, D1=10, D2=9, D3=8, RST=15
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"

static const char *TAG = "C6_FLASHER";

// Tab5-specific pin definitions
#define C6_UART_NUM         UART_NUM_1
#define C6_TX_PIN           GPIO_NUM_6   // P4 TX to C6 RX
#define C6_RX_PIN           GPIO_NUM_7   // P4 RX from C6 TX
#define C6_RESET_GPIO       GPIO_NUM_15  // C6 Reset control
#define C6_IO2_GPIO         GPIO_NUM_14  // C6 GPIO2 (boot mode)

// Buffer sizes
#define UART_BUF_SIZE       4096
#define USB_BUF_SIZE        4096

// Bridge state
static bool bridge_active = false;
static bool c6_in_download_mode = false;

// Buffers
static uint8_t uart_rx_buf[UART_BUF_SIZE];
static uint8_t usb_rx_buf[USB_BUF_SIZE];

// Statistics
static uint32_t bytes_to_c6 = 0;
static uint32_t bytes_from_c6 = 0;

void configure_c6_control_pins(void)
{
    ESP_LOGI(TAG, "Configuring C6 control pins (Tab5 configuration)");
    
    // Configure reset pin
    gpio_config_t reset_conf = {
        .pin_bit_mask = (1ULL << C6_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&reset_conf));
    
    // Configure IO2 pin (boot mode control)
    gpio_config_t io2_conf = {
        .pin_bit_mask = (1ULL << C6_IO2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Pulled down for download mode
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io2_conf));
    
    // Initial states
    gpio_set_level(C6_RESET_GPIO, 1);  // Not in reset
    gpio_set_level(C6_IO2_GPIO, 1);    // Normal boot mode initially
    
    ESP_LOGI(TAG, "C6 control pins configured: RST=GPIO%d, IO2=GPIO%d", C6_RESET_GPIO, C6_IO2_GPIO);
}

void configure_c6_uart(void)
{
    ESP_LOGI(TAG, "Configuring UART for C6 communication");
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(C6_UART_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(C6_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(C6_UART_NUM, C6_TX_PIN, C6_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "C6 UART configured: TX=GPIO%d, RX=GPIO%d, Baud=115200", C6_TX_PIN, C6_RX_PIN);
}

void configure_usb_serial(void)
{
    ESP_LOGI(TAG, "Configuring USB serial interface");
    
    usb_serial_jtag_driver_config_t usb_config = {
        .rx_buffer_size = USB_BUF_SIZE,
        .tx_buffer_size = USB_BUF_SIZE,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_config));
    
    ESP_LOGI(TAG, "USB serial configured");
}

void put_c6_in_download_mode(void)
{
    ESP_LOGI(TAG, "Putting ESP32-C6 into download mode...");
    
    // Set IO2 low for download mode (UART boot)
    gpio_set_level(C6_IO2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset sequence: Low -> High
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    c6_in_download_mode = true;
    ESP_LOGI(TAG, "ESP32-C6 should now be in download mode (UART)");
}

void put_c6_in_normal_mode(void)
{
    ESP_LOGI(TAG, "Putting ESP32-C6 into normal boot mode...");
    
    // Set IO2 high for normal SPI boot
    gpio_set_level(C6_IO2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset sequence
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Float IO2
    gpio_set_direction(C6_IO2_GPIO, GPIO_MODE_INPUT);
    
    c6_in_download_mode = false;
    ESP_LOGI(TAG, "ESP32-C6 reset to normal boot mode");
}

// USB to UART bridge task
void usb_to_uart_bridge_task(void *param)
{
    ESP_LOGI(TAG, "USB->UART bridge task started");
    
    while (1) {
        int len = usb_serial_jtag_read_bytes(usb_rx_buf, USB_BUF_SIZE, 10 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            // Forward data to C6 UART
            uart_write_bytes(C6_UART_NUM, (const char*)usb_rx_buf, len);
            bytes_to_c6 += len;
            
            // Auto-detect esptool sync sequence
            if (len >= 36 && usb_rx_buf[0] == 0xC0 && usb_rx_buf[1] == 0x00) {
                if (!c6_in_download_mode) {
                    ESP_LOGI(TAG, "Detected esptool sync - entering download mode");
                    put_c6_in_download_mode();
                }
            }
            
            // Check for manual commands
            if (len > 0 && usb_rx_buf[0] == '\n') {
                // Command processing would go here
            }
        }
        
        taskYIELD();
    }
}

// UART to USB bridge task
void uart_to_usb_bridge_task(void *param)
{
    ESP_LOGI(TAG, "UART->USB bridge task started");
    
    while (1) {
        int len = uart_read_bytes(C6_UART_NUM, uart_rx_buf, UART_BUF_SIZE, 10 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            // Forward data from C6 to USB
            usb_serial_jtag_write_bytes((const char*)uart_rx_buf, len, 100 / portTICK_PERIOD_MS);
            bytes_from_c6 += len;
        }
        
        taskYIELD();
    }
}

// Status reporting task
void status_task(void *param)
{
    uint32_t counter = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        counter++;
        
        ESP_LOGI(TAG, "Bridge Status [%lu]: %s | C6: %s | Bytes: TX:%lu RX:%lu", 
                 counter,
                 bridge_active ? "ACTIVE" : "INACTIVE",
                 c6_in_download_mode ? "DOWNLOAD" : "NORMAL",
                 bytes_to_c6, bytes_from_c6);
        
        if (counter % 12 == 0) {  // Every minute
            ESP_LOGI(TAG, "=== READY FOR C6 FLASHING ===");
            ESP_LOGI(TAG, "Use: esptool.py --chip esp32c6 -p /dev/cu.usbmodem2101 write_flash 0x0 firmware.bin");
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "ESP32-C6 Flasher Firmware for M5Stack Tab5");
    ESP_LOGI(TAG, "Version: 1.0 - Tab5 Pin Configuration");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "This firmware creates a UART bridge to flash ESP32-C6");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Tab5 Hardware Configuration:");
    ESP_LOGI(TAG, "  C6 UART: P4_TX(GPIO%d) -> C6_RX, P4_RX(GPIO%d) <- C6_TX", C6_TX_PIN, C6_RX_PIN);
    ESP_LOGI(TAG, "  C6 Reset: P4_GPIO%d -> C6_RST", C6_RESET_GPIO);
    ESP_LOGI(TAG, "  C6 Boot:  P4_GPIO%d -> C6_GPIO2", C6_IO2_GPIO);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SDIO Pins (for main firmware):");
    ESP_LOGI(TAG, "  CLK=12, CMD=13, D0=11, D1=10, D2=9, D3=8, RST=15");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Usage:");
    ESP_LOGI(TAG, "1. This P4 acts as USB-to-UART bridge for C6");
    ESP_LOGI(TAG, "2. Flash C6 with: esptool.py --chip esp32c6 -p /dev/cu.usbmodem2101 \\");
    ESP_LOGI(TAG, "     write_flash 0x0 ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin");
    ESP_LOGI(TAG, "3. After C6 flash, reflash P4 with main Tab5 firmware");
    ESP_LOGI(TAG, "========================================================");
    
    // Initialize hardware
    configure_c6_control_pins();
    configure_c6_uart();
    configure_usb_serial();
    
    // Put C6 in download mode immediately
    put_c6_in_download_mode();
    
    // Start bridge tasks
    xTaskCreate(usb_to_uart_bridge_task, "usb_to_uart", 4096, NULL, 10, NULL);
    xTaskCreate(uart_to_usb_bridge_task, "uart_to_usb", 4096, NULL, 10, NULL);
    xTaskCreate(status_task, "status", 2048, NULL, 5, NULL);
    
    bridge_active = true;
    
    ESP_LOGI(TAG, "C6 Flasher firmware ready!");
    ESP_LOGI(TAG, "Bridge tasks started - ready for esptool connection");
    
    // Main loop - keep alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // Could add commands here like:
        // - Toggle C6 boot mode
        // - Show bridge statistics  
        // - Reset C6
    }
}