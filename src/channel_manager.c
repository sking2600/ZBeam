#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "channel_manager.h"
#include "thermal_manager.h"

LOG_MODULE_REGISTER(channel_mgr, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)
#define NUM_EMITTERS DT_PROP_LEN(USER_NODE, pwms)
#define PWM_SPEC_GET(node_id, prop, idx) PWM_DT_SPEC_GET_BY_IDX(node_id, idx)

static const struct pwm_dt_spec emitters[NUM_EMITTERS] = {
    DT_FOREACH_PROP_ELEM_SEP(USER_NODE, pwms, PWM_SPEC_GET, (,))
};

static channel_mode_t current_mode = CHANNEL_MODE_SINGLE;

void channel_init(void)
{
    LOG_INF("Initializing %d emitters", NUM_EMITTERS);
    for (int i = 0; i < NUM_EMITTERS; i++) {
        if (!device_is_ready(emitters[i].dev)) {
            LOG_ERR("Emitter %d PWM device not ready", i);
        }
    }
    
    if (NUM_EMITTERS > 1) {
        current_mode = CHANNEL_MODE_50_50;
    } else {
        current_mode = CHANNEL_MODE_SINGLE;
    }
}

void channel_apply_mix(uint8_t master_level)
{
    // Apply thermal throttling first
    uint8_t throttled = thermal_apply_throttle(master_level);
    
    // Calculate weights based on mode
    uint16_t weights[NUM_EMITTERS];
    for (int i = 0; i < NUM_EMITTERS; i++) weights[i] = 0;

    switch (current_mode) {
        case CHANNEL_MODE_SINGLE:
            if (NUM_EMITTERS > 0) weights[0] = 255;
            break;

        case CHANNEL_MODE_50_50:
            for (int i = 0; i < NUM_EMITTERS; i++) weights[i] = 255; 
            // Note: In 50/50, we usually want full power if heat allows, 
            // but for "equal" power we might cap sum at 255.
            // Anduril typically allows 100% on both for max output.
            break;

        case CHANNEL_MODE_COLD:
            if (NUM_EMITTERS > 0) weights[0] = 255;
            break;

        case CHANNEL_MODE_WARM:
            if (NUM_EMITTERS > 1) weights[1] = 255;
            else if (NUM_EMITTERS > 0) weights[0] = 255;
            break;

        case CHANNEL_MODE_AUTO_TINT:
            if (NUM_EMITTERS >= 2) {
                // Shift from Emitter 1 (Warm) to Emitter 0 (Cold)
                weights[0] = throttled;       /* Cold increases with brightness */
                weights[1] = 255 - throttled; /* Warm decreases with brightness */
            } else if (NUM_EMITTERS > 0) {
                weights[0] = 255;
            }
            break;

        case CHANNEL_MODE_SEQUENTIAL:
            if (NUM_EMITTERS > 0) {
                uint8_t slice = 255 / NUM_EMITTERS;
                for (int i = 0; i < NUM_EMITTERS; i++) {
                    int32_t start = i * slice;
                    int32_t end = (i + 1) * slice;
                    if (throttled <= start) {
                        weights[i] = 0;
                    } else if (throttled >= end) {
                        weights[i] = 255;
                    } else {
                        // Linear interpolate within slice
                        weights[i] = (uint32_t)(throttled - start) * 255 / slice;
                    }
                }
            }
            break;

        default:
            if (NUM_EMITTERS > 0) weights[0] = 255;
            break;
    }

    // Apply to hardware
    for (int i = 0; i < NUM_EMITTERS; i++) {
        // Final duty = (throttled * weight) / 255
        // If mode is AUTO_TINT, we use weight directly as it already incorporates the mix
        uint32_t level;
        if (current_mode == CHANNEL_MODE_AUTO_TINT && NUM_EMITTERS >= 2) {
            level = weights[i]; // weights already scaled by throttled in this specific mode logic
            // Wait, no. My weights[0] = throttled; logic above means if master=10, weight=10.
            // Pulse = (Period * 10) / 255. Correct.
        } else {
            level = (uint32_t)throttled * weights[i] / 255;
        }

        uint32_t pulse = (emitters[i].period * level) / 255;
        pwm_set_pulse_dt(&emitters[i], pulse);
    }
}

void channel_cycle_mode(void)
{
    if (NUM_EMITTERS <= 1) return;

    current_mode = (current_mode + 1) % CHANNEL_MODE_COUNT;
    LOG_INF("Channel Mode: %d", current_mode);
    
    // Skip modes that aren't supported by hardware
    if (current_mode == CHANNEL_MODE_AUTO_TINT && NUM_EMITTERS < 2) {
        current_mode = CHANNEL_MODE_SINGLE;
    }
}
