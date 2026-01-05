/*
 * Thermal Manager (Stub)
 * Simulates temperature changes based on brightness output.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "thermal_manager.h"



#include <zephyr/drivers/sensor.h>
#include "nvs_manager.h"

LOG_MODULE_REGISTER(thermal_manager, LOG_LEVEL_INF);

/* Device Tree Sensor Node */
#ifdef CONFIG_ZTEST
static const struct device *temp_dev = NULL;
#else
#if DT_NODE_HAS_STATUS(DT_NODELABEL(coretemp), okay)
static const struct device *temp_dev = DEVICE_DT_GET(DT_NODELABEL(coretemp));
#else
static const struct device *temp_dev = NULL;
#endif
#endif



/* PID State */
static int32_t integral_error = 0;
static int32_t prev_error = 0;
static int32_t current_temp_mc = 25000;
static uint8_t throttle_factor = 255; 

/* Config (Cached from NVS) */
static int32_t temp_limit_mc = CONFIG_ZBEAM_THERMAL_LIMIT_DEFAULT * 1000;
static int32_t temp_offset_mc = 0;
/* TODO: Use to skip NVS load on subsequent calls after first init */
static bool __maybe_unused calibration_loaded = false;

/* Test Mocking Hook */
#ifdef CONFIG_ZTEST
static int32_t mock_temp_mc = -1; // -1 = use sensor
void thermal_test_set_temp(int32_t temp_c) { mock_temp_mc = temp_c * 1000; }
#endif

void thermal_init(void)
{
    /* Load Config from NVS */
    uint8_t stored_limit = 0;
    if (nvs_read_byte(NVS_ID_THERMAL_LIMIT, &stored_limit) == 0) {
        temp_limit_mc = stored_limit * 1000;
    }
    
    /* Load calibration (offset stored as int8_t degrees C in NVS byte? 
       No, nvs_manager only supports u8? 
       Wait, nvs_manager currently exposes byte read/write. 
       Standard Anduril uses a calibration factor. 
       Let's assume for now we store an unsigned 'offset' or use a system ID 
       if we need signed.
       Workaround: Store offset + 100 to fit in u8. 0 = -100C. 100 = 0C. 
    */
    uint8_t stored_offset = 0;
    if (nvs_read_byte(NVS_ID_TEMP_CALIB_OFFSET, &stored_offset) == 0) {
        temp_offset_mc = ((int32_t)stored_offset - 100) * 1000;
    }

    if (temp_dev == NULL || !device_is_ready(temp_dev)) {
        LOG_WRN("Temperature sensor not ready, using fallback/stub");
    } else {
        LOG_INF("Thermal Manager Initialized. Limit: %d C, Offset: %d mC", 
                 temp_limit_mc/1000, temp_offset_mc);
    }
}

static int32_t read_sensor_temp(void)
{
#ifdef CONFIG_ZTEST
    if (mock_temp_mc != -1) return mock_temp_mc;
#endif

    if (temp_dev == NULL || !device_is_ready(temp_dev)) {
        return 25000; // Safe fallback
    }

    struct sensor_value val;
    int rc = sensor_sample_fetch(temp_dev);
    if (rc < 0) return 25000;

    rc = sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &val);
    if (rc < 0) return 25000;
    
    // Convert to millicelsius
    return (val.val1 * 1000) + (val.val2 / 1000) + temp_offset_mc;
}

