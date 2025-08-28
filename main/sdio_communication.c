/**
 * @file sdio_communication.c
 * @brief ESP32-P4 to ESP32-C6 SDIO Communication Implementation for M5Stack Tab5
 */

#include "sdio_communication.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "TAB5_SDIO";

// Forward declarations
static void sdio_communication_task(void *arg);
static esp_err_t init_c6_hardware(void);
static esp_err_t init_sdio_host(tab5_sdio_handle_t *handle);

/**
 * Initialize C6 hardware control pins
 */
static esp_err_t init_c6_hardware(void)
{
    ESP_LOGI(TAG, "Initializing C6 hardware control pins");
    
    // Configure C6 control pins
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << C6_RESET_GPIO) | (1ULL << C6_BOOT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));
    
    // Set C6 to normal boot mode (GPIO2=1 for SPI boot)
    gpio_set_level(C6_BOOT_GPIO, 1);
    
    // Ensure C6 is not in reset
    gpio_set_level(C6_RESET_GPIO, 1);
    
    ESP_LOGI(TAG, "C6 hardware control pins initialized");
    return ESP_OK;
}

/**
 * Initialize SDIO host interface
 */
static esp_err_t init_sdio_host(tab5_sdio_handle_t *handle)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing SDIO host for Tab5");
    
    // Configure SDMMC host (following ESP-Hosted MCU recommendations)
    handle->host = (sdmmc_host_t)SDMMC_HOST_DEFAULT();
    handle->host.slot = SDMMC_HOST_SLOT_1;  // Use slot 1 for Tab5 pins
    handle->host.max_freq_khz = 5000;  // Start at 5MHz as recommended by ESP-Hosted
    handle->host.flags = SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_DEINIT_ARG;  // 4-bit mode
    
    // Configure SDIO slot
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;  // 4-bit SDIO
    slot_config.gpio_cd = GPIO_NUM_NC;  // No card detect
    slot_config.gpio_wp = GPIO_NUM_NC;  // No write protect
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;  // Use internal pull-ups
    
    // Custom pin mapping for Tab5
    slot_config.clk = TAB5_SDIO_CLK_GPIO;
    slot_config.cmd = TAB5_SDIO_CMD_GPIO;
    slot_config.d0 = TAB5_SDIO_D0_GPIO;
    slot_config.d1 = TAB5_SDIO_D1_GPIO;
    slot_config.d2 = TAB5_SDIO_D2_GPIO;
    slot_config.d3 = TAB5_SDIO_D3_GPIO;
    
    // Allocate card structure
    handle->card = (sdmmc_card_t *)calloc(1, sizeof(sdmmc_card_t));
    if (!handle->card) {
        ESP_LOGE(TAG, "Failed to allocate SDIO card structure");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize host
    ret = (handle->host.init)();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize host: %s", esp_err_to_name(ret));
        free(handle->card);
        handle->card = NULL;
        return ret;
    }
    
    // Initialize slot
    ret = sdmmc_host_init_slot(handle->host.slot, &slot_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize slot: %s", esp_err_to_name(ret));
        (handle->host.deinit)();
        free(handle->card);
        handle->card = NULL;
        return ret;
    }
    
    // Probe and initialize card
    ret = sdmmc_card_init(&handle->host, handle->card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO card: %s", esp_err_to_name(ret));
        (handle->host.deinit)();
        free(handle->card);
        handle->card = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "SDIO card initialized successfully");
    
    return ESP_OK;
}

/**
 * SDIO communication task
 */
