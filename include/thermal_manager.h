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

#endif
