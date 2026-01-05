#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
// #include <zephyr/usb/usb_device.h> // Not needed for C3 USB-Serial-JTAG

#include "ui_actions.h"
#include "fsm_engine.h"
#include "multi_tap_input.h"
#include "pwm_ramp.h"
#include "aux_manager.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* PWM LED from device tree */
static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET_OR(DT_ALIAS(pwm_led0), {0});

/* GPIO Matrix workaround for ESP32-C3 */
#if defined(CONFIG_SOC_SERIES_ESP32C3)
#define GPIO_BASE               0x60004000
#define GPIO_ENABLE_REG         (GPIO_BASE + 0x0020)
#define GPIO7_FUNC_OUT_SEL_REG  (GPIO_BASE + 0x0554 + 7 * 4)
#define LEDC_LS_SIG_OUT0        45

static void configure_gpio_for_ledc(void)
{
    uint32_t sel = sys_read32(GPIO7_FUNC_OUT_SEL_REG);
    sel = (sel & ~0xFF) | LEDC_LS_SIG_OUT0;
    sel |= (1 << 9);
    sys_write32(sel, GPIO7_FUNC_OUT_SEL_REG);
    
    uint32_t enable = sys_read32(GPIO_ENABLE_REG);
    enable |= (1 << 7);
    sys_write32(enable, GPIO_ENABLE_REG);
}
#else

static void configure_gpio_for_ledc(void) { }
#endif



int main(void)
{
    /* Boot Beacon: Blink Onboard LED (GPIO8) manually before anything else */
    /* This proves we are running code */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(aux_controller), okay)
    const struct gpio_dt_spec aux_test = GPIO_DT_SPEC_GET(DT_NODELABEL(aux_controller), gpios);
    
    if (gpio_is_ready_dt(&aux_test)) {
        gpio_pin_configure_dt(&aux_test, GPIO_OUTPUT_ACTIVE);
        k_msleep(100);
        gpio_pin_set_dt(&aux_test, 0);
        k_msleep(100);
        gpio_pin_set_dt(&aux_test, 1);
        k_msleep(100);
        gpio_pin_set_dt(&aux_test, 0);
    } 
#endif

    // Give time for USB Serial JTAG to connect
    k_sleep(K_SECONDS(1));


    LOG_INF("=== ZBeam Starting (USB-Serial-JTAG) ===");







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

    /* 3. Initialize UI Support (parses Kconfig PWM values, creates nodes) */
    ui_init();
    LOG_INF("UI Actions initialized");

    /* 4. Initialize FSM with start node */
    fsm_init(get_start_node());
    LOG_INF("FSM initialized");

    /* 5. Initialize multi-tap input */
    multi_tap_input_init();
    
    /* Verify gpio_keys driver is ready */
    const struct device *input_dev = DEVICE_DT_GET_ONE(gpio_keys);
    if (!device_is_ready(input_dev)) {
        LOG_ERR("Input driver (gpio_keys) NOT READY!");
    } else {
        LOG_INF("Input driver (gpio_keys) is ready.");
    }

    /* 6. Initialize AUX LED manager (Handled by ui_init) */
    // aux_init();


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