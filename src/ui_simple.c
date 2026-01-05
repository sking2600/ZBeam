/**
 * @file ui_simple.c
 * @brief Anduril 2 Simple Mode FSM Topology.
 */

#include "ui_actions.h"
#include <stddef.h>

/* Forward Declarations */
struct fsm_node simple_off;
struct fsm_node simple_on;
struct fsm_node simple_ramp;
struct fsm_node simple_moon;
struct fsm_node simple_turbo;
struct fsm_node simple_lockout;
struct fsm_node simple_battcheck;
struct fsm_node simple_reset;

/* Callbacks specific to Simple Mode topology */
static struct fsm_node* cb_simple_hold_from_off(struct fsm_node *self, int count) {
    action_moon(); // Sets brightness to floor
    start_ramping(1); // Start ramping up
    return NULL;
}

static struct fsm_node* cb_simple_hold_ramp_up(struct fsm_node *self, int count) {
    start_ramping(1); 
    return NULL;
}

static struct fsm_node* cb_simple_hold_ramp_down(struct fsm_node *self, int count) {
    start_ramping(-1); 
    return NULL;
}

static struct fsm_node* cb_simple_ramp_release(struct fsm_node *self, int count) {
    stop_ramping();
    return &simple_on;
}

static struct fsm_node* cb_simple_lockout_momentary(struct fsm_node *self, int count) {
    // 1H in Lockout -> Momentary Moon (Simple Mode)
    // Actually Anduril 2 Simple Mode Lockout:
    // 1H: Moon
    // 2H: Low (Not implemented yet, mapping to Moon for now)
    action_moon(); 
    return NULL;
}

static struct fsm_node* cb_simple_lockout_release(struct fsm_node *self, int count) {
    action_off(); // Turn off LED
    return NULL;
}

static struct fsm_node* cb_10h_toggle(struct fsm_node *self, int count) {
    return ui_toggle_mode();
}

/* Node Definitions */

struct fsm_node simple_off = {
    .id = NODE_OFF, .name = "SMP_OFF", .action_routine = action_off,
    .click_map = {
        [0] = &simple_on,         // 1C: ON
        [1] = &simple_turbo,      // 2C: Ceiling (Safe Turbo)
        [2] = &simple_battcheck,  // 3C: Batt Check
        [3] = &simple_lockout,    // 4C: Lockout
        // 10H handled via 13H logic or direct callback if MAX_NAV_SLOTS allows.
        // Assuming MAX_NAV_SLOTS is at least 5 (index 4). If not, we need a special handler.
        // For now, let's assume we can map 10 clicks via a special way or just check if it fits.
        // Wait, standard ZBeam FSM supports holding.
        // 10H means Click-Click...-Hold. 
    },
    .hold_map = {
        [0] = &simple_ramp,       // 1H: Moon -> Ramp
        [1] = &simple_turbo,      // 2H: Momentary Ceiling 
        [9] = NULL, 
    },
    .hold_callbacks = {
        [0] = cb_simple_hold_from_off,
        [9] = cb_10h_toggle, // 10H (requires MAX_NAV_SLOTS >= 10)
    }
};

struct fsm_node simple_on = {
    .id = NODE_ON, .name = "SMP_ON", .action_routine = action_on,
    .click_map = {
        [0] = &simple_off,        // 1C: OFF
        [1] = &simple_turbo,      // 2C: Ceiling
        [3] = &simple_lockout,    // 4C: Lockout
    },
    .hold_map = {
        [0] = &simple_ramp,       // 1H: Up
        [1] = &simple_ramp,       // 2H: Down
    },
    .hold_callbacks = {
        [0] = cb_simple_hold_ramp_up,
        [1] = cb_simple_hold_ramp_down,
    }
};

struct fsm_node simple_ramp = {
    .id = NODE_RAMP, .name = "SMP_RAMP", .action_routine = action_ramp,
    .release_callback = cb_simple_ramp_release,
    .click_map = { [0] = &simple_off },
};

struct fsm_node simple_moon = { // Transitional state usually
    .id = NODE_MOON, .name = "SMP_MOON", .action_routine = action_moon,
    .click_map = { [0] = &simple_off },
    .hold_map = { [0] = &simple_ramp },
    .hold_callbacks = { [0] = cb_simple_hold_ramp_up },
};

struct fsm_node simple_turbo = {
    .id = NODE_TURBO, .name = "SMP_CEIL", .action_routine = action_turbo, // Uses Ceiling in Simple Mode
    .click_map = { [0] = &simple_off, [1] = &simple_on },
};

static struct fsm_node* cb_simple_unlock_floor(struct fsm_node *self, int count) {
    ui_set_next_brightness_floor();
    return &simple_on; // Transition triggers action_on -> Override takes effect
}

/* TODO: Wire into lockout hold_callbacks when implementing "unlock to ceiling" feature */
static struct fsm_node* __maybe_unused cb_simple_unlock_ceiling(struct fsm_node *self, int count) {
    ui_set_next_brightness_ceiling();
    return &simple_on; // Transition triggers action_on -> Override takes effect
}



struct fsm_node simple_lockout = {
    .id = NODE_LOCKOUT, .name = "SMP_LOCK", .action_routine = action_lockout,
    .click_map = { 
        [2] = &simple_off, // 3C: Unlock to OFF
        [3] = &simple_on,  // 4C: Unlock to ON (Memorized)
        [4] = &simple_turbo, // 5C: Unlock to Ceiling (Direct to Turbo node works too)
    },     
    .hold_callbacks = { 
        [0] = cb_simple_lockout_momentary, // 1H: Momentary Moon
        [3] = cb_simple_unlock_floor,      // 4H: Unlock to Floor
    },
    .release_callback = cb_simple_lockout_release,
};

struct fsm_node simple_battcheck = {
    .id = NODE_BATTCHECK, .name = "SMP_BATT", .action_routine = action_battcheck,
    .timeout_ms = 4000, // Auto exit after blink
    .click_map = { [0] = &simple_off },
};

struct fsm_node simple_reset = {
    .id = NODE_FACTORY_RESET, .name = "SMP_RESET", .action_routine = action_factory_reset,
};

struct fsm_node *get_simple_off_node(void) {
    return &simple_off;
}
