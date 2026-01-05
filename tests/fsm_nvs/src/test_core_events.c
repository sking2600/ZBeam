#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "fsm_engine.h"
#include "multi_tap_input.h"

LOG_MODULE_REGISTER(Test_Core_Events, LOG_LEVEL_INF);

// --- MOCK CALLBACKS ---
static bool cb_hold_start_fired = false;
static bool cb_release_fired = false;
static struct fsm_node *last_callback_node = NULL;

static struct fsm_node *cb_test_hold(struct fsm_node *self) {
    cb_hold_start_fired = true;
    last_callback_node = self;
    return NULL; // Stay in node
}

static struct fsm_node *cb_test_release(struct fsm_node *self) {
    cb_release_fired = true;
    last_callback_node = self;
    return NULL; // Stay
}

// --- TEST FIXTURE ---
static struct fsm_node test_node = {
    .id = 99,
    .name = "TEST_NODE",
    .action_routine = NULL,
    .timeout_ms = 0,
    .hold_callbacks = {
        [0] = cb_test_hold, // 1 Hold
    },
    .release_callback = cb_test_release
};

static void reset_flags(void) {
    cb_hold_start_fired = false;
    cb_release_fired = false;
    last_callback_node = NULL;
}

// --- TESTS ---

ZTEST(fsm_nvs_suite, test_08_core_hold_event)
{
    reset_flags();
    
    // Manually inject a HOLD_START event to the FSM Engine
    struct multi_tap_input_action action = {
        .count = 1,
        .type = MULTI_TAP_EVENT_HOLD_START
    };
    
    // Manually set current node (normally FSM manages this)
    // We need to bypass fsm_init for this micro-test or ensure fsm_engine sees this node.
    // Hack: We can temporarily swap the 'current_node' by just initializing FSM with it?
    // fsm_init sets current_node.
    fsm_init(&test_node);
    
    fsm_process_input(&action);
    
    zassert_true(cb_hold_start_fired, "HOLD_START callback did not fire");
    zassert_equal(last_callback_node, &test_node, "Callback received wrong node ptr");
    zassert_false(cb_release_fired, "Release callback fired prematurely");
}

ZTEST(fsm_nvs_suite, test_09_core_release_event)
{
    reset_flags();
    fsm_init(&test_node);
    
    struct multi_tap_input_action action = {
        .count = 1,
        .type = MULTI_TAP_EVENT_HOLD_RELEASE
    };
    
    fsm_process_input(&action);
    
    zassert_true(cb_release_fired, "HOLD_RELEASE callback did not fire");
    zassert_false(cb_hold_start_fired, "Hold Start fired on Release event");
}
