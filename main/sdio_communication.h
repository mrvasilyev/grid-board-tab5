/**
 * @file sdio_communication.h
 * @brief ESP32-P4 to ESP32-C6 SDIO Communication Interface for M5Stack Tab5
 * 
 * This module provides SDIO communication between ESP32-P4 (host) and ESP32-C6 (slave)
 * Compatible with ESP-Hosted MCU (https://github.com/espressif/esp-hosted-mcu)
 * The C6 should be flashed with ESP-Hosted SDIO slave firmware
 */

#ifndef SDIO_COMMUNICATION_H
#define SDIO_COMMUNICATION_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// M5Stack Tab5 SDIO Pin Definitions (ESP32-P4 Host)
#define TAB5_SDIO_CLK_GPIO    GPIO_NUM_12  // P4 G12 → C6 CLK
#define TAB5_SDIO_CMD_GPIO    GPIO_NUM_13  // P4 G13 → C6 CMD
#define TAB5_SDIO_D0_GPIO     GPIO_NUM_11  // P4 G11 → C6 D0
#define TAB5_SDIO_D1_GPIO     GPIO_NUM_10  // P4 G10 → C6 D1
#define TAB5_SDIO_D2_GPIO     GPIO_NUM_9   // P4 G9  → C6 D2
#define TAB5_SDIO_D3_GPIO     GPIO_NUM_8   // P4 G8  → C6 D3

// ESP32-C6 Control Pins
#define C6_RESET_GPIO         GPIO_NUM_15  // P4 G15 → C6 RESET
#define C6_BOOT_GPIO          GPIO_NUM_14  // P4 G14 → C6 GPIO2 (boot mode)

// SDIO Communication Parameters
#define SDIO_BUFFER_SIZE      2048
#define SDIO_QUEUE_SIZE       10
#define SDIO_MAX_FREQ_KHZ     20000  // Start conservative, can increase to 40000
#define SDIO_TIMEOUT_MS       5000

// Control Register Addresses for Tab5
#define TAB5_REG_STATUS       0x00
#define TAB5_REG_COMMAND      0x01
#define TAB5_REG_DATA_LEN     0x02
#define TAB5_REG_WIFI_STATUS  0x10
#define TAB5_REG_BT_STATUS    0x11
#define TAB5_REG_FW_VERSION   0x20

// Command Types
typedef enum {
    CMD_NONE = 0,
    CMD_GET_STATUS,
    CMD_WIFI_CONNECT,
    CMD_WIFI_DISCONNECT,
    CMD_WIFI_SCAN,
    CMD_BT_ENABLE,
    CMD_BT_DISABLE,
    CMD_DATA_TRANSFER,
    CMD_FW_UPDATE,
    CMD_RESET
} tab5_command_t;

// Data Packet Structure
typedef struct {
    uint8_t data[SDIO_BUFFER_SIZE];
    size_t length;
    uint8_t type;  // Data type identifier
} sdio_packet_t;

// SDIO Communication Handle
typedef struct {
    sdmmc_host_t host;
    sdmmc_card_t *card;
    TaskHandle_t comm_task;
    QueueHandle_t tx_queue;
    QueueHandle_t rx_queue;
    bool is_initialized;
    bool c6_ready;
} tab5_sdio_handle_t;

// Main API Functions

/**
 * @brief Initialize SDIO communication with ESP32-C6
 * @param handle Pointer to SDIO handle structure
 * @return ESP_OK on success
 */
esp_err_t tab5_sdio_init(tab5_sdio_handle_t *handle);

/**
 * @brief Deinitialize SDIO communication
 * @param handle SDIO handle
 */
void tab5_sdio_deinit(tab5_sdio_handle_t *handle);

/**
 * @brief Reset ESP32-C6 and reinitialize communication
 * @param handle SDIO handle
 * @return ESP_OK on success
 */
esp_err_t tab5_c6_reset(tab5_sdio_handle_t *handle);

/**
 * @brief Send data packet to ESP32-C6
 * @param handle SDIO handle
 * @param packet Data packet to send
 * @return ESP_OK on success
 */
esp_err_t tab5_sdio_send(tab5_sdio_handle_t *handle, const sdio_packet_t *packet);

/**
 * @brief Receive data packet from ESP32-C6
 * @param handle SDIO handle
 * @param packet Buffer to receive data
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t tab5_sdio_receive(tab5_sdio_handle_t *handle, sdio_packet_t *packet, uint32_t timeout_ms);

/**
 * @brief Write command to ESP32-C6
 * @param handle SDIO handle
 * @param cmd Command to send
 * @return ESP_OK on success
 */
esp_err_t tab5_c6_write_command(tab5_sdio_handle_t *handle, tab5_command_t cmd);

/**
 * @brief Read status from ESP32-C6
 * @param handle SDIO handle
 * @param status Pointer to store status
 * @return ESP_OK on success
 */
esp_err_t tab5_c6_read_status(tab5_sdio_handle_t *handle, uint8_t *status);

/**
 * @brief Get C6 firmware version
 * @param handle SDIO handle
 * @param version Buffer to store version string
 * @param max_len Maximum buffer length
 * @return ESP_OK on success
 */
esp_err_t tab5_c6_get_fw_version(tab5_sdio_handle_t *handle, char *version, size_t max_len);

/**
 * @brief Check if C6 is ready for communication
 * @param handle SDIO handle
 * @return true if ready, false otherwise
 */
bool tab5_c6_is_ready(tab5_sdio_handle_t *handle);

/**
 * @brief Initialize WiFi on ESP32-C6
 * @param handle SDIO handle
 * @return ESP_OK on success
 */
esp_err_t tab5_c6_wifi_init(tab5_sdio_handle_t *handle);

/**
 * @brief Connect to WiFi network via C6
 * @param handle SDIO handle
 * @param ssid Network SSID
 * @param password Network password
 * @return ESP_OK on success
 */
esp_err_t tab5_c6_wifi_connect(tab5_sdio_handle_t *handle, const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif // SDIO_COMMUNICATION_H