void thermal_update(uint8_t current_brightness)
{
    current_temp_mc = read_sensor_temp();
    
    /* PID Logic */
    /* Setpoint: temp_limit_mc */
    /* Process Variable: current_temp_mc */
    
    int32_t error = current_temp_mc - temp_limit_mc;
    
    /* If we are below limit, we want to gently release throttle.
       If we are above limit, we want to increase throttle (reduce power).
       
       Throttle Factor: 255 = 100% Power, 0 = 0% Power.
       We want to DRIVE 'error' to 0.
       
       Let's formulate as: Output is Adjustment to Throttle Factor.
    */

    /* Simple P-only for safety start? No, requested PID. */
    
    // Constants x100
    int32_t Kp = CONFIG_ZBEAM_PID_KP;
    int32_t Ki = CONFIG_ZBEAM_PID_KI;
    int32_t Kd = CONFIG_ZBEAM_PID_KD;
    
    // If error < 0 (Cool), we can increase factor.
    // If error > 0 (Hot), we must decrease factor.
    
    // Integral Windup Guard
    if (abs(error) < 5000) { // Only integrate near setpoint (+/- 5C)
        integral_error += error;
    } else {
        integral_error = 0; // Reset if far away
    }
    
    // Clamp integral
    if (integral_error > 20000) integral_error = 20000;
    if (integral_error < -20000) integral_error = -20000;
    
    int32_t derivative = error - prev_error;
    prev_error = error;
    
    /* P term: Error * Kp. 
       If Error is 5000 (5C hot), Output should be negative (reduce throttle).
       So Term = - (Error * Kp)
    */
    
    int32_t p_term = (error * Kp) / 100;
    int32_t i_term = (integral_error * Ki) / 100;
    int32_t d_term = (derivative * Kd) / 100;
    
    int32_t adjustment = p_term + i_term + d_term;
    
    /* Application Logic */
    /* adjustment positive means -> Action needed to cool down -> Reduce Factor */
    /* Let's refine the factor adjustment.
       adjustment is in "milli-units" effectively due to K constants.
       Wait, if Kp is 50 (0.5), and error is 5000 (5C), adjustment = 2500.
       We want this to translate to a change in throttle_factor (0-255).
       Let's say we divide adjustment by 1000 to get a "steps per tick" value.
    */
    
    int32_t factor_change = adjustment / 1000;
    
    /* Minimum step size if far from target to avoid crawl */
    if (abs(error) > 5000 && factor_change == 0) {
        factor_change = (error > 0) ? 1 : -1;
    }

    /* Rate Limit: Don't drop more than 10 levels per 500ms and don't rise more than 2 levels 
       to prevent obvious flickering. */
    if (factor_change > 10) factor_change = 10;
    if (factor_change < -2) factor_change = -2;

    int32_t new_factor = (int32_t)throttle_factor - factor_change;
    
    /* Safety: If critically hot (> Limit + 10C), force faster drop regardless of PID */
    if (error > 10000 && new_factor > (throttle_factor - 5)) {
        new_factor = throttle_factor - 5;
    }
    
    /* Clamp */
    if (new_factor > 255) new_factor = 255;
    if (new_factor < 2)   new_factor = 2; // Emergency minimum (don't go to 0 as it looks like a crash)
    
    throttle_factor = (uint8_t)new_factor;
    
    if (abs(factor_change) > 0) {
        LOG_DBG("T:%d Limit:%d Err:%d Adj:%d Fac:%d", 
                current_temp_mc/1000, temp_limit_mc/1000, error, factor_change, throttle_factor);
    }
}

uint8_t thermal_apply_throttle(uint8_t requested_brightness)
{
    return (requested_brightness * throttle_factor) / 255;
}

int32_t thermal_get_temp_mc(void)
{
    return current_temp_mc;
}

void thermal_calibrate_current_temp(int32_t known_current_c)
{
    /* 1. Reset offset to 0 to get raw reading */
    temp_offset_mc = 0;
    int32_t raw_current = read_sensor_temp();
    
    /* 2. Calculate new offset */
    /* Target = Raw + Offset => Offset = Target - Raw */
    temp_offset_mc = (known_current_c * 1000) - raw_current;
    
    /* 3. Save to NVS (Offset + 100) */
    int32_t store_val = (temp_offset_mc / 1000) + 100;
    if (store_val < 0) store_val = 0;
    if (store_val > 255) store_val = 255;
    
    nvs_write_byte(NVS_ID_TEMP_CALIB_OFFSET, (uint8_t)store_val);
    
    LOG_INF("Calibrated. Raw: %d, Target: %d, New Offset: %d mC", 
            raw_current, known_current_c*1000, temp_offset_mc);
}

void thermal_set_limit(uint8_t limit_c)
{
    temp_limit_mc = limit_c * 1000;
    nvs_write_byte(NVS_ID_THERMAL_LIMIT, limit_c);
    LOG_INF("Thermal Limit Set: %d C", limit_c);
}

