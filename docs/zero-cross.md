# Zero-Cross Detection Architecture

## Overview
This document describes the zero-cross switching system for efficient LED driving using the MP3431/MP3432 boost converter on the **CH32X035** MCU. The goal is to maintain **Critical Conduction Mode (CrCM)** for optimal efficiency by detecting inductor current zero-crossings.

> [!IMPORTANT]
> This implementation targets the **CH32X035** MCU with its native Timer Capture and ADC peripherals.

---

## Hardware Interface

### SW Node Monitoring (Timer Capture)
```
                    100kΩ
SW (Inductor) ────/\/\/\/──┬──────> TIMx_CH1 (Input Capture)
                           │
                         10kΩ
                           │
                          GND

                    BAT54 (Dual Schottky)
TIMx_CH1 ─────┬───────|>|──── 3.3V (Clamp High)
              │
              └───────|<|──── GND  (Clamp Low)
```

**Voltage Divider**: 100kΩ / 10kΩ → 11:1 ratio
- VIN = 12V → VMCU = 1.09V ✓
- VIN = 18V → VMCU = 1.64V ✓
- VIN = 24V → VMCU = 2.18V ✓

**Protection**: Dual BAT54 Schottky diodes clamp the MCU pin to 0V–3.3V range, protecting against inductor back-EMF spikes.

### MP3431 MODE Pin Configuration
- Tied to **GND** → Forces PSM (Pulse Skipping Mode)
- Converter naturally skips pulses at low load
- Firmware fine-tunes pulse skipping via FB injection

### Feedback (FB) Injection Circuit
```
              RC Filter
PWM_FB ──────/\/\/\/──┬──────> FB Pin (MP3431)
              R_fb    │
                     ═══ C_fb
                      │
                     GND
```

**Purpose**: Inject a DC voltage into the FB pin to modulate the converter's output current. At low brightness, we can "trick" the converter into thinking the load is lighter than it is, encouraging more pulse skipping.

---

## CH32X035 Timer Capture Configuration

### Hardware Resources
The CH32X035 has multiple timers with input capture capability:
- **TIM1**: Advanced timer (16-bit), 4 channels
- **TIM2**: General-purpose timer (16-bit), 4 channels
- **TIM3**: General-purpose timer (16-bit), 2 channels

### Recommended Configuration
```c
/* Timer Capture for Zero-Cross Detection */
#define ZC_TIMER        TIM2
#define ZC_CHANNEL      TIM_CH1
#define ZC_PRESCALER    (48 - 1)  /* 48MHz / 48 = 1MHz tick (1µs resolution) */
#define ZC_PERIOD       0xFFFF    /* Free-running 16-bit counter */

/* Capture Configuration */
#define ZC_CAPTURE_EDGE TIM_CC1P    /* Falling edge = inductor current → 0 */
#define ZC_FILTER       0x03        /* 8 clock cycles of filtering */
```

### Zephyr Device Tree (Future)
```dts
/* Placeholder for Zephyr DT binding when CH32X035 port is complete */
&tim2 {
    status = "okay";
    
    zero_cross_capture: capture@0 {
        compatible = "st,stm32-timers-capture";  /* Similar to STM32 */
        reg = <0>;
        capture-edge = <TIM_EDGE_FALLING>;
    };
};
```

---

## Software Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      key_map.c                              │
│                   (brightness 0-255)                        │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   dimming_manager.c                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  brightness < ANALOG_THRESHOLD?                       │  │
│  │    YES → Use CrCM mode (zero_cross + FB injection)    │  │
│  │    NO  → Use standard PWM dimming                     │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────┬───────────────────────────────────┘
                          │
          ┌───────────────┼───────────────┐
          ▼               ▼               ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  LED_EN/PWM  │  │  FB_INJECT   │  │  SW_MONITOR  │
