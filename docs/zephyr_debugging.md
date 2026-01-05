# Zephyr Debugging on ESP32-C3

This document outlines the debugging strategies, tools, and common pitfalls encountered when working with the ESP32-C3 on Zephyr.

## 1. Hardware Verification (The "Bitbang" Test)
Before diving into complex driver debugging, always verify the basic hardware connectivity.
*   **Method**: Use `gpio_pin_configure` and `gpio_pin_set` to manually toggle the pin.
*   **Why**: Confirms the pin is correct, the LED is functional, and the strapping pins (e.g., GPIO 8/9/10) aren't interfering.
*   **Sign**: If `gpio_pin_set` works but `pwm_set_dt` fails, the issue is likely the GPIO matrix routing (see below).

## 2. PWM / LEDC Debugging

### 2.1 GPIO Matrix Routing Bug (CRITICAL)
**The Zephyr LEDC pinctrl does NOT configure the GPIO matrix!**

*   **Symptom**: `pwm_set_dt()` returns 0, GPIO toggle works, but PWM output is stuck/dead.
*   **Root Cause**: `GPIO_FUNC_OUT_SEL_CFG` register remains at 0x80 (GPIO direct output) instead of 0x2D (45 = LEDC signal).
*   **Diagnosis**: Read the register: `sys_read32(0x60004574)` for GPIO8.
    *   0x80 = GPIO mode (PWM won't work)
    *   0x2D = LEDC routed (PWM works)
*   **Workaround**: Manually configure GPIO matrix after PWM init. See `docs/lessons_learned.md` for code.

### 2.2 Resolution Limit 
ESP32-C3 LEDC has **14-bit max resolution**. With 80MHz APB clock:
*   **1kHz PWM**: Needs 17 bits → **TOO HIGH** → Driver falls back to RC_FAST (8MHz) → ~1.7Hz actual PWM!
*   **5kHz PWM**: Needs 14 bits → **OK** → Uses APB clock correctly.
*   **Fix**: Use ≥5kHz PWM frequency (200μs period or less).

### 2.3 Clock Source Verification
*   **Debug**: Enable `CONFIG_PWM_LOG_LEVEL_DBG=y` in prj.conf.
*   **Check log**: `clock_src=4` (APB 80MHz) = good, `clock_src=8` (RC_FAST 8MHz) = bad.

## 3. JTAG Debugging (OpenOCD)

### 3.1 Setup
*   **Command**: `west debug` (automates GDB + OpenOCD).
*   **Runner**: Uses `openocd` runner by default for ESP32.
*   **Config**: `board/esp32c3-builtin.cfg` (for USB-Serial-JTAG).

### 3.2 Common Errors
*   **"Connection timed out"**: OpenOCD GDB Server (Port 3333) didn't start or is blocked.
*   **"Monitor command not supported"**: You are connected to GDB, but not in a mode that supports OpenOCD monitor commands (try `target extended-remote`).
*   **"Cannot access memory"**: The target is not halted, or is in a reset state. Use `mon reset halt`.

### 3.3 Register Inspection
*   **LEDC Base Address**: `0x60019000` (ESP32-C3).
*   **GPIO Base Address**: `0x60004000` (ESP32-C3).
*   **Key Registers**:
    *   `GPIO8_FUNC_OUT_SEL`: `0x60004574` (should be 0x2D for LEDC)
    *   `LEDC_LS_CH0_CONF0`: `0x60019000`
    *   `LEDC_LS_CH0_DUTY`: `0x60019008`
    *   `LEDC_LS_TIMER0_CONF`: `0x600190A0`
*   **Command**: `x/32w 0x60019000` (In GDB) or `mdw 0x60019000 32` (In Telnet).

## 4. Instrumented Debugging (Alternative)
If JTAG is flaky, use `printk` to dump registers directly from firmware:
```c
#include <zephyr/sys/util.h>
uint32_t gpio_sel = sys_read32(0x60004574);  // GPIO8_FUNC_OUT_SEL
printk("GPIO8 signal: %d (should be 45 for LEDC)\n", gpio_sel & 0xFF);
```

