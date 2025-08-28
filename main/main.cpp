/**
 * @file main.cpp
 * @brief Grid Board adapted for M5Stack Tab5
 * 
 * Port of Grid Board project to M5Stack Tab5 hardware
 * Original by Eric Nam, adapted for Tab5
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "ble_server.h"
#include "esp_heap_caps.h"
#include "bsp/m5stack_tab5.h"
#include "esp_lcd_types.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

#include "grid_board.hpp"

extern const uint8_t card_pcm_start[] asm("_binary_card_pcm_start");
extern const uint8_t card_pcm_end[] asm("_binary_card_pcm_end");

static const char *TAG = "GridBoard_Tab5";

// Global grid board instance
static GridBoard grid_board;

static const char *device_name = "Grid_Board_Tab5";
static std::string target_text = "              WELCOME😀       TO     📌GRID BOARD❤            ";
static QueueHandle_t sfx_queue = nullptr;

void card_flip_sfx_task(void *param)
{
    static int64_t last_sfx_time = 0;
    const int MIN_SFX_INTERVAL_MS = 33;
    size_t pcm_size = card_pcm_end - card_pcm_start;
    bool active = true;

    while (1)
    {
        uint8_t msg;
        if (xQueueReceive(sfx_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (msg == 0)
            {
                active = false;
                continue;
            }
            int64_t now = esp_timer_get_time() / 1000; // ms
            if (now - last_sfx_time >= MIN_SFX_INTERVAL_MS)
            {
                last_sfx_time = now;
                // Note: Audio playback disabled for now - Tab5 uses different audio codec
                // TODO: Implement ES8388 audio playback
            }
            active = true;
        }
    }
}

void start_sfx_task()
{
    if (!sfx_queue)
        sfx_queue = xQueueCreate(8, sizeof(uint8_t));
    xTaskCreate(card_flip_sfx_task, "card_flip_sfx_task", 4096, nullptr, 3, nullptr);
}

void start_card_flip_sound_task()
{
    if (sfx_queue)
    {
        uint8_t msg = 1;
        xQueueSend(sfx_queue, &msg, 0);
    }
}

void stop_card_flip_sound_task()
{
    if (sfx_queue)
    {
        uint8_t msg = 0;
        xQueueSend(sfx_queue, &msg, 0);
    }
}

void on_ble_connect(bool connected)
{
    ESP_LOGI(TAG, "BLE %s", connected ? "Connected" : "Disconnected");
}

void on_ble_write(const uint8_t *data, uint16_t len)
{
    std::string received_text((const char *)data, len);
    ESP_LOGI(TAG, "BLE Received: %s", received_text.c_str());
    
    // Wait for any current animations to finish before starting new ones
    while (grid_board.is_animation_running())
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Process the received text and animate grid
    grid_board.process_text_and_animate(received_text);
}

void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Grid Board for M5Stack Tab5 starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize display
    ESP_LOGI(TAG, "Initializing display");
    lv_display_t* disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    
    // Set display rotation to landscape
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    
    // Get display and initialize grid
    ESP_LOGI(TAG, "Initializing Grid Board UI");
    lv_obj_t *screen = lv_display_get_screen_active(disp);
    
    // Initialize grid board
    grid_board.initialize(screen);
    grid_board.set_sound_callback(start_card_flip_sound_task, stop_card_flip_sound_task);
    
    // Start sound effects task (audio disabled for now)
    start_sfx_task();
    
    // Create LVGL task
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, NULL);
    
    // Start BLE server
    ESP_LOGI(TAG, "Starting BLE server");
    ble_server_register_callbacks(on_ble_connect, on_ble_write);
    ble_server_start(device_name);
    
    // Display initial welcome message
    ESP_LOGI(TAG, "Displaying welcome message");
    grid_board.process_text_and_animate(target_text);
    
    ESP_LOGI(TAG, "Grid Board initialized successfully!");
    
    // Main loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}