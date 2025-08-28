/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi TX rate configuration
 * 
 * This is a compatibility structure for ESP-IDF 5.4
 * In ESP-IDF 5.5+, this structure is defined in esp_wifi_types_generic.h
 */
#include "esp_idf_version.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
typedef struct {
    bool dcm;           /**< Using DCM rate to send frame */
    bool ersu;          /**< Using ERSU mode (Extended Range Single User) for long distance transmission */
    uint8_t reserved[2]; /**< Reserved for future use */
} wifi_tx_rate_config_t;
#endif

#ifdef __cplusplus
}
#endif