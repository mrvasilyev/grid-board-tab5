/**
 * @file sd_card_helper.h
 * @brief SD card helper for Tab5 using native SDMMC interface
 */

#ifndef SD_CARD_HELPER_H
#define SD_CARD_HELPER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SD card using native SDMMC interface
 * 
 * @return ESP_OK on success
 */
esp_err_t sd_card_init(void);

/**
 * @brief Deinitialize SD card
 */
void sd_card_deinit(void);

/**
 * @brief Check if SD card is initialized
 * 
 * @return true if initialized
 */
bool sd_card_is_initialized(void);

/**
 * @brief Get SD card mount point
 * 
 * @return Mount point path (e.g., "/sdcard")
 */
const char* sd_card_get_mount_point(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_HELPER_H