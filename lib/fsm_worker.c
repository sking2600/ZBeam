/**
 * @file fsm_worker.c
 * @brief FSM Worker Thread Implementation.
 *
 * Single worker thread that processes all FSM-related messages
 * in a safe, non-ISR context.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "fsm_worker.h"
#include "fsm_engine.h"
#include "zbeam_msg.h"

LOG_MODULE_REGISTER(fsm_worker, LOG_LEVEL_INF);

/* Message queue - statically allocated */
K_MSGQ_DEFINE(fsm_msgq, sizeof(struct zbeam_msg), 
              CONFIG_ZBEAM_FSM_MSGQ_DEPTH, 4);

static volatile bool worker_running = false;

/**
 * @brief FSM worker thread entry point.
 *
 * Blocks on message queue and dispatches to FSM engine.
 */
static void fsm_worker_entry(void *p1, void *p2, void *p3)
{
    struct zbeam_msg msg;

    LOG_INF("FSM worker thread started");
    worker_running = true;

    while (1) {
        /* Block waiting for message */
        int ret = k_msgq_get(&fsm_msgq, &msg, K_FOREVER);
        if (ret != 0) {
            continue;
        }

        LOG_DBG("Processing msg type=%d count=%d", msg.type, msg.count);

        switch (msg.type) {
        /* Safety Events - highest priority handling */
        case MSG_SAFETY_SHUTDOWN:
            LOG_WRN("SAFETY SHUTDOWN received!");
            fsm_emergency_off();
            break;

        case MSG_SAFETY_THERMAL_WARN:
            LOG_WRN("Thermal warning: severity=%d", msg.severity);
            /* TODO: Implement gradual power reduction */
            break;

        /* Input Events */
        case MSG_INPUT_TAP:
        case MSG_INPUT_HOLD_START:
        case MSG_INPUT_HOLD_RELEASE:
            fsm_process_msg(&msg);
            break;

        /* Timer Events */
        case MSG_TIMEOUT_INACTIVITY:
        case MSG_TIMEOUT_RAMP_TICK:
            fsm_process_timer(&msg);
            break;

        /* System Events */
        case MSG_SYSTEM_SHUTDOWN:
            LOG_INF("System shutdown requested");
            fsm_emergency_off();
            break;

        default:
            LOG_WRN("Unknown message type: %d", msg.type);
            break;
        }
    }
}

int fsm_worker_post_msg(const struct zbeam_msg *msg)
{
    if (msg == NULL) {
        return -EINVAL;
    }
    
    int ret = k_msgq_put(&fsm_msgq, msg, K_NO_WAIT);
    if (ret != 0) {
        LOG_WRN("FSM queue full, dropping msg type=%d", msg->type);
    }
    return ret;
}

struct k_msgq *fsm_worker_get_queue(void)
{
    return &fsm_msgq;
}

bool fsm_worker_is_running(void)
{
    return worker_running;
}

/* Define the thread statically */
K_THREAD_DEFINE(fsm_worker_tid,
                CONFIG_ZBEAM_FSM_WORKER_STACK_SIZE,
                fsm_worker_entry, NULL, NULL, NULL,
                CONFIG_ZBEAM_FSM_WORKER_PRIORITY, 0, 0);
