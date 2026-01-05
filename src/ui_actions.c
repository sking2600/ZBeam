/**
 * @file ui_actions.c
 * @brief Common UI Actions and State Logic for ZBeam.
 * 
 * Implements the shared behavior used by both Simple and Advanced UI trees.
 * Handles LED hardware updates, thermal regulation, strobes, and memory modes.
 *
 * @author Scott King
 * @license GPLv3
 */

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/reboot.h>
#include <stdlib.h>
#include <string.h>

#include "fsm_engine.h"
#include "batt_check.h"
#include "nvs_manager.h"
#include "thermal_manager.h"
#include "pm_manager.h"
#include "aux_manager.h"
#include "channel_manager.h"
#include "pwm_ramp.h"

#include "ui_actions.h" // Formerly key_map.h

LOG_MODULE_REGISTER(UiActions, LOG_LEVEL_INF);

/* ========== Hardware Interface ========== */
/* Main beam PWM handled via channel_manager.c */

/* ========== State Variables ========== */
static enum ui_mode current_ui_mode = 
#ifdef CONFIG_ZBEAM_DEFAULT_UI_MODE_ADVANCED
    UI_ADVANCED;
#else
    UI_SIMPLE;
#endif

static enum memory_mode current_mem_mode = 
#if defined(CONFIG_ZBEAM_MEMORY_MANUAL)
    MEM_MANUAL;
#elif defined(CONFIG_ZBEAM_MEMORY_HYBRID)
    MEM_HYBRID;
#else
    MEM_AUTO;
#endif

static uint32_t hybrid_timeout_ms = 
#ifdef CONFIG_ZBEAM_DEFAULT_HYBRID_MEM_TIMEOUT_MIN
    CONFIG_ZBEAM_DEFAULT_HYBRID_MEM_TIMEOUT_MIN * 60 * 1000;
#else
    0;
#endif

static uint8_t manual_mem_level = 
#ifdef CONFIG_ZBEAM_DEFAULT_MANUAL_MEM_LEVEL
    CONFIG_ZBEAM_DEFAULT_MANUAL_MEM_LEVEL;
#else
    128;
#endif

static uint8_t current_brightness = 0;
static uint8_t override_brightness = 0;
static uint8_t memorized_brightness = 128;
static uint8_t brightness_floor = CONFIG_ZBEAM_BRIGHTNESS_FLOOR;
static uint8_t brightness_ceiling = CONFIG_ZBEAM_BRIGHTNESS_CEILING;

static enum ramp_style current_ramp_style = 
#ifdef CONFIG_ZBEAM_DEFAULT_RAMP_STYLE_SMOOTH
    RAMP_SMOOTH;
#else
    RAMP_STEPPED;
#endif

static enum strobe_type current_strobe_mode = STROBE_PARTY;

static uint8_t stepped_ramp_steps = CONFIG_ZBEAM_STEPPED_RAMP_STEPS;

#define BRIGHTNESS_FLOOR   brightness_floor
#define BRIGHTNESS_CEILING brightness_ceiling

/* Strobe State */
static struct k_timer strobe_timer;
static uint8_t strobe_frequency = 12;
static bool strobe_on = false;
static bool party_mode = false;

/* Ramping State */
static struct k_timer ramp_timer;
static int ramp_direction = 0;
static bool ramp_active = false;
#define RAMP_STEP_SIZE 1

enum control_param { PARAM_BRIGHTNESS, PARAM_FREQUENCY };
static enum control_param active_param = PARAM_BRIGHTNESS;

/* Internal helpers */

/**
 * @brief Updates the LED hardware with the requested brightness level.
 * 
 * Wraps channel_apply_mix() to handle the actual hardware interaction.
 * 
 * @param level Brightness level (0-255)
 */
static void update_led_hardware(uint8_t level) {
    channel_apply_mix(level);
}

