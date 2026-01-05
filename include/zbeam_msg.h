/**
 * @file zbeam_msg.h
 * @brief Message types for inter-thread communication.
 *
 * All worker threads communicate via k_msgq using these message types.
 */

#ifndef ZBEAM_MSG_H
#define ZBEAM_MSG_H

#include <stdint.h>

/**
 * @brief Message types for the FSM message queue.
 */
enum zbeam_msg_type {
    /* Input Events (from multi_tap_input) */
    MSG_INPUT_TAP,           /**< Multi-tap complete (count = number of taps) */
    MSG_INPUT_HOLD_START,    /**< Hold threshold reached */
    MSG_INPUT_HOLD_RELEASE,  /**< Button released after hold */

    /* Timer Events */
    MSG_TIMEOUT_INACTIVITY,  /**< FSM inactivity timeout */
    MSG_TIMEOUT_RAMP_TICK,   /**< Ramping step timer */

    /* Safety Events (from safety_monitor) */
    MSG_SAFETY_SHUTDOWN,     /**< Emergency shutdown - LED off immediately */
    MSG_SAFETY_THERMAL_WARN, /**< Soft thermal limit - reduce power */

    /* System Events */
    MSG_SYSTEM_SHUTDOWN,     /**< Clean shutdown request */
};

/**
 * @brief Message structure for k_msgq.
 * 
 * Kept minimal (4 bytes) for efficient queue operations.
 */
struct zbeam_msg {
    uint8_t type;      /**< enum zbeam_msg_type */
    uint8_t count;     /**< Click/hold count (for input events) */
    uint8_t severity;  /**< For safety events: 0=info, 255=critical */
    uint8_t reserved;  /**< Alignment padding */
};

#endif /* ZBEAM_MSG_H */
