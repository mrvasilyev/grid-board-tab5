/**
 * @file grid_board_simple_app.h
 * @brief Grid Board app interface for M5Stack Tab5
 */

#ifndef GRID_BOARD_SIMPLE_APP_H
#define GRID_BOARD_SIMPLE_APP_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Grid Board app
 * @param parent Parent LVGL object
 */
void grid_board_app_init(lv_obj_t* parent);

/**
 * @brief Update Grid Board with new text
 * @param text Text to display (max 60 characters recommended)
 */
void grid_board_app_update(const char* text);

/**
 * @brief Clean up Grid Board app
 */
void grid_board_app_deinit();

/**
 * @brief Cycle through demo messages
 */
void grid_board_app_demo_cycle();

#ifdef __cplusplus
}
#endif

#endif // GRID_BOARD_SIMPLE_APP_H