/**
 * @file main.c
 * @brief Unit tests for FSM Engine Logic.
 */

#include <zephyr/ztest.h>
#include "fsm_engine.h"
#include "zbeam_msg.h"

/* --- Mock Nodes & Routines --- */

static int node_a_action_count = 0;
static int node_b_action_count = 0;
static int click_callback_count = 0;

static void routine_a(void) { node_a_action_count++; }
static void routine_b(void) { node_b_action_count++; }

/* Forward decls */
extern struct fsm_node node_a;
extern struct fsm_node node_b;
extern struct fsm_node node_off; // Satisfy extern in fsm_engine.c

static struct fsm_node *cb_goto_b(struct fsm_node *curr) {
    click_callback_count++;
    return &node_b;
}

static struct fsm_node *cb_stay(struct fsm_node *curr) {
    click_callback_count++;
    return NULL;
}

struct fsm_node node_a = {
    .id = 1, .name = "A",
    .action_routine = routine_a,
    .click_map = {
        [0] = &node_b, /* 1C -> B */
    },
    .hold_map = {
        [0] = &node_b, /* 1H -> B */
    },
    .click_callbacks = {
        [1] = cb_goto_b, /* 2C callback -> B */
        [2] = cb_stay,   /* 3C callback -> Stay (intercept) */
    }
};

struct fsm_node node_b = {
    .id = 2, .name = "B",
    .action_routine = routine_b,
};

/* Real definition of node_off to satisfy fsm_engine.c extern */
struct fsm_node node_off = {
    .id = 0, .name = "OFF",
};

/* --- Test Setup --- */

static void before(void *fixture)
{
    node_a_action_count = 0;
    node_b_action_count = 0;
    click_callback_count = 0;
    fsm_init(&node_a);
}

ZTEST_SUITE(fsm_core_suite, NULL, NULL, before, NULL, NULL);

/* --- Tests --- */

ZTEST(fsm_core_suite, test_init)
{
    zassert_equal(fsm_get_current_node(), &node_a, "Start node should be A");
    zassert_equal(node_a_action_count, 1, "Init should run action routine");
}

ZTEST(fsm_core_suite, test_manual_transition)
{
    fsm_transition_to(&node_b);
    zassert_equal(fsm_get_current_node(), &node_b, "Should be in Node B");
    zassert_equal(node_b_action_count, 1, "Node B action should execute");
}

ZTEST(fsm_core_suite, test_click_map_navigation)
{
    /* 1C on A maps to B */
    struct zbeam_msg msg = { .type = MSG_INPUT_TAP, .count = 1 };
    fsm_process_msg(&msg);
    zassert_equal(fsm_get_current_node(), &node_b, "1C should go to B");
}

ZTEST(fsm_core_suite, test_hold_map_navigation)
{
    /* 1H on A maps to B */
    struct zbeam_msg msg = { .type = MSG_INPUT_HOLD_START, .count = 1 };
    fsm_process_msg(&msg);
    zassert_equal(fsm_get_current_node(), &node_b, "1H should go to B");
}

ZTEST(fsm_core_suite, test_callback_transition)
{
    /* 2C on A has callback `cb_goto_b` */
    struct zbeam_msg msg = { .type = MSG_INPUT_TAP, .count = 2 };
    fsm_process_msg(&msg);
    
    zassert_equal(click_callback_count, 1, "Callback should fire");
    zassert_equal(fsm_get_current_node(), &node_b, "Callback should transition to B");
}

ZTEST(fsm_core_suite, test_callback_intercept)
{
    /* 3C on A has callback `cb_stay`. It performs action but returns NULL (no transition) */
    struct zbeam_msg msg = { .type = MSG_INPUT_TAP, .count = 3 };
    fsm_process_msg(&msg);
    
    zassert_equal(click_callback_count, 1, "Callback should fire");
    zassert_equal(fsm_get_current_node(), &node_a, "Should stay in A");
}

ZTEST(fsm_core_suite, test_emergency_off)
{
    fsm_transition_to(&node_a);
    fsm_emergency_off();
    zassert_equal(fsm_get_current_node(), &node_off, "Emergency off should go to OFF node");
}

void test_main(void)
{
    ztest_run_test_suites(NULL, false, 1, 1);
}
