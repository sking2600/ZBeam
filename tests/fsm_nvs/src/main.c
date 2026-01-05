#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "nvs_manager.h"
#include "key_map.h"

LOG_MODULE_REGISTER(Test_FSM_NVS, LOG_LEVEL_INF);

// Suite setup
static void *setup(void)
{
    int rc = nvs_init_fs();
    zassert_equal(rc, 0, "NVS Init failed");
    key_map_init();
    return NULL;
}

ZTEST_SUITE(fsm_nvs_suite, NULL, setup, NULL, NULL, NULL);

ZTEST(fsm_nvs_suite, test_01_defaults_load)
{
    // 1. Ensure we start fresh or load known defaults
    // Since we can't easily wipe the emulator file from code efficiently without re-init,
    // we assume the test runner might be clean or we just verify structure.
    
    // Check OFF node defaults from key_map.c
    // [0] -> NODE_ON
    struct fsm_node *off = all_nodes[NODE_OFF];
    zassert_not_null(off, "Off node is null");
    zassert_equal(off->id, NODE_OFF, "ID Mismatch");
    
    // By default compile time: click[0] should be NODE_RAMP
    struct fsm_node *target = off->click_map[0];
    zassert_not_null(target, "Default target is null");
    zassert_equal(target->id, NODE_RAMP, "Default target is not RAMP");
}

ZTEST(fsm_nvs_suite, test_02_save_and_restore)
{
    struct fsm_node *off = all_nodes[NODE_OFF];
    
    // 1. Create a modified config
    struct node_config_data new_config = {0};
    
    // Preserve timeout
    new_config.timeout_ms = off->timeout_ms;
    
    // Change: 1 click (index 0) -> STROBE (instead of ON)
    new_config.target_click_ids[0] = NODE_STROBE;
    // Set others to invalid/null
    for(int i=1; i<MAX_NAV_SLOTS; i++) new_config.target_click_ids[i] = NODE_COUNT;
    for(int i=0; i<MAX_NAV_SLOTS; i++) new_config.target_hold_ids[i] = NODE_COUNT;

    // 2. Save it
    nvs_save_node_config(NODE_OFF, &new_config);
    
    // 3. Corrupt the RAM pointer to prove reload works
    off->click_map[0] = NULL;
    
    // 4. Reload from NVS
    nvs_load_runtime_config();
    
    // 5. Verify restoration
    struct fsm_node *restored_target = off->click_map[0];
    zassert_not_null(restored_target, "Failed to restore pointer");
    zassert_equal(restored_target->id, NODE_STROBE, "Failed to restore correct ID (Strobe)");
    
    LOG_INF("Test Verify: OFF[0] points to %s", restored_target->name);
}

ZTEST(fsm_nvs_suite, test_03_system_config_defaults)
{
    // Wipe system config to test defaults
    nvs_wipe_all();
    
    struct system_config config;
    int rc = nvs_load_system_config(&config);
    zassert_not_equal(rc, 0, "Should handle missing config");

    // In main application logic, failing to load falls back to Kconfig.
    // Here we verify that constants are available.
    zassert_equal(CONFIG_ZBEAM_CLICK_TIMEOUT_MS, 400, "Default Click Timeout Wrong");
    zassert_equal(CONFIG_ZBEAM_HOLD_DURATION_MS, 500, "Default Hold Duration Wrong");
}

ZTEST(fsm_nvs_suite, test_04_system_config_update)
{
    struct system_config new_cfg = {
        .click_timeout_ms = 800,
        .hold_duration_ms = 1200,
        .monitor_key_code = 45
    };
    
    nvs_save_system_config(&new_cfg);
    
    struct system_config loaded_cfg = {0};
    int rc = nvs_load_system_config(&loaded_cfg);
    
    zassert_equal(rc, 0, "Failed to load system config");
    zassert_equal(loaded_cfg.click_timeout_ms, 800, "Click Timeout Check Failed");
    zassert_equal(loaded_cfg.hold_duration_ms, 1200, "Hold Duration Check Failed");
}

