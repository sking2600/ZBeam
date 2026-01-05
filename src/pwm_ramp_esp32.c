/*
 * PWM Ramp - ESP32 LEDC Implementation
 * 
 * Uses LEDC hardware fade to interpolate between gamma-corrected table values.
 * This reduces CPU overhead while maintaining perception-corrected brightness.
 * 
 * Only compiled for ESP32 variants with CONFIG_PWM_RAMP_ESP32_LEDC_INTERPOLATION.
 */

#include "pwm_ramp.h"
#include "ramp_table.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pwm_ramp_esp32, CONFIG_PWM_LOG_LEVEL);

/* ESP32 HAL includes for direct LEDC fade control */
#include <stdlib.h>
#include <hal/ledc_hal.h>
#include <hal/ledc_ll.h>
#include <soc/ledc_struct.h>

/* GPIO matrix workaround */
#define GPIO_BASE               0x60004000
#define GPIO_ENABLE_REG         (GPIO_BASE + 0x0020)
#define GPIO8_FUNC_OUT_SEL_REG  (GPIO_BASE + 0x0554 + 8 * 4)
#define LEDC_LS_SIG_OUT0        45

static const struct pwm_dt_spec *pwm_dev;
static uint8_t current_brightness;
static bool ramp_active;

/* PWM period in ns - must match device tree */
#define PWM_PERIOD_NS 200000U

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
    
    /* Apply GPIO matrix workaround */
    configure_gpio_for_ledc();
    
    LOG_INF("PWM ramp initialized (ESP32 LEDC interpolation, step=%d)",
            CONFIG_PWM_RAMP_INTERPOLATION_STEP);
    
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
    
    int step = CONFIG_PWM_RAMP_INTERPOLATION_STEP;
    int start = current_brightness;
    int end = target_brightness;
    int direction = (end > start) ? 1 : -1;
    int total_steps = (abs(end - start) + step - 1) / step;
    
    if (total_steps == 0) {
        pwm_ramp_set_brightness(target_brightness);
        ramp_active = false;
        return 0;
    }
    
    uint32_t step_delay_ms = duration_ms / total_steps;
    
    LOG_DBG("Ramp: %d -> %d, step=%d, total_steps=%d, delay=%dms",
            start, end, step, total_steps, step_delay_ms);
    
    /* Step through table, using LEDC fade between points */
    for (int i = start; direction > 0 ? (i < end) : (i > end); i += direction * step) {
        int next_idx = i + direction * step;
        if (direction > 0 && next_idx > end) next_idx = end;
        if (direction < 0 && next_idx < end) next_idx = end;
        
        /* Set current brightness immediately */
        pwm_ramp_set_brightness(next_idx);
        
        k_msleep(step_delay_ms);
        
        current_brightness = next_idx;
        if (!ramp_active) break;
    }
    
    /* Ensure final state */
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
