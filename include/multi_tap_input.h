#ifndef MULTI_TAP_INPUT_H
#define MULTI_TAP_INPUT_H

#include <zephyr/kernel.h>

/**
 * @brief Defines the structure for the final action reported by the library.
 */
struct multi_tap_input_action {
    int count;          // Number of clicks detected (1, 2, 3, or more)
    bool is_hold;       // True if the action included a hold.
};

/**
 * @brief Type definition for the user-provided callback function.
 */
typedef void (*multi_tap_input_handler_t)(const struct multi_tap_input_action *action);

/**
 * @brief Initializes the Multi-Tap Input core.
 * @param handler The application-defined function to be called when an action is complete.
 */
void multi_tap_input_init(multi_tap_input_handler_t handler);

/**
 * @brief Processes a raw key event (Down or Up) from any source.
 * @param value 1 for Key Down (Press), 0 for Key Up (Release).
 */
void multi_tap_input_process_key(int value);

#endif // MULTI_TAP_INPUT_H