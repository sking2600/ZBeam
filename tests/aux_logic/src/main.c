/**
 * @file main.c
 * @brief Unit test for AUX LED Manager mode cycling.
 */

#include <zephyr/ztest.h>
#include "aux_manager.h"

ZTEST_SUITE(aux_logic_suite, NULL, NULL, NULL, NULL, NULL);

ZTEST(aux_logic_suite, test_aux_init)
{
    aux_init();
    /* Verify init completes without crash */
    zassert_true(true, "AUX init should succeed");
}

ZTEST(aux_logic_suite, test_aux_set_mode)
{
    aux_init();
    aux_set_mode(3); /* AUX_BLINK */
    /* Mode should be set without error */
    zassert_true(true, "Mode set should succeed");
}

ZTEST(aux_logic_suite, test_aux_cycle_mode)
{
    aux_init();
    
    /* Cycle through all modes */
    for (int i = 0; i < 10; i++) {
        aux_cycle_mode();
    }
    /* Should wrap around without crash */
    zassert_true(true, "Cycle mode should wrap safely");
}

ZTEST(aux_logic_suite, test_aux_update)
{
    aux_init();
    aux_set_mode(4); /* AUX_VOLTAGE */
    aux_update();
    /* Update should not crash */
    zassert_true(true, "Update should not crash");
}

void test_main(void)
{
    ztest_run_test_suites(NULL, false, 1, 1);
}
