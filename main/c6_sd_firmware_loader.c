/**
 * @file c6_sd_firmware_loader.c
 * @brief SD Card-based firmware loader for ESP32-C6 on M5Stack Tab5
 * 
 * This module loads C6 firmware from SD card and flashes it via UART
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_card_helper.h"

static const char *TAG = "C6_SD_LOADER";

// SD Card paths
#define C6_FIRMWARE_FILENAME "c6_firmware.bin"
#define C6_FIRMWARE_BACKUP_FILENAME "c6_firmware_backup.bin"

// C6 UART pins (match Tab5 C6 configuration)
#define C6_UART_NUM     UART_NUM_1
#define C6_TX_PIN       GPIO_NUM_6   // P4 TX to C6 RX
#define C6_RX_PIN       GPIO_NUM_7   // P4 RX from C6 TX
#define C6_RESET_GPIO   GPIO_NUM_15  // Reset control
#define C6_BOOT_GPIO    GPIO_NUM_14  // Boot mode control (C6 GPIO9)

// Firmware update parameters
#define CHUNK_SIZE      1024
#define SYNC_TIMEOUT_MS 5000
#define FLASH_TIMEOUT_MS 30000

// ESP32-C6 ROM bootloader protocol constants
#define SLIP_END        0xC0
#define SLIP_ESC        0xDB
#define SLIP_ESC_END    0xDC
#define SLIP_ESC_ESC    0xDD

#define ESP_SYNC        0x08
#define ESP_FLASH_BEGIN 0x02
#define ESP_FLASH_DATA  0x03
#define ESP_FLASH_END   0x04


/**
 * Configure C6 UART for firmware upload
 */
