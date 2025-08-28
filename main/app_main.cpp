/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <app.h>
#include <hal/hal.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "app_c6_ota.h"

// Workaround for I2C driver conflict - initialize immediately
extern "C" bool g_i2c_master_bus_created_by_new_driver[2] = {true, true};

// Use constructor attribute to set flags before any initialization
__attribute__((constructor(101))) 
static void i2c_driver_workaround_init(void)
{
    g_i2c_master_bus_created_by_new_driver[0] = true;
    g_i2c_master_bus_created_by_new_driver[1] = true;
}

extern "C" void app_main(void)
{
    
    // 应用层初始化回调
    app::InitCallback_t callback;

    callback.onHalInjection = []() {
        // 注入桌面平台的硬件抽象
        hal::Inject(std::make_unique<HalEsp32>());
    };

    // 应用层启动
    app::Init(callback);
    
    // Start ESP32-C6 OTA update task (for testing)
    // Comment this out after successful update
    #ifdef CONFIG_TAB5_WIFI_REMOTE_ENABLE
    start_c6_firmware_update();
    #endif
    
    while (!app::IsDone()) {
        app::Update();
        vTaskDelay(1);
    }
    app::Destroy();
}
