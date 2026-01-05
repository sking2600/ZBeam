/**
 * @file batt_check.h
 * @brief Battery Check Logic and Stub.
 */

#ifndef BATT_CHECK_H
#define BATT_CHECK_H

#include <stdint.h>

/**
 * @brief Read the current battery voltage in millivolts.
 * 
 * This function is defined as WEAK in the source, allowing tests
 * to override it with a mock.
 * 
 * @return Voltage in mV (e.g. 3800 for 3.8V).
 */
uint16_t batt_read_voltage_mv(void);

/**
 * @brief Calculate the blink sequence for a given voltage.
 * 
 * e.g. 3.8V -> 3 blinks, pause, 8 blinks.
 * 
 * @param mv Voltage in millivolts.
 * @param major Pointer to store major blinks (integer part).
 * @param minor Pointer to store minor blinks (decimal part).
 */
void batt_calculate_blinks(uint16_t mv, uint8_t *major, uint8_t *minor);

#endif /* BATT_CHECK_H */
