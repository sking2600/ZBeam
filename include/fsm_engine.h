/**
 * @file fsm_engine.h
 * @brief Finite State Machine Engine for ZBeam.
 * 
 * Defines the core node structure and the engine API to process inputs
 * and manage state transitions.
 */

#ifndef FSM_ENGINE_H
#define FSM_ENGINE_H

#include <zephyr/kernel.h>
#include "zbeam_msg.h"

// Maximum number of taps to index in the transition arrays
// Index 0 = 1 tap, Index 4 = 5 taps.
#define MAX_NAV_SLOTS CONFIG_ZBEAM_MAX_NAV_SLOTS 

/**
 * @brief Serialized configuration data for a node (NVS friendly).
 */
struct node_config_data {
    uint8_t target_click_ids[MAX_NAV_SLOTS]; ///< Enum IDs of target nodes for clicks
    uint8_t target_hold_ids[MAX_NAV_SLOTS];  ///< Enum IDs of target nodes for holds
    uint32_t timeout_ms;                     ///< Inactivity timeout duration
};

struct fsm_node; // Forward decl

/**
 * @brief Callback for processing input without leaving the node.
 * @param self The current node.
 * @return NULL to stay/continue normal processing, or a pointer to a new node to force a transition.
 */
typedef struct fsm_node* (*fsm_callback_t)(struct fsm_node *self);

/**
 * @brief Runtime FSM Node definition.
 */
struct fsm_node {
    uint8_t id;                        // Unique Enum ID (0 to NODE_COUNT-1)
    const char *name;                  // Name for debugging (e.g., "ON", "OFF")
    void (*action_routine)(void);      // Function to execute when entering this state
    
    // Navigation Tables
    // If the user performs N taps, we look at index [N-1].
    // If the entry is NULL, the input is ignored (no transition).
    struct fsm_node *click_map[MAX_NAV_SLOTS]; 
    struct fsm_node *hold_map[MAX_NAV_SLOTS];
    
    // Callbacks (Optional) - Run before map lookup
    fsm_callback_t click_callbacks[MAX_NAV_SLOTS];
    fsm_callback_t hold_callbacks[MAX_NAV_SLOTS];
    
    // Release Callback: Triggered when a HOLD is released
    fsm_callback_t release_callback;

    uint32_t timeout_ms;               // Milliseconds of inactivity to return to Home (0 = never)
    bool timeout_reverts;              // If true, timeout returns to PREVIOUS node instead of Home.
};

/**
 * @brief Get the current active node.
 */
struct fsm_node *fsm_get_current_node(void);

/**
 * @brief Initialize the FSM with a starting node (usually OFF).
 * @param start_node Pointer to the initial state.
 */
void fsm_init(struct fsm_node *start_node);

/**
 * @brief Force a transition to a specific node (e.g. from a timer/callback).
 */
void fsm_transition_to(struct fsm_node *next_node);

/**
 * @brief Process an input message from the worker thread.
 * @param msg Input message (TAP, HOLD_START, HOLD_RELEASE).
 */
void fsm_process_msg(const struct zbeam_msg *msg);

/**
 * @brief Process a timer message from the worker thread.
 * @param msg Timer message (INACTIVITY, RAMP_TICK).
 */
void fsm_process_timer(const struct zbeam_msg *msg);

/**
 * @brief Emergency off - immediate LED shutdown.
 * 
 * Called when safety monitor triggers shutdown.
 * Stops all timers and forces LED to off state.
 */
void fsm_emergency_off(void);

#endif // FSM_ENGINE_H