│  (GPIO/PWM)  │  │  (PWM→DAC)   │  │  (Capture)   │
└──────────────┘  └──────────────┘  └──────────────┘
```

### Components

| File | Purpose |
|------|---------|
| `lib/dimming_manager.c` | High-level dimming strategy, mode selection |
| `lib/zero_cross.c` | Timer capture ISR, CrCM frequency adjustment |
| `lib/analog_dimming.c` | PWM→Voltage conversion for FB injection |

---

## Zero-Cross Detection Algorithm

### Timer Capture ISR
```c
static volatile uint32_t last_capture_time;
static volatile bool edge_detected;

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET) {
        last_capture_time = TIM_GetCapture1(TIM2);
        edge_detected = true;
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
    }
}
```

### CrCM Measurement Procedure
```
1. Record T_pwm_off (timestamp when LED_EN goes LOW or duty → 0)
2. Wait for falling edge on SW_MONITOR
3. Capture T_zc (zero-cross timestamp)
4. Calculate: T_dead = T_zc - T_pwm_off
5. Adjust PWM period based on T_dead:
   - T_dead > TARGET_NS + HYSTERESIS → Increase frequency
   - T_dead < TARGET_NS - HYSTERESIS → Decrease frequency
6. Goal: Maintain T_dead ≈ TARGET_NS (sweet spot)
```

### Frequency Adjustment (P-Controller)
```c
#define CRM_TARGET_NS       200     /* Target dead-time in ns */
#define CRM_HYSTERESIS_NS   50      /* ±50ns tolerance band */
#define CRM_KP              0.1f    /* Proportional gain (TBD: tune on hardware) */
#define CRM_FREQ_MIN_HZ     50000   /* 50kHz minimum switching */
#define CRM_FREQ_MAX_HZ     500000  /* 500kHz maximum switching */

void crm_adjust_frequency(uint32_t t_dead_ns) {
    int32_t error = (int32_t)t_dead_ns - CRM_TARGET_NS;
    
    /* Within tolerance band - no adjustment needed */
    if (abs(error) < CRM_HYSTERESIS_NS) {
        return;
    }
    
    /* Proportional adjustment */
    float delta = CRM_KP * error;
    current_freq_hz = CLAMP(current_freq_hz + (int32_t)delta, 
                            CRM_FREQ_MIN_HZ, CRM_FREQ_MAX_HZ);
    
    timer_set_period(current_freq_hz);
}
```

### State Machine
```
         ┌──────────────┐
         │  IDLE        │
         │  (LED OFF)   │
         └──────┬───────┘
                │ brightness > 0 && brightness < THRESHOLD
                ▼
         ┌──────────────┐
         │  ACQUIRE     │◄────────────────────┐
         │  (Waiting)   │                     │
         └──────┬───────┘                     │
                │ edge_detected               │ timeout OR
                ▼                             │ no edge detected
         ┌──────────────┐                     │
         │  MEASURE     │                     │
         │  (Calculate) │─────────────────────┘
         └──────┬───────┘
                │ T_dead valid
                ▼
         ┌──────────────┐
         │  ADJUST      │◄────┐
         │  (Tune freq) │     │ |error| > HYSTERESIS
         └──────┬───────┘     │
                │ |error| < HYSTERESIS (N consecutive samples)
                ▼             │
         ┌──────────────┐     │
         │  LOCKED      │─────┘
         │  (Tracking)  │
         └──────────────┘
```

---

## Safety & Failure Handling

### Edge Detection Timeout
- If no falling edge is detected within `ACQUIRE_TIMEOUT_MS`, transition to **FALLBACK** mode
- Fallback: Use fixed PWM frequency (conservative, less efficient)

### Overcurrent Protection
- Monitor FB pin voltage via ADC
- If FB voltage exceeds threshold, immediately reduce LED_EN duty cycle

### Clamp Diode Stress
- BAT54 can handle intermittent clamping, but prolonged clamping indicates design issue
- Log warning if capture values consistently hit 0V or 3.3V rails

### Mode Oscillation Prevention
- Use **hysteresis band** (ANALOG_THRESHOLD ± 5 levels) when switching between CrCM and PWM modes
- Example: Switch to CrCM at brightness < 25, switch back to PWM at brightness > 30

---

## Kconfig Options

```kconfig
config ZBEAM_ZERO_CROSS_ENABLED
    bool "Enable Zero-Cross Detection for CrCM"
    default n
    help
      Enable inductor zero-cross detection for Critical Conduction Mode
      dimming. Requires hardware: voltage divider + clamping diodes on
      SW node connected to timer capture input.

if ZBEAM_ZERO_CROSS_ENABLED

config ZBEAM_ANALOG_DIM_THRESHOLD
    int "Brightness threshold for analog dimming (0-255)"
    default 30
    range 10 100
    help
      Below this brightness level, use CrCM + FB injection.
      Above this level, use standard PWM dimming.