/**
 * @brief Periodic thermal regulation handler.
 * 
 * Called by thermal_timer. Reads temperature and adjusts output if necessary.
 * Note: Actual regulation logic is inside thermal_update(), this just triggers it.
 * 
 * @param timer Pointer to the k_timer instance
 */
static struct k_timer thermal_timer;
static void thermal_timer_handler(struct k_timer *timer) {
    thermal_update(current_brightness);
    update_led_hardware(current_brightness);
}

/* Strobe Delay Calculation */
static uint32_t get_strobe_delay_ms(uint8_t freq_idx) {
    uint32_t delay_max = 1000 / CONFIG_ZBEAM_STROBE_MIN_FREQ;
    uint32_t delay_min = 1000 / CONFIG_ZBEAM_STROBE_MAX_FREQ;
    uint32_t range = delay_max - delay_min;
    return delay_max - ((range * freq_idx) / 255);
}

/* ========== Ramp Logic ========== */

static void ramp_timer_handler(struct k_timer *timer) {
    uint8_t *target_val = (active_param == PARAM_BRIGHTNESS) ? 
                          &current_brightness : &strobe_frequency;
    
    // Use shared configuration for limits (Simple & Advanced share the same ramp config currently)
    uint8_t floor = (active_param == PARAM_BRIGHTNESS) ? brightness_floor : 1;
    uint8_t ceiling = (active_param == PARAM_BRIGHTNESS) ? brightness_ceiling : 255;
    
    /* Stepped Ramp Logic */
    if (active_param == PARAM_BRIGHTNESS && current_ramp_style == RAMP_STEPPED) {
        if (stepped_ramp_steps < 2) stepped_ramp_steps = 7; // Safety
        
        // Find current step index
        // Val = Floor + (Idx * Range) / (Steps - 1)
        // Inverse: Idx = (Val - Floor) * (Steps - 1) / Range
        // Use careful rounding to find nearest step
        
        uint32_t range = ceiling - floor;
        if (range == 0) range = 1;

        int32_t current_idx = ((int32_t)(*target_val - floor) * (stepped_ramp_steps - 1) + (range/2)) / range;
        
        // Move to next step
        if (ramp_direction > 0) current_idx++;
        else current_idx--;
        
        // Clamp
        if (current_idx >= stepped_ramp_steps) {
            current_idx = stepped_ramp_steps - 1;
            ramp_direction = -1; // Bounce
        }
        if (current_idx < 0) {
            current_idx = 0;
            ramp_direction = 1; // Bounce
        }
        
        // Calculate new value
        uint32_t new_val = floor + (current_idx * range) / (stepped_ramp_steps - 1);
        *target_val = (uint8_t)new_val;
        
    } else {
        /* Smooth / Linear Ramping */
        if (ramp_direction > 0) {
            if (*target_val < ceiling - RAMP_STEP_SIZE) *target_val += RAMP_STEP_SIZE;
            else { *target_val = ceiling; ramp_direction = -1; /* Bounce at top */ }
        } else if (ramp_direction < 0) {
            if (*target_val > floor + RAMP_STEP_SIZE) *target_val -= RAMP_STEP_SIZE;
            else { *target_val = floor; ramp_direction = 1; /* Bounce at bottom */ }
        }
    }
    
    if (active_param == PARAM_BRIGHTNESS) update_led_hardware(current_brightness);
}

void start_ramping(int direction) {
    ramp_direction = direction;
    ramp_active = true;
    
    uint32_t step_ms;
    
    if (active_param == PARAM_BRIGHTNESS) {
        if (current_ramp_style == RAMP_STEPPED) {
            // Stepped: Slower updates. E.g., one step every 200ms?
            step_ms = 200; 
        } else {
            // Smooth: Fast updates
            uint32_t duration_ms = CONFIG_ZBEAM_BRIGHTNESS_SWEEP_DURATION_MS;
            step_ms = duration_ms / 255;
            if (step_ms < 1) step_ms = 1;
        }
    } else {
        // Strobe always smooth-ish
        uint32_t duration_ms = CONFIG_ZBEAM_STROBE_SWEEP_DURATION_MS;
        step_ms = duration_ms / 255;
        if (step_ms < 1) step_ms = 1;
    }

    k_timer_start(&ramp_timer, K_MSEC(step_ms), K_MSEC(step_ms));
}

