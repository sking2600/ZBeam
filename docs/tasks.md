# ZBeam Development Tasks
> **Principle**: Use Zephyr Abstractions ONLY. No direct HAL/SDK calls.

## ESP32-C3 Port
- [x] **Cleanup**: Remove temporary blinky samples.
- [x] **Rename**: Rename project from `zlight` to `ZBeam`.
- [x] **Analysis**: Identify PWM LED (GPIO 8) and Button (GPIO 9) requirements.
- [x] **Board Config**:
    - [x] Create `boards/esp32c3_supermini.overlay` (PWM, Button).
    - [x] Create `boards/esp32c3_supermini.conf` (Kconfig).
- [x] **Implementation**:
    - [x] Fix Kconfig sourcing issue (`source "Kconfig.zephyr"`).
    - [x] Implement hardware PWM driver in `key_map.c`.
    - [x] Fix Device Tree node lookup.
- [x] **Verification**:
    - [x] Build ZBeam for ESP32-C3.
    - [x] Flash to device.
    - [x] Verify Button Interaction (Logs Confirmed).
    - [ ] Verify LED Hardware (Pending Flash).

## Zero-Cross Detection (CrCM) - Planned
> See `docs/zero-cross.md` for full implementation details.

- [ ] **Phase 1: Infrastructure**
    - [ ] Create `zero_cross.h` / `zero_cross.c`
    - [ ] Implement CH32X035 timer capture initialization
    - [ ] Edge detection ISR with timestamp logging
- [ ] **Phase 2: FB Injection**
    - [ ] Create `analog_dimming.c`
    - [ ] Hardware: RC filter for PWM→voltage
    - [ ] Calibrate FB voltage vs. LED current
- [ ] **Phase 3: CrCM Control Loop**
    - [ ] P-controller frequency adjustment
    - [ ] State machine (ACQUIRE → ADJUST → LOCKED)
    - [ ] Tune gains on hardware
- [ ] **Phase 4: Integration**
    - [ ] Create `dimming_manager.c`
    - [ ] Wire into `key_map.c` brightness path
    - [ ] Safety: overcurrent, timeout, mode oscillation

**TBD Components**: R_fb, C_fb, CRM_KP, CRM_TARGET_NS (requires hardware calibration)

---

## Next Session: Resumption Context
**Status**: The user logged out to apply `dialout` group permissions.
**Pending Firmware**: The code in `ZBeam/` has **critical fixes** that have likely **NOT** been successfully flashed yet due to permission errors:
1.  **PWM**: Frequency lowered to 1kHz (was 1MHz), Polarity Inverted.
2.  **Logging**: `KeyMap` set to `LOG_LEVEL_DBG` to show "HW Update" messages.
**Immediate Action**: 
1.  Run `west flash` immediately upon return.
2.  Check logs for `HW Update: Level=...`.
3.  If logs appear but LED is off, investigate GPIO 8 vs Board Schematic.