static void sdio_communication_task(void *arg)
{
    tab5_sdio_handle_t *handle = (tab5_sdio_handle_t *)arg;
    sdio_packet_t packet;
    
    ESP_LOGI(TAG, "SDIO communication task started");
    
    while (1) {
        // Check for data to transmit
        if (xQueueReceive(handle->tx_queue, &packet, 10 / portTICK_PERIOD_MS) == pdTRUE) {
            // Send data via SDIO
            esp_err_t ret = sdmmc_io_write_bytes(handle->card, 1, 0, &packet.data[0], packet.length);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send SDIO data: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGD(TAG, "Sent %d bytes via SDIO", packet.length);
            }
        }
        
        // Check for incoming data (polling for now)
        uint8_t status;
        esp_err_t ret = sdmmc_io_read_byte(handle->card, 1, TAB5_REG_STATUS, &status);
        if (ret == ESP_OK && (status & 0x01)) {  // Data available flag
            // Read data length
            uint32_t data_len;
            ret = sdmmc_io_read_bytes(handle->card, 1, TAB5_REG_DATA_LEN, &data_len, 4);
            if (ret == ESP_OK && data_len > 0 && data_len <= SDIO_BUFFER_SIZE) {
                // Read data
                packet.length = data_len;
                ret = sdmmc_io_read_bytes(handle->card, 1, 0x100, packet.data, data_len);
                if (ret == ESP_OK) {
                    ESP_LOGD(TAG, "Received %d bytes via SDIO", data_len);
                    xQueueSend(handle->rx_queue, &packet, 0);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * Initialize SDIO communication with ESP32-C6
 */
esp_err_t tab5_sdio_init(tab5_sdio_handle_t *handle)
{
    esp_err_t ret;
    
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing Tab5 SDIO communication");
    
    // Initialize hardware control
    ret = init_c6_hardware();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Reset C6 before initialization
    ESP_LOGI(TAG, "Resetting ESP32-C6");
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(500));  // Wait for C6 to boot
    
    // Initialize SDIO host
    ret = init_sdio_host(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDIO host");
        return ret;
    }
    
    // Create communication queues
    handle->tx_queue = xQueueCreate(SDIO_QUEUE_SIZE, sizeof(sdio_packet_t));
    handle->rx_queue = xQueueCreate(SDIO_QUEUE_SIZE, sizeof(sdio_packet_t));
    
    if (!handle->tx_queue || !handle->rx_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
        tab5_sdio_deinit(handle);
        return ESP_ERR_NO_MEM;
    }
    
    // Create communication task
    xTaskCreate(sdio_communication_task, "sdio_comm", 4096, handle, 10, &handle->comm_task);
    
    handle->is_initialized = true;
    handle->c6_ready = false;
    
    // Check if C6 is ready
    uint8_t status;
    ret = tab5_c6_read_status(handle, &status);
    if (ret == ESP_OK) {
        handle->c6_ready = (status == 0xAA);  // Ready signature
        ESP_LOGI(TAG, "C6 ready status: %s (0x%02x)", handle->c6_ready ? "YES" : "NO", status);
    }
    
    ESP_LOGI(TAG, "Tab5 SDIO communication initialized");
    return ESP_OK;
}

/**
 * Deinitialize SDIO communication
 */
void tab5_sdio_deinit(tab5_sdio_handle_t *handle)
{
    if (!handle || !handle->is_initialized) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing SDIO communication");
    
    // Stop communication task
    if (handle->comm_task) {
        vTaskDelete(handle->comm_task);
        handle->comm_task = NULL;
    }
    
    // Delete queues
    if (handle->tx_queue) {
        vQueueDelete(handle->tx_queue);
        handle->tx_queue = NULL;
    }
    if (handle->rx_queue) {
        vQueueDelete(handle->rx_queue);
        handle->rx_queue = NULL;
    }
    
    // Deinitialize SDIO
    if (handle->card) {
        (handle->host.deinit)();
        free(handle->card);
        handle->card = NULL;
    }
    
    handle->is_initialized = false;
    handle->c6_ready = false;
    
    ESP_LOGI(TAG, "SDIO communication deinitialized");
}

/**
 * Reset ESP32-C6 and reinitialize communication
 */
esp_err_t tab5_c6_reset(tab5_sdio_handle_t *handle)
{
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Resetting ESP32-C6");
    
    // Toggle reset pin
    gpio_set_level(C6_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(C6_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for C6 to boot
    
    // Re-initialize SDIO
    if (handle->card) {
        esp_err_t ret = sdmmc_card_init(&handle->host, handle->card);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to reinitialize SDIO after reset: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Check if C6 is ready
    uint8_t status;
    esp_err_t ret = tab5_c6_read_status(handle, &status);
    if (ret == ESP_OK) {
        handle->c6_ready = (status == 0xAA);
        ESP_LOGI(TAG, "C6 ready after reset: %s", handle->c6_ready ? "YES" : "NO");
    }
    
    return ESP_OK;
}

/**
 * Send data packet to ESP32-C6
 */
esp_err_t tab5_sdio_send(tab5_sdio_handle_t *handle, const sdio_packet_t *packet)
{
    if (!handle || !handle->is_initialized || !packet) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->c6_ready) {
        ESP_LOGW(TAG, "C6 not ready, cannot send packet");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xQueueSend(handle->tx_queue, packet, pdMS_TO_TICKS(SDIO_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to queue packet for transmission");
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

/**
 * Receive data packet from ESP32-C6
 */
esp_err_t tab5_sdio_receive(tab5_sdio_handle_t *handle, sdio_packet_t *packet, uint32_t timeout_ms)
{
    if (!handle || !handle->is_initialized || !packet) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xQueueReceive(handle->rx_queue, packet, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

/**
 * Write command to ESP32-C6
 */
esp_err_t tab5_c6_write_command(tab5_sdio_handle_t *handle, tab5_command_t cmd)
{
    if (!handle || !handle->is_initialized || !handle->card) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t cmd_byte = (uint8_t)cmd;
    return sdmmc_io_write_byte(handle->card, 1, TAB5_REG_COMMAND, cmd_byte, NULL);
}

/**
 * Read status from ESP32-C6
 */
esp_err_t tab5_c6_read_status(tab5_sdio_handle_t *handle, uint8_t *status)
{
    if (!handle || !handle->is_initialized || !handle->card || !status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return sdmmc_io_read_byte(handle->card, 1, TAB5_REG_STATUS, status);
}

/**
 * Get C6 firmware version
 */
esp_err_t tab5_c6_get_fw_version(tab5_sdio_handle_t *handle, char *version, size_t max_len)
{
    if (!handle || !handle->is_initialized || !handle->card || !version || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t fw_bytes[32];
    esp_err_t ret = sdmmc_io_read_bytes(handle->card, 1, TAB5_REG_FW_VERSION, fw_bytes, 32);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Ensure null termination
    fw_bytes[31] = '\0';
    strncpy(version, (char *)fw_bytes, max_len - 1);
    version[max_len - 1] = '\0';
    
    return ESP_OK;
}

/**
 * Check if C6 is ready for communication
 */
bool tab5_c6_is_ready(tab5_sdio_handle_t *handle)
{
    if (!handle || !handle->is_initialized) {
        return false;
    }
    
    return handle->c6_ready;
}

/**
 * Initialize WiFi on ESP32-C6
 */
esp_err_t tab5_c6_wifi_init(tab5_sdio_handle_t *handle)
{
    if (!handle || !handle->is_initialized || !handle->c6_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send WiFi init command
    esp_err_t ret = tab5_c6_write_command(handle, CMD_WIFI_CONNECT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send WiFi init command");
        return ret;
    }
    
    // Wait for response
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Check WiFi status
    uint8_t wifi_status;
    ret = sdmmc_io_read_byte(handle->card, 1, TAB5_REG_WIFI_STATUS, &wifi_status);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi status: 0x%02x", wifi_status);
    return (wifi_status != 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Connect to WiFi network via C6
 */
esp_err_t tab5_c6_wifi_connect(tab5_sdio_handle_t *handle, const char *ssid, const char *password)
{
    if (!handle || !handle->is_initialized || !handle->c6_ready || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Prepare connection packet
    sdio_packet_t packet = {0};
    packet.type = CMD_WIFI_CONNECT;
    
    // Format: SSID\0Password\0
    size_t ssid_len = strlen(ssid);
    size_t pass_len = password ? strlen(password) : 0;
    
    if (ssid_len + pass_len + 2 > SDIO_BUFFER_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    memcpy(packet.data, ssid, ssid_len);
    packet.data[ssid_len] = '\0';
    if (password) {
        memcpy(&packet.data[ssid_len + 1], password, pass_len);
    }
    packet.data[ssid_len + 1 + pass_len] = '\0';
    packet.length = ssid_len + pass_len + 2;
    
    return tab5_sdio_send(handle, &packet);
}