void stop_ramping(void) {
    k_timer_stop(&ramp_timer);
    ramp_direction = 0;
    if (ramp_active && active_param == PARAM_BRIGHTNESS) {
        memorized_brightness = current_brightness;
        // Persistence (Auto Memory behavior) - save immediately on ramp stop?
        // Or wait for turn off? Anduril usually saves on "1C" off or ramp stop.
        // For simplicity, we save "memorized" value here but write NVS later/seldom.
        #ifdef CONFIG_ZBEAM_NVS_ENABLED
        nvs_write_byte(NVS_ID_MEM_BRIGHTNESS, memorized_brightness);
        #endif
    }
    ramp_active = false;
}

/* ========== Strobe Logic ========== */
static uint32_t bike_counter = 0;

/* Helper to get random number in range [min, max] */
static uint32_t get_random(uint32_t min, uint32_t max) {
    if (max <= min) return min;
    return min + (sys_rand32_get() % (max - min + 1));
}

static void strobe_timer_handler(struct k_timer *timer) {
    uint32_t delay = 0;
    
    switch (current_strobe_mode) {
        case STROBE_PARTY:
            // Standard variable frequency strobe
            // ON for very short time (freeze motion), OFF for delay
            if (!strobe_on) {
                // Was OFF, turn ON
                strobe_on = true;
                update_led_hardware(255);
                delay = 2; // Short flash (2ms)
            } else {
                // Was ON, turn OFF
                strobe_on = false;
                update_led_hardware(0);
                // Delay based on frequency
                delay = get_strobe_delay_ms(strobe_frequency); 
                if (delay < 5) delay = 5; 
            }
            break;

        case STROBE_TACTICAL:
            // 50% duty cycle, annoying
            strobe_on = !strobe_on;
            update_led_hardware(strobe_on ? 255 : 0);
            delay = get_strobe_delay_ms(strobe_frequency);
            break;

        case STROBE_CANDLE:
            // Random flicker: emulate flame
            // Every tick, move towards target, update target randomly
            {
                // Simple implementation: random brightness every ~20ms
                // Bias towards lower-mid range for steady burn, occasional dips/flares
                // Center around brightness 50-100? Or use 'current_brightness' as base?
                // Let's use current_brightness as the "base" level settable by user (not yet impl)
                // Hardcode base 50 for now.
                uint8_t base = 80;
                // +/- 40
                uint8_t var = get_random(0, 80);
                uint8_t val = (base > 40) ? (base - 40 + var) : (var);
                update_led_hardware(val);
                delay = get_random(15, 30); // 15-30ms steps
            }
            break;

        case STROBE_BIKE:
            // Steady low, Pulse high
            // Period: 1000ms. High for 80ms.
            bike_counter += 20; // 50Hz tick base
            if (bike_counter > 1000) bike_counter = 0;
            
            if (bike_counter < 80) {
                update_led_hardware(255); // Flash
            } else {
                update_led_hardware(40);  // Steady
            }
            delay = 20;
            break;
            
        default:
            delay = 100;
            break;
    }
    
    k_timer_start(&strobe_timer, K_MSEC(delay), K_NO_WAIT);
}

void action_strobe_party(void) {
    LOG_INF("Action: Strobe PARTY");
    current_strobe_mode = STROBE_PARTY;
    strobe_on = false;
    pm_resume();
    k_timer_start(&strobe_timer, K_NO_WAIT, K_NO_WAIT);
}

void action_strobe_tactical(void) {
    LOG_INF("Action: Strobe TACTICAL");
    current_strobe_mode = STROBE_TACTICAL;
    strobe_on = false;
    pm_resume();
    k_timer_start(&strobe_timer, K_NO_WAIT, K_NO_WAIT);
}

