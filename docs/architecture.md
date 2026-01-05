# System Architecture

## Overview
The **ZBeam** application is a Zephyr-based embedded system designed to control a flashlight UI using a single button. It features a multi-tap detection engine, a node-based Finite State Machine (FSM), and Non-Volatile Storage (NVS) for runtime configuration persistence.

## Core Components

### 1. Multi-Tap Input Engine (`lib/multi_tap_input.c`)
*   **Purpose**: Detects complex button patterns (clicks, presses, holds).
*   **Events Generated**:
    *   `MULTI_TAP_EVENT_TAP` - Standard click sequence (1-5 taps)
    *   `MULTI_TAP_EVENT_HOLD_START` - Fires immediately when hold threshold reached
    *   `MULTI_TAP_EVENT_HOLD_RELEASE` - Fires when button released after a hold
*   **Configuration**: Click timeout and hold duration via Kconfig/NVS.

### 2. Finite State Machine (`lib/fsm_engine.c`)
*   **Structure**: A graph of `struct fsm_node` elements.
*   **Navigation**:
    *   `click_map[]` / `hold_map[]` - Static transition tables
    *   `click_callbacks[]` / `hold_callbacks[]` - Dynamic handlers (priority over maps)
    *   `release_callback` - Triggered on `HOLD_RELEASE` events
*   **Execution**: Entering a node triggers its `action_routine()`.

### 3. NVS Persistence (`lib/nvs_manager.c`)
*   Uses Zephyr's NVS subsystem on `storage_partition`.
*   Stores per-node configs and global system settings.

### 4. Key Map (`src/key_map.c`)
*   Central definition of the FSM graph and all node behaviors.
*   Users edit this file to customize the UI.

---

## Brightness Ramping Modes

ZBeam supports two ramping styles, controlled by which node you transition to:

### NODE_RAMP (Anduril-Style) - Default
*   **Behavior**: Directional ramping with memory.
*   **1-Hold**: Ramp UP (50ms steps)
*   **2-Hold**: Ramp DOWN
*   **Release**: Stop ramping, lock current level
*   **Limit**: Transitions to `NODE_BLINK` at min/max

### NODE_SWEEP (Cyclic)
*   **Behavior**: Smooth sine-wave cycling through all brightness levels.
*   **1-Hold**: Start cycling (uses sine LUT, 32 steps)
*   **Release**: Stop and lock to nearest discrete PWM level
*   **Speed**: Configurable via `ZBEAM_SWEEP_SPEED_MS`

---

## Configuration

| Feature | Kconfig Option | Default |
| :--- | :--- | :--- |
| Click Timeout | `CONFIG_ZBEAM_CLICK_TIMEOUT_MS` | 400ms |
| Hold Duration | `CONFIG_ZBEAM_HOLD_DURATION_MS` | 500ms |
| Monitor Key | `CONFIG_ZBEAM_MONITOR_KEY_CODE` | 30 |
| Max Nav Slots | `CONFIG_ZBEAM_MAX_NAV_SLOTS` | 5 |
| PWM Levels | `CONFIG_ZBEAM_PWM_VALUES` | "1,10,40,75,100" |
| Blink Return | `CONFIG_ZBEAM_BLINK_RETURN_MS` | 200ms |
| Sweep Speed | `CONFIG_ZBEAM_SWEEP_SPEED_MS` | 10ms |
| Ramp Step | `CONFIG_ZBEAM_RAMP_STEP_MS` | 50ms |
| Max PWM Levels | `CONFIG_ZBEAM_MAX_PWM_LEVELS` | 10 |
| Use DMA | `CONFIG_ZBEAM_USE_DMA` | n |

---

## Testing

```bash
cd ZBeam/tests/fsm_nvs
west build -b native_sim -t run
```

**Test Coverage** (15 tests):
- NVS persistence and factory reset
- Callback execution and state transitions
- Timer-based ramping (up and down limits)
- Hold/Release event dispatching
- Sweep mode cycling and index snapping
- Multi-tap input sequences (single, double, hold)
- PWM accessor API validation

