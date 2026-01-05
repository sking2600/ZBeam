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

### 3. FSM Worker Thread (`lib/fsm_worker.c`)
*   **Purpose**: Dedicated thread processes input events from message queue.
*   **Message Types**: `MSG_INPUT_TAP`, `MSG_INPUT_HOLD_START`, `MSG_INPUT_HOLD_RELEASE`.
*   **Configuration**: Stack size and priority via Kconfig.

### 4. Safety Monitor (`lib/safety_monitor.c`)
*   **Purpose**: Periodic watchdog for overheat/overcurrent/undervoltage.
*   **Rate**: Configurable via `CONFIG_ZBEAM_SAFETY_RATE_HZ` (default 10Hz).
*   **Actions**: Calls `fsm_emergency_off()` on threshold violation.
*   **Thresholds**: Configured via `ZBEAM_TEMP_*`, `ZBEAM_CURRENT_*`, `ZBEAM_VOLTAGE_*`.

### 5. Battery Check (`src/batt_check.c`)
*   **Purpose**: Reads battery voltage and calculates blink pattern.
*   **API**: `batt_read_voltage_mv()`, `batt_calculate_blinks()`.
*   **Display**: Major blinks = whole volts, Minor blinks = tenths.

### 6. NVS Persistence (`lib/nvs_manager.c`)
*   Uses Zephyr's NVS subsystem on `storage_partition`.
*   Stores per-node configs and global system settings.
*   **Optional**: Enabled via `CONFIG_ZBEAM_NVS_ENABLED`.

### 7. Key Map (`src/key_map.c`)
*   Central definition of the FSM graph and all node behaviors.
*   Users edit this file to customize the UI.

---

## Brightness Handling

ZBeam supports smooth linear ramping with configurable floors and ceilings.

### NODE_RAMP (Anduril-Style)
*   **Behavior**: Directional ramping with memory.
*   **1-Hold**: Ramp UP (from OFF or ON)
*   **2-Hold**: Ramp DOWN (from ON)
*   **Release**: Stop ramping, lock current level
*   **Speed**: Configurable via `ZBEAM_BRIGHTNESS_SWEEP_DURATION_MS`

### NODE_STROBE
*   **Behavior**: Variable frequency strobe (12Hz - 80Hz default).
*   **1-Hold**: Increase Frequency (Faster)
*   **2-Hold**: Decrease Frequency (Slower)
*   **Implementation**: Uses a **Recursive One-Shot Timer** to allow period changes without restarting the phase.
*   **Persistence**: Configurable to use last-known brightness (`ZBEAM_STROBE_USE_ALC_BRIGHTNESS`).

---

## Configuration

| Feature | Kconfig Option | Default |
| :--- | :--- | :--- |
| Click Timeout | `CONFIG_ZBEAM_CLICK_TIMEOUT_MS` | 400ms |
| Hold Duration | `CONFIG_ZBEAM_HOLD_DURATION_MS` | 500ms |
| Brightness Floor | `CONFIG_ZBEAM_BRIGHTNESS_FLOOR` | 1 |
| Brightness Ceiling | `CONFIG_ZBEAM_BRIGHTNESS_CEILING` | 255 |
| Sweep Duration (Dim) | `CONFIG_ZBEAM_BRIGHTNESS_SWEEP_DURATION_MS` | 2000ms |
| Sweep Duration (Freq) | `CONFIG_ZBEAM_STROBE_SWEEP_DURATION_MS` | 3000ms |
| Strobe Waveform | `CONFIG_ZBEAM_STROBE_WAVEFORM_SAWTOOTH` | y |
| Strobe Persistence | `CONFIG_ZBEAM_STROBE_USE_ALC_BRIGHTNESS` | y |
| NVS Enable (Optional) | `CONFIG_ZBEAM_NVS_ENABLED` | y |

---

## Additional Subsystems (Phase 4 & 5)

### 8. Config Menu System
*   **Anduril-Style Sequential Menus**: Implements the "Blink-Buzz" configuration flow.
*   **States**:
    *   `NODE_CONFIG_FLOOR`: Buzzes for clicks to set minimum level.
    *   `NODE_CONFIG_CEILING`: Buzzes for clicks to set maximum level (Inverted: 256-N).
*   **Generic Input**: Uses `any_click_callback` in the FSM engine to handle arbitrary numbers of taps.
*   **Persistence**: Automatically saves confirmed values to specific NVS IDs (`NVS_ID_RAMP_FLOOR`, `NVS_ID_RAMP_CEILING`).
*   **Future**: Extensible via `ZMetric` serial protocol for USB-side tuning.

