/**
 * @file fsm_worker.h
 * @brief FSM Worker Thread API.
 *
 * The FSM worker thread owns the message queue and dispatches
 * all events to the FSM engine in a safe, non-ISR context.
 */

#ifndef FSM_WORKER_H
#define FSM_WORKER_H

#include <zephyr/kernel.h>
#include "zbeam_msg.h"

/**
 * @brief Post a message to the FSM worker queue.
 *
 * Thread-safe and ISR-safe (non-blocking).
 *
 * @param msg Pointer to message to post.
 * @return 0 on success, -ENOMSG if queue full.
 */
int fsm_worker_post_msg(const struct zbeam_msg *msg);

/**
 * @brief Get the message queue (for testing/injection).
 * @return Pointer to the k_msgq.
 */
struct k_msgq *fsm_worker_get_queue(void);

/**
 * @brief Check if worker thread is running.
 * @return true if initialized and running.
 */
bool fsm_worker_is_running(void);

#endif /* FSM_WORKER_H */
