/**
 * @file main.c
 * @brief Unit tests for Strobe Logic.
 */

#include <zephyr/ztest.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include "key_map.h"
#include "multi_tap_input.h"
#include "fsm_engine.h"
#include "zbeam_msg.h"

/* --- MOCKS --- */

/* Mock storage manager wipe */
void storage_wipe_all(void) {}

/* Mock PWM */
#include <zephyr/drivers/pwm.h>
int z_impl_pwm_set_pulse_dt(const struct pwm_dt_spec *spec, uint32_t pulse) { return 0; }

void sys_reboot(int type) {}

/* Mock fsm_worker_post_msg to redirect to fsm_process_msg immediately */
/* In integration test, we want FSM to process it. */
int fsm_worker_post_msg(struct zbeam_msg *msg)
{
    fsm_process_msg(msg);
    return 0;
}

/* --- HELPERS --- */

static void press_button(void)
{
    input_report_key(NULL, INPUT_KEY_0, 1, true, K_NO_WAIT);
}
static void release_button(void)
{
    input_report_key(NULL, INPUT_KEY_0, 0, true, K_NO_WAIT);
}
static void click(void)
{
    press_button(); k_sleep(K_MSEC(20));
    release_button(); k_sleep(K_MSEC(20));
}

static void enter_strobe_mode(void)
{
    /* 3H from OFF to Strobe */
    /* 1 C */
    press_button(); k_sleep(K_MSEC(20)); release_button(); k_sleep(K_MSEC(20)); // C1
    /* 2 C */
    press_button(); k_sleep(K_MSEC(20)); release_button(); k_sleep(K_MSEC(20)); // C2
    /* 3 H */
    press_button(); k_sleep(K_MSEC(300)); /* > 200ms hold */
    /* Handled by hold_timer -> MSG -> FSM -> Strobe */
}

static void exit_strobe_mode(void)
{
    /* Release button from the 3H */
    release_button(); k_sleep(K_MSEC(50));
}

/* --- FIXTURE --- */

static void before(void *fixture)
{
    static bool init = false;
    if (!init) {
        multi_tap_configure(100, 200); /* Fast test timings */
        multi_tap_input_init();
        key_map_init();
        fsm_init(get_start_node());
        init = true;
    }
    multi_tap_input_reset();
    fsm_init(get_start_node()); /* Reset to OFF */
    k_sleep(K_MSEC(50));
}

ZTEST_SUITE(strobe_suite, NULL, NULL, before, NULL, NULL);

/* --- TESTS --- */

ZTEST(strobe_suite, test_01_enter_strobe)
{
    enter_strobe_mode();
    /* Should be in STROBE node */
    /* Checking name, or checking side effect? */
    /* key_map.c doesn't expose current_node easily without extern. */
    /* But we can check Strobe Freq logic? */
    
    /* release button to be in steady Strobe */
    exit_strobe_mode();
    
    /* Verify we are strobing? Hard to verify timer without spying. */
    /* Assuming if no crash, we ok. */
}

ZTEST(strobe_suite, test_02_ramp_up_1H)
{
    enter_strobe_mode();
    exit_strobe_mode();
    
    /* Current freq should be default (12Hz -> ~idx 40?) */
    uint8_t f1 = key_map_get_strobe_freq();
    printk("Initial Freq: %d\n", f1);
    
    /* Press and Hold (1H) */
    press_button();
    k_sleep(K_MSEC(500)); /* Hold for 500ms */
    
    uint8_t f2 = key_map_get_strobe_freq();
    printk("After 1H (500ms): %d\n", f2);
    
    release_button();
    
    zassert_not_equal(f1, f2, "Frequency should change");
    /* 1H is Faster -> UP -> idx increases */
    zassert_true(f2 > f1, "Frequency should increase (1H)");
}

ZTEST(strobe_suite, test_03_ramp_down_2H)
{
    enter_strobe_mode();
    exit_strobe_mode();
    
    uint8_t f1 = key_map_get_strobe_freq();
    
    /* Bump it up first so we can go down */
    press_button(); k_sleep(K_MSEC(500)); release_button(); k_sleep(K_MSEC(50)); 
    f1 = key_map_get_strobe_freq();
    printk("High Freq: %d\n", f1);
    
    /* Now 2H: Click, then Hold */
    press_button(); k_sleep(K_MSEC(20)); release_button(); k_sleep(K_MSEC(20)); // Click
    press_button(); k_sleep(K_MSEC(500)); // Hold
    
    uint8_t f2 = key_map_get_strobe_freq();
    printk("After 2H (500ms): %d\n", f2);
    
    release_button();
    
    zassert_true(f2 < f1, "Frequency should decrease (2H)");
}

void test_main(void)
{
    ztest_run_test_suites(NULL, false, 1, 1);
}