ZTEST(fsm_nvs_suite, test_05_factory_reset)
{
    // 1. Set up a non-default state
    struct fsm_node *off = all_nodes[NODE_OFF];
    struct node_config_data new_config = {0};
    // Map click[0] to STROBE
    new_config.target_click_ids[0] = NODE_STROBE; 
    
    nvs_save_node_config(NODE_OFF, &new_config);
    
    // Save system config too
    struct system_config sys_cfg = {.click_timeout_ms = 999};
    nvs_save_system_config(&sys_cfg);
    
    // 2. Perform Wipe (Simulation of Factory Reset routine)
    nvs_wipe_all();
    
    // 3. Verify System Config is gone
    struct system_config loaded_sys;
    zassert_not_equal(nvs_load_system_config(&loaded_sys), 0, "System config should be wiped");
    
    // 4. Verify Node Config is gone (Simulate reload logic)
    // Attempt to read from NVS should fail, so we'd normally keep defaults.
    // We can check if the underlying NVS read fails for node 0.
    // But since nvs_load_runtime_config() logic is "if read succeeds, update",
    // we should manually clear the RAM pointer first to prove it DOESN'T get updated to Strobe.
    
    off->click_map[0] = NULL; // Clear it
    
    // We expect this load to look at NVS, find nothing, and do nothing (leaving it NULL)
    // Wait, nvs_load_runtime_config relies on NVS being present.
    // If NVS is wiped, nvs_load_runtime_config will just skip everything.
    // So the pointer remains NULL.
    // BUT, in a real reboot, the code starts with static defaults.
    // So let's simulate that by resetting the pointer to "ON" (default)
    off->click_map[0] = all_nodes[NODE_ON];
    
    nvs_load_runtime_config();
    
    // It should STILL be ON. If NVS wasn't wiped, it would have loaded STROBE.
    zassert_equal(off->click_map[0]->id, NODE_ON, "Should be default ON, not Strobe");
}

ZTEST(fsm_nvs_suite, test_06_callback_exec)
{
    struct fsm_node *ramp = all_nodes[NODE_RAMP];
    zassert_not_null(ramp, "RAMP node not found");
    
    // 1. Verify callbacks are wired up
    zassert_not_null(ramp->hold_callbacks[0], "1-hold callback missing");
    zassert_not_null(ramp->hold_callbacks[1], "2-hold callback missing");
    zassert_not_null(ramp->release_callback, "release callback missing");
    
    // 2. Initialize FSM at RAMP node
    fsm_init(ramp);
    struct fsm_node *initial = fsm_get_current_node();
    zassert_equal(initial->id, NODE_RAMP, "Should start in RAMP");
    
    // 3. Call hold callback - should return NULL (stay in state, start timer)
    struct fsm_node *next = ramp->hold_callbacks[0](ramp);
    zassert_is_null(next, "Hold callback should return NULL to stay in state");
    
    // 4. Wait briefly and verify we're STILL in RAMP (timer running, not transitioned yet)
    k_msleep(30); // Less than one ramp step (50ms)
    struct fsm_node *curr = fsm_get_current_node();
    zassert_equal(curr->id, NODE_RAMP, "Should still be in RAMP while ramping");
    
    // 5. Release and verify cleanup
    struct fsm_node *release_result = ramp->release_callback(ramp);
    zassert_is_null(release_result, "Release should return NULL");
    
    // 6. Transition to OFF to reset state
    fsm_transition_to(all_nodes[NODE_OFF]);
}

ZTEST(fsm_nvs_suite, test_07_blink_limit)
{
    struct fsm_node *ramp = all_nodes[NODE_RAMP];
    fsm_init(ramp); // Start at RAMP
    
    // Start Ramp Up
    ramp->hold_callbacks[0](ramp);
    
    // Wait for ramp to hit limit and transition to BLINK
    // Max index is ~5, interval 50ms. Should take ~250ms.
    // Wait up to 1s.
    bool found_blink = false;
    for(int i=0; i<20; i++) {
        k_msleep(50);
        struct fsm_node *curr = fsm_get_current_node();
        if (curr && curr->id == NODE_BLINK) {
            found_blink = true;
            break;
        }
    }
    
    zassert_true(found_blink, "Should hit limit and transition to BLINK node via timer");
    
    // Cleanup: Transition to OFF to stop any active timers from BLINK node
    // BLINK has a return timeout, which might fire during next test init.
    struct fsm_node *off = all_nodes[NODE_OFF];
    fsm_transition_to(off);
    k_msleep(10); // Give it a tick to stop timer
}

