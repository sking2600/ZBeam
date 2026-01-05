/**
 * @file key_map.c
 * @brief Anduril-style FSM Graph Implementation.
 *
 * Implements Simple Mode with sweep-lock ramping:
 * - Hold to ramp brightness up/down
 * - Release to lock at current level
 * - Direction reverses at floor/ceiling
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/reboot.h>
#include <stdlib.h>
#include <string.h>

#include "fsm_engine.h"
#include "key_map.h"
#include "storage_manager.h"

LOG_MODULE_REGISTER(KeyMap, LOG_LEVEL_INF);

/* ========== Hardware Interface ========== */

/* 
 * PWM LED spec from devicetree.
 * Polarity (ACTIVE_LOW vs ACTIVE_HIGH) is handled by the "flags" field
 * in the devicetree definition (PWM_POLARITY_NORMAL/INVERTED).
 * See boards/esp32c3_supermini.overlay for the concrete configuration.
 */
static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET_OR(DT_ALIAS(pwm_led0), {0});

static void update_led_hardware(uint8_t level)
{
    if (!device_is_ready(pwm_led.dev)) {
        LOG_ERR("PWM device not ready");
        return;
    }
    
    /* Convert 0-255 to PWM pulse width */
    uint32_t pulse = (pwm_led.period * level) / 255;
    pwm_set_pulse_dt(&pwm_led, pulse);
    LOG_DBG("LED: %d", level);
}

/* ========== Brightness State ========== */

/* Smooth ramping: direct PWM value 0-255 */
static uint8_t current_brightness = 0;
static uint8_t memorized_brightness = 128;  /* Default to mid brightness */

/* Floor and ceiling for ramping */
#define BRIGHTNESS_FLOOR   1    /* Minimum (moonlight) */
#define BRIGHTNESS_CEILING 255  /* Maximum (turbo) */

/* Ramping state */
static struct k_timer ramp_timer;
static int ramp_direction = 0;  /* 1 = up, -1 = down, 0 = stopped */
static bool ramp_active = false;

/* Ramp speed: step size per tick (higher = faster) */
#define RAMP_STEP_SIZE 1  /* 1 PWM level per tick */

/* ========== Ramp Timer Handler ========== */

static void ramp_timer_handler(struct k_timer *timer)
{
    if (ramp_direction > 0) {
        /* Ramping UP */
        if (current_brightness < BRIGHTNESS_CEILING - RAMP_STEP_SIZE) {
            current_brightness += RAMP_STEP_SIZE;
        } else {
            current_brightness = BRIGHTNESS_CEILING;
            /* Hit ceiling - reverse direction */
            ramp_direction = -1;
        }
    } else if (ramp_direction < 0) {
        /* Ramping DOWN */
        if (current_brightness > BRIGHTNESS_FLOOR + RAMP_STEP_SIZE) {
            current_brightness -= RAMP_STEP_SIZE;
        } else {
            current_brightness = BRIGHTNESS_FLOOR;
            /* Hit floor - reverse direction */
            ramp_direction = 1;
        }
    }
    
    update_led_hardware(current_brightness);
}

static void start_ramping(int direction)
{
    ramp_direction = direction;
    ramp_active = true;
    uint32_t step_ms = CONFIG_ZBEAM_SWEEP_DURATION_MS / 255;
    if (step_ms < 1) step_ms = 1;
    
    k_timer_start(&ramp_timer, K_MSEC(step_ms), K_MSEC(step_ms));
    LOG_INF("Ramp start: dir=%d", direction);
}

static void stop_ramping(void)
{
    k_timer_stop(&ramp_timer);
    ramp_direction = 0;
    
    if (ramp_active) {
        memorized_brightness = current_brightness;  /* Save locked position */
    }
    ramp_active = false;
    LOG_INF("Ramp stop: locked at PWM %d", current_brightness);
}

/* ========== Node Routines ========== */

static void routine_off(void)
{
    stop_ramping();
    update_led_hardware(0);
    printk(">>> OFF state\n");
    LOG_INF(">>> OFF");
}

static void routine_on(void)
{
    stop_ramping();
    current_brightness = memorized_brightness;
    update_led_hardware(current_brightness);
    LOG_INF(">>> ON: PWM %d", current_brightness);
}

static void routine_moon(void)
{
    stop_ramping();
    current_brightness = BRIGHTNESS_FLOOR;
    update_led_hardware(current_brightness);
    LOG_INF(">>> MOON: PWM %d", current_brightness);
}

static void routine_turbo(void)
{
    stop_ramping();
    current_brightness = BRIGHTNESS_CEILING;
    update_led_hardware(current_brightness);
    LOG_INF(">>> TURBO: PWM %d", current_brightness);
}

static void routine_ramp(void)
{
    /* Ramping is handled by timer, just log entry */
    LOG_INF(">>> RAMP mode");
}

static void routine_lockout(void)
{
    stop_ramping();
    update_led_hardware(0);
    LOG_INF(">>> LOCKOUT (4C to unlock)");
}

