/**
 * @file main.c
 * @brief Integration test for Input Logic.
 */

#include <zephyr/ztest.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include "zbeam_msg.h"

/* Mock Message Queue */
K_MSGQ_DEFINE(zbeam_msgq, sizeof(struct zbeam_msg), 10, 4);

/* Externs to satisfy multi_tap_input link requirements */
/* multi_tap_input calls fsm_worker_post_msg. We implement it here to capture output. */
/* Externs to satisfy multi_tap_input link requirements */
/* multi_tap_input calls fsm_worker_post_msg. We implement it here to capture output. */
int fsm_worker_post_msg(struct zbeam_msg *msg)
{
    printk("DEBUG: Mock fsm_worker_post_msg type=%d count=%d\n", msg->type, msg->count);
    return k_msgq_put(&zbeam_msgq, msg, K_NO_WAIT);
}

#include "multi_tap_input.h" 
extern void multi_tap_input_init(void);

/* Helper to simulate press */
static void press_button(void)
{
    /* Simulate GPIO button press (Active Low -> 1=Pressed in input system usually, 
       but depends on DT. Let's assume input_report_key handles logical state via value=1) */
    input_report_key(NULL, INPUT_KEY_0, 1, true, K_NO_WAIT);
}

static void release_button(void)
{
    input_report_key(NULL, INPUT_KEY_0, 0, true, K_NO_WAIT);
}

static void click(void)
{
    press_button();
    k_sleep(K_MSEC(50));
    release_button();
    k_sleep(K_MSEC(50));
}

static void before(void *fixture)
{
    static bool initialized = false;
    if (!initialized) {
        multi_tap_input_init();
        initialized = true;
    }
    multi_tap_input_reset();
    k_msgq_purge(&zbeam_msgq);
    /* Yield to let timers process cancellation */
    k_sleep(K_MSEC(10)); 
}

ZTEST_SUITE(input_logic_suite, NULL, NULL, before, NULL, NULL);

ZTEST(input_logic_suite, test_single_click)
{
    /* 1 Click */
    click();
    
    /* Wait for timeout (200ms configured in prj.conf + margin) */
    k_sleep(K_MSEC(300));
    
    struct zbeam_msg msg;
    int ret = k_msgq_get(&zbeam_msgq, &msg, K_NO_WAIT);
    
    zassert_equal(ret, 0, "Should have received a message");
    zassert_equal(msg.type, MSG_INPUT_TAP, "Should be TAP");
    zassert_equal(msg.count, 1, "Count should be 1");
}

ZTEST(input_logic_suite, test_double_click)
{
    click(); // 1
    click(); // 2
    
    k_sleep(K_MSEC(300));
    
    struct zbeam_msg msg;
    int ret = k_msgq_get(&zbeam_msgq, &msg, K_NO_WAIT);
    
    zassert_equal(ret, 0, "Should have received a message");
    zassert_equal(msg.type, MSG_INPUT_TAP, "Should be TAP");
    zassert_equal(msg.count, 2, "Count should be 2");
}

ZTEST(input_logic_suite, test_hold)
{
    /* Press and hold > 300ms */
    press_button();
    k_sleep(K_MSEC(400));
    
    /* Check for HOLD_START immediate message */
    struct zbeam_msg msg;
    int ret = k_msgq_get(&zbeam_msgq, &msg, K_NO_WAIT);
    
    zassert_equal(ret, 0, "Should get HOLD_START");
    zassert_equal(msg.type, MSG_INPUT_HOLD_START, "Type mismatch");
    zassert_equal(msg.count, 1, "Count should be 1 (1H)");
    
    /* Release */
    release_button();
    k_sleep(K_MSEC(50));
    
    /* Check for HOLD_RELEASE */
    ret = k_msgq_get(&zbeam_msgq, &msg, K_NO_WAIT);
    zassert_equal(ret, 0, "Should get HOLD_RELEASE");
    zassert_equal(msg.type, MSG_INPUT_HOLD_RELEASE, "Type mismatch");
}

void test_main(void)
{
    ztest_run_test_suites(NULL, false, 1, 1);
}