ZTEST(fsm_nvs_suite, test_10_sweep_node)
{
    struct fsm_node *sweep = all_nodes[NODE_SWEEP];
    zassert_not_null(sweep, "SWEEP node not found");
    zassert_not_null(sweep->hold_callbacks[0], "Sweep hold callback missing");
    zassert_not_null(sweep->release_callback, "Sweep release callback missing");
    
    // 1. Initialize at SWEEP node
    fsm_init(sweep);
    struct fsm_node *initial = fsm_get_current_node();
    zassert_equal(initial->id, NODE_SWEEP, "Should start in SWEEP");
    
    // 2. Start Sweep (Hold) - should return NULL and start timer
    struct fsm_node *next = sweep->hold_callbacks[0](sweep);
    zassert_is_null(next, "Sweep hold should return NULL");
    
    // 3. Wait long enough for multiple timer ticks to prove cycling works
    // Sweep speed is CONFIG_ZBEAM_SWEEP_SPEED_MS (default 10ms)
    // Wait 100ms = ~10 ticks through the sine LUT
    k_msleep(100);
    
    // 4. We're still in SWEEP (no automatic transition out of sweep)
    struct fsm_node *during = fsm_get_current_node();
    zassert_equal(during->id, NODE_SWEEP, "Should still be in SWEEP during sweep");
    
    // 5. Release (Lock) - stops timer, syncs index
    struct fsm_node *release_result = sweep->release_callback(sweep);
    zassert_is_null(release_result, "Release should return NULL");
    
    // 6. Verify we're STILL in Sweep node after release (lock behavior)
    struct fsm_node *after = fsm_get_current_node();
    zassert_equal(after->id, NODE_SWEEP, "Should stay in Sweep node after release");
    
    // 7. Cleanup: Transition to OFF
    fsm_transition_to(all_nodes[NODE_OFF]);
}

/**
 * @brief Test ramping DOWN to minimum triggers BLINK.
 */
ZTEST(fsm_nvs_suite, test_11_down_ramp_limit)
{
    struct fsm_node *ramp = all_nodes[NODE_RAMP];
    fsm_init(ramp);
    
    // Start Ramp DOWN (2-hold)
    ramp->hold_callbacks[1](ramp);
    
    // Wait for ramp to hit min limit
    bool found_blink = false;
    for(int i=0; i<20; i++) {
        k_msleep(CONFIG_ZBEAM_RAMP_STEP_MS);
        struct fsm_node *curr = fsm_get_current_node();
        if (curr && curr->id == NODE_BLINK) {
            found_blink = true;
            break;
        }
    }
    
    zassert_true(found_blink, "Should hit min limit and transition to BLINK");
    fsm_transition_to(all_nodes[NODE_OFF]);
}

/**
 * @brief Test PWM accessor returns correct value.
 */
ZTEST(fsm_nvs_suite, test_12_pwm_accessor)
{
    // Re-initialize to reset PWM index
    key_map_init();
    
    // Get initial PWM (should be first level = 1)
    uint8_t pwm = key_map_get_current_pwm();
    zassert_equal(pwm, 1, "Initial PWM should be 1 (first level)");
    
    // Start ramp, wait for one step
    struct fsm_node *ramp = all_nodes[NODE_RAMP];
    fsm_init(ramp);
    ramp->hold_callbacks[0](ramp); // Ramp up
    k_msleep(CONFIG_ZBEAM_RAMP_STEP_MS + 10);
    ramp->release_callback(ramp);
    
    // PWM should have increased
    uint8_t pwm_after = key_map_get_current_pwm();
    zassert_true(pwm_after > pwm, "PWM should increase after ramping");
    
    fsm_transition_to(all_nodes[NODE_OFF]);
}
