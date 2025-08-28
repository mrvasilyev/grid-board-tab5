/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start ESP32-C6 firmware OTA update
 * 
 * This function initiates an OTA (Over-The-Air) update for the ESP32-C6
 * WiFi co-processor. It creates a task that downloads and flashes the
 * firmware from a HTTP server.
 * 
 * Prerequisites:
 * - HTTP server must be running with the firmware file
 * - ESP32-P4 must be connected to the network (or will connect via C6)
 * - CONFIG_TAB5_WIFI_REMOTE_ENABLE must be set
 */
void start_c6_firmware_update(void);

#ifdef __cplusplus
}
#endif