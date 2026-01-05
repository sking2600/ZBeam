/*
 * AUX LED Manager
 * Controls secondary RGB LEDs (e.g. SK6812) or PWM-based single color LEDs.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include "aux_manager.h"
#include "../include/ramp_sine_13bit_g28.h"

LOG_MODULE_REGISTER(aux_manager, LOG_LEVEL_INF);

#define DT_DRV_COMPAT pwm_leds

static const char* aux_mode_to_str(aux_mode_t mode)
{
    switch(mode) {
        case AUX_OFF: return "OFF";
        case AUX_LOW: return "LOW";
        case AUX_HIGH: return "HIGH";
        case AUX_BLINK: return "BLINK";
        case AUX_SINE: return "SINE";
        default: return "UNKNOWN";
    }
}

static aux_mode_t current_mode = AUX_OFF;

/* Get PWM spec from the "aux_path" node label in overlay */
static const struct pwm_dt_spec aux_pwm = PWM_DT_SPEC_GET(DT_NODELABEL(aux_led));

/* For patterns */
static struct k_timer aux_timer;
static struct k_work aux_work;
static uint32_t ticks = 0;
static uint8_t sine_index = 0;

/* PWM Parameters */
#define AUX_PWM_PERIOD_NS     10000000  // 100Hz base (10ms)
// Note: ledc driver handles period setting. Overlay says 10000ns/100kHz? 
// Actually overlay says <&ledc0 1 10000 PWM_POLARITY_NORMAL>; 10000ns = 100kHz.
// We will rely on pwm_set_pulse_dt using the dt-spec period.

/* Work handler - updates PWM */
static void aux_work_handler(struct k_work *work)
{
    ticks++;
    uint32_t pulse_ns = 0;

    switch (current_mode) {
        case AUX_OFF:
            pulse_ns = 0;
            break;
            
        case AUX_LOW:
            // Constant Low Brightness (approx 10%)
            pulse_ns = aux_pwm.period / 10; 
            break;
            
        case AUX_HIGH:
            // Full Brightness
            pulse_ns = aux_pwm.period;
            break;
            
        case AUX_BLINK:
            // Slow Blink: 3s cycle. On for 100ms.
            // Blink Mode uses HIGH brightness during ON time
            if ((ticks % 300) < 10) { // 10 ticks * 10ms = 100ms
                 pulse_ns = aux_pwm.period;
            } else {
                 pulse_ns = 0;
            }
            break;

        case AUX_SINE:
            // Breathing Effect using Sine Table
            // Update sine index every 3 ticks (30ms) for slower breathe
            if (ticks % 3 == 0) {
                sine_index++;
            }
            // Map 13-bit table (0-8191) to PWM Period
            // pulse = (value * period) / 8191
            uint32_t table_val = pwm_sine_table_13bit_g28[sine_index];
            pulse_ns = (uint64_t)table_val * aux_pwm.period / PWM_SINE_13BIT_G28_MAX_DUTY;
            break;
            
        default:
            pulse_ns = 0;
            break;
    }

    if (device_is_ready(aux_pwm.dev)) {
        pwm_set_pulse_dt(&aux_pwm, pulse_ns);
    }
}

/* Timer ISR - just submits work */
static void aux_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&aux_work);
}

void aux_init(void)
{
    LOG_INF("Aux Init: Start");

    if (!device_is_ready(aux_pwm.dev)) {
        LOG_ERR("AUX PWM Device not ready");
        return;
    }

    LOG_INF("Aux Init: PWM Device Ready. Period=%d ns", aux_pwm.period);
    
    k_work_init(&aux_work, aux_work_handler);
    k_timer_init(&aux_timer, aux_timer_handler, NULL);
    k_timer_start(&aux_timer, K_MSEC(10), K_MSEC(10)); // 100Hz Update Rate

    LOG_INF("AUX Manager Initialized (PWM Mode).");
    current_mode = AUX_OFF;
    pwm_set_pulse_dt(&aux_pwm, 0);
}

void aux_set_mode(uint8_t mode)
{
    if (mode >= AUX_MODE_COUNT) mode = AUX_OFF;
    current_mode = (aux_mode_t)mode;
    LOG_INF("AUX Mode Handle: %d", current_mode);
}

void aux_cycle_mode(void)
{
    current_mode = (current_mode + 1) % AUX_MODE_COUNT;
    LOG_INF("AUX Mode Cycled to: %s", aux_mode_to_str(current_mode));
}

void aux_update(void)
{
    // Implementation handled by periodic timer/workqueue
}

