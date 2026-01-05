/**
 * @file multi_tap_input.h
 * @brief Multi-Tap Input Detection API.
 *
 * Posts input events (tap, hold) to FSM worker via message queue.
 */

#ifndef MULTI_TAP_INPUT_H
#define MULTI_TAP_INPUT_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize the Multi-Tap Input engine.
 */
void multi_tap_input_init(void);

/**
 * @brief Configure timing parameters at runtime.
 * @param click_ms Click timeout in milliseconds.
 * @param hold_ms Hold duration in milliseconds.
 */
void multi_tap_configure(uint32_t click_ms, uint32_t hold_ms);

#endif /* MULTI_TAP_INPUT_H */