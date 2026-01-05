# Proposal: Multi-Channel PWM Control (Tint Mixing & Secondary Emitters)

## 1. Problem
Currently, ZBeam is hardcoded to control a single PWM output (`pwm_led0`). This prevents support for:
- **Tint Mixing**: Dual-channel lights (Cold White + Warm White).
- **Secondary Emitters**: Side-mounted COB LEDs or UV/Red secondary channels.
- **Multi-LED Regulated Sinks**: Lights where different LED groups require independent PWM control for efficiency.

## 2. Proposed Solution: The "Virtual Emitter" Abstraction
Introduce a logical layer between the UI Ramp and the physical PWM hardware.

### A. DeviceTree Hardware Mapping
Instead of a single alias, the board overlay defines an array of emitters in a well-defined node.

```dts
/ {
    /* Define logical emitters for the firmware */
    zbeam_emitters {
        compatible = "zbeam,emitter-group";
        emitters = <&ledc0 0 1000000 0>, /* Index 0: Cold White */
                  <&ledc0 1 1000000 0>; /* Index 1: Warm White */
        emitter-names = "cold", "warm";
    };
};
```

### B. Logical Channel Modes
The UI maintains a "Channel Mode". Each mode defines a **Weight Template** for the available hardware.

| Mode ID | Name | Emitter 0 | Emitter 1 | Total Effect |
| :--- | :--- | :--- | :--- | :--- |
| 0 | **Cold Only** | 100% | 0% | Standard sharp white |
| 1 | **Warm Only** | 0% | 100% | Sunset/Indoor mood |
| 2 | **50/50 Mix** | 50% | 50% | Neutral White |
| 3 | **Auto-Tint** | 100% -> 0% | 0% -> 100% | Tint changes with brightness (Anduril 2 style) |
| 4 | **Sequential** | Segmented | Segmented | 1st 25% = LED 0, 2nd 25% = LED 1, etc. |

### D. Sequential (Continuum) Logic
For efficiency-focused drivers (e.g., multiple 7135 regulators) or sequential tint shifting:
- The 0-255 ramp is divided into `N` slices.
- `Pulse[i] = Clamp((L - Start[i]) / Slice_Size)`
- This allows the firmware to "sweep" through physical emitters. E.g., at low levels only the most efficient regulator is active; at high levels, the high-power FET or additional banks kick in.

### C. The Processing Pipeline
1. **Input**: User ramps to brightness level `L`.
2. **Throttle**: `L` is reduced by the Thermal Manager if necessary.
3. **Mixer**: For the active `ChannelMode`, calculate physical duty for each pin:
   - `Pin[i]_Duty = (L * ModeWeight[i]) / 255`
4. **Output**: Update PWM registers for all mapped pins.

## 3. Preserving Simplicity
To ensure this doesn't overcomplicate single-LED lights:

1. **Automatic Scaling**: At compile time, if `DT_NUM_INST_STATUS_OKS(zbeam_emitter_group)` is 0 or 1, the mixer defaults to a simple 1:1 pass-through.
2. **Transparent UI**: The "Channel Switch" menu (e.g., 3C from ON) is only compiled/enabled if a multi-emitter group is detected in DeviceTree.
3. **No Overhead**: For single-LED boards, the loop over emitters becomes a single optimized statement, maintaining 0-cost abstraction.

## 4. Implementation Plan
1. **Firmware**: Update `ui_actions.c` to store an array of `struct pwm_dt_spec`.
2. **Logic**: Add a `channel_manager.c` to handle the weight math and mode transitions.
3. **UI**: Map 3C (from ON) to the `channel_manager_next_mode()` function.
