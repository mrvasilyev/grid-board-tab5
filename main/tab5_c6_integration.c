/**
 * @file tab5_c6_integration.c
 * @brief Integration of ESP32-C6 communication for M5Stack Tab5
 * 
 * This module integrates SDIO communication and UART bridge functionality
 * for the Grid Board Tab5 application
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sdio_communication.h"
#include "c6_sd_firmware_loader.h"
#include "c6_firmware_prepare.h"

static const char *TAG = "TAB5_C6_INTEGRATION";

// Event group for C6 status
static EventGroupHandle_t c6_event_group;
#define C6_READY_BIT        BIT0
#define C6_WIFI_CONNECTED   BIT1
#define C6_BT_ENABLED       BIT2
#define C6_ERROR_BIT        BIT3

// Global SDIO handle
static tab5_sdio_handle_t sdio_handle;

// Function prototypes
extern void c6_uart_bridge_main(void);
extern bool should_enter_bridge_mode(void);
esp_err_t tab5_c6_system_init(bool force_bridge_mode);
void tab5_c6_status_task(void *pvParameters);
esp_err_t tab5_c6_send_message(const char *message);

/**
 * Initialize C6 communication system
 */
esp_err_t tab5_c6_system_init(bool force_bridge_mode)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "M5Stack Tab5 ESP32-C6 Communication System");
    ESP_LOGI(TAG, "==============================================");
    
    // Check if we should enter UART bridge mode for firmware upload
    if (force_bridge_mode || should_enter_bridge_mode()) {
        ESP_LOGI(TAG, "Entering UART bridge mode for C6 firmware upload");
        ESP_LOGI(TAG, "To exit bridge mode, reset the device");
        c6_uart_bridge_main();
        // This function won't return if bridge mode is entered
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing SDIO communication with C6");
    
    // SD card firmware features disabled - use external USB-UART adapter for C6 flashing
    // The automatic firmware transfer doesn't use proper esptool protocol
    /*
    // First, prepare firmware on SD card if needed
    ESP_LOGI(TAG, "Preparing C6 firmware on SD card from embedded binary...");
    esp_err_t prep_ret = c6_prepare_firmware_on_sd();
    if (prep_ret == ESP_OK) {
        ESP_LOGI(TAG, "C6 firmware ready on SD card");
    } else if (prep_ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "SD card not available for firmware preparation");
    }
    
    // Then check if there's firmware on SD card to flash
    ESP_LOGI(TAG, "Checking for C6 firmware update on SD card...");
    esp_err_t fw_ret = c6_check_and_update_firmware();
    if (fw_ret == ESP_OK) {
        ESP_LOGI(TAG, "C6 firmware updated from SD card!");
        vTaskDelay(pdMS_TO_TICKS(2000));  // Give C6 time to boot with new firmware
    } else if (fw_ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "No firmware update found on SD card");
    } else {
        ESP_LOGW(TAG, "SD card check failed: %s", esp_err_to_name(fw_ret));
    }
    */
    
    // Create event group
    c6_event_group = xEventGroupCreate();
    if (!c6_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize SDIO communication
    ret = tab5_sdio_init(&sdio_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO: %s", esp_err_to_name(ret));
        vEventGroupDelete(c6_event_group);
        return ret;
    }
    
    // Check if C6 is ready
    if (tab5_c6_is_ready(&sdio_handle)) {
        xEventGroupSetBits(c6_event_group, C6_READY_BIT);
        ESP_LOGI(TAG, "C6 is ready for communication");
        
        // Get firmware version
        char fw_version[32];
        ret = tab5_c6_get_fw_version(&sdio_handle, fw_version, sizeof(fw_version));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "C6 Firmware: %s", fw_version);
        }
    } else {
        ESP_LOGW(TAG, "C6 not ready, attempting reset...");
        ret = tab5_c6_reset(&sdio_handle);
        if (ret == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            if (tab5_c6_is_ready(&sdio_handle)) {
                xEventGroupSetBits(c6_event_group, C6_READY_BIT);
                ESP_LOGI(TAG, "C6 ready after reset");
            }
        }
    }
    
    // Create status monitoring task
    xTaskCreate(tab5_c6_status_task, "c6_status", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "C6 communication system initialized");
    return ESP_OK;
}

/**
 * Task to monitor C6 status and handle events
 */
