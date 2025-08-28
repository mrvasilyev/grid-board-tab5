/**
 * @file delete_backup.c
 * @brief Delete C6 firmware backup to exit bridge mode
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "sd_card_helper.h"

static const char *TAG = "DELETE_BACKUP";

esp_err_t delete_c6_backup(void)
{
    esp_err_t ret;
    
    // Initialize SD card
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SD card: %s", esp_err_to_name(ret));
        return ret;
    }
    
    const char *mount_point = sd_card_get_mount_point();
    char backup_path[256];
    snprintf(backup_path, sizeof(backup_path), "%s/c6_firmware_backup.bin", mount_point);
    
    ESP_LOGI(TAG, "Deleting backup file: %s", backup_path);
    
    if (remove(backup_path) == 0) {
        ESP_LOGI(TAG, "✅ Backup file deleted successfully!");
        ESP_LOGI(TAG, "Bridge mode will be disabled on next reboot");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to delete backup file");
        return ESP_FAIL;
    }
}