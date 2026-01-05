/**
 * @file ui_advanced.c
 * @brief Anduril 2 Advanced Mode FSM Topology.
 * 
 * Migrated from the original key_map.c layout.
 */

#include "ui_actions.h"
#include <stddef.h>

/* Forward Declarations */
struct fsm_node adv_off;
struct fsm_node adv_on;
struct fsm_node adv_ramp;
struct fsm_node adv_moon;
struct fsm_node adv_turbo;
struct fsm_node adv_lockout;
struct fsm_node adv_battcheck;
struct fsm_node adv_tempcheck;
struct fsm_node adv_blink;
struct fsm_node adv_reset;
struct fsm_node adv_strobe;     // Menu
struct fsm_node adv_party_strobe; 
struct fsm_node adv_config_floor;
struct fsm_node adv_config_ceiling;
struct fsm_node adv_config_steps;
struct fsm_node adv_aux_config;
struct fsm_node adv_cal_voltage;
struct fsm_node adv_cal_thermal_current;
struct fsm_node adv_cal_thermal_limit;

/* Callbacks (Reused from original key_map.c logic) */
static struct fsm_node* cb_adv_hold_from_off(struct fsm_node *self, int count) {
    action_moon(); 
    start_ramping(1); 
    return NULL;
}
static struct fsm_node* cb_adv_hold_ramp_up(struct fsm_node *self, int count) { start_ramping(1); return NULL; }
static struct fsm_node* cb_adv_hold_ramp_down(struct fsm_node *self, int count) { start_ramping(-1); return NULL; }
static struct fsm_node* cb_adv_ramp_release(struct fsm_node *self, int count) { stop_ramping(); return &adv_on; }
static struct fsm_node* cb_adv_lockout_momentary(struct fsm_node *self, int count) { action_moon(); return NULL; } // Improve later
static struct fsm_node* cb_adv_lockout_release(struct fsm_node *self, int count) { action_off(); return NULL; }
static struct fsm_node* cb_adv_strobe_release(struct fsm_node *self, int count) { stop_ramping(); return NULL; } // Should stop strobe
static struct fsm_node* cb_10h_toggle(struct fsm_node *self, int count) { return ui_toggle_mode(); }
static struct fsm_node* cb_adv_toggle_ramp_style(struct fsm_node *self, int count) { return action_toggle_ramp_style(count); }
static struct fsm_node* cb_strobe_next(struct fsm_node *self, int count) { return action_strobe_next(count); }

/* Node Definitions */

struct fsm_node adv_off = {
    .id = NODE_OFF, .name = "ADV_OFF", .action_routine = action_off,
    .click_map = {
        [0] = &adv_on,         // 1C
        [1] = &adv_turbo,      // 2C
        [2] = &adv_battcheck,  // 3C
        [3] = &adv_lockout,    // 4C
        [4] = &adv_reset,      // 5C (Factory Reset)
        [6] = &adv_aux_config, // 7C
    },
    .hold_map = {
        [0] = &adv_ramp,       // 1H
        [2] = &adv_strobe,     // 3H (Strobe)
    },
    .hold_callbacks = {
        [0] = cb_adv_hold_from_off,
        [9] = cb_10h_toggle,   // 10H
    }
};

struct fsm_node adv_on = {
    .id = NODE_ON, .name = "ADV_ON", .action_routine = action_on,
    .click_map = {
        [0] = &adv_off,        // 1C
        [1] = &adv_turbo,      // 2C
        [3] = &adv_lockout,    // 4C
    },
    .click_callbacks = {
        [2] = cb_adv_toggle_ramp_style, // 3C
    },
    .hold_map = {
        [0] = &adv_ramp,
        [1] = &adv_ramp,
        [6] = &adv_config_floor, // 7H -> Ramp Config
    },
    .hold_callbacks = {
        [0] = cb_adv_hold_ramp_up,
        [1] = cb_adv_hold_ramp_down,
    }
};

struct fsm_node adv_ramp = {
    .id = NODE_RAMP, .name = "ADV_RAMP", .action_routine = action_ramp,
    .release_callback = cb_adv_ramp_release, .click_map = { [0]=&adv_off },
};

