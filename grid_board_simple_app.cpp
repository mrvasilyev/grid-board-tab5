/**
 * @file grid_board_simple_app.cpp
 * @brief Simplified Grid Board app for M5Stack Tab5 demo
 * 
 * This can be integrated into the M5Stack Tab5 demo as an app
 */

#include "lvgl.h"
#include <string>
#include <vector>
#include <cstring>

// Grid configuration for Tab5 (1280x720)
#define GRID_COLS 12
#define GRID_ROWS 5
#define GRID_SLOT_WIDTH 96
#define GRID_SLOT_HEIGHT 126
#define GRID_GAP 10
#define GRID_SCREEN_WIDTH 1280
#define GRID_SCREEN_HEIGHT 720

class SimpleGridBoard {
private:
    lv_obj_t* grid_container;
    lv_obj_t* slots[GRID_ROWS][GRID_COLS];
    std::string current_text;
    int current_index;
    lv_timer_t* animation_timer;

public:
    SimpleGridBoard() : grid_container(nullptr), current_index(0), animation_timer(nullptr) {
        memset(slots, 0, sizeof(slots));
    }

    void init(lv_obj_t* parent) {
        // Create main container
        grid_container = lv_obj_create(parent);
        lv_obj_set_size(grid_container, GRID_SCREEN_WIDTH, GRID_SCREEN_HEIGHT);
        lv_obj_center(grid_container);
        lv_obj_clear_flag(grid_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(grid_container, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_border_width(grid_container, 0, 0);
        lv_obj_set_style_pad_all(grid_container, 0, 0);

        // Calculate grid position to center it
        int total_width = GRID_COLS * GRID_SLOT_WIDTH + (GRID_COLS - 1) * GRID_GAP;
        int total_height = GRID_ROWS * GRID_SLOT_HEIGHT + (GRID_ROWS - 1) * GRID_GAP;
        int x_start = (GRID_SCREEN_WIDTH - total_width) / 2;
        int y_start = (GRID_SCREEN_HEIGHT - total_height) / 2;

        // Create grid slots
        for (int row = 0; row < GRID_ROWS; row++) {
            for (int col = 0; col < GRID_COLS; col++) {
                int x = x_start + col * (GRID_SLOT_WIDTH + GRID_GAP);
                int y = y_start + row * (GRID_SLOT_HEIGHT + GRID_GAP);
                
                lv_obj_t* slot = lv_obj_create(grid_container);
                lv_obj_set_size(slot, GRID_SLOT_WIDTH, GRID_SLOT_HEIGHT);
                lv_obj_set_pos(slot, x, y);
                lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_pad_all(slot, 0, 0);
                lv_obj_set_style_border_width(slot, 2, 0);
                lv_obj_set_style_border_color(slot, lv_color_hex(0x3A3A3A), 0);
                lv_obj_set_style_bg_color(slot, lv_color_hex(0x2A2A2A), 0);
                lv_obj_set_style_radius(slot, 8, 0);

                // Add label for character
                lv_obj_t* label = lv_label_create(slot);
                lv_label_set_text(label, " ");
                lv_obj_center(label);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
                
                slots[row][col] = slot;
            }
        }
    }

    void display_text(const std::string& text) {
        current_text = text;
        current_index = 0;
        
        // Clear all slots first
        clear_grid();
        
        // Start animation timer
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
                lv_obj_set_style_bg_color(slots[row][col], lv_color_hex(0x2A2A2A), 0);
            }
        }
    }

    static void animation_callback(lv_timer_t* timer) {
        SimpleGridBoard* board = (SimpleGridBoard*)timer->user_data;
        board->animate_next_character();
    }

    void animate_next_character() {
        if (current_index >= current_text.length()) {
            // Animation complete
            if (animation_timer) {
                lv_timer_del(animation_timer);
                animation_timer = nullptr;
            }
            return;
        }

        // Calculate position in grid
        int total_slots = GRID_ROWS * GRID_COLS;
        int slot_index = current_index % total_slots;
        int row = slot_index / GRID_COLS;
        int col = slot_index % GRID_COLS;

        // Get character
        char ch = current_text[current_index];
        char str[2] = {ch, '\0'};

        // Update slot
        lv_obj_t* slot = slots[row][col];
        lv_obj_t* label = lv_obj_get_child(slot, 0);
        
        // Animate background color
        lv_obj_set_style_bg_color(slot, lv_color_hex(0x4A4A4A), 0);
        
        // Set character with animation
        lv_label_set_text(label, str);
        
        // Create drop animation
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, label);
        lv_anim_set_values(&anim, -20, 0);
        lv_anim_set_time(&anim, 300);
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)anim_y_cb);
        lv_anim_set_path_cb(&anim, lv_anim_path_bounce);
        lv_anim_start(&anim);

        // Fade in animation
        lv_anim_t anim_opa;
        lv_anim_init(&anim_opa);
        lv_anim_set_var(&anim_opa, label);
        lv_anim_set_values(&anim_opa, 0, 255);
        lv_anim_set_time(&anim_opa, 200);
        lv_anim_set_exec_cb(&anim_opa, (lv_anim_exec_xcb_t)anim_opa_cb);
        lv_anim_start(&anim_opa);

        current_index++;
    }

    static void anim_y_cb(void* var, int32_t value) {
        lv_obj_set_y((lv_obj_t*)var, value);
    }

    static void anim_opa_cb(void* var, int32_t value) {
        lv_obj_set_style_opa((lv_obj_t*)var, value, 0);
    }
};

// App interface for M5Stack Tab5 demo
extern "C" {
    static SimpleGridBoard* grid_board = nullptr;
    static lv_obj_t* app_screen = nullptr;

    void grid_board_app_init(lv_obj_t* parent) {
        // Create app screen
        app_screen = lv_obj_create(parent);
        lv_obj_set_size(app_screen, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(app_screen, lv_color_hex(0x000000), 0);
        lv_obj_clear_flag(app_screen, LV_OBJ_FLAG_SCROLLABLE);

        // Initialize grid board
        if (!grid_board) {
            grid_board = new SimpleGridBoard();
        }
        grid_board->init(app_screen);

        // Display welcome message
        grid_board->display_text("WELCOME TO M5STACK TAB5 GRID BOARD DEMO!");
    }

    void grid_board_app_update(const char* text) {
        if (grid_board && text) {
            grid_board->display_text(text);
        }
    }

    void grid_board_app_deinit() {
        if (app_screen) {
            lv_obj_del(app_screen);
            app_screen = nullptr;
        }
        if (grid_board) {
            delete grid_board;
            grid_board = nullptr;
        }
    }

    // Demo messages to cycle through
    const char* grid_board_demo_messages[] = {
        "HELLO M5STACK TAB5!",
        "ESP32-P4 POWERED",
        "1280 X 720 DISPLAY",
        "GRID BOARD DEMO",
        "LVGL ANIMATIONS",
        "TOUCH ENABLED",
        "5 INCH IPS SCREEN",
        "DEVELOPED BY ERIC NAM",
        "PORTED TO TAB5",
        nullptr
    };

    int current_demo_message = 0;

    void grid_board_app_demo_cycle() {
        if (!grid_board) return;
        
        if (grid_board_demo_messages[current_demo_message] == nullptr) {
            current_demo_message = 0;
        }
        
        grid_board->display_text(grid_board_demo_messages[current_demo_message]);
        current_demo_message++;
    }
}