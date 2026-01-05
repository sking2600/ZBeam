# Hardware Validation Tracking

This document tracks the verification status of software-implemented features on physical ZBeam hardware (primarily ESP32-C3 and CH32X035 targets).

## 1. Multi-Channel Hardware
| Feature | Status | Notes |
| :--- | :--- | :--- |
| **Dual-Channel Mixing** | ❌ Unverified | Logic implemented, needs dual-FET hardware to verify duty cycle blending. |
| **Auto-Tint Ramping** | ❌ Unverified | Logic implemented. Needs verification that PWM transition is smooth. |
| **Sequential (Continuum)** | ❌ Unverified | Logic implemented. Crucial for multiple 7135 regulator banks. |

## 2. Safety & Sensors
| Feature | Status | Notes |
| :--- | :--- | :--- |
| **ADC Battery Scaling** | ⚠️ Partial | Basic ADC read verified, but `zbeam,battery-divider-factor` accuracy not tested with multimeter. |
| **Temperature Step-down** | ❌ Unverified | PID logic implemented. Needs thermal stress test to verify smooth regulation. |
| **ADC Calibration Offset** | ⚠️ Partial | NVS storage verified. Effect on UI blinks confirmed in simulator. |

## 3. UI & Aux
| Feature | Status | Notes |
| :--- | :--- | :--- |
| **HW PWM Sine (Aux)** | ✅ Verified | Tested on ESP32-C3 with 13-bit LEDC. |
| **Multi-tap (7H, 10H)** | ✅ Verified | Confirmed 7H enters config menu. 10H logic verified via logs. |
| **NVS Persistence** | ✅ Verified | Floor/Ceiling survive reboots on ESP32-C3. |
| **Stepped Ramp** | ✅ Verified | 3C toggle works. Smooth/Stepped logs confirmed. |
| **Utility Modes** | ✅ Verified | 3H entry verified. 2C cycling verified (Party->Tac->Candle->Bike). |
| **Calibration UI** | ❌ Unverified | Implemented. Need to test 7H from Batt/Temp check menus. |

## 4. Pending Physical Tests
1. **Multimeter Calibration**: Measure physical battery voltage and compare to `batt_read_voltage_mv` log output.
2. **Thermal Stress**: Use a hairdryer/heat source to trigger throttling and verify the main beam dims gradually without flickering.
3. **Multi-Scope Check**: Use a 2-channel oscilloscope to verify that `Cold` and `Warm` PWM signals cross-fade correctly in Auto-Tint mode.
4. **Stepped Ramp Feel**: Verify that 3C toggles mode and steps feel distinct and evenly spaced.