static void routine_battcheck(void)
{
    stop_ramping();
    /* TODO: Read ADC, blink voltage */
    LOG_INF(">>> BATTCHECK (not implemented)");
    /* Placeholder: blink 3 times for 3.7V */
    for (int i = 0; i < 3; i++) {
        update_led_hardware(50);
        k_msleep(200);
        update_led_hardware(0);
        k_msleep(200);
    }
}

static void routine_blink(void)
{
    /* Quick feedback blink */
    update_led_hardware(100);
    k_msleep(50);
    update_led_hardware(0);
}

static void routine_factory_reset(void)
{
    stop_ramping();
    LOG_WRN("!!! FACTORY RESET !!!");
    storage_wipe_all();
    k_msleep(100);
    sys_reboot(SYS_REBOOT_COLD);
}

/* ========== Callbacks ========== */

/* Hold from OFF → start at moon, ramp up */
static struct fsm_node* cb_hold_from_off(struct fsm_node *self)
{
    current_brightness = BRIGHTNESS_FLOOR;
    update_led_hardware(current_brightness);
    start_ramping(1);  /* Ramp UP */
    return NULL;  /* Stay in RAMP state */
}

/* Hold from ON → ramp up from current */
static struct fsm_node* cb_hold_ramp_up(struct fsm_node *self)
{
    start_ramping(1);
    return NULL;
}

/* 2H from ON → ramp down from current */
static struct fsm_node* cb_hold_ramp_down(struct fsm_node *self)
{
    start_ramping(-1);
    return NULL;
}

/* Release → stop ramping, lock brightness, go to ON */
static struct fsm_node* cb_ramp_release(struct fsm_node *self)
{
    stop_ramping();
    return &node_on;
}

/* Momentary moon from lockout */
static struct fsm_node* cb_lockout_momentary(struct fsm_node *self)
{
    update_led_hardware(BRIGHTNESS_FLOOR);
    return NULL;  /* Stay in lockout, light while held */
}

static struct fsm_node* cb_lockout_release(struct fsm_node *self)
{
    update_led_hardware(0);
    return NULL;  /* Stay in lockout */
}

/* ========== Forward Declarations ========== */

/* All nodes need forward declaration for cross-references */
struct fsm_node node_off;
struct fsm_node node_on;
struct fsm_node node_ramp;
struct fsm_node node_moon;
struct fsm_node node_turbo;
struct fsm_node node_lockout;
struct fsm_node node_battcheck;
struct fsm_node node_blink;
struct fsm_node node_factory_reset;

/* ========== Node Definitions ==========
 *
 * IMPORTANT NOTES FOR CUSTOMIZATION:
 * 
 * 1. RELEASE CALLBACKS:
 *    - Only use release_callback on TRANSITIONAL states (e.g., RAMP)
 *    - Do NOT put release_callback on STEADY states (e.g., ON, OFF)
 *    - Why: release_callback fires on EVERY button release. If ON has a
 *      release_callback, clicking 1C to turn OFF will trigger the callback
 *      on release, potentially re-transitioning somewhere unexpected.
 *
 * 2. HOLD CALLBACKS vs HOLD MAP:
 *    - hold_callbacks[N]: Called when hold-N starts, return NULL to stay
 *      in current state, or return a node pointer to transition.
 *    - hold_map[N]: If no callback or callback returns NULL, this is the
 *      automatic transition target for hold-N.
 *
 * 3. CLICK CALLBACKS vs CLICK MAP:
 *    - Same pattern as hold: callback runs first, map is fallback.
 *    - Use callbacks for conditional logic before transitioning.
 *
 * 4. TRANSITIONAL vs STEADY STATES:
 *    - STEADY: User stays here until they press something (OFF, ON, LOCKOUT)
 *    - TRANSITIONAL: User passes through quickly (RAMP, BLINK)
 *    - Transitional states need release_callback to return to steady state.
 *
 * 5. MEMORY BEHAVIOR:
 *    - memorized_brightness stores the "locked" brightness on release.
 *    - When entering ON, we restore memorized_brightness.
 */


struct fsm_node node_off = {
    .id = NODE_OFF,
    .name = "OFF",
    .action_routine = routine_off,
    .timeout_ms = 0,
    
    .click_map = {
        [0] = &node_on,           /* 1C → ON */
        [1] = &node_turbo,        /* 2C → TURBO */
        [2] = &node_battcheck,    /* 3C → BATTCHECK */
        [3] = &node_lockout,      /* 4C → LOCKOUT */
        [4] = &node_factory_reset,/* 5C → FACTORY RESET */
    },
    .hold_map = {
        [0] = &node_ramp,         /* 1H → RAMP (moon + up) */
    },
    .hold_callbacks = {
        [0] = cb_hold_from_off,
    },
};

/*
 * NODE_ON: STEADY state - user stays here at locked brightness.
 * No release_callback! Clicks/holds transition to other states.
 */
struct fsm_node node_on = {
    .id = NODE_ON,
    .name = "ON",
    .action_routine = routine_on,
    .timeout_ms = 0,
    
