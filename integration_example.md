# Grid Board Integration Guide for M5Stack Tab5

## How to integrate Grid Board into M5Stack Tab5 Demo

### 1. Copy Files
Copy these files to your M5Stack Tab5 project:
- `grid_board_simple_app.cpp`
- `grid_board_simple_app.h`
- `ShareTech140.c` (font file)
- `NotoEmoji64.c` (emoji font)

### 2. Add to CMakeLists.txt
Add the Grid Board files to your main/CMakeLists.txt:
```cmake
idf_component_register(SRCS 
    # ... existing files ...
    "grid_board_simple_app.cpp"
    "ShareTech140.c"
    "NotoEmoji64.c"
    # ... other files ...
    INCLUDE_DIRS "."
    # ... other configurations ...
)
```

### 3. Add Grid Board as an App
In your main app file, add Grid Board to the app list:

```cpp
#include "grid_board_simple_app.h"

// In your app initialization
void init_apps() {
    // ... existing apps ...
    
    // Add Grid Board app
    app_t grid_board_app = {
        .name = "Grid Board",
        .icon = grid_board_icon, // You'll need to create an icon
        .init = grid_board_app_init,
        .update = grid_board_app_update,
        .deinit = grid_board_app_deinit
    };
    register_app(grid_board_app);
}
```

### 4. Create Timer for Demo Mode
To automatically cycle through messages:

```cpp
// Create a timer to cycle messages
static void grid_board_timer_cb(lv_timer_t * timer) {
    grid_board_app_demo_cycle();
}

// In your app init after grid_board_app_init()
lv_timer_create(grid_board_timer_cb, 5000, NULL); // Change message every 5 seconds
```

### 5. Add Touch Interaction (Optional)
```cpp
// Add touch event to change message on tap
static void grid_board_touch_cb(lv_event_t * e) {
    grid_board_app_demo_cycle();
}

// Add to screen after init
lv_obj_add_event_cb(screen, grid_board_touch_cb, LV_EVENT_CLICKED, NULL);
```

## Features
- 12x5 character grid (60 characters total)
- Animated character appearance with bounce effect
- Supports ASCII characters and basic emoji
- Optimized for Tab5's 1280x720 display
- Touch interaction support
- Lightweight implementation using LVGL

## Customization
You can customize the grid by modifying these defines in `grid_board_simple_app.cpp`:
```cpp
#define GRID_COLS 12          // Number of columns
#define GRID_ROWS 5           // Number of rows  
#define GRID_SLOT_WIDTH 96    // Width of each slot
#define GRID_SLOT_HEIGHT 126  // Height of each slot
#define GRID_GAP 10           // Gap between slots
```

## API Usage
```cpp
// Initialize the grid board
grid_board_app_init(parent_obj);

// Display custom text
grid_board_app_update("YOUR MESSAGE HERE!");

// Cycle through demo messages
grid_board_app_demo_cycle();

// Clean up when done
grid_board_app_deinit();
```

## Memory Considerations
- Uses approximately 100KB for font data
- LVGL objects use heap memory
- Animation timers are automatically cleaned up

## Performance
- Smooth animations at 60 FPS on ESP32-P4
- Optimized for Tab5's MIPI-DSI display
- Minimal CPU usage between animations