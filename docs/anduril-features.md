# Anduril Features Catalog

## Hardware Configuration

### Development Hardware (ESP32-C3 SuperMini)
| Component | Status | Notes |
|-----------|--------|-------|
| Button | ✅ Connected | GPIO 9 (active-low) |
| Primary LED | ✅ Connected | GPIO 8 (PWM) |
| Aux LED (SK6812) | ⏳ Planned | RGB addressable LED |
| Temperature Sensor | ⏳ Stubbed | NTC thermistor TBD |
| Voltage Sense | ⏳ Planned | ADC for battery check |

### Target Production Hardware (CH32X035)
- **MCU**: CH32X035 (QFN20, 62KB Flash, 20KB RAM)
- **Primary Emitter**: Single/Multi-channel LED with PWM driver
- **Aux LED**: SK6812 (WS2812 compatible)
- **Tint Control**: Dual-channel warm/cool emitter (future)
- **USB-C PD**: Built-in (crystalless operation)

> [!NOTE]
> Development uses ESP32-C3 for convenience (larger memory, USB-serial).
> Production target is CH32X035 for cost and USB-PD integration.

---

## API Design Requirements

### Multi-Channel Ramp Tables
```c
// Support 1-3 PWM channels per ramp step
struct ramp_point {
    uint16_t ch1;  // Primary channel (or warm)
    uint16_t ch2;  // Secondary channel (or cool) 
    uint16_t ch3;  // Tertiary channel (RGB aux)
};

// Ramp table with N steps
struct ramp_table {
    uint8_t num_channels;    // 1, 2, or 3
    uint8_t num_steps;       // e.g., 150
    const struct ramp_point *points;
};
```

### Tint Ramping API
```c
// Tint position: 0 = full warm, 255 = full cool
void led_set_tint(uint8_t tint_position);

// Combined brightness + tint
void led_set_output(uint8_t brightness, uint8_t tint);
```

### Aux LED API (SK6812)
```c
// RGB color for aux LED
void aux_led_set_color(uint8_t r, uint8_t g, uint8_t b);

// Aux LED modes
enum aux_led_mode {
    AUX_OFF,
    AUX_LOW,
    AUX_HIGH,
    AUX_BLINK,
    AUX_VOLTAGE_COLOR,  // Green→Yellow→Red based on battery
};
```

---

## Feature Status

### Legend
- ✅ Implemented
- 🔨 In Progress
- ⏳ Planned
- ❌ Blocked
- 🔬 Needs Testing

---

## Phase 1: Simple Mode (Priority: HIGH)

### Core Functions

| Feature | Status | Notes |
|---------|--------|-------|
| OFF → ON (1C) | ✅ | Toggle with memory |
| ON → OFF (1C) | ✅ | Toggle |
| **Sweep-Lock Ramping** | ✅ | Hold to ramp, release to lock |
| Ramp Up (1H from ON) | ✅ | Smooth brightness increase |
| Ramp Down (2H from ON) | ✅ | Smooth brightness decrease |
| Moon (1H from OFF) | ✅ | Start at floor, ramp up |
| Turbo (2C) | ✅ | Jump to ceiling |
| Lockout (4C) | ✅ | Momentary moon while held |
| Battery Check (3C from OFF) | 🔨 | Placeholder blinks (needs ADC) |
| Brightness Memory | ✅ | Remembers last locked level |

### Sweep-Lock Ramping (Special Feature)
**Behavior**: User holds button, brightness sweeps up/down continuously. Upon release, brightness locks at current level.

```
1H from OFF → Start at floor, ramp up
              Release → Lock at current level
              
1H from ON → Ramp up from current
2H from ON → Ramp down from current
             Release → Lock at current level
             
At ceiling → Reverse direction
At floor → Reverse direction (or blink)
```

### Problems/Blockers
- [ ] None currently

---

## Phase 2: Utility Modes (Priority: MEDIUM)

| Feature | Status | Notes |
|---------|--------|-------|
| Temperature Check | ⏳ | Blink °C (needs sensor) |
| SOS Mode | ⏳ | Morse code pattern |
| Beacon Mode | ⏳ | Periodic flash |
| Sunset Timer | ⏳ | Gradual fade-off |
| Momentary Mode | ⏳ | Light only while held |

### Problems/Blockers
- [ ] Temperature sensor not connected (using stub)

---

## Phase 3: Strobe Modes (Priority: LOW)

| Feature | Status | Notes |
|---------|--------|-------|
| Tactical Strobe | ⏳ | Disorienting ~10Hz |
| Party Strobe | ⏳ | Variable speed |
| Candle Mode | ⏳ | Flickering flame simulation |
| Lightning Mode | ⏳ | Random flashes |
| Bike Flasher | ⏳ | Stutter pattern |

### Problems/Blockers
- [ ] None currently

---

## Phase 4: Configuration (Priority: MEDIUM)

| Feature | Status | Notes |
|---------|--------|-------|
| Ramp Floor Config | ⏳ | Set minimum brightness |
| Ramp Ceiling Config | ⏳ | Set maximum brightness |
| Stepped vs Smooth Toggle | ⏳ | 3C while ON |
| Simple/Advanced UI Toggle | ⏳ | 10H from OFF |
| Factory Reset | ✅ | 5C from OFF |
| Aux LED Config | ⏳ | 7C from OFF |

### Problems/Blockers
- [ ] None currently

---

