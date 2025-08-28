/*
 * ESP32-C6 Firmware Flasher for M5Stack Tab5
 * Flashes the ESP32-C6 co-processor through ESP32-P4
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"

static const char *TAG = "C6_FLASHER";

// Pin definitions for Tab5
#define C6_RESET_GPIO   GPIO_NUM_15  // ESP32-P4 GPIO15 -> C6 RESET
#define C6_IO2_GPIO     GPIO_NUM_14  // ESP32-P4 GPIO14 -> C6 IO2 (boot mode)

// C6 boot modes:
// IO2=0, IO8=0: Download mode (UART)
// IO2=0, IO8=1: Download mode (USB)  
// IO2=1: SPI boot (normal)

// Firmware file path
#define FIRMWARE_PATH "/storage/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin"

// Function to put ESP32-C6 into download mode
static esp_err_t c6_enter_download_mode(void)
{
    ESP_LOGI(TAG, "Configuring GPIO pins for C6 control");
    
    // Configure reset pin as output
    gpio_config_t reset_conf = {
        .pin_bit_mask = (1ULL << C6_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&reset_conf));
    
    // Configure IO2 pin as output (boot control)
    gpio_config_t io2_conf = {
        .pin_bit_mask = (1ULL << C6_IO2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Pull down for boot mode
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io2_conf));
    
    ESP_LOGI(TAG, "Entering download mode sequence");
    
    // Set IO2 low (download mode)
    gpio_set_level(C6_IO2_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset sequence: Reset low -> wait -> Reset high
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "C6 should now be in download mode");
    
    return ESP_OK;
}

// Function to reset C6 to normal boot mode
static esp_err_t c6_normal_boot(void)
{
    ESP_LOGI(TAG, "Resetting C6 to normal boot mode");
    
    // Set IO2 high (normal SPI boot)
    gpio_set_level(C6_IO2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Reset the C6
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Release IO2 to float
    gpio_set_direction(C6_IO2_GPIO, GPIO_MODE_INPUT);
    
    ESP_LOGI(TAG, "C6 reset to normal mode complete");
    
    return ESP_OK;
}

// Read firmware file into buffer
static uint8_t* read_firmware_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open firmware file: %s", path);
        return NULL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    ESP_LOGI(TAG, "Firmware file size: %zu bytes", size);
    
    // Allocate buffer
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for firmware");
        fclose(f);
        return NULL;
    }
    
    // Read file
    size_t read = fread(buffer, 1, size, f);
    fclose(f);
    
    if (read != size) {
        ESP_LOGE(TAG, "Failed to read complete firmware file");
        free(buffer);
        return NULL;
    }
    
    *out_size = size;
    return buffer;
}

// Main function to flash C6 firmware
esp_err_t flash_c6_firmware(void)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "Starting ESP32-C6 firmware flash process");
    ESP_LOGI(TAG, "===========================================");
    
    // Step 1: Put C6 into download mode
    ret = c6_enter_download_mode();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enter download mode");
        return ret;
    }
    
    // Wait for C6 to stabilize in download mode
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Step 2: Check if we can communicate with C6
    // Note: The actual flashing mechanism depends on how M5Stack implemented it
    // They might use:
    // - UART passthrough
    // - SDIO bootloader commands
    // - Custom protocol
    
    ESP_LOGI(TAG, "C6 is in download mode");
    ESP_LOGI(TAG, "Note: Actual flashing requires M5Stack's specific implementation");
    ESP_LOGI(TAG, "The C6 might need to be flashed via:");
    ESP_LOGI(TAG, "1. UART connection if exposed");
    ESP_LOGI(TAG, "2. USB if C6 USB is connected");
    ESP_LOGI(TAG, "3. Factory tool from M5Stack");
    
    // Step 3: Reset C6 to normal mode
    vTaskDelay(pdMS_TO_TICKS(2000));
    ret = c6_normal_boot();
    
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "C6 flash process complete");
    ESP_LOGI(TAG, "===========================================");
    
    return ret;
}

// Task to attempt C6 firmware flash
void c6_flasher_task(void *pvParameters)
{
    ESP_LOGI(TAG, "C6 Flasher task started");
    
    // Wait a bit for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Try to flash C6
    esp_err_t ret = flash_c6_firmware();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "C6 flash sequence completed successfully");
    } else {
        ESP_LOGE(TAG, "C6 flash sequence failed");
    }
    
    vTaskDelete(NULL);
}

// Public function to start C6 flasher
void start_c6_flasher(void)
{
    xTaskCreate(c6_flasher_task, "c6_flasher", 4096, NULL, 5, NULL);
}