void tab5_c6_status_task(void *pvParameters)
{
    uint8_t status;
    esp_err_t ret;
    sdio_packet_t rx_packet;
    
    ESP_LOGI(TAG, "C6 status task started");
    
    while (1) {
        // Check if C6 is ready
        EventBits_t bits = xEventGroupGetBits(c6_event_group);
        if (bits & C6_READY_BIT) {
            // Read C6 status
            ret = tab5_c6_read_status(&sdio_handle, &status);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "C6 Status: 0x%02x", status);
                
                // Update event bits based on status
                if (status & 0x01) {  // WiFi connected bit
                    xEventGroupSetBits(c6_event_group, C6_WIFI_CONNECTED);
                } else {
                    xEventGroupClearBits(c6_event_group, C6_WIFI_CONNECTED);
                }
                
                if (status & 0x02) {  // BT enabled bit
                    xEventGroupSetBits(c6_event_group, C6_BT_ENABLED);
                } else {
                    xEventGroupClearBits(c6_event_group, C6_BT_ENABLED);
                }
            }
            
            // Check for incoming data
            ret = tab5_sdio_receive(&sdio_handle, &rx_packet, 100);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Received from C6: %.*s", (int)rx_packet.length, rx_packet.data);
                
                // Process received data based on type
                switch (rx_packet.type) {
                    case CMD_WIFI_CONNECT:
                        ESP_LOGI(TAG, "WiFi connection status update");
                        break;
                    case CMD_DATA_TRANSFER:
                        ESP_LOGI(TAG, "Data packet received");
                        break;
                    default:
                        ESP_LOGW(TAG, "Unknown packet type: %d", rx_packet.type);
                        break;
                }
            }
        } else {
            // Try to recover communication
            ESP_LOGW(TAG, "C6 not ready, attempting recovery...");
            ret = tab5_c6_reset(&sdio_handle);
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                if (tab5_c6_is_ready(&sdio_handle)) {
                    xEventGroupSetBits(c6_event_group, C6_READY_BIT);
                    ESP_LOGI(TAG, "C6 communication recovered");
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * Send a message to C6
 */
esp_err_t tab5_c6_send_message(const char *message)
{
    if (!message) {
        return ESP_ERR_INVALID_ARG;
    }
    
    EventBits_t bits = xEventGroupGetBits(c6_event_group);
    if (!(bits & C6_READY_BIT)) {
        ESP_LOGW(TAG, "C6 not ready, cannot send message");
        return ESP_ERR_INVALID_STATE;
    }
    
    sdio_packet_t packet = {0};
    packet.type = CMD_DATA_TRANSFER;
    size_t msg_len = strlen(message);
    if (msg_len > SDIO_BUFFER_SIZE - 1) {
        msg_len = SDIO_BUFFER_SIZE - 1;
    }
    memcpy(packet.data, message, msg_len);
    packet.length = msg_len;
    
    esp_err_t ret = tab5_sdio_send(&sdio_handle, &packet);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Message sent to C6: %s", message);
    } else {
        ESP_LOGE(TAG, "Failed to send message: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * Connect to WiFi via C6 (wrapper function)
 */
esp_err_t tab5_c6_connect_wifi(const char *ssid, const char *password)
{
    EventBits_t bits = xEventGroupGetBits(c6_event_group);
    if (!(bits & C6_READY_BIT)) {
        ESP_LOGW(TAG, "C6 not ready, cannot connect to WiFi");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Requesting WiFi connection: %s", ssid);
    
    // Initialize WiFi if needed
    esp_err_t ret = tab5_c6_wifi_init(&sdio_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Connect to network
    ret = tab5_c6_wifi_connect(&sdio_handle, ssid, password);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connection request sent");
        
        // Wait for connection
        EventBits_t result = xEventGroupWaitBits(c6_event_group,
                                                 C6_WIFI_CONNECTED,
                                                 pdFALSE,
                                                 pdTRUE,
                                                 pdMS_TO_TICKS(30000));
        
        if (result & C6_WIFI_CONNECTED) {
            ESP_LOGI(TAG, "WiFi connected successfully");
        } else {
            ESP_LOGW(TAG, "WiFi connection timeout");
            ret = ESP_ERR_TIMEOUT;
        }
    }
    
    return ret;
}

/**
 * Example usage in Grid Board application
 */
void tab5_c6_demo(void)
{
    ESP_LOGI(TAG, "Starting C6 communication demo");
    
    // Initialize C6 system (normal SDIO mode)
    esp_err_t ret = tab5_c6_system_init(false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize C6 system");
        return;
    }
    
    // Wait for C6 to be ready
    EventBits_t bits = xEventGroupWaitBits(c6_event_group,
                                          C6_READY_BIT,
                                          pdFALSE,
                                          pdTRUE,
                                          pdMS_TO_TICKS(5000));
    
    if (bits & C6_READY_BIT) {
        ESP_LOGI(TAG, "C6 is ready, starting demo");
        
        // Send a test message
        tab5_c6_send_message("Hello from ESP32-P4!");
        
        // Connect to WiFi (example - replace with your credentials)
        // tab5_c6_wifi_connect("YourSSID", "YourPassword");
        
        // Main demo loop
        int counter = 0;
        char msg_buffer[64];
        
        while (1) {
            // Send periodic messages
            snprintf(msg_buffer, sizeof(msg_buffer), "Grid Board message #%d", counter++);
            tab5_c6_send_message(msg_buffer);
            
            // Check status
            bits = xEventGroupGetBits(c6_event_group);
            ESP_LOGI(TAG, "C6 Status - Ready:%s WiFi:%s BT:%s",
                    (bits & C6_READY_BIT) ? "Yes" : "No",
                    (bits & C6_WIFI_CONNECTED) ? "Connected" : "Disconnected",
                    (bits & C6_BT_ENABLED) ? "Enabled" : "Disabled");
            
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    } else {
        ESP_LOGE(TAG, "C6 not ready after timeout");
    }
}

/**
 * Function to trigger UART bridge mode for firmware update
 */
void tab5_c6_enter_firmware_update_mode(void)
{
    ESP_LOGI(TAG, "Entering firmware update mode");
    ESP_LOGI(TAG, "Restarting in UART bridge mode...");
    
    // Set a flag in NVS to indicate bridge mode on next boot
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("system", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_set_u8(nvs_handle, "c6_bridge_mode", 1);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    // Restart the system
    esp_restart();
}