#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "multi_tap_input.h"

LOG_MODULE_REGISTER(Test_MultiTap, LOG_LEVEL_INF);

// --- MOCK HANDLER ---
static struct multi_tap_input_action last_action = {0};
static int action_count = 0;

static void mock_handler(const struct multi_tap_input_action *action) {
    last_action = *action;
    action_count++;
    LOG_INF("Mock Handler: count=%d, type=%d", action->count, action->type);
}

static void reset_mock(void) {
    last_action.count = 0;
    last_action.type = 0;
    action_count = 0;
}

// --- TESTS ---

/**
 * @brief Test single tap sequence.
 */
ZTEST(fsm_nvs_suite, test_13_single_tap)
{
    reset_mock();
    multi_tap_input_init(mock_handler);
    
    // Simulate: Press -> Release -> Wait for timeout
    multi_tap_input_process_key(1); // Press
    k_msleep(50);                   // Short press
    multi_tap_input_process_key(0); // Release
    
    // Wait for click timeout to finalize
    k_msleep(CONFIG_ZBEAM_CLICK_TIMEOUT_MS + 50);
    
    zassert_equal(action_count, 1, "Should fire one action");
    zassert_equal(last_action.count, 1, "Should be 1 tap");
    zassert_equal(last_action.type, MULTI_TAP_EVENT_TAP, "Should be TAP event");
}

/**
 * @brief Test double tap sequence.
 */
ZTEST(fsm_nvs_suite, test_14_double_tap)
{
    reset_mock();
    multi_tap_input_init(mock_handler);
    
    // First tap
    multi_tap_input_process_key(1);
    k_msleep(50);
    multi_tap_input_process_key(0);
    
    // Quick second tap (before timeout)
    k_msleep(100);
    multi_tap_input_process_key(1);
    k_msleep(50);
    multi_tap_input_process_key(0);
    
    // Wait for timeout
    k_msleep(CONFIG_ZBEAM_CLICK_TIMEOUT_MS + 50);
    
    zassert_equal(action_count, 1, "Should fire one action (combined)");
    zassert_equal(last_action.count, 2, "Should be 2 taps");
    zassert_equal(last_action.type, MULTI_TAP_EVENT_TAP, "Should be TAP event");
}

/**
 * @brief Test hold generates HOLD_START then HOLD_RELEASE.
 */
ZTEST(fsm_nvs_suite, test_15_hold_events)
{
    reset_mock();
    multi_tap_input_init(mock_handler);
    
    // Press and hold past threshold
    multi_tap_input_process_key(1);
    k_msleep(CONFIG_ZBEAM_HOLD_DURATION_MS + 50);
    
    // Should have fired HOLD_START
    zassert_equal(action_count, 1, "Should fire HOLD_START");
    zassert_equal(last_action.type, MULTI_TAP_EVENT_HOLD_START, "Should be HOLD_START");
    
    // Release
    multi_tap_input_process_key(0);
    
    // Should fire HOLD_RELEASE
    zassert_equal(action_count, 2, "Should fire HOLD_RELEASE");
    zassert_equal(last_action.type, MULTI_TAP_EVENT_HOLD_RELEASE, "Should be HOLD_RELEASE");
}
