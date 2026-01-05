/*
 * ZBeam - PWM LED Demo (Sine Wave)
 * Uses perception-corrected sine wave table for smooth breathing effect.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>

#include "pwm_ramp.h"
#include "ramp_table.h"

static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));

#define PWM_PERIOD_NS       200000U
#define CYCLE_DURATION_MS   3000
#define STEP_DELAY_MS       (CYCLE_DURATION_MS / SINE_TABLE_SIZE)

/* GPIO Matrix workaround */
#define GPIO_BASE               0x60004000
#define GPIO_ENABLE_REG         (GPIO_BASE + 0x0020)
#define GPIO8_FUNC_OUT_SEL_REG  (GPIO_BASE + 0x0554 + 8 * 4)
#define LEDC_LS_SIG_OUT0        45

static void configure_gpio_for_ledc(void)
{
    uint32_t sel = sys_read32(GPIO8_FUNC_OUT_SEL_REG);
    sel = (sel & ~0xFF) | LEDC_LS_SIG_OUT0;
    sel |= (1 << 9);
    sys_write32(sel, GPIO8_FUNC_OUT_SEL_REG);
    
    uint32_t enable = sys_read32(GPIO_ENABLE_REG);
    enable |= (1 << 8);
    sys_write32(enable, GPIO_ENABLE_REG);
}

static void set_duty_from_table(uint16_t duty)
{
    uint32_t pulse_ns = ((uint32_t)duty * PWM_PERIOD_NS) / SINE_TABLE_MAX_DUTY;
    pwm_set_dt(&pwm_led, PWM_PERIOD_NS, pulse_ns);
}

int main(void)
{
    printk("\n=== ZBeam Sine Wave Breathing Demo ===\n");
    printk("Using gamma-corrected sine table (g2.8, blue LED optimized)\n");
    printk("Cycle: %dms, %d steps\n", CYCLE_DURATION_MS, SINE_TABLE_SIZE);

    if (!pwm_is_ready_dt(&pwm_led)) {
        printk("ERROR: PWM device not ready\n");
        return -ENODEV;
    }

    configure_gpio_for_ledc();

    printk("Starting breathing loop...\n");

    while (1) {
        /* Walk through sine table for smooth breathing effect */
        for (int i = 0; i < SINE_TABLE_SIZE; i++) {
            set_duty_from_table(pwm_sine_table[i]);
            k_msleep(STEP_DELAY_MS);
        }
    }

    return 0;
}