static esp_err_t configure_c6_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(C6_UART_NUM, 4096, 4096, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(C6_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(C6_UART_NUM, C6_TX_PIN, C6_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "C6 UART configured for firmware upload");
    return ESP_OK;
}

/**
 * Put C6 into bootloader mode
 */
static void c6_enter_bootloader_mode(void)
{
    ESP_LOGI(TAG, "Putting C6 into bootloader mode");
    
    // Configure control pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << C6_RESET_GPIO) | (1ULL << C6_BOOT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Ensure normal state first
    gpio_set_level(C6_RESET_GPIO, 1);
    gpio_set_level(C6_BOOT_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Enter download mode: Hold BOOT low, pulse RESET
    gpio_set_level(C6_BOOT_GPIO, 0);  // Pull BOOT low
    vTaskDelay(pdMS_TO_TICKS(10));
    
    gpio_set_level(C6_RESET_GPIO, 0); // Assert reset
    vTaskDelay(pdMS_TO_TICKS(100));
    
    gpio_set_level(C6_RESET_GPIO, 1); // Release reset
    vTaskDelay(pdMS_TO_TICKS(50));
    
    gpio_set_level(C6_BOOT_GPIO, 1);  // Release BOOT
    
    ESP_LOGI(TAG, "C6 should be in bootloader mode");
}

/**
 * Reset C6 to normal mode
 */
static void c6_reset_normal(void)
{
    ESP_LOGI(TAG, "Resetting C6 to normal mode");
    
    // Set BOOT high for normal boot
    gpio_set_level(C6_BOOT_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    
    ESP_LOGI(TAG, "C6 reset to normal mode");
}

/**
 * Send SLIP encoded packet to C6
 */
static esp_err_t send_slip_packet(uint8_t *data, size_t len)
{
    uart_write_bytes(C6_UART_NUM, &(uint8_t){SLIP_END}, 1);
    
    for (size_t i = 0; i < len; i++) {
        if (data[i] == SLIP_END) {
            uart_write_bytes(C6_UART_NUM, &(uint8_t){SLIP_ESC}, 1);
            uart_write_bytes(C6_UART_NUM, &(uint8_t){SLIP_ESC_END}, 1);
        } else if (data[i] == SLIP_ESC) {
            uart_write_bytes(C6_UART_NUM, &(uint8_t){SLIP_ESC}, 1);
            uart_write_bytes(C6_UART_NUM, &(uint8_t){SLIP_ESC_ESC}, 1);
        } else {
            uart_write_bytes(C6_UART_NUM, &data[i], 1);
        }
    }
    
    uart_write_bytes(C6_UART_NUM, &(uint8_t){SLIP_END}, 1);
    return ESP_OK;
}

/**
 * Sync with C6 bootloader
 */
static esp_err_t sync_with_bootloader(void)
{
    ESP_LOGI(TAG, "Syncing with C6 bootloader...");
    
    uint8_t sync_cmd[] = {
        0x00, ESP_SYNC,
        0x24, 0x00,  // Length
        0x00, 0x00, 0x00, 0x00,  // Checksum placeholder
        // Sync sequence
        0x07, 0x07, 0x12, 0x20,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    };
    
    // Send sync command multiple times
    for (int i = 0; i < 10; i++) {
        send_slip_packet(sync_cmd, sizeof(sync_cmd));
        
        // Check for response
        uint8_t response[64];
        int len = uart_read_bytes(C6_UART_NUM, response, sizeof(response), 100 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            ESP_LOGI(TAG, "Received sync response (%d bytes)", len);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Failed to sync with bootloader");
    return ESP_ERR_TIMEOUT;
}

/**
 * Simple UART flash - just write the binary directly
 */
static esp_err_t simple_uart_flash(const char *firmware_path)
{
    ESP_LOGI(TAG, "Using simple UART flash method");
    
    FILE *f = fopen(firmware_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open firmware file");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    ESP_LOGI(TAG, "Firmware size: %d bytes", file_size);
    
    // Configure UART at 115200 for bootloader
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(C6_UART_NUM, 4096, 4096, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(C6_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(C6_UART_NUM, C6_TX_PIN, C6_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Enter bootloader mode
    c6_enter_bootloader_mode();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Try to detect bootloader
    ESP_LOGI(TAG, "Checking for bootloader response...");
    
    // Send a simple test pattern
    const char *test = "SYNC\r\n";
    uart_write_bytes(C6_UART_NUM, test, strlen(test));
    
    // Check for any response
    uint8_t response[256];
    int len = uart_read_bytes(C6_UART_NUM, response, sizeof(response), 500 / portTICK_PERIOD_MS);
    if (len > 0) {
        ESP_LOGI(TAG, "Got response from C6: %d bytes", len);
        ESP_LOG_BUFFER_HEX(TAG, response, len);
    } else {
        ESP_LOGW(TAG, "No response from C6 bootloader");
    }
    
    ESP_LOGI(TAG, "Proceeding with firmware transfer anyway...");
    
    // Transfer firmware in chunks
    uint8_t *buffer = malloc(1024);
    if (!buffer) {
        fclose(f);
        uart_driver_delete(C6_UART_NUM);
        return ESP_ERR_NO_MEM;
    }
    
    size_t total_written = 0;
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, 1024, f)) > 0) {
        uart_write_bytes(C6_UART_NUM, (char *)buffer, bytes_read);
        total_written += bytes_read;
        
        if (total_written % 10240 == 0) {
            ESP_LOGI(TAG, "Progress: %d/%d bytes (%.1f%%)",
                    total_written, file_size,
                    (float)total_written * 100.0 / file_size);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    ESP_LOGI(TAG, "Firmware transfer complete: %d bytes", total_written);
    
    free(buffer);
    fclose(f);
    
    // Wait for flash to complete
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Reset C6 to normal mode
    c6_reset_normal();
    
    uart_driver_delete(C6_UART_NUM);
    
    return ESP_OK;
}

/**
 * Flash firmware from SD card to C6
 */
esp_err_t c6_flash_firmware_from_sd(const char *firmware_path)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Starting C6 firmware update from SD card");
    ESP_LOGI(TAG, "Firmware path: %s", firmware_path);
    
    // Check if firmware file exists
    struct stat file_stat;
    if (stat(firmware_path, &file_stat) != 0) {
        ESP_LOGE(TAG, "Firmware file not found: %s", firmware_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Firmware size: %ld bytes", file_stat.st_size);
    
    // Try simple UART flash first
    ret = simple_uart_flash(firmware_path);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Simple UART flash failed, trying protocol-based flash");
        
        // Fall back to protocol-based flash
        FILE *f = fopen(firmware_path, "rb");
        if (!f) {
            return ESP_ERR_NOT_FOUND;
        }
        
        uint8_t *buffer = malloc(CHUNK_SIZE);
        if (!buffer) {
            fclose(f);
            return ESP_ERR_NO_MEM;
        }
        
        // Configure UART
        ret = configure_c6_uart();
        if (ret != ESP_OK) {
            free(buffer);
            fclose(f);
            return ret;
        }
        
        // Enter bootloader mode
        c6_enter_bootloader_mode();
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Sync with bootloader
        ret = sync_with_bootloader();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to sync with bootloader");
            free(buffer);
            fclose(f);
            uart_driver_delete(C6_UART_NUM);
            return ret;
        }
        
        ESP_LOGI(TAG, "Successfully synced with C6 bootloader");
        
        free(buffer);
        fclose(f);
        uart_driver_delete(C6_UART_NUM);
        
        // Reset C6 to normal mode
        c6_reset_normal();
    }
    
    return ret;
}

/**
 * Check for C6 firmware on SD card and update if found
 */
esp_err_t c6_check_and_update_firmware(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Checking for C6 firmware on SD card");
    
    // Initialize SD card
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
        return ret;
    }
    
    const char *mount_point = sd_card_get_mount_point();
    char firmware_path[256];
    char backup_path[256];
    snprintf(firmware_path, sizeof(firmware_path), "%s/%s", mount_point, C6_FIRMWARE_FILENAME);
    snprintf(backup_path, sizeof(backup_path), "%s/%s", mount_point, C6_FIRMWARE_BACKUP_FILENAME);
    
    // Check if firmware file exists
    struct stat file_stat;
    if (stat(firmware_path, &file_stat) == 0) {
        ESP_LOGI(TAG, "Found C6 firmware on SD card: %s", firmware_path);
        ESP_LOGI(TAG, "Size: %ld bytes", file_stat.st_size);
        
        // Flash firmware to C6
        ret = c6_flash_firmware_from_sd(firmware_path);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "C6 firmware update successful!");
            
            // Delete the original firmware file - don't create backup
            // Since we already transferred it, we don't need bridge mode
            remove(firmware_path);
            ESP_LOGI(TAG, "Firmware file deleted after successful transfer");
            
            // Create a marker file to indicate firmware was flashed
            FILE *marker = fopen(backup_path, "w");
            if (marker) {
                fprintf(marker, "C6 firmware flashed successfully\n");
                fclose(marker);
            }
        } else {
            ESP_LOGE(TAG, "C6 firmware update failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "No C6 firmware found on SD card");
        ret = ESP_ERR_NOT_FOUND;
    }
    
    return ret;
}

/**
 * Copy firmware from internal storage to SD card for future updates
 */
esp_err_t c6_copy_firmware_to_sd(const uint8_t *firmware_data, size_t firmware_size)
{
    esp_err_t ret;
    FILE *f = NULL;
    
    ESP_LOGI(TAG, "Copying C6 firmware to SD card");
    
    // Initialize SD card
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
        return ret;
    }
    
    // Write firmware to SD card
    const char *mount_point = sd_card_get_mount_point();
    char firmware_path[256];
    snprintf(firmware_path, sizeof(firmware_path), "%s/%s", mount_point, C6_FIRMWARE_FILENAME);
    
    f = fopen(firmware_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create firmware file on SD card");
        return ESP_FAIL;
    }
    
    size_t written = fwrite(firmware_data, 1, firmware_size, f);
    fclose(f);
    
    if (written == firmware_size) {
        ESP_LOGI(TAG, "Firmware copied to SD card: %d bytes", written);
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to write complete firmware");
        ret = ESP_FAIL;
    }
    
    return ret;
}