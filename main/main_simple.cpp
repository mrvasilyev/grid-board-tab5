/**
 * @file main_simple.cpp
 * @brief Simple Grid Board demo for M5Stack Tab5 (no BLE)
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/display.h"
#include "esp_lcd_types.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

#include "grid_board.hpp"
#include "sd_card_helper.h"

static const char *TAG = "GridBoard_Tab5";

// Global grid board instance  
static GridBoard grid_board;

static std::string demo_text = "EVA AND YULIA WELCOME HOME 😊❤❤❤";

static bool grid_initialized = false;
static lv_display_t* main_disp = NULL;
static int msg_index = 0;
static uint32_t last_message_time = 0;

static const char* messages[] = {
    "EVA AND YULIA WELCOME HOME 😊❤❤❤",
    "WE LOVE YOU ❤❤❤❤❤",
    "HAPPY TO SEE YOU 😊😊😊",
    "HOME SWEET HOME ❤❤",
    "FAMILY TOGETHER ❤😊❤"
};

void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    
    // Wait a bit for initialization to complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize grid board once LVGL is ready
    if (!grid_initialized && main_disp != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200)); // Give LVGL time to settle
        
        ESP_LOGI(TAG, "Initializing Grid Board UI from LVGL task");
        lv_obj_t *screen = lv_display_get_screen_active(main_disp);
        
        // Initialize grid board
        grid_board.initialize(screen);
        
        // Display initial message
        grid_board.process_text_and_animate(messages[msg_index]);
        last_message_time = esp_timer_get_time() / 1000; // Get time in ms
        
        grid_initialized = true;
        ESP_LOGI(TAG, "Grid board initialized successfully!");
    }
    
    while (1)
    {
        // Check if LVGL is ready before calling handler
        if (lv_display_get_default() != NULL) {
            lv_timer_handler();
            
            // Check if it's time to change message (every 30 seconds)
            if (grid_initialized) {
                uint32_t current_time = esp_timer_get_time() / 1000;
                if (current_time - last_message_time > 30000) {
                    msg_index = (msg_index + 1) % 5;
                    ESP_LOGI(TAG, "Changing to message %d: %s", msg_index, messages[msg_index]);
                    grid_board.process_text_and_animate(messages[msg_index]);
                    last_message_time = current_time;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// External C6 integration function
extern "C" {
    esp_err_t tab5_c6_system_init(bool force_bridge_mode);
    void tab5_c6_demo(void);
    void c6_uart_bridge_main(void);
    bool should_enter_bridge_mode(void);
    esp_err_t delete_c6_backup(void);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Grid Board Demo for M5Stack Tab5 starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Check if we should enter bridge mode for C6 firmware flashing
    // Initialize SD card first to check for firmware status
    sd_card_init();
    
    // AUTO DELETE: Remove backup file to exit bridge mode
    // Since we already transferred the firmware, we don't need bridge mode
    if (delete_c6_backup() == ESP_OK) {
        ESP_LOGI(TAG, "Backup file deleted, continuing with normal boot");
    }
    
    if (should_enter_bridge_mode()) {
        ESP_LOGI(TAG, "Entering C6 UART bridge mode for firmware flashing");
        ESP_LOGI(TAG, "==============================================");
        ESP_LOGI(TAG, "UART BRIDGE MODE ACTIVE");
        ESP_LOGI(TAG, "Use esptool.py to flash C6 firmware:");
        ESP_LOGI(TAG, "esptool.py --chip esp32c6 -p /dev/cu.usbmodem1101 -b 460800 write_flash 0x0 firmware.bin");
        ESP_LOGI(TAG, "==============================================");
        c6_uart_bridge_main();
        // Never returns from bridge mode
        return;
    }
    
    // Initialize display exactly like reference implementation
    ESP_LOGI(TAG, "Initializing display with landscape orientation");
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,  // Full frame buffer like reference
        .double_buffer = true,
        .flags = {
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
            .buff_dma = false,
#else
            .buff_dma = true,  // Enable DMA like reference
#endif
            .buff_spiram = true,
            .sw_rotate = true,  // Enable software rotation
        }
    };
    
    lv_display_t* disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    
    // Store display for LVGL task
    main_disp = disp;
    
    // Rotate to landscape: 90 for normal viewing, 270 for inverted (bottom mount)
    // Using 90 for normal landscape orientation
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    
    // Unlock display after configuration
    bsp_display_unlock();
    
    // Create LVGL task - it will handle grid initialization
    ESP_LOGI(TAG, "Starting LVGL task");
    xTaskCreate(lvgl_task, "lvgl_task", 8192, NULL, 5, NULL);
    
    // Initialize ESP32-C6 communication
    ESP_LOGI(TAG, "Initializing ESP32-C6 communication system");
    ret = tab5_c6_system_init(false);  // false = normal SDIO mode, not bridge
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "C6 communication initialized, starting demo");
        // Run C6 demo in background task
        xTaskCreate([](void* arg) { tab5_c6_demo(); }, "c6_demo", 4096, NULL, 3, NULL);
    } else {
        ESP_LOGW(TAG, "C6 communication initialization failed: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "ESP32-C6 may not have SDIO slave firmware installed");
        ESP_LOGW(TAG, "To flash C6 firmware:");
        ESP_LOGW(TAG, "1. Hold BOOT button and press RESET to enter bridge mode");
        ESP_LOGW(TAG, "2. Or modify should_enter_bridge_mode() to return true");
    }
    
    // Grid board initialization and message cycling happens in LVGL task
    ESP_LOGI(TAG, "Grid Board initialization delegated to LVGL task");
    
    // Main task just keeps running
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Just keep the task alive
    }
}