/**
 * @file ui_actions.h
 * @brief Common UI Actions and State for ZBeam.
 *
 * Defines shared enums, state accessors, and action routines used by
 * specific UI implementations (Simple, Advanced, etc.).
 */

#ifndef UI_ACTIONS_H
#define UI_ACTIONS_H

#include "fsm_engine.h"

/**
 * @brief UI Operation Modes
 */
enum ui_mode {
    UI_SIMPLE,      /**< Safe/Simple mode (Anduril 2 Style) */
    UI_ADVANCED,    /**< Full feature set */
    /* UI_TACTICAL, UI_KIDS, etc. could be added here */
};

/**
 * @brief Memory Modes
 */
enum memory_mode {
    MEM_AUTO,       /**< Remember last used brightness */
    MEM_MANUAL,     /**< Always start at specific level */
    MEM_HYBRID,     /**< Manual after N minutes, Auto within timeframe */
};

/**
 * @brief Ramp Styles
 */
enum ramp_style {
    RAMP_SMOOTH,    /**< Continuous ramping (1/256 steps) */
    RAMP_STEPPED,   /**< Discrete steps (defined by Kconfig) */
};

/**
 * @brief Strobe / Utility Mode Types
 */
enum strobe_type {
    STROBE_PARTY,     /**< Variable frequency strobe */
    STROBE_TACTICAL,  /**< High visibility strobe */
    STROBE_CANDLE,    /**< Simulated candle flicker */
    STROBE_BIKE,      /**< Bike flasher (Steady + Pulse) */
    STROBE_COUNT,     // Total number of modes
};

/**
 * @brief Standard FSM Node IDs (Shared across UIs)
 */
enum fsm_node_id {
    /* Core States */
    NODE_OFF = 0,
    NODE_ON,
    NODE_RAMP,
    NODE_MOON,
    NODE_TURBO,
    NODE_LOCKOUT,
    
    /* Utility States */
    NODE_BATTCHECK,
    NODE_TEMPCHECK,
    NODE_SOS,
    NODE_BEACON,
    
    /* Strobe States */
    NODE_STROBE,
    NODE_PARTY_STROBE,
    NODE_STROBE_TACTICAL,
    NODE_STROBE_CANDLE,
    
    /* Config/Feedback States */
    NODE_CONFIG_FLOOR,
    NODE_CONFIG_CEILING,
    NODE_CONFIG_STEPS,
    NODE_AUX_CONFIG,
    NODE_BLINK,
    NODE_FACTORY_RESET,
    
    NODE_COUNT
};

/* Override API */
void ui_set_next_brightness(uint8_t level);
void ui_set_next_brightness_floor(void);
void ui_set_next_brightness_ceiling(void);

/* ========== Shared Action Routines ========== */
/* These are implemented in ui_actions.c and used by UI trees */

void action_off(void);
void action_on(void);
void action_ramp(void);
void action_moon(void);
void action_turbo(void);
void action_lockout(void);
void action_strobe(void);
void action_battcheck(void);
void action_tempcheck(void);
void action_aux_config(void);
void action_factory_reset(void);

struct fsm_node* action_channel_cycle(uint8_t count);

/* Config Actions */
void action_config_floor(void);
void action_config_ceiling(void);
void action_config_steps(void);

struct fsm_node* cb_config_floor_set(struct fsm_node *self, int count);
struct fsm_node* cb_config_ceiling_set(struct fsm_node *self, int count);
struct fsm_node* cb_config_steps_set(struct fsm_node *self, int count);


/* Helpers */
void start_ramping(int direction);
void stop_ramping(void);

/* ========== Entry Points ========== */

/* Implemented in ui_simple.c */
struct fsm_node *get_simple_off_node(void);

/* Implemented in ui_advanced.c */
struct fsm_node *get_advanced_off_node(void);

/**
 * @brief Initialize UI system (persistence, timers).
 */
void ui_init(void);

/**
 * @brief Get the start node based on current UI Mode.
 */
struct fsm_node *get_start_node(void);

/**
 * @brief Toggle between Simple and Advanced UI modes.
 * @return The new start node (OFF state of the new mode).
 */
struct fsm_node *ui_toggle_mode(void);

/**
 * @brief Toggle between Smooth and Stepped ramping.
 */
struct fsm_node *action_toggle_ramp_style(uint8_t count);

/**
 * @brief Strobe Mode Actions
 */
void action_strobe(void);
void action_strobe_party(void);
void action_strobe_tactical(void);
void action_strobe_candle(void);
void action_strobe_bike(void);
struct fsm_node *action_strobe_next(uint8_t count);

/* Calibration Actions */
void action_cal_voltage_entry(void);
struct fsm_node *cb_cal_voltage_set(struct fsm_node *self, int count);
void action_cal_thermal_entry(void);
struct fsm_node *cb_cal_thermal_set(struct fsm_node *self, int count);
void action_cal_thermal_limit_entry(void);
struct fsm_node *cb_cal_thermal_limit_set(struct fsm_node *self, int count);

/* Wrappers for state access */
uint8_t ui_get_current_pwm(void);
uint8_t ui_get_strobe_freq(void);
enum ui_mode ui_get_current_mode(void);

#endif /* UI_ACTIONS_H */
