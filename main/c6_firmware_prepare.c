/**
 * @file c6_firmware_prepare.c
 * @brief Prepare SD card with C6 firmware from P4 internal storage
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_err.h"
#include "sd_card_helper.h"

static const char *TAG = "C6_FW_PREPARE";

// SD Card paths
#define C6_FIRMWARE_FILENAME "c6_firmware.bin"
#define C6_FIRMWARE_BACKUP_FILENAME "c6_firmware_backup.bin"  

// Embedded C6 firmware (will be linked from external file)
extern const uint8_t c6_firmware_start[] asm("_binary_c6_firmware_bin_start");
extern const uint8_t c6_firmware_end[] asm("_binary_c6_firmware_bin_end");


/**
 * Check if C6 firmware needs to be copied to SD card
 */
esp_err_t c6_prepare_firmware_on_sd(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "C6 Firmware SD Card Preparation");
    ESP_LOGI(TAG, "==============================================");
    
    // Initialize SD card
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available: %s", esp_err_to_name(ret));
        return ret;
    }
    
    const char *mount_point = sd_card_get_mount_point();
    ESP_LOGI(TAG, "SD card mount point: %s", mount_point);
    
    // Check if mount point exists and is accessible
    struct stat mount_stat;
    if (stat(mount_point, &mount_stat) == 0) {
        ESP_LOGI(TAG, "Mount point exists, is %s", S_ISDIR(mount_stat.st_mode) ? "directory" : "not a directory");
    } else {
        ESP_LOGE(TAG, "Cannot stat mount point: errno=%d", errno);
    }
    
    char firmware_path[256];
    char backup_path[256];
    snprintf(firmware_path, sizeof(firmware_path), "%s/%s", mount_point, C6_FIRMWARE_FILENAME);
    snprintf(backup_path, sizeof(backup_path), "%s/%s", mount_point, C6_FIRMWARE_BACKUP_FILENAME);
    
    ESP_LOGI(TAG, "Firmware path will be: %s", firmware_path);
    ESP_LOGI(TAG, "Backup path will be: %s", backup_path);
    
    // Check if firmware already exists
    struct stat file_stat;
    bool firmware_exists = (stat(firmware_path, &file_stat) == 0);
    bool backup_exists = (stat(backup_path, &file_stat) == 0);
    
    if (firmware_exists) {
        ESP_LOGI(TAG, "C6 firmware already exists on SD card");
        ESP_LOGI(TAG, "Size: %ld bytes", file_stat.st_size);
        return ESP_OK;
    }
    
    if (backup_exists) {
        ESP_LOGI(TAG, "C6 firmware backup exists, firmware was already flashed");
        return ESP_OK;
    }
    
    // Calculate embedded firmware size
    size_t firmware_size = c6_firmware_end - c6_firmware_start;
    ESP_LOGI(TAG, "Embedded C6 firmware size: %d bytes", firmware_size);
    
    if (firmware_size == 0 || firmware_size > 2*1024*1024) {
        ESP_LOGE(TAG, "Invalid embedded firmware size");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Copy firmware to SD card
    ESP_LOGI(TAG, "Copying C6 firmware to SD card...");
    
    // Test if we can create a simple test file first
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "%s/test.txt", mount_point);
    ESP_LOGI(TAG, "Testing file creation with: %s", test_path);
    FILE *test_f = fopen(test_path, "w");
    if (test_f) {
        fprintf(test_f, "Test file\n");
        fclose(test_f);
        ESP_LOGI(TAG, "Test file created successfully, removing it");
        remove(test_path);
    } else {
        ESP_LOGE(TAG, "Failed to create test file: errno=%d", errno);
        perror("Test file creation");
    }
    
    ESP_LOGI(TAG, "Creating firmware file: %s", firmware_path);
    FILE *f = fopen(firmware_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create firmware file: %s (errno=%d)", firmware_path, errno);
        perror("fopen");
        
        // Try alternative approach - create in chunks
        ESP_LOGI(TAG, "Trying alternative: creating empty file first");
        f = fopen(firmware_path, "w");
        if (f) {
            fclose(f);
            f = fopen(firmware_path, "ab");  // Append binary mode
        }
        
        if (!f) {
            ESP_LOGE(TAG, "Alternative approach also failed");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Using alternative file creation method");
    }
    
    size_t written = fwrite(c6_firmware_start, 1, firmware_size, f);
    fclose(f);
    
    if (written == firmware_size) {
        ESP_LOGI(TAG, "✅ C6 firmware copied to SD card successfully!");
        ESP_LOGI(TAG, "Path: %s", firmware_path);
        ESP_LOGI(TAG, "Size: %d bytes", written);
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "The firmware will be automatically flashed to C6 on next boot");
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to write complete firmware (wrote %d of %d bytes)", written, firmware_size);
        // Try to remove incomplete file
        remove(firmware_path);
        ret = ESP_FAIL;
    }
    
    return ret;
}

/**
 * Remove C6 firmware files from SD card (for testing)
 */
esp_err_t c6_cleanup_firmware_on_sd(void)
{
    esp_err_t ret = sd_card_init();
    if (ret != ESP_OK) {
        return ret;
    }
    
    ESP_LOGI(TAG, "Cleaning up C6 firmware files from SD card");
    
    const char *mount_point = sd_card_get_mount_point();
    char firmware_path[256];
    char backup_path[256];
    snprintf(firmware_path, sizeof(firmware_path), "%s/%s", mount_point, C6_FIRMWARE_FILENAME);
    snprintf(backup_path, sizeof(backup_path), "%s/%s", mount_point, C6_FIRMWARE_BACKUP_FILENAME);
    
    if (remove(firmware_path) == 0) {
        ESP_LOGI(TAG, "Removed %s", firmware_path);
    }
    
    if (remove(backup_path) == 0) {
        ESP_LOGI(TAG, "Removed %s", backup_path);
    }
    
    return ESP_OK;
}

/**
 * Get C6 firmware status on SD card
 */
esp_err_t c6_get_firmware_status(void)
{
    esp_err_t ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
        return ret;
    }
    
    const char *mount_point = sd_card_get_mount_point();
    char firmware_path[256];
    char backup_path[256];
    snprintf(firmware_path, sizeof(firmware_path), "%s/%s", mount_point, C6_FIRMWARE_FILENAME);
    snprintf(backup_path, sizeof(backup_path), "%s/%s", mount_point, C6_FIRMWARE_BACKUP_FILENAME);
    
    struct stat file_stat;
    
    ESP_LOGI(TAG, "C6 Firmware Status on SD Card:");
    ESP_LOGI(TAG, "-------------------------------");
    
    if (stat(firmware_path, &file_stat) == 0) {
        ESP_LOGI(TAG, "✓ Firmware ready: %s (%ld bytes)", firmware_path, file_stat.st_size);
        ESP_LOGI(TAG, "  Will be flashed on next boot");
    } else {
        ESP_LOGI(TAG, "✗ No firmware at %s", firmware_path);
    }
    
    if (stat(backup_path, &file_stat) == 0) {
        ESP_LOGI(TAG, "✓ Backup exists: %s (%ld bytes)", backup_path, file_stat.st_size);
        ESP_LOGI(TAG, "  Firmware was already flashed");
    } else {
        ESP_LOGI(TAG, "✗ No backup at %s", backup_path);
    }
    
    // Calculate embedded firmware size
    size_t embedded_size = c6_firmware_end - c6_firmware_start;
    ESP_LOGI(TAG, "Embedded firmware: %d bytes", embedded_size);
    
    return ESP_OK;
}