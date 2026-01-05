/**
 * @file main.c
 * @brief Unit test for Battery Check blink calculation.
 */

#include <zephyr/ztest.h>
#include "batt_check.h"

ZTEST_SUITE(batt_check_suite, NULL, NULL, NULL, NULL, NULL);

/* Test Helper */
static void assert_blinks(uint16_t mv, uint8_t expected_major, uint8_t expected_minor)
{
    uint8_t major, minor;
    batt_calculate_blinks(mv, &major, &minor);
    zassert_equal(major, expected_major, "Voltage %d: Expected major %d, got %d", mv, expected_major, major);
    zassert_equal(minor, expected_minor, "Voltage %d: Expected minor %d, got %d", mv, expected_minor, minor);
}

ZTEST(batt_check_suite, test_blink_calculation_exact)
{
    /* 3.8V -> 3 blinks, 8 blinks */
    assert_blinks(3800, 3, 8);
    
    /* 4.2V -> 4 blinks, 2 blinks */
    assert_blinks(4200, 4, 2);
    
    /* 3.0V -> 3 blinks, 0 blinks */
    assert_blinks(3000, 3, 0);
}

ZTEST(batt_check_suite, test_blink_calculation_rounding)
{
    /* 3.84V -> Round down to 3.8 */
    assert_blinks(3849, 3, 8);
    
    /* 3.85V -> Round up to 3.9 */
    assert_blinks(3850, 3, 9);
    
    /* 3.99V -> Round up to 4.0 */
    assert_blinks(3990, 4, 0);
}

ZTEST(batt_check_suite, test_blink_calculation_limits)
{
    /* < 2.5V clamped to 2.5V */
    assert_blinks(2000, 2, 5);
    
    /* > 4.5V clamped to 4.5V */
    assert_blinks(5000, 4, 5);
}

void test_main(void)
{
    printk(">>> DEBUG: test_main() called <<<\n");
    ztest_run_test_suites(NULL, false, 1, 1);
}