void action_strobe_candle(void) {
    LOG_INF("Action: Strobe CANDLE");
    current_strobe_mode = STROBE_CANDLE;
    pm_resume();
    k_timer_start(&strobe_timer, K_NO_WAIT, K_NO_WAIT);
}

void action_strobe_bike(void) {
    LOG_INF("Action: Strobe BIKE");
    current_strobe_mode = STROBE_BIKE;
    bike_counter = 0;
    pm_resume();
    k_timer_start(&strobe_timer, K_NO_WAIT, K_NO_WAIT);
}

struct fsm_node* action_strobe_next(uint8_t count) {
    int next = (int)current_strobe_mode + 1;
    if (next >= STROBE_COUNT) next = 0;
    current_strobe_mode = (enum strobe_type)next;
    
    // Restart timer with new logic
    k_timer_stop(&strobe_timer);
    
    switch (current_strobe_mode) {
        case STROBE_PARTY: action_strobe_party(); break;
        case STROBE_TACTICAL: action_strobe_tactical(); break;
        case STROBE_CANDLE: action_strobe_candle(); break;
        case STROBE_BIKE: action_strobe_bike(); break;
        default: action_strobe_party(); break;
    }
    
    return NULL; // Stay in same state (ADV_STROBE group)
}

/* ========== Public Action Routines ========== */

/* ========== Memory State w/ Hybrid Support ========== */
static int64_t last_off_time = 0;

void action_off(void) {
    stop_ramping();
    update_led_hardware(0);
    k_timer_stop(&thermal_timer);
    k_timer_stop(&strobe_timer);
    
    /* Record timestamp for Hybrid Memory */
    last_off_time = k_uptime_get();
    
    pm_suspend();
    LOG_INF("Action: OFF (Ts: %lld)", last_off_time);
}

void action_on(void) {
    pm_resume();
    k_timer_start(&thermal_timer, K_MSEC(500), K_MSEC(500));
    stop_ramping();
    
    if (override_brightness > 0) {
        current_brightness = override_brightness;
        override_brightness = 0; // Consume one-shot override
        update_led_hardware(current_brightness);
        LOG_INF("Action: ON (Override: %d)", current_brightness);
        return;
    }

    uint8_t target_pwm = memorized_brightness;

    // Memory Logic
    switch (current_mem_mode) {
        case MEM_MANUAL:
            target_pwm = manual_mem_level;
            break;

        case MEM_HYBRID:
            if (hybrid_timeout_ms > 0) {
                int64_t now = k_uptime_get();
                if ((now - last_off_time) > hybrid_timeout_ms) {
                     // Exceeded timeout -> Use Manual Level
                     target_pwm = manual_mem_level;
                } else {
                    // Within timeout -> Use Auto (last used)
                    target_pwm = memorized_brightness;
                }
            } else {
                target_pwm = memorized_brightness;
            }
            break;

        case MEM_AUTO:
        default:
            target_pwm = memorized_brightness;
            break;
    }
    
    current_brightness = target_pwm;
    update_led_hardware(current_brightness);
    LOG_INF("Action: ON (%d) [Mode: %d]", current_brightness, current_mem_mode);
}

void action_moon(void) {
    pm_resume();
    k_timer_stop(&strobe_timer);
    current_brightness = BRIGHTNESS_FLOOR;
    update_led_hardware(current_brightness);
    LOG_INF("Action: MOON");
}

void action_turbo(void) {
    pm_resume();
    stop_ramping();
    current_brightness = BRIGHTNESS_CEILING; // Or 255 absolute turbo
    update_led_hardware(current_brightness);
    LOG_INF("Action: TURBO");
}

void action_ramp(void) {
    // Just a placeholder, usually entered via hold holding
    LOG_INF("Action: RAMP");
}

void action_lockout(void) {
    stop_ramping();
    update_led_hardware(0);
    LOG_INF("Action: LOCKOUT");
}

