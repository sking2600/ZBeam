/*
 * PWM Ramp - Generic Implementation
 * 
 * Simple CPU-driven loop through ramp table.
 * Used for platforms without hardware fade support.
 * 
 * Compiled only when ESP32-specific implementation is not used.
 */

#include "pwm_ramp.h"
#include "ramp_table.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pwm_ramp_generic, CONFIG_PWM_LOG_LEVEL);

static const struct pwm_dt_spec *pwm_dev;
static uint8_t current_brightness;
static bool ramp_active;

/* PWM period in ns - should match device tree */
#define PWM_PERIOD_NS 200000U

static uint32_t brightness_to_pulse_ns(uint8_t brightness)
{
    uint16_t duty = pwm_ramp_table[brightness];
    return ((uint32_t)duty * PWM_PERIOD_NS) / RAMP_TABLE_MAX_DUTY;
}

int pwm_ramp_init(const struct pwm_dt_spec *pwm_spec)
{
    if (!device_is_ready(pwm_spec->dev)) {
        LOG_ERR("PWM device not ready");
        return -ENODEV;
    }
    
    pwm_dev = pwm_spec;
    current_brightness = 0;
    ramp_active = false;
    
    LOG_INF("PWM ramp initialized (generic CPU loop)");
    
    return 0;
}

void pwm_ramp_set_brightness(uint8_t brightness)
{
    if (pwm_dev == NULL) return;
    
    uint32_t pulse_ns = brightness_to_pulse_ns(brightness);
    pwm_set_dt(pwm_dev, PWM_PERIOD_NS, pulse_ns);
    current_brightness = brightness;
}

int pwm_ramp_start(uint8_t target_brightness, uint32_t duration_ms)
{
    if (pwm_dev == NULL) return -ENODEV;
    
    ramp_active = true;
    
    int start = current_brightness;
    int end = target_brightness;
    int steps = abs(end - start);
    
    if (steps == 0) {
        ramp_active = false;
        return 0;
    }
    
    uint32_t step_delay_ms = duration_ms / steps;
    if (step_delay_ms < 1) step_delay_ms = 1;
    
    int direction = (end > start) ? 1 : -1;
    
    for (int i = start; i != end; i += direction) {
        pwm_ramp_set_brightness(i);
        k_msleep(step_delay_ms);
        
        if (!ramp_active) break;
    }
    
    pwm_ramp_set_brightness(target_brightness);
    ramp_active = false;
    
    return 0;
}

bool pwm_ramp_is_active(void)
{
    return ramp_active;
}

void pwm_ramp_stop(void)
{
    ramp_active = false;
}

uint8_t pwm_ramp_get_brightness(void)
{
    return current_brightness;
}
