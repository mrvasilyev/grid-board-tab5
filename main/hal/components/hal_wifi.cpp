/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <mooncake_log.h>
#include <vector>
#include <memory>
#include <string.h>
#include <bsp/m5stack_tab5.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include "sdkconfig.h"
#include "../../wifi_compat.h"  // Include compatibility types before WiFi headers
#if CONFIG_TAB5_WIFI_REMOTE_ENABLE
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#endif

#define TAG "wifi"

#define WIFI_SSID    "LSEQ2G"
#define WIFI_PASS    "JuLiA.1984"
#define MAX_STA_CONN 4

// Event group for WiFi connection
#if CONFIG_TAB5_WIFI_REMOTE_ENABLE
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;
#define ESP_MAXIMUM_RETRY  5
#endif

#if CONFIG_TAB5_WIFI_REMOTE_ENABLE
// HTTP 处理函数
esp_err_t hello_get_handler(httpd_req_t* req)
{
    const char* html_response = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Hello</title>
            <style>
                body {
                    display: flex;
                    flex-direction: column;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    margin: 0;
                    font-family: sans-serif;
                    background-color: #f0f0f0;
                }
                h1 {
                    font-size: 48px;
                    color: #333;
                    margin: 0;
                }
                p {
                    font-size: 18px;
                    color: #666;
                    margin-top: 10px;
                }
            </style>
        </head>
        <body>
            <h1>Hello World</h1>
            <p>From M5Tab5</p>
        </body>
        </html>
    )rawliteral";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// URI 路由
httpd_uri_t hello_uri = {.uri = "/", .method = HTTP_GET, .handler = hello_get_handler, .user_ctx = nullptr};

// 启动 Web Server
httpd_handle_t start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = nullptr;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello_uri);
    }
    return server;
}
#endif // CONFIG_TAB5_WIFI_REMOTE_ENABLE

#if CONFIG_TAB5_WIFI_REMOTE_ENABLE
// WiFi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, attempting to connect to SSID: %s", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", event->reason);
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry %d/%d to connect to AP: %s", s_retry_num, ESP_MAXIMUM_RETRY, WIFI_SSID);
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", ESP_MAXIMUM_RETRY);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "*** WiFi Connected Successfully! ***");
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
#endif

// Initialize Wi-Fi Station mode
static void wifi_init_sta()
{
#if !CONFIG_TAB5_WIFI_REMOTE_ENABLE
    return;
#else
    ESP_LOGI(TAG, "Starting WiFi Station initialization...");
    ESP_LOGI(TAG, "Target SSID: %s", WIFI_SSID);
    
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return;
    }

    ESP_LOGI(TAG, "Initializing network interface...");
    
    // Initialize TCP/IP stack (can be called multiple times safely)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    
    // Create event loop only if it doesn't exist
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    
    // Create default WiFi station if not already created
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL) {
        esp_netif_create_default_wifi_sta();
    }

    ESP_LOGI(TAG, "Initializing WiFi with default config...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_LOGI(TAG, "Registering event handlers...");
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_LOGI(TAG, "Configuring WiFi credentials...");
    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID, sizeof(wifi_config.sta.ssid));
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_LOGI(TAG, "Setting WiFi mode to STA and applying config...");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_LOGI(TAG, "Starting WiFi...");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi started successfully, waiting for connection...");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    ESP_LOGI(TAG, "Waiting for connection result...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(30000));  // 30 second timeout

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "=== WiFi Connection Success ===");
        ESP_LOGI(TAG, "Connected to AP: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "=== WiFi Connection Failed ===");
        ESP_LOGE(TAG, "Failed to connect to SSID: %s after %d retries", WIFI_SSID, ESP_MAXIMUM_RETRY);
    } else {
        ESP_LOGE(TAG, "=== WiFi Connection Timeout ===");
        ESP_LOGE(TAG, "Connection attempt timed out after 30 seconds");
    }
#endif
}

// 初始化 Wi-Fi AP 模式
static void wifi_init_softap()
{
#if !CONFIG_TAB5_WIFI_REMOTE_ENABLE
    return;
#else
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), WIFI_SSID, sizeof(wifi_config.ap.ssid));
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.password), WIFI_PASS, sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len       = std::strlen(WIFI_SSID);
    wifi_config.ap.max_connection = MAX_STA_CONN;
    wifi_config.ap.authmode       = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
    return;
#endif
}

static void wifi_sta_task(void* param)
{
    mclog::tagInfo(TAG, "WiFi station task entry point reached!");
    
#if CONFIG_TAB5_WIFI_REMOTE_ENABLE
    ESP_LOGI(TAG, "=== WiFi Station Task Started ===");
    ESP_LOGI(TAG, "Task running on core: %d", xPortGetCoreID());
    
    // Small delay to ensure system is ready
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    wifi_init_sta();
    
#if CONFIG_TAB5_WIFI_REMOTE_ENABLE
    // Only start webserver after successful connection
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Starting web server on connected network...");
        httpd_handle_t server = start_webserver();
        if (server) {
            ESP_LOGI(TAG, "*** Web server started successfully! ***");
            ESP_LOGI(TAG, "You can now access the device via web browser");
        } else {
            ESP_LOGE(TAG, "Failed to start web server");
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected, web server not started");
    }
#endif

    // Keep task alive and periodically report status
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Log every 30 seconds
        
#if CONFIG_TAB5_WIFI_REMOTE_ENABLE        
        // Periodically report WiFi status
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi Status: Connected to %s, RSSI: %d dBm", 
                    (char*)ap_info.ssid, ap_info.rssi);
        } else {
            ESP_LOGI(TAG, "WiFi Status: Not connected");
        }
#endif
    }
#endif
    vTaskDelete(NULL);
}

bool HalEsp32::wifi_init()
{
    static bool wifi_initialized = false;
    
    mclog::tagInfo(TAG, "wifi init");
#if !CONFIG_TAB5_WIFI_REMOTE_ENABLE
    // Skipped by configuration
    return false;
#else
    // Prevent duplicate initialization
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized, skipping...");
        return true;
    }
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Creating WiFi station task...");
    BaseType_t result = xTaskCreate(wifi_sta_task, "wifi_sta", 8192, nullptr, 5, nullptr);
    if (result == pdPASS) {
        ESP_LOGI(TAG, "WiFi station task created successfully");
        wifi_initialized = true;
    } else {
        ESP_LOGE(TAG, "Failed to create WiFi station task!");
    }
    return wifi_initialized;
#endif
}

void HalEsp32::setExtAntennaEnable(bool enable)
{
    _ext_antenna_enable = enable;
    mclog::tagInfo(TAG, "set ext antenna enable: {}", _ext_antenna_enable);
    bsp_set_ext_antenna_enable(_ext_antenna_enable);
}

bool HalEsp32::getExtAntennaEnable()
{
    return _ext_antenna_enable;
}

void HalEsp32::startWifiAp()
{
    wifi_init();
}