void action_battcheck(void) {
    stop_ramping();
    LOG_INF("Action: BATTCHECK");
    uint16_t mv = batt_read_voltage_mv();
    uint8_t major, minor;
    batt_calculate_blinks(mv, &major, &minor);
    // Blocking blink sequence (simple implementation)
    for (int i=0; i<major; i++) { update_led_hardware(100); k_msleep(100); update_led_hardware(0); k_msleep(300); }
    k_msleep(800);
    for (int i=0; i<minor; i++) { update_led_hardware(100); k_msleep(100); update_led_hardware(0); k_msleep(300); }
}

void action_tempcheck(void) {
    stop_ramping();
    LOG_INF("Action: TEMPCHECK");
    int32_t mc = thermal_get_temp_mc();
    int32_t c = (mc + 500) / 1000;
    
    uint8_t major = c / 10;
    uint8_t minor = c % 10;
    
    for (int i=0; i<major; i++) { update_led_hardware(100); k_msleep(100); update_led_hardware(0); k_msleep(300); }
    k_msleep(800);
    for (int i=0; i<minor; i++) { update_led_hardware(100); k_msleep(100); update_led_hardware(0); k_msleep(300); }
}

void action_strobe(void) {
    stop_ramping();
    party_mode = false;
    active_param = PARAM_FREQUENCY; 
    uint32_t delay = get_strobe_delay_ms(strobe_frequency);
    k_timer_start(&strobe_timer, K_MSEC(delay), K_NO_WAIT);
    update_led_hardware(255);
    strobe_on = true;
    LOG_INF("Action: STROBE");
}

void action_aux_config(void)
{
    LOG_INF("Action: AUX Config (Cycle Mode)");
    aux_cycle_mode();
    
    /* Visual feedback: Blink main beam briefly */
    uint8_t prev = ui_get_current_pwm();
    update_led_hardware(255);
    k_msleep(20);
    update_led_hardware(prev);
}

/* Config Buzz Logic */
static struct k_timer buzz_timer;
static bool buzz_state = false;
static void buzz_timer_handler(struct k_timer *timer) {
    buzz_state = !buzz_state;
    update_led_hardware(buzz_state ? 4 : 1);
}

void action_config_floor(void) {
    LOG_INF("Config: Floor (Wait for clicks)");
    k_timer_init(&buzz_timer, buzz_timer_handler, NULL);
    k_timer_start(&buzz_timer, K_MSEC(20), K_MSEC(20)); // 50Hz Buzz
}

void action_config_ceiling(void) {
    LOG_INF("Config: Ceiling (Wait for clicks)");
    k_timer_init(&buzz_timer, buzz_timer_handler, NULL);
    k_timer_start(&buzz_timer, K_MSEC(20), K_MSEC(20));
}

void action_config_steps(void) {
    LOG_INF("Config: Steps (Wait for clicks)");
    k_timer_init(&buzz_timer, buzz_timer_handler, NULL);
    k_timer_start(&buzz_timer, K_MSEC(20), K_MSEC(20));
}

struct fsm_node* cb_config_floor_set(struct fsm_node *self, int count) {
    k_timer_stop(&buzz_timer);
    if (count > 0) {
        brightness_floor = (uint8_t)count;
        LOG_INF("Floor set to: %d", brightness_floor);
        #ifdef CONFIG_ZBEAM_NVS_ENABLED
        nvs_write_byte(NVS_ID_RAMP_FLOOR, brightness_floor);
        #endif
    }
    // Transition to Ceiling (defined in ui_advanced.c)
    extern struct fsm_node adv_config_ceiling;
    return &adv_config_ceiling;
}

struct fsm_node* cb_config_ceiling_set(struct fsm_node *self, int count) {
    k_timer_stop(&buzz_timer);
    if (count > 0) {
        // Ceiling in Anduril is 151 - N. In ZBeam 1-255:
        // Let's do 256 - N.
        brightness_ceiling = (uint8_t)(256 - count);
        if (brightness_ceiling < brightness_floor) brightness_ceiling = brightness_floor + 1;
        LOG_INF("Ceiling set to: %d", brightness_ceiling);
        #ifdef CONFIG_ZBEAM_NVS_ENABLED
        nvs_write_byte(NVS_ID_RAMP_CEILING, brightness_ceiling);
        #endif
    }
    // Transition back to ON
    extern struct fsm_node adv_on;
    return &adv_on;
}