### 9. Thermal Regulation (Stub)
*   **Component**: `lib/thermal_manager.c`
*   **Logic**: Monitors simulated temperature. If > 50C, applies a linear throttle factor (0-255) to the requested brightness.
*   **Integration**: Polled periodically by `key_map` (1Hz timer).

### 10. PWM Ramping (Abstraction)
*   **Goal**: Platform-agnostic ramping (ESP32 LEDC vs CH32V DMA).
*   **Current State**: 
    *   API defined in `pwm_ramp.h`.
    *   Currently using **Manual Software Timer** in `key_map.c` (legacy mode) due to platform regressions with abstract driver.
    *   Future: Will switch to `pwm_ramp_generic.c` or specific hardware drivers once stable.

### 11. AUX LED Manager (Stub)
*   **Component**: `lib/aux_manager.c`
*   **Purpose**: Controls secondary RGB LED (e.g., SK6812) for status indication.
*   **Modes**: OFF, LOW, HIGH, BLINK, VOLTAGE (battery color), RAINBOW.
*   **Access**: 7 Clicks from OFF cycles through modes.

### 12. Power Manager (Stub)
*   **Component**: `lib/pm_manager.c`
*   **Purpose**: Manages low-power states.
*   **Integration**: `pm_suspend()` called in `routine_off()`, `pm_resume()` called in `routine_on()`.
*   **Future**: Will hook into Zephyr Power Management subsystem for real deep sleep.

---

## Testing

```bash
# Run all tests:
cd ZBeam/tests/<suite_name>
west build -b native_sim -t run
```

**Test Suites:**
| Suite | Purpose |
|-------|---------|
| `fsm_core` | Basic FSM transitions and callbacks |
| `fsm_nvs` | NVS persistence and factory reset |
| `input_logic` | Multi-tap detection |
| `strobe_logic` | Strobe frequency and waveforms |
| `batt_check` | Voltage-to-blink calculation |
| `nvs_logic` | NVS read/write byte functions |
| `thermal_logic` | Thermal throttle simulation |
| `aux_logic` | AUX LED mode cycling |

---

## Roadmap & Next Steps

### Near-Term (Priority)
| Task | Status | Notes |
|------|--------|-------|
| **Ramp Config (7H)** | ✅ Implemented | Floor/Ceiling setup with visual buzz feedback |
| **Hardware LED Verification** | ⏳ Pending | Verify ON/OFF, ramping, strobe on physical device |
| **AUX Mode Hardware Test** | ⏳ Pending | Test 7C cycles modes (requires SK6812 wiring) |
| **NVS Persistence Test** | ⏳ Pending | Verify settings survive power cycle |
| **Thermal Sensor Integration** | 📋 Planned | Replace stub with real NTC/ADC driver |

### Medium-Term (Phase 6)
| Task | Status | Notes |
|------|--------|-------|
| **Real Deep Sleep** | 📋 Planned | Integrate Zephyr PM subsystem (`sys_pm_state_set`) |
| **Hardware PWM Fading** | 📋 Planned | Fix `pwm_ramp_generic.c` or expose LEDC HAL |
| **SK6812 AUX Driver** | 📋 Planned | Implement WS2812/SK6812 protocol via SPI/DMA |
| **Enhanced Config Menu** | 📋 Planned | Add Step Mode, Memory Timer, AUX Color settings |
| **CH32V Port Validation** | 📋 Planned | Build and flash to CH32V303 EVT board |

### Long-Term (Future)
| Task | Status | Notes |
|------|--------|-------|
| **USB-PD Integration** | 📋 Planned | CH32X035 TCPC driver for voltage negotiation |
| **OTA Updates** | 📋 Planned | MCUBOOT + DFU over USB |
| **Advanced Strobes** | 📋 Planned | Lightning, Candle, Police patterns |
| **Beacon Mode** | 📋 Planned | Periodic flash with configurable interval |
| **SOS Mode** | 📋 Planned | Morse code pattern |

### Known Issues
| Issue | Severity | Resolution |
|-------|----------|------------|
| `pwm_ramp_generic.c` not wired | Low | Using software timer in `key_map.c` instead |
| `is_turbo` unused warning | Low | Will be used when Thermal stepdown implemented |
| LEDC HAL functions hidden | Medium | Requires Zephyr driver patch for hardware fading |
