# Configuration Guide

## 1. Developer Configuration (FSM Navigation)

The ZBeam UI is built on a flexible Finite State Machine (FSM). Developers can customize how buttons navigate between states by modifying the `fsm_node` definitions in `src/key_map.c`.

### Node Structure

Each state (e.g., `OFF`, `ON`, `STROBE`) is defined as a `struct fsm_node`.

```c
struct fsm_node node_example = {
    .id = NODE_EXAMPLE,
    .name = "EXAMPLE",
    .action_routine = routine_example,   // Function to run on entry
    
    /* Navigation Maps */
    .click_map = { 
        [0] = &node_off,   // 1 Click -> Go to OFF
        [1] = &node_turbo, // 2 Clicks -> Go to TURBO
    },
    .hold_map = {
        [0] = &node_ramp,  // 1 Hold -> Go to RAMP
    },
    
    /* Custom Callbacks */
    .click_callbacks = { ... },
    .hold_callbacks = { ... },
    .release_callback = cb_release,
};
```

### Wiring Transitions

To change what a button press does:
1.  **Locate the Node**: Find the `struct fsm_node` for the state you want to modify (e.g., `node_off`).
2.  **Update the Map**: Assign the target node pointer to the corresponding index in `click_map` or `hold_map`.
    *   Index `0` = 1 Click / 1 Hold
    *   Index `1` = 2 Clicks / 2 Holds
    *   ...
    *   Index `4` = 5 Clicks

**Example**: Make 3 Clicks from OFF go to a new `node_custom`:
```c
// In src/key_map.c
struct fsm_node node_off = {
    ...
    .click_map = { 
        [2] = &node_custom, // 3 Clicks (Index 2)
    },
    ...
};
```

---

## 2. End-User Configuration (Config Menus)

ZBeam features a "Blink-Buzz-Click" configuration system (similar to Anduril) allowing users to change settings without external tools.

### How to Use
1.  **Enter Config Mode**: Navigate to the config node (e.g., **5 Hold** from ON, or specific sequence).
2.  **Wait for the Item**: The light will Blink N times to indicate the Item Number, then "Buzz" (stutter flash) for a few seconds.
3.  **Click to Set**: While the light is "Buzzing", click the button M times to set the value to M.
    *   **No Clicks**: The setting remains unchanged.
    *   **Clicking**: The light stops buzzing and counts your clicks.
4.  **Wait**: After you stop clicking, the light flashes to confirm and moves to the next item.

### Example: Ramp Configuration
**Menu Item 1: Floor Level** (Min Brightness)
- Light blinks **1 time**, then buzzes.
- Click N times to set floor level to N/150. (1=Moonlight, higher=Brighter).

**Menu Item 2: Ceiling Level** (Max Brightness)
- Light blinks **2 times**, then buzzes.
- Click N times to set ceiling level to (151 - N). (1=Full Turbo, 2=Slightly lower).

### Adding New Config Items (Developer)

To add a new setting to a menu:
1.  **Define the Callback**:
    ```c
    static void cb_set_my_setting(uint8_t val) { 
        my_variable = val; 
    }
    ```
2.  **Add to Item List**:
    ```c
    static const struct config_item my_menu_items[] = {
        { .nvs_id = NVS_ID_MY_SETTING, .blinks = 1, .apply_cb = cb_set_my_setting },
        /* ... more items ... */
    };
    ```
3.  **Create the Node**:
    ```c
    static void routine_my_config(void) { 
        start_config_menu(my_menu_items, ARRAY_SIZE(my_menu_items)); 
    }
    ```
