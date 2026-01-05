/**
 * @file main.c
 * @brief Unit test for Thermal Manager throttle logic.
 */

#include <zephyr/ztest.h>
#include "thermal_manager.h"

ZTEST_SUITE(thermal_logic_suite, NULL, NULL, NULL, NULL, NULL);

ZTEST(thermal_logic_suite, test_thermal_init)
{
    thermal_init();
    /* After init, no throttle applied */
    zassert_equal(thermal_apply_throttle(255), 255, "No throttle expected after init");
    zassert_equal(thermal_get_temp_mc(), 25000, "Initial temp should be 25C");
}

ZTEST(thermal_logic_suite, test_thermal_update_heating)
{
    thermal_init();
    
    /* Simulate high brightness for multiple cycles */
    for (int i = 0; i < 100; i++) {
        thermal_update(200); /* High brightness */
    }
    
    /* Temperature should have increased */
    zassert_true(thermal_get_temp_mc() > 25000, "Temp should rise with high brightness");
}

ZTEST(thermal_logic_suite, test_thermal_throttle_kicks_in)
{
    thermal_init();
    
    /* Force temp above ceiling (simulate hot) */
    for (int i = 0; i < 200; i++) {
        thermal_update(255); /* Max brightness */
    }
    
    /* Throttle should now be active */
    uint8_t throttled = thermal_apply_throttle(255);
    zassert_true(throttled < 255, "Throttle should reduce brightness");
}

ZTEST(thermal_logic_suite, test_thermal_cooling)
{
    thermal_init();
    
    /* First heat up */
    for (int i = 0; i < 100; i++) {
        thermal_update(255);
    }
    int32_t hot_temp = thermal_get_temp_mc();
    
    /* Then cool down */
    for (int i = 0; i < 200; i++) {
        thermal_update(0); /* LED off */
    }
    
    zassert_true(thermal_get_temp_mc() < hot_temp, "Temp should decrease when LED off");
}

void test_main(void)
{
    ztest_run_test_suites(NULL, false, 1, 1);
}
