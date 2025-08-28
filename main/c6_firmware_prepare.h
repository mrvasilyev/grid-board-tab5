/**
 * @file c6_firmware_prepare.h
 * @brief Prepare SD card with C6 firmware from P4 internal storage
 */

#ifndef C6_FIRMWARE_PREPARE_H
#define C6_FIRMWARE_PREPARE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copy embedded C6 firmware to SD card if not already present
 * 
 * This function checks if c6_firmware.bin exists on the SD card.
 * If not, it copies the embedded firmware from P4's flash to SD card.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t c6_prepare_firmware_on_sd(void);

/**
 * @brief Remove C6 firmware files from SD card (for testing)
 * 
 * Removes both c6_firmware.bin and c6_firmware_backup.bin
 * 
 * @return ESP_OK on success
 */
esp_err_t c6_cleanup_firmware_on_sd(void);

/**
 * @brief Get status of C6 firmware files on SD card
 * 
 * Prints information about firmware files present on SD card
 * 
 * @return ESP_OK if SD card accessible
 */
esp_err_t c6_get_firmware_status(void);

#ifdef __cplusplus
}
#endif

#endif // C6_FIRMWARE_PREPARE_H