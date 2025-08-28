/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app_c6_ota.h"
#include "hal/hal_esp32.h"
#include <mooncake_log.h>
#include <esp_log.h>
#include <esp_err.h>
#include <bsp/m5stack_tab5.h>

// Only compile if WiFi remote is enabled
#if CONFIG_TAB5_WIFI_REMOTE_ENABLE && CONFIG_ESP_WIFI_REMOTE_ENABLED

#include "esp_hosted.h"

static const char *TAG = "c6_ota";

// Function to perform OTA update on ESP32-C6
esp_err_t update_esp32c6_firmware(const char *firmware_url) {
    mclog::tagInfo(TAG, "Starting ESP32-C6 OTA update from: {}", firmware_url);
    
    // Make sure WiFi module is powered on
    bsp_set_wifi_power_enable(true);
    vTaskDelay(pdMS_TO_TICKS(500));  // Give C6 time to power up
    
    // Perform OTA update
    esp_err_t err = esp_hosted_slave_ota(firmware_url);
    
    if (err != ESP_OK) {
        mclog::tagError(TAG, "Failed to start OTA update: {}", esp_err_to_name(err));
        return err;
    }
    
    mclog::tagInfo(TAG, "OTA update initiated successfully");
    return ESP_OK;
}

// Task to monitor OTA progress
void c6_ota_task(void *pvParameters) {
    const char *firmware_url = (const char *)pvParameters;
    
    mclog::tagInfo(TAG, "ESP32-C6 OTA Task started");
    
    // Wait for ESP-Hosted connection to be established
    mclog::tagInfo(TAG, "Waiting 30 seconds for ESP-Hosted connection...");
    vTaskDelay(pdMS_TO_TICKS(30000));  // Give time for ESP-Hosted to connect
    
    // Perform the OTA update
    esp_err_t result = update_esp32c6_firmware(firmware_url);
    
    if (result == ESP_OK) {
        mclog::tagInfo(TAG, "OTA update completed successfully!");
        mclog::tagInfo(TAG, "ESP32-C6 should now be running the new firmware");
    } else {
        mclog::tagError(TAG, "OTA update failed with error: {}", esp_err_to_name(result));
    }
    
    // Task cleanup
    vTaskDelete(NULL);
}

// Public function to initiate C6 firmware update
extern "C" void start_c6_firmware_update() {
    // Update this URL to match your HTTP server's IP address
    const char *firmware_url = "http://192.168.88.243:8080/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin";
    
    mclog::tagInfo(TAG, "Creating ESP32-C6 OTA update task");
    
    // Create task for OTA update with larger stack
    xTaskCreate(c6_ota_task, 
                "c6_ota", 
                16384,  // Increased stack size to prevent overflow
                (void *)firmware_url,
                5, 
                NULL);
}

#else

// Stub implementation when WiFi is not enabled
extern "C" void start_c6_firmware_update() {
    ESP_LOGW(TAG, "ESP32-C6 OTA update not available - WiFi Remote is disabled");
}

#endif // CONFIG_TAB5_WIFI_REMOTE_ENABLE