struct fsm_node* cb_config_steps_set(struct fsm_node *self, int count) {
    k_timer_stop(&buzz_timer);
    // Steps logic not fully impl, just return
    extern struct fsm_node adv_on;
    return &adv_on;
}

void action_cal_voltage_entry(void) {
    LOG_INF("Cal: Voltage (Wait for clicks)");
    k_timer_init(&buzz_timer, buzz_timer_handler, NULL);
    k_timer_start(&buzz_timer, K_MSEC(20), K_MSEC(20));
}

struct fsm_node* cb_cal_voltage_set(struct fsm_node *self, int count) {
    k_timer_stop(&buzz_timer);
    if (count > 0) {
        // Count = Voltage * 10. e.g. 42 = 4.2V.
        uint16_t mv = count * 100;
        batt_calibrate_voltage(mv);
    }
    extern struct fsm_node adv_battcheck;
    return &adv_battcheck;
}

void action_cal_thermal_entry(void) {
    LOG_INF("Cal: Thermal Current (Wait for clicks)");
    k_timer_init(&buzz_timer, buzz_timer_handler, NULL);
    k_timer_start(&buzz_timer, K_MSEC(20), K_MSEC(20));
}

struct fsm_node* cb_cal_thermal_set(struct fsm_node *self, int count) {
    k_timer_stop(&buzz_timer);
    if (count > 0) {
        // Count = Degrees C
        thermal_calibrate_current_temp((int32_t)count);
    }
    // Transition to Limit
    extern struct fsm_node adv_cal_thermal_limit;
    return &adv_cal_thermal_limit;
}

void action_cal_thermal_limit_entry(void) {
    LOG_INF("Cal: Thermal Limit (Wait for clicks)");
    k_timer_init(&buzz_timer, buzz_timer_handler, NULL);
    k_timer_start(&buzz_timer, K_MSEC(20), K_MSEC(20));
}

struct fsm_node* cb_cal_thermal_limit_set(struct fsm_node *self, int count) {
    k_timer_stop(&buzz_timer);
    if (count > 0) {
        // Limit = 30 + Count
        uint8_t limit = 30 + count;
        thermal_set_limit(limit);
    }
    extern struct fsm_node adv_tempcheck;
    return &adv_tempcheck;
}

void action_factory_reset(void)
 {
    stop_ramping();
    LOG_INF("Action: FACTORY RESET");
    #ifdef CONFIG_ZBEAM_NVS_ENABLED
    nvs_wipe_all();
    #endif
    sys_reboot(SYS_REBOOT_COLD);
}

/* ========== System Init & Mode Switching ========== */

struct fsm_node *get_start_node(void) {
    if (current_ui_mode == UI_SIMPLE) {
        return get_simple_off_node();
    } else {
        return get_advanced_off_node();
    }
}

struct fsm_node *ui_toggle_mode(void) {
    if (current_ui_mode == UI_SIMPLE) {
        current_ui_mode = UI_ADVANCED;
        LOG_INF("UI Mode -> ADVANCED");
    } else {
        current_ui_mode = UI_SIMPLE;
        LOG_INF("UI Mode -> SIMPLE");
    }
    
    #ifdef CONFIG_ZBEAM_NVS_ENABLED
    nvs_write_byte(NVS_ID_UI_MODE, (uint8_t)current_ui_mode);
    #endif
    
    return get_start_node();
}

