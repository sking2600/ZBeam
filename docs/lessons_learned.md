# Lessons Learned

## Hardware Configuration
### ESP32-C3 SuperMini LED Polarity
- **Issue**: The onboard blue LED (GPIO 8) was ON at boot and required inverted logic to control correctly.
- **Cause**: The LED is wired in an **Active Low** configuration (connected to VCC, grounded by GPIO to turn ON).
- **Resolution**: 
  - Standard PWM (`PWM_POLARITY_NORMAL`) outputs 0V at 0% duty, which turns an Active Low LED **ON**.
  - We must use `PWM_POLARITY_INVERTED` in the device tree overlay. This ensures 0% duty outputs Logic High (3.3V), keeping the LED **OFF**.
  - **Lesson**: Always check if the LED is Active High or Active Low when configuring PWM polarity in Zephyr device trees.

## Zephyr Input Subsystem
- **Direct Usage**: It is cleaner to use `INPUT_CALLBACK_DEFINE` within a module (like `multi_tap_input.c`) rather than manually routing events from `main.c`. This improves encapsulation.
- **Callback Signature**: The callback signature for `INPUT_CALLBACK_DEFINE` is `void cb(struct input_event *evt, void *user_data)`. Omitting `user_data` causes build warnings/errors.

## Project Rules
- **Manual Sudo**: Any command requiring `sudo` (e.g., `chmod`, `apt install`) must be explicitly requested from the user to run manually. Do not attempt to auto-run or prompt for these commands within the agent.
- **Zephyr Abstractions Only**: application code MUST use Zephyr drivers and subsystems (e.g., `drivers/pwm.h`, `drivers/gpio.h`). DO NOT use vendor SDK headers (e.g., `esp_rom_gpio.h`) or direct register access unless strictly wrapped in a driver shim.

## Permanent Configurations
- **Serial Permissions**: To avoid running `chmod` after every plug-in/reset, add your user to the `dialout` group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
  **Note**: You must **log out and log back in** (or restart) for this to take effect.

### Tooling and Build System
- **`esptool` Requirement**: The `west build` process for ESP32 targets strictly requires `esptool` to be in the system PATH. Even if the Zephyr SDK is present, `esptool` is often not included or not found by CMake.
  - **Fix**: Install `esptool` in the project's virtual environment: `.venv/bin/pip install esptool`.
  - **Critical**: Ensure the virtual environment's `bin` directory is in your `PATH` when running `west build`: `export PATH=$PWD/../.venv/bin:$PATH` (if running from `ZBeam/`) or source it: `source ../.venv/bin/activate`.
  - **West Not Found**: If `west` is not found, it is likely because the venv is not active. Always use the full path or activate the venv first.

### Board Support
- **Super Mini Board**: The "ESP32-C3 Super Mini" is supported in Zephyr via `esp32c3_supermini` (found in `boards/others`).
- **USB Serial**: The default console is via the USB-CDC (USB-JTAG) interface, appearing as `/dev/ttyACM*`.
- **Input Configuration**: The ESP32-C3 Super Mini overlay default for the user button uses `INPUT_KEY_0` (code 11). If your application expects `INPUT_KEY_A` (code 30), you must override `zephyr,code` in your overlay or update your app logic.

### Permissions
- **Serial Port Access**: Flashing and monitoring requires access to `/dev/ttyACM0`.
  - **Issue**: "Permission denied" errors.
  - **Fix**: See "Permanent Configurations" above.
  - **Quick Fix**: `sudo chmod 666 /dev/ttyACM0` (resets on reboot/replug).
### Console and reset
  - **Auto-Reset/DFU**: While `esptool` attempts to reset via DTR/RTS, the "Super Mini" board often fails to reset automatically after flashing.
    - **Action**: You must manually press the **RESET** button on the board after `west flash` completes to fail-safe into the application.
  - **No Output?**: If you don't see output, the console might be initializing before your terminal connects. Use `CONFIG_LOG_MODE_IMMEDIATE=y` or add a startup delay.
