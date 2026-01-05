#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include "key_map.h"
#include "fsm_engine.h"
#include "multi_tap_input.h"
#include "pwm_ramp.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* PWM LED from device tree */
static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET_OR(DT_ALIAS(pwm_led0), {0});

/* GPIO Matrix workaround for ESP32-C3 */
#if defined(CONFIG_SOC_SERIES_ESP32C3)
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
#else
static void configure_gpio_for_ledc(void) { }
#endif



int main(void)
{
    LOG_INF("=== ZBeam Starting ===");

    /* 1. Configure GPIO matrix (ESP32-C3 specific) */
    configure_gpio_for_ledc();

    /* 2. Initialize PWM ramp subsystem */
    if (pwm_led.dev != NULL && device_is_ready(pwm_led.dev)) {
        if (pwm_ramp_init(&pwm_led) == 0) {
            LOG_INF("PWM ramp initialized");
        }
    } else {
        LOG_WRN("PWM LED not available");
    }

    /* 3. Initialize key map (parses Kconfig PWM values, creates nodes) */
    key_map_init();
    LOG_INF("Key map initialized");

    /* 4. Initialize FSM with start node */
    fsm_init(get_start_node());
    LOG_INF("FSM initialized");

    /* 5. Initialize multi-tap input */
    multi_tap_input_init();

    /* 
     * Worker threads (fsm_worker, safety_monitor) are started 
     * automatically via K_THREAD_DEFINE at boot.
     * Input events are handled via callback registered with INPUT_CALLBACK_DEFINE.
     */

    LOG_INF("=== ZBeam Ready ===");

    /* Main thread keep-alive */
    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}