struct fsm_node adv_moon = {
    .id = NODE_MOON, .name = "ADV_MOON", .action_routine = action_moon,
    .click_map = { [0]=&adv_off }, .hold_map = { [0]=&adv_ramp }, .hold_callbacks = { [0]=cb_adv_hold_ramp_up },
};

struct fsm_node adv_turbo = {
    .id = NODE_TURBO, .name = "ADV_TURBO", .action_routine = action_turbo,
    .click_map = { [0]=&adv_off, [1]=&adv_on },
};

struct fsm_node adv_lockout = {
    .id = NODE_LOCKOUT, .name = "ADV_LOCK", .action_routine = action_lockout,
    .click_map = { [3]=&adv_off }, 
    .hold_callbacks = { [0]=cb_adv_lockout_momentary }, 
    .release_callback = cb_adv_lockout_release,
};

struct fsm_node adv_battcheck = {
    .id = NODE_BATTCHECK, .name = "ADV_BATT", .action_routine = action_battcheck, 
    .timeout_ms = 4000, 
    .click_map = { 
        [0] = &adv_off, 
        [1] = &adv_tempcheck  // 2C -> Temp Check
    },
    .hold_map = {
        [6] = &adv_cal_voltage, // 7H -> Voltage Cal
    },
};

struct fsm_node adv_tempcheck = {
    .id = NODE_BATTCHECK, .name = "ADV_TEMP", .action_routine = action_tempcheck, 
    .timeout_ms = 4000, 
    .click_map = { 
        [0] = &adv_off,
        [1] = &adv_battcheck  // 2C -> Back to Batt Check
    },
    .hold_map = {
        [6] = &adv_cal_thermal_current, // 7H -> Thermal Cal
    },
};

struct fsm_node adv_reset = {
    .id = NODE_FACTORY_RESET, .name = "ADV_RESET", .action_routine = action_factory_reset,
};

struct fsm_node adv_strobe = {
    .id = NODE_STROBE, .name = "ADV_STROBE", .action_routine = action_strobe,
    .release_callback = cb_adv_strobe_release, 
    .click_map = {
        [0] = &adv_off,
    },
    .click_callbacks = {
        [1] = cb_strobe_next, // 2C -> Next Strobe
    },
    // TODO: Implement Hold maps for speed adjustment in a future update
};

struct fsm_node adv_config_floor = {
    .id = NODE_CONFIG_FLOOR, .name = "CFG_FLOOR", .action_routine = action_config_floor,
    .any_click_callback = cb_config_floor_set,
    .timeout_ms = 2000, .timeout_node = &adv_config_ceiling,
};

struct fsm_node adv_config_ceiling = {
    .id = NODE_CONFIG_CEILING, .name = "CFG_CEIL", .action_routine = action_config_ceiling,
    .any_click_callback = cb_config_ceiling_set,
    .timeout_ms = 2000, .timeout_node = &adv_on,
};

struct fsm_node adv_aux_config = {
    .id = NODE_AUX_CONFIG, .name = "ADV_AUX", 
    .action_routine = action_aux_config,
    .click_map = {
        [0] = &adv_off,        // 1C -> Exit to OFF
        [6] = &adv_aux_config, // 7H -> Cycle Next (Re-enter)
    },
};

struct fsm_node adv_cal_voltage = {
    .id = NODE_BATTCHECK, .name = "CAL_VOLT", .action_routine = action_cal_voltage_entry,
    .any_click_callback = cb_cal_voltage_set,
    .timeout_ms = 4000, .timeout_node = &adv_battcheck,
};

struct fsm_node adv_cal_thermal_current = {
    .id = NODE_BATTCHECK, .name = "CAL_T_CUR", .action_routine = action_cal_thermal_entry,
    .any_click_callback = cb_cal_thermal_set,
    .timeout_ms = 4000, .timeout_node = &adv_cal_thermal_limit,
};

struct fsm_node adv_cal_thermal_limit = {
    .id = NODE_BATTCHECK, .name = "CAL_T_LIM", .action_routine = action_cal_thermal_limit_entry,
    .any_click_callback = cb_cal_thermal_limit_set,
    .timeout_ms = 4000, .timeout_node = &adv_tempcheck,
};


struct fsm_node *get_advanced_off_node(void) {
    return &adv_off;
}