## Phase 5: Multi-Channel Support (Priority: FUTURE)

| Feature | Status | Notes |
|---------|--------|-------|
| Tint Ramping (3H) | ⏳ | Warm↔Cool blend |
| RGB Aux LED | ⏳ | SK6812 driver |
| Voltage-based Aux Color | ⏳ | Battery indicator |
| Multi-channel Ramp Tables | ⏳ | API designed |

### Problems/Blockers
- [ ] Hardware not yet assembled
- [ ] SK6812 driver not implemented

---

## Button Mapping Summary

### From OFF

| Action | Simple Mode | Advanced Mode |
|--------|-------------|---------------|
| 1C | ON (memory) | ON (memory) |
| 1H | Moon → Ramp up | Moon → Ramp up |
| 2C | Turbo | Turbo |
| 3C | Battery check | Battery check |
| 3H | — | Strobe modes |
| 4C | Lockout | Lockout |
| 5C | — | Momentary |
| 10H | Adv UI toggle | Simple UI toggle |
| 13H | — | Factory reset |

### From ON

| Action | Simple Mode | Advanced Mode |
|--------|-------------|---------------|
| 1C | OFF | OFF |
| 1H | Ramp up | Ramp up |
| 2C | Turbo | Turbo |
| 2H | Ramp down | Ramp down |
| 3C | — | Smooth/Step toggle |
| 3H | — | Tint ramp |
| 4C | Lockout | Lockout |
| 5H | — | Sunset timer |
| 7H | — | Ramp config |

---

## FSM Architecture (Extensibility)

> [!IMPORTANT]
> **Design Priority**: Keep the FSM flexible so end users can easily create custom UIs.

### Node Structure
Each state is a self-contained `fsm_node` struct with:
- `action_routine`: Function called on state entry
- `click_map[5]`: Navigate to another node on 1-5 clicks
- `hold_map[5]`: Navigate to another node on 1-5 hold
- `click_callbacks[5]`: Custom logic for click events
- `hold_callbacks[5]`: Custom logic for hold events
- `release_callback`: Called when button is released
- `timeout_ms`: Auto-transition after N milliseconds

### Adding a Custom Node
```c
/* 1. Define your routine */
static void routine_custom(void) {
    LOG_INF("Custom mode activated!");
    update_led_hardware(128);
}

/* 2. Create the node */
struct fsm_node node_custom = {
    .id = NODE_CUSTOM,
    .name = "CUSTOM",
    .action_routine = routine_custom,
    .click_map = {
        [0] = &node_off,  /* 1C → OFF */
    },
};

/* 3. Wire it into existing nodes */
// In node_off.click_map:
//   [2] = &node_custom,  /* 3C → CUSTOM */

/* 4. Register in all_nodes[] */
all_nodes[NODE_CUSTOM] = &node_custom;
```

### Design Principles
1. **No hardcoded transitions** - All navigation via maps/callbacks
2. **Nodes are independent** - Each node defines its own behavior
3. **Callbacks for complex logic** - Return `NULL` to stay, or pointer to transition
4. **Forward declarations** - Enable circular references between nodes

### ⚠️ Steady vs Transitional States

> [!CAUTION]
> **Do NOT put `release_callback` on STEADY states!**

| Type | Examples | `release_callback` |
|------|----------|-------------------|
| **STEADY** | OFF, ON, LOCKOUT | ❌ Never |
| **TRANSITIONAL** | RAMP, BLINK | ✅ Required |

**Why?** `release_callback` fires on EVERY button release. If you put it on a STEADY state like ON:
1. User clicks 1C to turn OFF
2. FSM transitions to OFF
3. But then release_callback fires and transitions back!

**Rule**: Only add `release_callback` to states where the user is actively holding the button (ramping, momentary modes).

---

## Memory Budget Tracking

| Category | Estimated | Actual | Notes |
|----------|-----------|--------|-------|
| Baseline (no features) | 50 KB | — | Kernel + drivers |
| Simple Mode | +2.5 KB | — | Phase 1 |
| Utility Modes | +1.5 KB | — | Phase 2 |
| Strobe Modes | +1.5 KB | — | Phase 3 |
| Config Menus | +1.8 KB | — | Phase 4 |
| Multi-channel | +1.0 KB | — | Phase 5 |
| **Total** | ~58 KB | — | Budget: 62 KB |

---

## Development Log

### 2024-12-19
- Created feature catalog
- Confirmed hardware setup (1 button, 1 LED)
- Priority: Simple Mode with sweep-lock ramping
- API requirements defined for future multi-channel support
- Implemented Simple Mode FSM with:
  - OFF, ON, RAMP, MOON, TURBO, LOCKOUT, BATTCHECK nodes
  - Sweep-lock ramping (hold to sweep, release to lock)
  - Direction reversal at floor/ceiling
  - Brightness memory
- Added FSM extensibility documentation
- Build successful: 134KB flash, 54KB RAM (ESP32-C3 dev)
- Refactored input handling to use Zephyr Input subsystem directly
  - Removed manual callback glue from `main.c`
  - Encapsulated input event processing in `multi_tap_input` library
- **Debugging Success**:
  - Resolved "2 presses to turn ON" bug (Device Tree overlay conflict)
  - Fixed system boot crash (Safety Monitor stack overflow)
  - Fixed "LED stays OFF" bug (Brightness memory overwrite)
  - Adjusted sweep time to 3s (configurable)

