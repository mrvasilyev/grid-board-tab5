# M5Stack Tab5 Grid Board - Display Rotation and LVGL Challenge Summary

## Project Overview
**Device**: M5Stack Tab5 with ESP32-P4
**Display**: 720x1280 native portrait, needs 1280x720 landscape
**Framework**: ESP-IDF v5.5 with LVGL
**Purpose**: Display animated welcome messages in a grid layout for bottom-mounted device

## Key Challenges Encountered and Solutions

### 1. Display Orientation Issues
**Problem**: 
- Native display is 720x1280 (portrait)
- Needed 1280x720 (landscape) for grid layout
- Device mounted on bottom side requiring 180-degree rotation
- User reported "seeing only half of the interface"

**Solution**:
```cpp
// Use software rotation with full frame buffer
bsp_display_cfg_t cfg = {
    .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
    .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,  // Full frame buffer
    .double_buffer = true,
    .flags = {
        .buff_dma = true,      // Enable DMA (not disabled!)
        .buff_spiram = true,
        .sw_rotate = true,     // Enable software rotation
    }
};

// Apply rotation AFTER display init
lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);  // For normal landscape
// Use LV_DISPLAY_ROTATION_270 for inverted landscape (bottom mounting)
```

### 2. LVGL Rendering Assertion Errors
**Problem**: 
```
[Error] lv_inv_area: Asserted at expression: !disp->rendering_in_progress 
(Invalidate area is not allowed during rendering.)
```

**Root Cause**: Creating/modifying UI elements while LVGL was actively rendering

**Failed Attempts**:
1. Using `bsp_display_lock(0)` around UI creation - still crashed
2. Adding delays after display initialization - insufficient
3. Creating UI in main task - timing conflicts with LVGL task

**Successful Solution**:
Move ALL UI operations into the LVGL task itself:

```cpp
void lvgl_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize grid board INSIDE LVGL task
    if (!grid_initialized && main_disp != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200)); // Let LVGL settle
        
        lv_obj_t *screen = lv_display_get_screen_active(main_disp);
        grid_board.initialize(screen);
        grid_board.process_text_and_animate(messages[msg_index]);
        
        grid_initialized = true;
    }
    
    while (1) {
        if (lv_display_get_default() != NULL) {
            lv_timer_handler();
            
            // Handle message cycling HERE, not in main task
            if (grid_initialized) {
                uint32_t current_time = esp_timer_get_time() / 1000;
                if (current_time - last_message_time > 30000) {
                    msg_index = (msg_index + 1) % 5;
                    grid_board.process_text_and_animate(messages[msg_index]);
                    last_message_time = current_time;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 3. PPA (Pixel Processing Accelerator) Alignment Errors
**Problem**: ESP32-P4's PPA hardware requires specific memory alignment
```
ppa_do_scale_rotate_mirror(178): out.buffer addr or out.buffer_size 
not aligned to cache line size
```

**Solution**: Use full frame buffer with proper configuration from reference implementation

### 4. Emoji Display Issues
**Problems**:
- Variation selector (U+FE0F) causing crashes
- House emoji (🏠) not displaying

**Solutions**:
- Remove variation selectors: "❤️" → "❤"
- Replace unsupported emojis with supported ones

## Critical Lessons Learned

### DO:
1. **Always defer UI operations to LVGL task** - Never create/modify UI elements from other tasks
2. **Use reference implementations** - Check M5Tab5-UserDemo-fresh for proper configuration
3. **Enable DMA with full frame buffer** for ESP32-P4 display operations
4. **Add `bsp_display_unlock()` after display configuration** before starting LVGL task
5. **Use proper task stack size** (8192 bytes) for LVGL task with complex UI

### DON'T:
1. **Don't disable DMA** thinking it will fix PPA issues - it makes things worse
2. **Don't create UI elements immediately after display init** - LVGL needs settling time
3. **Don't update display from multiple tasks** even with locking
4. **Don't use partial buffers** with rotation on ESP32-P4

## Working Configuration Summary

```cpp
// Display init with proper buffer and rotation
bsp_display_cfg_t cfg = {
    .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
    .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
    .double_buffer = true,
    .flags = {
        .buff_dma = true,
        .buff_spiram = true,
        .sw_rotate = true,
    }
};

lv_display_t* disp = bsp_display_start_with_config(&cfg);
lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);  // or 270 for inverted
bsp_display_backlight_on();
bsp_display_unlock();  // CRITICAL: Unlock before LVGL task

// Create LVGL task with sufficient stack
xTaskCreate(lvgl_task, "lvgl_task", 8192, NULL, 5, NULL);

// All UI operations happen INSIDE lvgl_task!
```

## Device-Specific Notes
- BSP_LCD_H_RES = 720 (native width)
- BSP_LCD_V_RES = 1280 (native height)  
- After rotation: 1280x720 landscape
- Grid: 12 columns × 5 rows
- LVGL timer handler runs every 10ms
- Message rotation every 30 seconds

## Final Working State
✅ Display properly oriented in landscape (1280x720)
✅ No LVGL assertion errors
✅ All 5 welcome messages cycling correctly
✅ Smooth animations without crashes
✅ Proper memory alignment for PPA hardware

## References
- Reference implementation: `/Users/a.vasilyev/Documents/my_code/IoT/m5stack/tab5/M5Tab5-UserDemo-fresh/`
- Key file: `platforms/tab5/main/hal/hal_esp32.cpp` - Shows proper display initialization
- CLAUDE.md in project root has hardware pin mappings and specifications