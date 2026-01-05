/**
 * @file multi_tap_input.c
 * @brief Multi-Tap Input Detection Engine.
 *
 * Detects clicks, holds, and multi-tap sequences.
 * Posts events to FSM worker via message queue.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include "multi_tap_input.h"
#include "fsm_worker.h"
#include "zbeam_msg.h"

LOG_MODULE_REGISTER(MultiTap, LOG_LEVEL_INF);

/* Timing Configuration */
static uint32_t click_timeout_ms = CONFIG_ZBEAM_CLICK_TIMEOUT_MS;
static uint32_t hold_duration_ms = CONFIG_ZBEAM_HOLD_DURATION_MS;

/* FSM States */
enum multi_tap_state {
    STATE_IDLE,
    STATE_PRESSED,
    STATE_WAIT_TIMEOUT,
};

/* State Variables */
static enum multi_tap_state current_state = STATE_IDLE;
static int click_count = 0;
static bool is_holding = false;

/* Timers */
static struct k_timer click_timer;
static struct k_timer hold_timer;

static void post_event(uint8_t type, uint8_t count)
{
    struct zbeam_msg msg = {
        .type = type,
        .count = count,
    };
    fsm_worker_post_msg(&msg);
}

static void click_timer_handler(struct k_timer *timer_id)
{
    if (current_state == STATE_WAIT_TIMEOUT) {
        post_event(MSG_INPUT_TAP, click_count);
        click_count = 0;
        is_holding = false;
        current_state = STATE_IDLE;
    }
}

static void hold_timer_handler(struct k_timer *timer_id)
{
    if (current_state == STATE_PRESSED) {
        is_holding = true;
        post_event(MSG_INPUT_HOLD_START, click_count);
    }
}

static void process_key_event(int value)
{
    LOG_INF("Input raw: %d", value);
    if (value == 1) {  /* Key Down */
        switch (current_state) {
        case STATE_IDLE:
            click_count = 1;
            current_state = STATE_PRESSED;
            k_timer_start(&hold_timer, K_MSEC(hold_duration_ms), K_NO_WAIT);
            LOG_INF("State: IDLE -> PRESSED");
            break;

        case STATE_WAIT_TIMEOUT:
            k_timer_stop(&click_timer);
            click_count++;
            current_state = STATE_PRESSED;
            k_timer_start(&hold_timer, K_MSEC(hold_duration_ms), K_NO_WAIT);
            LOG_INF("State: WAIT -> PRESSED (count=%d)", click_count);
            break;

        case STATE_PRESSED:
            LOG_WRN("Ignored press while PRESSED");
            break;
        }
    }
    else {  /* Key Up */
        if (current_state == STATE_PRESSED) {
            k_timer_stop(&hold_timer);

            if (is_holding) {
                post_event(MSG_INPUT_HOLD_RELEASE, click_count);
                click_count = 0;
                is_holding = false;
                current_state = STATE_IDLE;
                LOG_INF("State: PRESSED -> IDLE (Hold Release)");
            } else {
                current_state = STATE_WAIT_TIMEOUT;
                k_timer_start(&click_timer, K_MSEC(click_timeout_ms), K_NO_WAIT);
                LOG_INF("State: PRESSED -> WAIT");
            }
        } else {
            LOG_WRN("Ignored release in state %d", current_state);
        }
    }
}

/* Zephyr Input Subsystem Callback */
static void input_cb(struct input_event *evt, void *user_data)
{
    if (evt->type == INPUT_EV_KEY && evt->code == INPUT_KEY_0) {
        process_key_event(evt->value);
    }
}
INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

void multi_tap_configure(uint32_t click_ms, uint32_t hold_ms)
{
    click_timeout_ms = click_ms;
    hold_duration_ms = hold_ms;
}

void multi_tap_input_init(void)
{
    k_timer_init(&click_timer, click_timer_handler, NULL);
    k_timer_init(&hold_timer, hold_timer_handler, NULL);
    LOG_INF("Multi-Tap init: click=%dms hold=%dms", 
            click_timeout_ms, hold_duration_ms);
}

void multi_tap_input_reset(void)
{
    k_timer_stop(&click_timer);
    k_timer_stop(&hold_timer);
    click_count = 0;
    current_state = STATE_IDLE;
    is_holding = false;
    LOG_INF("MultiTap Reset");
}