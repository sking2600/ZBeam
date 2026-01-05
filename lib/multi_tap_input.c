// lib/multi_tap_input.c

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "multi_tap_input.h"

LOG_MODULE_REGISTER(MultiTap, LOG_LEVEL_INF);

// --- FSM CONFIGURATION (Hardcoded for maximum stability) ---
#define CLICK_TIMEOUT_MS 400
#define HOLD_DURATION_MS 500
// -----------------------------------------------------------

// --- FSM STATE DEFINITIONS ---
enum multi_tap_state {
    STATE_IDLE,         // Waiting for first press
    STATE_PRESSED,      // Button is currently down
    STATE_WAIT_RELEASE, // Waiting for button release to count tap
    STATE_WAIT_TIMEOUT, // Waiting for click timeout to finalize action
};

// --- FSM VARIABLES ---
static enum multi_tap_state current_state = STATE_IDLE;
static int click_count = 0;
static bool is_holding = false;
static multi_tap_input_handler_t action_handler = NULL;

// --- KERNEL OBJECTS ---
static struct k_timer click_timer;
static struct k_timer hold_timer;

// --- TIMER HANDLERS (These were the source of the syntax errors previously) ---

static void click_timer_handler(struct k_timer *timer_id)
{
    // Click timeout occurred while waiting for another tap. Finalize the action.
    if (current_state == STATE_WAIT_TIMEOUT) {
        LOG_DBG("Click timeout. Finalizing action: %d taps.", click_count);
        
        // Report the final action to the application
        struct multi_tap_input_action action = {
            .count = click_count,
            .is_hold = false
        };
        if (action_handler) {
            action_handler(&action);
        }

        // Reset the FSM
        click_count = 0;
        is_holding = false;
        current_state = STATE_IDLE;
    }
}

static void hold_timer_handler(struct k_timer *timer_id)
{
    // Hold timeout occurred while button is still pressed.
    if (current_state == STATE_PRESSED) {
        LOG_DBG("Hold timeout. Registering hold.");
        is_holding = true;
        // Do not change state; remain STATE_PRESSED until release.
    }
}

// --- FSM IMPLEMENTATION (The core logic) ---

void multi_tap_input_process_key(int value)
{
    // Key Down (Press)
    if (value == 1) {
        switch (current_state) {
            case STATE_IDLE:
                // First press: Start counting and start the hold timer.
                click_count = 1;
                current_state = STATE_PRESSED;
                k_timer_start(&hold_timer, K_MSEC(HOLD_DURATION_MS), K_NO_WAIT);
                LOG_DBG("State: PRESSED (Count: 1). Hold timer started.");
                break;

            case STATE_WAIT_TIMEOUT:
                // Press before timeout: Cancel the timeout, count the tap, and wait for release.
                k_timer_stop(&click_timer);
                click_count++;
                current_state = STATE_PRESSED;
                k_timer_start(&hold_timer, K_MSEC(HOLD_DURATION_MS), K_NO_WAIT);
                LOG_DBG("State: PRESSED (Count: %d). Hold timer restarted.", click_count);
                break;
            
            case STATE_PRESSED:
            case STATE_WAIT_RELEASE:
                // Ignore key down events while already pressed or waiting for release.
                LOG_DBG("State: Already Pressed or Waiting for Release. Ignored key down.");
                break;
        }
    }

    // Key Up (Release)
    else if (value == 0) {
        switch (current_state) {
            case STATE_PRESSED:
                // Stop the hold timer immediately upon release
                k_timer_stop(&hold_timer);

                if (is_holding) {
                    // Holding completed: Finalize the action immediately.
                    LOG_DBG("Release after HOLD. Finalizing action: %d taps + HOLD.", click_count);
                    
                    struct multi_tap_input_action action = {
                        .count = click_count,
                        .is_hold = true
                    };
                    if (action_handler) {
                        action_handler(&action);
                    }
                    
                    // Reset FSM
                    click_count = 0;
                    is_holding = false;
                    current_state = STATE_IDLE;
                
                } else {
                    // Simple tap: Start the click timeout to wait for another tap.
                    current_state = STATE_WAIT_TIMEOUT;
                    k_timer_start(&click_timer, K_MSEC(CLICK_TIMEOUT_MS), K_NO_WAIT);
                    LOG_DBG("State: WAIT_TIMEOUT. Click timer started.");
                }
                break;
            
            case STATE_WAIT_RELEASE:
            case STATE_WAIT_TIMEOUT:
            case STATE_IDLE:
                // Ignore key up events in all other states.
                LOG_DBG("State: Release ignored.");
                break;
        }
    }
}

// --- INITIALIZATION ---

void multi_tap_input_init(multi_tap_input_handler_t handler)
{
    // 1. Set the application's callback handler
    action_handler = handler;
    
    // 2. Initialize timer structures and link handlers
    k_timer_init(&click_timer, click_timer_handler, NULL);
    k_timer_init(&hold_timer, hold_timer_handler, NULL);

    LOG_INF("Multi-Tap Input Core initialized. Click Timeout: %dms, Hold Duration: %dms",
            CLICK_TIMEOUT_MS, HOLD_DURATION_MS);
}