    .click_map = {
        [0] = &node_off,          /* 1C → OFF */
        [1] = &node_turbo,        /* 2C → TURBO */
        [3] = &node_lockout,      /* 4C → LOCKOUT */
    },
    .hold_map = {
        [0] = &node_ramp,         /* 1H → RAMP UP */
        [1] = &node_ramp,         /* 2H → RAMP DOWN */
    },
    .hold_callbacks = {
        [0] = cb_hold_ramp_up,    /* Sets direction before entering RAMP */
        [1] = cb_hold_ramp_down,
    },
    /* NOTE: No release_callback! This is a STEADY state. */
};

/*
 * NODE_RAMP: TRANSITIONAL state - user is actively ramping.
 * HAS release_callback to return to NODE_ON when button released.
 */
struct fsm_node node_ramp = {
    .id = NODE_RAMP,
    .name = "RAMP",
    .action_routine = routine_ramp,
    .timeout_ms = 0,
    
    /* CRITICAL: release_callback returns to ON with locked brightness */
    .release_callback = cb_ramp_release,
    
    .click_map = {
        [0] = &node_off,          /* 1C → OFF (cancel ramp) */
    },
};

struct fsm_node node_moon = {
    .id = NODE_MOON,
    .name = "MOON",
    .action_routine = routine_moon,
    .timeout_ms = 0,
    
    .click_map = {
        [0] = &node_off,
    },
    .hold_map = {
        [0] = &node_ramp,
    },
    .hold_callbacks = {
        [0] = cb_hold_ramp_up,
    },
};

struct fsm_node node_turbo = {
    .id = NODE_TURBO,
    .name = "TURBO",
    .action_routine = routine_turbo,
    .timeout_ms = 0,
    
    .click_map = {
        [0] = &node_off,          /* 1C → OFF */
        [1] = &node_on,           /* 2C → ON (memorized) */
    },
};

struct fsm_node node_lockout = {
    .id = NODE_LOCKOUT,
    .name = "LOCKOUT",
    .action_routine = routine_lockout,
    .timeout_ms = 0,
    
    .click_map = {
        [3] = &node_off,          /* 4C → unlock → OFF */
    },
    .hold_callbacks = {
        [0] = cb_lockout_momentary,
    },
    .release_callback = cb_lockout_release,
};

struct fsm_node node_battcheck = {
    .id = NODE_BATTCHECK,
    .name = "BATTCHECK",
    .action_routine = routine_battcheck,
    .timeout_ms = 2000,           /* Auto-return after 2s */
    .timeout_reverts = false,
    
    .click_map = {
        [0] = &node_off,
    },
};

struct fsm_node node_blink = {
    .id = NODE_BLINK,
    .name = "BLINK",
    .action_routine = routine_blink,
    .timeout_ms = CONFIG_ZBEAM_BLINK_RETURN_MS,
    .timeout_reverts = true,
};

struct fsm_node node_factory_reset = {
    .id = NODE_FACTORY_RESET,
    .name = "FACTORY_RESET",
    .action_routine = routine_factory_reset,
    .timeout_ms = 0,
};

/* Placeholder nodes for forward compatibility */
static struct fsm_node node_strobe_placeholder = {
    .id = NODE_STROBE,
    .name = "STROBE",
    .action_routine = routine_blink,
    .timeout_ms = 0,
    .click_map = { [0] = &node_off },
};

static struct fsm_node node_sweep_placeholder = {
    .id = NODE_SWEEP,
    .name = "SWEEP",
    .action_routine = routine_blink,
    .timeout_ms = 0,
    .click_map = { [0] = &node_off },
};

/* ========== Node Registry ========== */

struct fsm_node *all_nodes[NODE_COUNT] = {
    [NODE_OFF] = &node_off,
    [NODE_ON] = &node_on,
    [NODE_RAMP] = &node_ramp,
    [NODE_MOON] = &node_moon,
    [NODE_TURBO] = &node_turbo,
    [NODE_LOCKOUT] = &node_lockout,
    [NODE_BATTCHECK] = &node_battcheck,
    [NODE_BLINK] = &node_blink,
    [NODE_FACTORY_RESET] = &node_factory_reset,
    [NODE_STROBE] = &node_strobe_placeholder,
    [NODE_SWEEP] = &node_sweep_placeholder,
    /* Other nodes NULL by default */
};

/* ========== Initialization ========== */

void key_map_init(void)
{
    /* Initialize ramp timer */
    k_timer_init(&ramp_timer, ramp_timer_handler, NULL);
    
    /* Set default brightness to middle */
    memorized_brightness = 128;
    current_brightness = 0;
    
    LOG_INF("KeyMap initialized. Smooth ramping 0-255.");
}

struct fsm_node *get_start_node(void)
{
    return &node_off;
}

uint8_t key_map_get_current_pwm(void)
{
    return current_brightness;
}

int key_map_get_brightness_index(void)
{
    return current_brightness;  /* Now same as PWM value */
}

void key_map_set_brightness_index(int brightness)
{
    if (brightness >= 0 && brightness <= 255) {
        current_brightness = (uint8_t)brightness;
        memorized_brightness = current_brightness;
    }
}

