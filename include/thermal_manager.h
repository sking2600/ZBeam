/*
 * Thermal Manager Header
 */

#ifndef THERMAL_MANAGER_H
#define THERMAL_MANAGER_H

#include <stdint.h>

void thermal_init(void);
void thermal_update(uint8_t current_brightness);
uint8_t thermal_apply_throttle(uint8_t requested_brightness);
int32_t thermal_get_temp_mc(void);

/**
 * @brief Calibrate the thermal sensor.
 * 
 * Sets the offset such that the current sensor reading matches the provided
 * 'known_current_c' temperature. Saves to NVS.
 * 
 * @param known_current_c Current ambient temperature in degrees C.
 */
void thermal_calibrate_current_temp(int32_t known_current_c);
void thermal_set_limit(uint8_t limit_c);


#endif
