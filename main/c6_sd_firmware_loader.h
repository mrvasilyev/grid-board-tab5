/**
 * @file c6_sd_firmware_loader.h
 * @brief SD Card-based firmware loader for ESP32-C6 on M5Stack Tab5
 */

#ifndef C6_SD_FIRMWARE_LOADER_H
#define C6_SD_FIRMWARE_LOADER_H

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check for C6 firmware on SD card and update if found
 * 
 * Looks for /sdcard/c6_firmware.bin and flashes it to C6 if present
 * After successful update, renames file to c6_firmware_backup.bin
 * 
 * @return ESP_OK on successful update, ESP_ERR_NOT_FOUND if no firmware, error otherwise
 */
esp_err_t c6_check_and_update_firmware(void);

/**
 * @brief Flash firmware from SD card to C6
 * 
 * @param firmware_path Path to firmware file on SD card
 * @return ESP_OK on success
 */
esp_err_t c6_flash_firmware_from_sd(const char *firmware_path);

/**
 * @brief Copy firmware from internal storage to SD card
 * 
 * Useful for preparing SD card with firmware for future updates
 * 
 * @param firmware_data Pointer to firmware data
 * @param firmware_size Size of firmware in bytes
 * @return ESP_OK on success
 */
esp_err_t c6_copy_firmware_to_sd(const uint8_t *firmware_data, size_t firmware_size);

#ifdef __cplusplus
}
#endif

#endif // C6_SD_FIRMWARE_LOADER_H