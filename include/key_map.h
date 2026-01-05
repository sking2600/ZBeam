/**
 * @file key_map.h
 * @brief FSM Node Definitions and Key Map API.
 *
 * Defines all states for the Anduril-style flashlight UI.
 */

#ifndef KEY_MAP_H
#define KEY_MAP_H

#include "fsm_engine.h"

/**
 * @brief FSM Node IDs for all states.
 */
enum fsm_node_id {
    /* Core States */
    NODE_OFF = 0,       /**< Light is off */
    NODE_ON,            /**< Steady output at memorized level */
    NODE_RAMP,          /**< Active ramping (hold to change brightness) */
    NODE_MOON,          /**< Lowest brightness (moonlight) */
    NODE_TURBO,         /**< Maximum brightness */
    NODE_LOCKOUT,       /**< Locked - prevents accidental activation */
    
    /* Utility States */
    NODE_BATTCHECK,     /**< Blink battery voltage */
    NODE_TEMPCHECK,     /**< Blink temperature (future) */
    NODE_SOS,           /**< SOS morse pattern (future) */
    NODE_BEACON,        /**< Periodic flash (future) */
    
    /* Strobe States */
    NODE_STROBE,        /**< Generic strobe entry (Variable Hz) */
    NODE_PARTY_STROBE,  /**< Freeze-motion strobe (Short pulse) */
    NODE_STROBE_TACTICAL,
    NODE_STROBE_CANDLE,
    
    /* Config/Feedback States */
    NODE_CONFIG_RAMP,   /**< Config menu for Ramp settings */
    NODE_AUX_CONFIG,    /**< AUX LED mode config (7C from OFF) */
    NODE_BLINK,         /**< Visual feedback blink */
    NODE_FACTORY_RESET, /**< Wipe settings and reboot */
    
    NODE_COUNT          /**< Must be last */
};

/* Global array of all nodes, indexed by ID */
extern struct fsm_node *all_nodes[NODE_COUNT];

/* Node declarations for external access */
extern struct fsm_node node_off;
extern struct fsm_node node_on;
extern struct fsm_node node_ramp;
extern struct fsm_node node_moon;
extern struct fsm_node node_turbo;
extern struct fsm_node node_lockout;
extern struct fsm_node node_battcheck;
extern struct fsm_node node_blink;
extern struct fsm_node node_strobe;
extern struct fsm_node node_party_strobe;
extern struct fsm_node node_config_ramp;
extern struct fsm_node node_aux_config;

/**
 * @brief Initialize key map (parse Kconfig, setup timers).
 */
void key_map_init(void);

/**
 * @brief Get the start node for FSM initialization.
 * @return Pointer to the initial state (NODE_OFF).
 */
struct fsm_node *get_start_node(void);

/**
 * @brief Get the current PWM duty cycle value.
 * @return PWM value 0-255.
 */
uint8_t key_map_get_current_pwm(void);

/**
 * @brief Get the current strobe frequency index (0-255).
 * @return Frequency index.
 */
uint8_t key_map_get_strobe_freq(void);

/**
 * @brief Get the current brightness index.
 * @return Index into pwm_levels array.
 */
int key_map_get_brightness_index(void);

/**
 * @brief Set brightness by index.
 * @param index Index into pwm_levels array.
 */
void key_map_set_brightness_index(int index);

#endif /* KEY_MAP_H */