void ui_init(void) {
    k_timer_init(&ramp_timer, ramp_timer_handler, NULL);
    k_timer_init(&strobe_timer, strobe_timer_handler, NULL);
    k_timer_init(&thermal_timer, thermal_timer_handler, NULL);
    
    thermal_init();
    batt_init();
    pm_init();
    channel_init();
    aux_init();
    
    memorized_brightness = 128;
    
    // Default from Kconfig
#ifdef CONFIG_ZBEAM_DEFAULT_UI_MODE_ADVANCED
    current_ui_mode = UI_ADVANCED;
#else
    current_ui_mode = UI_SIMPLE;
#endif
    
    #ifdef CONFIG_ZBEAM_NVS_ENABLED
    nvs_init_fs();
    nvs_read_byte(NVS_ID_MEM_BRIGHTNESS, &memorized_brightness);
    nvs_read_byte(NVS_ID_RAMP_FLOOR, &brightness_floor);
    nvs_read_byte(NVS_ID_RAMP_CEILING, &brightness_ceiling);
    
    uint8_t mode_val = 0;
    if (nvs_read_byte(NVS_ID_UI_MODE, &mode_val) == 0) {
        current_ui_mode = (enum ui_mode)mode_val;
    }
    
    // Load Memory config
    uint8_t mem_mode_val = 0;
    if (nvs_read_byte(NVS_ID_MEMORY_MODE, &mem_mode_val) == 0) {
        current_mem_mode = (enum memory_mode)mem_mode_val;
    }
    
    nvs_read_byte(NVS_ID_MANUAL_MEM_LEVEL, &manual_mem_level);
    
    // Hybrid timeout is 32-bit, but our simple NVS wrapper is 8-bit.
    // For now, let's assume it's stored in minutes in an 8-bit slot OR strict NVS read needed.
    // Let's implement a quick fix: NVS_ID_HYBRID_TIMEOUT stores MINUTES.
    uint8_t hybrid_mins = 0;
    if (nvs_read_byte(NVS_ID_HYBRID_TIMEOUT, &hybrid_mins) == 0) {
        hybrid_timeout_ms = hybrid_mins * 60 * 1000;
    }
    
    uint8_t ramp_style_val = 0;
    if (nvs_read_byte(NVS_ID_RAMP_STYLE, &ramp_style_val) == 0) {
        current_ramp_style = (enum ramp_style)ramp_style_val;
    }
    #endif
    
    LOG_INF("UI Init Complete. Current Mode: %s (%d)", 
            (current_ui_mode == UI_ADVANCED) ? "ADVANCED" : "SIMPLE",
            current_ui_mode);

}

/* Wrappers */
uint8_t ui_get_current_pwm(void) { return current_brightness; }
uint8_t ui_get_strobe_freq(void) { return strobe_frequency; }
enum ui_mode ui_get_current_mode(void) { return current_ui_mode; }

void ui_set_next_brightness(uint8_t level) {
    override_brightness = level;
}
void ui_set_next_brightness_floor(void) { override_brightness = brightness_floor; }
void ui_set_next_brightness_ceiling(void) { override_brightness = brightness_ceiling; }

struct fsm_node* action_channel_cycle(uint8_t count) {
    channel_cycle_mode();
    // Return a short blink for feedback (using existing blink-buzz if possible or just returning current state)
    return NULL; 
}

struct fsm_node *action_toggle_ramp_style(uint8_t count) {
    if (current_ramp_style == RAMP_SMOOTH) current_ramp_style = RAMP_STEPPED;
    else current_ramp_style = RAMP_SMOOTH;
    
    LOG_INF("Ramp Style: %s", (current_ramp_style == RAMP_SMOOTH) ? "SMOOTH" : "STEPPED");
    
    #ifdef CONFIG_ZBEAM_NVS_ENABLED
    nvs_write_byte(NVS_ID_RAMP_STYLE, (uint8_t)current_ramp_style);
    #endif
    
    // Feedback: Blink once for stepped, buzz for smooth? Or just blink.
    uint8_t prev = ui_get_current_pwm();
    update_led_hardware(0);
    k_msleep(100);
    update_led_hardware(prev);
    
    return NULL;
}
