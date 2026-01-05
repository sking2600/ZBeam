/*
 * Thermal Manager (Stub)
 * Simulates temperature changes based on brightness output.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "thermal_manager.h"

LOG_MODULE_REGISTER(thermal_manager, LOG_LEVEL_INF);

static int32_t current_temp_mc = 25000; /* 25.000 C */
static uint8_t throttle_factor = 255;    /* 255 = No throttle */
static bool is_turbo = false;

/* Config */
#define THERMAL_CEILING_MC 50000  /* 50.0 C */
#define HEATING_RATE_MC    500    /* +0.5 C per tick at Turbo */
#define COOLING_RATE_MC    100    /* -0.1 C per tick when Off */

void thermal_init(void)
{
    current_temp_mc = 25000;
    throttle_factor = 255;
    LOG_INF("Thermal Manager Initialized (Stub)");
}

void thermal_update(uint8_t current_brightness)
{
    /* Simulate Heating/Cooling */
    if (current_brightness > 150) {
        current_temp_mc += HEATING_RATE_MC;
    } else {
        if (current_temp_mc > 25000) {
            current_temp_mc -= COOLING_RATE_MC;
        }
    }
    
    /* PID Logic (Simple P-controller for stub) */
    if (current_temp_mc > THERMAL_CEILING_MC) {
        if (throttle_factor > 10) throttle_factor--;
        LOG_WRN("Thermal Throttling: %dC factor=%d", current_temp_mc/1000, throttle_factor);
    } else {
        if (throttle_factor < 255) throttle_factor++;
    }
    
    // LOG_DBG("Temp: %d mC, Factor: %d", current_temp_mc, throttle_factor);
}

uint8_t thermal_apply_throttle(uint8_t requested_brightness)
{
    return (requested_brightness * throttle_factor) / 255;
}

int32_t thermal_get_temp_mc(void)
{
    return current_temp_mc;
}
