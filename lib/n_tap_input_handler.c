#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/input.h>
#include "multi_tap_fsm.h"

LOG_MODULE_REGISTER(AppInput, LOG_LEVEL_INF);

// We are monitoring the 'a' key, which is code 30
#define BUTTON_CODE 30

/**
 * @brief Input driver callback function.
 * This function filters the raw input events and passes the relevant ones
 * to the generic FSM processor.
 */
static void input_cb(const struct device *dev, struct input_event *evt)
{
    // Filter for the specific key
    if (evt->code != BUTTON_CODE) {
        return;
    }

    // Pass the raw event value (1 for down, 0 for up) directly to the FSM library.
    // The FSM library is now completely decoupled from 'struct input_event'.
    multi_tap_fsm_process_key(evt->value);
}

// Register the callback function with the input subsystem
INPUT_CALLBACK_DEFINE(NULL, input_cb);

// No main function or initialization needed here; registration is static.