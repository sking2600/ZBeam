# Anduril 2 Gap Analysis

*Current Status as of 2026-01-01*

This document tracks the feature parity gap between ZBeam and the original Anduril 2 UI.

## 1. High-Level Summary
- **Core UI**: ✅ Mostly Complete (Simple & Advanced Modes, Ramping, Lockout)
- **Aux LEDs**: ✅ Mostly Complete (Full cycle: Off/Low/High/Blink/Sine, Hardware PWM)
- **Hardware**: ⚠️ Prototype (ESP32-C3) vs Target (CH32X035) Note: Driver bug in Zephyr LEDC/ESP32C3 patched.

## 2. Detailed Feature Matrix

### Basic Operation
| Feature | Anduril 2 | ZBeam Status | Notes |
|---------|-----------|--------------|-------|
| **On / Off** | 1C | ✅ Implemented | Memory works |
| **Ramping** | 1H (Up/Down) | ✅ Implemented | Sweep-Lock style |
| **Instant Turbo** | 2C | ✅ Implemented | |
| **Moon Mode** | 1H from Off | ✅ Implemented | |
| **Batt Check*** | 3C from Off | ✅ Implemented | Blinks working. NVS offset support added. |
| **Lockout** | 4C from Off | ✅ Implemented | Momentary moon works |
| **Factory Reset** | 13H / 5C | ✅ Implemented | Uses NVS wipe |

### Aux LED & Multi-Channel
| Feature | Anduril 2 | ZBeam Status | Notes |
|---------|-----------|--------------|-------|
| **Enter Config** | 7C | ✅ Implemented | |
| **Cycle Modes** | 7C (Next) | ✅ Implemented | Cycles Off -> Low -> High -> Blink -> Sine |
| **Colors** | 7C (Hold) | ❌ Missing | No color control (RGB driver pending) |
| **Voltage Mode** | Dynamic Color | ❌ Missing | Requires Voltage Color logic |
| **Blink Modes** | 4 Modes | ✅ Implemented | **Sine Mode** (Breathing) implemented. |
| **Multi-Channel** | 3C from ON | ✅ Implemented | Virtual Emitter logic in `channel_manager`. |
| **Auto-Tint** | Dynamic Mix | ✅ Implemented | Part of Multi-Channel work. |
| **Sequential** | Continuum | ✅ Implemented | Segmented ramp logic for efficiency. |

> [!NOTE]
> **Aux Progress**: The FSM transitions for Aux Configuration are fully working. You can cycle (Off/Low/High/Blink/Sine). **Hardware PWM** is enabled for ultra-smooth breathing effects using a 13-bit sine table. A permanent fix for the Zephyr LEDC driver was implemented to resolve ESP32-C3 pin-multiplexing issues. RGB colors are still pending.

### Strobe & Utility Modes (3H from Off)
| Feature | Anduril 2 | ZBeam Status | Notes |
|---------|-----------|--------------|-------|
| **Candle Mode** | Flicker | ✅ Implemented | Logic with Random brightness. |
| **Bike Flasher** | Stutter | ✅ Implemented | Stutter timing implemented. |
| **Party Strobe** | Adjustable | ✅ Implemented | Variable frequency. |
| **Tactical Strobe** | 10Hz | ✅ Implemented | 50% duty cycle. |
| **Lightning** | Random | ⏳ Planned | Uses Candle mode logic for now. |

### Configuration Menus (Advanced)
| Feature | Anduril 2 | ZBeam Status | Notes |
|---------|-----------|--------------|-------|
| **Ramp Config** | 7H from On | ✅ Implemented | Supports Floor & Ceiling. |
| **Step Config** | (In Ramp Menu) | ✅ Implemented | Menu maps to Steps config (placeholder logic). |
| **Temp Check*** | (Nav from 3C) | ✅ Implemented | 3C -> 2C -> Temp Check blinks. |
| **Temp Config*** | 7H from 3C | ✅ Implemented | 7H Enters Cal Menu (Current & Limit). |
| **Voltage Cal** | 7H from 3C | ✅ Implemented | 7H Enters Cal Menu. |
| **Memory Config** | 10H from On | 🔨 In Progress | Kconfig defaults added. Logic coming. |
| **Auto-Lock** | 10H / 4C menu | 🔨 In Progress | Kconfig support added. |

1.  **Stepped Ramp Logic**: Dividing the 1-255 space based on Kconfig `ZBEAM_STEPPED_RAMP_STEPS`.
2.  **Strobe/Utility Modes**: Candle, Bike Flasher, Lightning.
3.  **UI Configuration**: Voltage and Temp Calibration UI via long-press.

---

### Incomplete Code Stubs
The following functions/variables are defined but not yet wired into the main logic:

| Item | Location | Purpose | Status |
|------|----------|---------|--------|
| `cb_simple_unlock_ceiling` | `src/ui_simple.c` | Unlock from lockout directly to ceiling brightness | Stub defined, not wired into FSM |
| `calibration_loaded` | `lib/thermal_manager.c` | Skip redundant NVS reads after first thermal init | Stub defined, not used |

These are protected with `__maybe_unused` to prevent build errors until implementation.

### * Verification Pendencies
Due to current hardware setup constraints, the following features require external verification:
- **Battery/ADC**: Compare 3C blinks against a calibrated multimeter.
- **Thermal Logic**: Verify step-down behavior using a controlled heat source or forced sensor override.
- **NVS Persistence**: Confirm settings survive power cycles once calibration is done.

