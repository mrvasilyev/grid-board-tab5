/**
 * @file main_standalone.cpp
 * @brief Standalone Grid Board demo for M5Stack Tab5
 */

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>
#include <vector>
#include <cstring>

static const char *TAG = "GridBoard";

// Tab5 Display Configuration
#define LCD_H_RES              1280
#define LCD_V_RES              720
#define LCD_BIT_PER_PIXEL      16
#define MIPI_DSI_LANE_NUM      2
#define MIPI_DSI_LANE_RATE_MBPS 500
#define LCD_BACKLIGHT_GPIO     22

// Grid configuration
#define GRID_COLS 12
#define GRID_ROWS 5
#define GRID_SLOT_WIDTH 96
#define GRID_SLOT_HEIGHT 126
#define GRID_GAP 10

class GridBoardDemo {
private:
    lv_obj_t* slots[GRID_ROWS][GRID_COLS];
    std::string current_text;
    int current_index;
    lv_timer_t* animation_timer;

public:
    GridBoardDemo() : current_index(0), animation_timer(nullptr) {
        memset(slots, 0, sizeof(slots));
    }

    void init(lv_obj_t* parent) {
        // Set black background
        lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
        
        // Calculate grid position
        int total_width = GRID_COLS * GRID_SLOT_WIDTH + (GRID_COLS - 1) * GRID_GAP;
        int total_height = GRID_ROWS * GRID_SLOT_HEIGHT + (GRID_ROWS - 1) * GRID_GAP;
        int x_start = (LCD_H_RES - total_width) / 2;
        int y_start = (LCD_V_RES - total_height) / 2;

        // Create grid slots
        for (int row = 0; row < GRID_ROWS; row++) {
            for (int col = 0; col < GRID_COLS; col++) {
                int x = x_start + col * (GRID_SLOT_WIDTH + GRID_GAP);
                int y = y_start + row * (GRID_SLOT_HEIGHT + GRID_GAP);
                
                lv_obj_t* slot = lv_obj_create(parent);
                lv_obj_set_size(slot, GRID_SLOT_WIDTH, GRID_SLOT_HEIGHT);
                lv_obj_set_pos(slot, x, y);
                lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_border_width(slot, 2, 0);
                lv_obj_set_style_border_color(slot, lv_color_hex(0x00FF00), 0);
                lv_obj_set_style_bg_color(slot, lv_color_hex(0x1A1A1A), 0);
                lv_obj_set_style_radius(slot, 8, 0);

                // Add label
                lv_obj_t* label = lv_label_create(slot);
                lv_label_set_text(label, " ");
                lv_obj_center(label);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_42, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
                
                slots[row][col] = slot;
            }
        }
    }

    void display_text(const std::string& text) {
        current_text = text;
        current_index = 0;
        clear_grid();
        
        if (animation_timer) {
            lv_timer_del(animation_timer);
        }
        animation_timer = lv_timer_create(animation_callback, 50, this);
    }

    void clear_grid() {
        for (int row = 0; row < GRID_ROWS; row++) {
            for (int col = 0; col < GRID_COLS; col++) {
                lv_obj_t* label = lv_obj_get_child(slots[row][col], 0);
                if (label) {
                    lv_label_set_text(label, " ");
                }
            }
        }
    }

    static void animation_callback(lv_timer_t* timer) {
        GridBoardDemo* board = (GridBoardDemo*)timer->user_data;
        board->animate_next_character();
    }

    void animate_next_character() {
        if (current_index >= current_text.length()) {
            if (animation_timer) {
                lv_timer_del(animation_timer);
                animation_timer = nullptr;
            }
            return;
        }

        int slot_index = current_index % (GRID_ROWS * GRID_COLS);
        int row = slot_index / GRID_COLS;
        int col = slot_index % GRID_COLS;

        char ch = current_text[current_index];
        char str[2] = {ch, '\0'};

        lv_obj_t* slot = slots[row][col];
        lv_obj_t* label = lv_obj_get_child(slot, 0);
        lv_label_set_text(label, str);

        // Simple fade animation
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, slot);
        lv_anim_set_values(&anim, LV_OPA_0, LV_OPA_COVER);
        lv_anim_set_time(&anim, 300);
        lv_anim_set_exec_cb(&anim, [](void* var, int32_t value) {
            lv_obj_set_style_opa((lv_obj_t*)var, value, 0);
        });
        lv_anim_start(&anim);

        current_index++;
    }
};

static GridBoardDemo* grid_board = nullptr;

void init_display() {
    ESP_LOGI(TAG, "Initialize display backlight");
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_BACKLIGHT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)LCD_BACKLIGHT_GPIO, 1));

    ESP_LOGI(TAG, "Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = MIPI_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = MIPI_DSI_LANE_RATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 4096,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "Create display");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = NULL,
        .buffer_size = LCD_H_RES * 50,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
            .swap_bytes = false,
        }
    };
    lv_disp_t* disp = lvgl_port_add_disp(&disp_cfg);
    
    ESP_LOGI(TAG, "Display initialized");
}

void demo_task(void *param) {
    const char* messages[] = {
        "HELLO M5STACK TAB5!",
        "GRID BOARD DEMO",
        "ESP32-P4 POWERED",
        "1280 X 720 PIXELS",
        "LVGL ANIMATIONS",
        "TOUCH DISPLAY",
        "MADE WITH LOVE"
    };
    int msg_count = sizeof(messages) / sizeof(messages[0]);
    int current = 0;

    while (1) {
        if (grid_board) {
            grid_board->display_text(messages[current]);
            current = (current + 1) % msg_count;
        }
        vTaskDelay(pdMS_TO_TICKS(8000)); // Change message every 8 seconds
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Grid Board Demo for M5Stack Tab5");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize display and LVGL
    init_display();
    
    // Create Grid Board
    ESP_LOGI(TAG, "Creating Grid Board");
    grid_board = new GridBoardDemo();
    lv_obj_t* screen = lv_scr_act();
    grid_board->init(screen);
    
    // Display initial message
    grid_board->display_text("INITIALIZING...");
    
    // Start demo task
    xTaskCreate(demo_task, "demo_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Grid Board running!");
    
    // Main LVGL loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}