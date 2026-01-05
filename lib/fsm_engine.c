/**
 * @file fsm_engine.c
 * @brief Finite State Machine Engine.
 *
 * Processes messages from the worker thread and manages state transitions.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "fsm_engine.h"
#include "zbeam_msg.h"

LOG_MODULE_REGISTER(FSM_Engine, LOG_LEVEL_INF);

/* FSM State */
static struct fsm_node *current_node = NULL;
static struct fsm_node *home_node = NULL;
static struct fsm_node *previous_node = NULL;
static struct k_timer inactivity_timer;
static volatile bool emergency_shutdown_active = false;

/* External node from key_map.c */
extern struct fsm_node node_off;

static void reset_inactivity_timer(void)
{
    if (current_node && current_node->timeout_ms > 0) {
        k_timer_start(&inactivity_timer, K_MSEC(current_node->timeout_ms), K_NO_WAIT);
    }
}

static void inactivity_timer_handler(struct k_timer *timer_id)
{
    if (current_node && current_node->timeout_reverts && previous_node) {
        LOG_INF("FSM: Timeout -> Previous [%s]", previous_node->name);
        fsm_transition_to(previous_node);
    } else {
        LOG_INF("FSM: Timeout -> Home");
        fsm_transition_to(home_node);
    }
}

void fsm_transition_to(struct fsm_node *next_node)
{
    if (!next_node) return;

    k_timer_stop(&inactivity_timer);
    
    if (!next_node->timeout_reverts) {
        previous_node = current_node;
    }

    current_node = next_node;
    LOG_INF("FSM: -> [%s]", current_node->name ? current_node->name : "?");

    if (current_node->action_routine) {
        current_node->action_routine();
    }

    if (current_node->timeout_ms > 0) {
        k_timer_start(&inactivity_timer, K_MSEC(current_node->timeout_ms), K_NO_WAIT);
    }
}

void fsm_init(struct fsm_node *start_node)
{
    LOG_INF("FSM: Init");
    home_node = start_node;
    k_timer_init(&inactivity_timer, inactivity_timer_handler, NULL);
    fsm_transition_to(start_node);
}

struct fsm_node *fsm_get_current_node(void)
{
    return current_node;
}

/**
 * @brief Internal dispatch for input events.
 */
static void dispatch_input(uint8_t type, int count)
{
    if (!current_node) return;

    reset_inactivity_timer();

    int index = count - 1;

    /* Handle HOLD_RELEASE */
    if (type == MSG_INPUT_HOLD_RELEASE) {
        if (current_node->release_callback) {
            struct fsm_node *next = current_node->release_callback(current_node);
            if (next) fsm_transition_to(next);
        }
        return;
    }

    /* Bounds check */
    if (count < 1 || count > MAX_NAV_SLOTS) {
        LOG_WRN("Invalid count: %d", count);
        return;
    }

    struct fsm_node *next_node = NULL;
    fsm_callback_t cb = NULL;

    if (type == MSG_INPUT_TAP) {
        cb = current_node->click_callbacks[index];
        if (cb) {
            next_node = cb(current_node);
            if (next_node) {
                fsm_transition_to(next_node);
                return;
            }
        }
        next_node = current_node->click_map[index];
        if (next_node) {
            LOG_INF("Click[%d]: -> %s", count, next_node->name);
            fsm_transition_to(next_node);
        }
    }
    else if (type == MSG_INPUT_HOLD_START) {
        cb = current_node->hold_callbacks[index];
        if (cb) {
            next_node = cb(current_node);
            if (next_node) {
                fsm_transition_to(next_node);
                return;
            }
        }
        next_node = current_node->hold_map[index];
        if (next_node) {
            LOG_INF("Hold[%d]: -> %s", count, next_node->name);
            fsm_transition_to(next_node);
        }
    }
}

void fsm_process_msg(const struct zbeam_msg *msg)
{
    if (!msg) return;
    dispatch_input(msg->type, msg->count);
}

void fsm_process_timer(const struct zbeam_msg *msg)
{
    if (!msg) return;

    switch (msg->type) {
    case MSG_TIMEOUT_INACTIVITY:
        if (current_node && current_node->timeout_reverts && previous_node) {
            fsm_transition_to(previous_node);
        } else {
            fsm_transition_to(home_node);
        }
        break;

    case MSG_TIMEOUT_RAMP_TICK:
        /* TODO: handle ramp tick */
        break;

    default:
        break;
    }
}

void fsm_emergency_off(void)
{
    LOG_WRN("FSM: EMERGENCY OFF!");
    emergency_shutdown_active = true;
    k_timer_stop(&inactivity_timer);
    current_node = &node_off;
    if (current_node->action_routine) {
        current_node->action_routine();
    }
}
