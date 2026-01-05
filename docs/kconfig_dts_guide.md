# ZBeam Kconfig & DeviceTree Customization Guide

This guide explains how to customize the ZBeam firmware. Settings are divided into **User Preferences** (Kconfig) and **Hardware Definitions** (DeviceTree Overlays).

## 1. User Preferences (Kconfig)
These settings can be changed in `prj.conf` or via `west build -t menuconfig`. They control UI behavior and software-level safety limits.

### Safety Thresholds
- `CONFIG_ZBEAM_TEMP_WARN_C10`: Temperature (in 0.1°C) to start throttling (Default: 600 = 60°C).
- `CONFIG_ZBEAM_VOLTAGE_MIN_MV`: Low voltage cutoff in millivolts (Default: 2900mV).
- `CONFIG_ZBEAM_VOLTAGE_MAX_MV`: High voltage cutoff in millivolts (Default: 4200mV).

### UI Modes & Navigation
- `CONFIG_ZBEAM_DEFAULT_UI_MODE_ADVANCED`: Set to `y` to start in Advanced UI (9H from OFF to switch runtime).
- `CONFIG_ZBEAM_CLICK_TIMEOUT_MS`: Window for multi-tap detection (Default: 500ms).
- `CONFIG_ZBEAM_HOLD_DURATION_MS`: Minimum time for a "hold" event (Default: 500ms).
- `CONFIG_ZBEAM_AUTO_LOCK_TIMEOUT_MIN`: Automatic lockout after N minutes of inactivity (0 = disabled).

### Memory Modes
- `CONFIG_ZBEAM_DEFAULT_MEMORY_MODE`:
  - `ZBEAM_MEMORY_AUTOMATIC`: Remembers last level.
  - `ZBEAM_MEMORY_MANUAL`: Always starts at a fixed level.
  - `ZBEAM_MEMORY_HYBRID`: Returns to manual level after a timeout.
- `CONFIG_ZBEAM_DEFAULT_MANUAL_MEM_LEVEL`: The fixed level (1-255) for manual/hybrid modes.
- `CONFIG_ZBEAM_DEFAULT_HYBRID_MEM_TIMEOUT_MIN`: Timeout for hybrid memory.

### Ramp & Stepped UI
- `CONFIG_ZBEAM_DEFAULT_RAMP_STYLE`: Choose between `ZBEAM_RAMP_SMOOTH` or `ZBEAM_RAMP_STEPPED`.
- `CONFIG_ZBEAM_STEPPED_RAMP_STEPS`: Number of discrete levels between floor and ceiling (Default: 7).
- `CONFIG_ZBEAM_BRIGHTNESS_FLOOR`: Lowest level in ramp (Default: 1).
- `CONFIG_ZBEAM_BRIGHTNESS_CEILING`: Highest level in ramp (Default: 255).
- `CONFIG_ZBEAM_PWM_VALUES`: Comma-separated list for custom ramp curves.

---

## 2. Hardware Definitions (DeviceTree Overlay)
Hardware-specific constants (pins, channels, resistor dividers) are defined in the board overlay file (e.g., `boards/your_board.overlay`). 

> [!IMPORTANT]
> These settings depend on your physical hardware. Do not change them unless you have modified the circuit (e.g., swapped resistors or moved LED wires).

### Battery sense & ADC
The firmware pulls hardware constants from the `zephyr,user` node.

```dts
/ {
    zephyr,user {
        /* ADC channel for battery sensing */
        io-channels = <&adc0 0>;
        io-channel-names = "BATT_SENSE";

        /* Battery Divider Factor (R_high + R_low) / R_low * 1000 */
        /* For 100k/47k ohm resistors: (147 / 47) * 1000 = 3127 */
        zbeam,battery-divider-factor = <3125>;
    };
};

&adc0 {
    status = "okay";
    battery_sense: channel@0 {
        reg = <0>;
        zephyr,gain = "ADC_GAIN_1_4";
        zephyr,reference = "ADC_REF_INTERNAL";
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };
};
```

### AUX LED Configuration
The AUX LED is defined by the `aux_led` node label. The firmware assumes a PWM-based AUX LED by default if this node is present.

```dts
aux_pwm: aux_pwm {
    compatible = "pwm-leds";
    aux_led: aux_path {
         pwms = <&ledc0 0 10000 PWM_POLARITY_INVERTED>;
         label = "Aux LED";
    };
};
```

### Main Beam PWM (LEDC on ESP32)
The main beam resolution and interpolation are automatically derived from the SoC series, but the pin mapping is defined in `pinctrl`:

```dts
&pinctrl {
    ledc0_default: ledc0_default {
        group1 {
            pinmux = <LEDC_CH1_GPIO7>; /* Main Beam Pin */
            output-enable;
        };
    };
};
```

---

## 3. Calibration (Runtime)
Basic calibration can be done without rebuilding the firmware by modifying NVS values.
- **Battery Offset**: `NVS_ID_BATT_CALIB_OFFSET` (100 = 0V, steps of 0.1V).
- **Thermal Offset**: `NVS_ID_THERMAL_CALIB_OFFSET` (100 = 0°C, steps of 1°C).
