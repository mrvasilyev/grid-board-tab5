/*
 * Test program for ESP32-C6 UART bridge
 * Flash this to ESP32-P4 to enable UART passthrough to C6
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

extern "C" {
    void c6_uart_bridge_main(void);
    void start_c6_uart_bridge(void);
}

static const char *TAG = "TEST_C6";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "===================================================");
    ESP_LOGI(TAG, "ESP32-C6 UART Bridge Test Program");
    ESP_LOGI(TAG, "===================================================");
    ESP_LOGI(TAG, "This program creates a UART bridge to the ESP32-C6");
    ESP_LOGI(TAG, "You can use esptool to flash the C6 through this bridge");
    ESP_LOGI(TAG, "===================================================");
    
    // Start the UART bridge
    start_c6_uart_bridge();
    
    // Main loop - just keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Bridge is running...");
    }
}