### Build
### 6. Changing Timer Frequency Live (Zephyr)
When implementing a Strobe light where frequency changes dynamically (frequency sweep), simply calling `k_timer_start` on an already running periodic timer to change its period DOES NOT work reliably if done from within another ISR (like a ramp timer) or sometimes just glitches.
**Solution**: Use a **Recursive One-Shot Timer**.
- Configure `k_timer` with `duration=X` and `period=0` (one-shot).
- In the timer callback, calculate the *next* delay and re-arm the timer with `k_timer_start(timer, K_MSEC(new_delay), K_NO_WAIT)`.
- This ensures the frequency updates on every cycle without fighting an active periodic schedule.

### 7. C Structure Visibility in Forward Declarations
If you have a function forward declaration that uses a struct type in its prototype (e.g. `void func(struct config_item *ptr)`), that struct MUST be defined *before* the function prototype, or at least forward declared as `struct config_item;`. Otherwise, the compiler treats it as a scoped incomplete type, and it will conflict with the actual definition later.
```c
/* WRONG */
void func(struct Foo *f); 
struct Foo { int x; }; /* "conflicting types" error */

/* RIGHT */
struct Foo { int x; };
void func(struct Foo *f);
```
and Configuration
- **Application Kconfig**: If you define a `Kconfig` in your project root, it overrides the default loading mechanism. You *must* add `source "Kconfig.zephyr"` at the end to pull in the core Zephyr configuration options (like `CONFIG_GPIO`, `CONFIG_SERIAL`).
- **Device Tree Lookups**: `DT_NODELABEL(pwmleds)` relies on the label existing in the final generated DTS. If that fails, `DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)` is a robust fallback to find *any* enabled node of that compatible type.
- **Subsystem Dependencies**: enabling `CONFIG_LED_PWM` requires `CONFIG_LED=y` to be explicitly set.

### PWM / LEDC on ESP32-C3

#### Resolution Limit (Critical!)
ESP32-C3 LEDC has a **14-bit maximum timer resolution**. When using the 80MHz APB clock:
- Max resolution = log2(Clock / PWM_Freq)
- 1kHz PWM: log2(80MHz / 1kHz) = log2(80000) ≈ **17 bits** → **EXCEEDS 14-bit limit!**
- The Zephyr driver silently falls back to RC_FAST (8MHz) clock, resulting in ~1.7Hz PWM instead of 1kHz.
- **Fix**: Use **5kHz or higher** PWM frequency: log2(80MHz / 5kHz) = log2(16000) ≈ **14 bits** → Works!

#### GPIO Matrix Routing Bug (Critical!)
The Zephyr LEDC driver does **NOT** configure the GPIO matrix to route the LEDC signal to the output pin. Even with correct pinctrl in the device tree, the pin stays on signal 128 (GPIO direct) instead of signal 45 (LEDC_LS_SIG_OUT0).

**Symptoms**:
- `pwm_set_dt()` returns 0 (success)
- Debug logs show correct clock source and timer config
- GPIO toggle test works on the same pin
- But LED doesn't respond to PWM duty changes

**Diagnosis**: Read `GPIO8_FUNC_OUT_SEL_CFG` register (0x60004574 for GPIO8):
- Value 0x80 (128) = GPIO direct output → **PWM won't work**
- Value 0x2D (45) = LEDC_LS_SIG_OUT0 → **PWM works**

**Workaround**: Manually configure GPIO matrix in application code:
```c
#include <zephyr/sys/util.h>

#define GPIO_BASE               0x60004000
#define GPIO_ENABLE_REG         (GPIO_BASE + 0x0020)
#define GPIO8_FUNC_OUT_SEL_REG  (GPIO_BASE + 0x0554 + 8 * 4)
#define LEDC_LS_SIG_OUT0        45

static void configure_gpio8_for_ledc(void)
{
    uint32_t sel = sys_read32(GPIO8_FUNC_OUT_SEL_REG);
    sel = (sel & ~0xFF) | LEDC_LS_SIG_OUT0;  // Signal 45
    sel |= (1 << 9);  // oen_sel = 1
    sys_write32(sel, GPIO8_FUNC_OUT_SEL_REG);
    
    uint32_t enable = sys_read32(GPIO_ENABLE_REG);
    enable |= (1 << 8);
    sys_write32(enable, GPIO_ENABLE_REG);
}
```