config ZBEAM_CRM_TARGET_NS
    int "Target dead-time for CrCM (nanoseconds)"
    default 200
    range 100 1000
    help
      The target time between PWM-off and inductor zero-cross.
      Smaller values = more aggressive (higher efficiency risk).
      Larger values = more conservative (lower efficiency).

config ZBEAM_CRM_HYSTERESIS_NS
    int "Hysteresis band for frequency adjustment (ns)"
    default 50
    range 10 200

config ZBEAM_ACQUIRE_TIMEOUT_MS
    int "Timeout for edge acquisition (ms)"
    default 100
    range 10 500
    help
      If no zero-cross edge is detected within this time,
      fall back to fixed-frequency PWM.

endif # ZBEAM_ZERO_CROSS_ENABLED
```

---

## Implementation Phases

### Phase 1: Basic Infrastructure
- [ ] Create `include/zero_cross.h` and `lib/zero_cross.c`
- [ ] Implement timer capture initialization (CH32X035 HAL)
- [ ] Implement edge detection ISR with timestamp logging
- [ ] Add debug logging for captured timestamps

### Phase 2: FB Injection (Analog Dimming)
- [ ] Create `lib/analog_dimming.c`
- [ ] Configure PWM output for FB injection
- [ ] Implement RC filter on hardware
- [ ] Calibrate FB voltage vs. LED current curve

### Phase 3: CrCM Control Loop
- [ ] Implement P-controller for frequency adjustment
- [ ] Add state machine (IDLE → ACQUIRE → MEASURE → ADJUST → LOCKED)
- [ ] Tune controller gains on hardware
- [ ] Add timeout and fallback handling

### Phase 4: Integration & Safety
- [ ] Create `lib/dimming_manager.c` as unified interface
- [ ] Integrate with `key_map.c` brightness path
- [ ] Add overcurrent protection (ADC monitoring)
- [ ] Add mode oscillation prevention (hysteresis)
- [ ] Comprehensive hardware testing

---

## Hardware Bill of Materials

| Ref | Component | Value | Notes |
|-----|-----------|-------|-------|
| R1 | Resistor (divider high) | 100kΩ | 1% tolerance, 0402/0603 |
| R2 | Resistor (divider low) | 10kΩ | 1% tolerance, 0402/0603 |
| D1 | Schottky Diode | BAT54S | Dual series, SOT-23 package |
| R_fb | FB injection resistor | **TBD** | See calibration section |
| C_fb | FB filter capacitor | **TBD** | See calibration section |

---

## TBD Components (Requires Calibration)

> [!WARNING]
> The following values must be determined during hardware bring-up.

| Component | Purpose | Determination Method |
|-----------|---------|---------------------|
| **R_fb** | FB injection series resistor | Calculate from MP3431 FB input impedance and desired voltage swing. Start with 10kΩ. |
| **C_fb** | FB low-pass filter capacitor | Choose for ~1kHz cutoff: `C = 1 / (2π × R_fb × f_c)`. Start with 100nF. |
| **CRM_KP** | P-controller gain | Tune empirically. Start with 0.1, increase until oscillation, then back off 20%. |
| **CRM_TARGET_NS** | Dead-time target | Measure DCM threshold on specific inductor. Default 200ns is conservative. |
| **LED Current vs. FB Voltage** | Calibration curve | Sweep FB voltage from 0-1.2V, measure LED current at each step. |

### Calibration Procedure
1. **FB Injection Range**:
   - Connect scope to FB pin
   - Sweep PWM duty 0-100%
   - Record FB voltage at each step
   - Target: 0.1V (min brightness) to 1.0V (threshold brightness)

2. **CrCM Tuning**:
   - Set brightness to analog threshold - 5
   - Monitor T_dead on scope
   - Adjust CRM_TARGET_NS until efficiency peaks
   - Add ±20% margin for temperature variation

---

## References
- MP3431 Datasheet: Section 8.3 (PWM Dimming), Section 8.5 (Feedback)
- CH32X035 Reference Manual: Chapter 13 (General Purpose Timers)
- TI SLVA477: Zero-Voltage Switching in Boost Converters
- Anduril2: `ToyKeeper/hwdef-*.h` (FET+1 driver implementations)
