/**
 * @file sd_card_helper.c
 * @brief SD card helper for Tab5 using native SDMMC interface
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static const char *TAG = "SD_HELPER";

// SD Card mount point
#define MOUNT_POINT "/sdcard"

// Tab5 SD Card pins (using native SDMMC interface)
#define SDMMC_BUS_WIDTH 4
#define GPIO_SDMMC_CLK  GPIO_NUM_43
#define GPIO_SDMMC_CMD  GPIO_NUM_44
#define GPIO_SDMMC_D0   GPIO_NUM_39
#define GPIO_SDMMC_D1   GPIO_NUM_40
#define GPIO_SDMMC_D2   GPIO_NUM_41
#define GPIO_SDMMC_D3   GPIO_NUM_42

// LDO channel for SD card power (LDO_VO4)
#define BSP_LDO_PROBE_SD_CHAN 4

static sdmmc_card_t *sd_card = NULL;
static sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
static bool sd_card_initialized = false;

/**
 * Initialize SD card using native SDMMC interface
 */
esp_err_t sd_card_init(void)
{
    if (sd_card_initialized) {
        return ESP_OK;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing SD card using SDMMC interface");
    
    // Configure host
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    
    // Configure LDO power for SD card
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = BSP_LDO_PROBE_SD_CHAN,
    };
    
    if (pwr_ctrl_handle == NULL) {
        ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create LDO power control: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
    
    // Configure slot
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = SDMMC_BUS_WIDTH;
    slot_config.clk = GPIO_SDMMC_CLK;
    slot_config.cmd = GPIO_SDMMC_CMD;
    slot_config.d0 = GPIO_SDMMC_D0;
    slot_config.d1 = GPIO_SDMMC_D1;
    slot_config.d2 = GPIO_SDMMC_D2;
    slot_config.d3 = GPIO_SDMMC_D3;
    
    // Mount configuration with long filename support
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };
    
    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else if (ret == ESP_ERR_INVALID_RESPONSE) {
            ESP_LOGE(TAG, "SD card not detected. Please insert an SD card.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    sd_card_initialized = true;
    sdmmc_card_print_info(stdout, sd_card);
    
    return ESP_OK;
}

/**
 * Deinitialize SD card
 */
void sd_card_deinit(void)
{
    if (!sd_card_initialized) {
        return;
    }
    
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, sd_card);
    sd_card = NULL;
    sd_card_initialized = false;
    ESP_LOGI(TAG, "SD card unmounted");
}

/**
 * Check if SD card is initialized
 */
bool sd_card_is_initialized(void)
{
    return sd_card_initialized;
}

/**
 * Get mount point
 */
const char* sd_card_get_mount_point(void)
{
    return MOUNT_POINT;
}