Call `configure_gpio8_for_ledc()` after `pwm_is_ready_dt()` but before `pwm_set_dt()`.

#### Recommended Settings
- **Minimum PWM frequency**: ~5kHz (200μs period) with APB clock
- **Debug logging**: Enable `CONFIG_PWM_LOG_LEVEL_DBG=y` to see clock_src selection
- **Debug tip**: If `clock_src=8` (RC_FAST) when you expected `clock_src=4` (APB), your frequency is too low

### PWM Ramp Tables (Perception Correction)

Human eyes perceive brightness logarithmically. Linear duty cycle changes appear "fast at start, slow at end." 

#### Architecture
- `scripts/generate_ramp_table.py` - Python generator for ramp/sine tables
- `include/ramp_table.h` - Selector for resolution-specific tables
- `src/pwm_ramp_esp32.c` - ESP32 LEDC interpolation
- `src/pwm_ramp_dma.c` - DMA-driven ramping (stub, for MCUs with Timer+DMA)
- `src/pwm_ramp_generic.c` - CPU loop fallback

#### Gamma Values by LED Color
| Gamma | Suitable For |
|-------|-------------|
| 2.0-2.2 | White, warm white, red LEDs |
| 2.3-2.5 | Green, amber LEDs |
| 2.6-3.0 | Blue, cool white LEDs |

#### Generating Tables
```bash
# Linear ramp (gamma 2.8 for blue)
python3 scripts/generate_ramp_table.py --bits 13 --gamma 2.8 > include/ramp_table_13bit_g28.h

# Sine wave (breathing effect)
python3 scripts/generate_ramp_table.py --bits 13 --gamma 2.8 --sine > include/ramp_sine_13bit_g28.h
```

#### Kconfig Options
- `CONFIG_PWM_RAMP_ESP32_LEDC_INTERPOLATION` - Use LEDC fade for ESP32
- `CONFIG_PWM_RAMP_INTERPOLATION_STEP` - Table step size (1-64, default 8)
- `CONFIG_PWM_RAMP_DMA` - DMA-driven ramping (for MCUs with Timer+DMA support)

### Debugging & Stability Lessons
- **Device Tree Overlay Conflicts**: If you define a node in an overlay that conflicts with a default board definition (e.g., duplicated `gpio-keys` on the same pin), it can cause unstable input behavior or build errors.
  - **Fix**: Always **override** the existing node label (e.g., `&user_button1`) instead of defining a new conflicting node. Inherit property values (like `gpios`) where possible to avoid mismatches.
  - **Input Codes**: Be aware of board defaults. The SuperMini defaults to `INPUT_KEY_0` (11). Mixing this with `INPUT_KEY_A` (30) in your code can cause confusion if not consistently updated in the overlay.

- **Stack Overflows**: Heavy usage of logging (especially with `LOG_IMMEDIATE`) or deep call stacks in worker threads can easily overflow default stack sizes.
  - **Symptom**: `Instruction Access Fault` or `Illegal Instruction` at boot or during specific actions.
  - **Fix**: Increase thread stack sizes in `prj.conf` (e.g., `CONFIG_ZBEAM_SAFETY_WORKER_STACK_SIZE=2048`). Use `CONFIG_THREAD_ANALYZER` to tune.

- **State Persistence Bugs**: Be careful when initializing state variables.
  - **Issue**: `key_map_init` set brightness to 0. `routine_off` called `stop_ramping`, which unconditionally saved current brightness (0) to `memorized_brightness`.
  - **Result**: Turning the light ON resulted in 0 brightness (OFF).
  - **Fix**: Only update persistent state (memory) if a relevant action (ramping) was actually active.
