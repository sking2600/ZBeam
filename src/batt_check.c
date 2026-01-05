/**
 * @file batt_check.c
 * @brief Battery monitor implementation with stub capability.
 */

#include "batt_check.h"
#include <zephyr/kernel.h> // For __weak attribute if needed, though usually compiler built-in

/* Weak implementation of voltage read - returns 3.8V dummy */
__attribute__((weak)) uint16_t batt_read_voltage_mv(void)
{
    /* Placeholder: Return 3.8V */
    return 3800;
}

void batt_calculate_blinks(uint16_t mv, uint8_t *major, uint8_t *minor)
{
    /* 
     * Logic:
     * < 3.0V -> 2.9V behavior
     * Display V - 3.0? Or absolute? 
     * Anduril typically does: 3.8V -> 3 blinks, 8 blinks.
     * 12.5V -> 1 blink, 2 blinks, 5 blinks? No, usually just X.Y volts.
     * We'll assume simple X.Y display.
     */
    
    if (mv < 2500) mv = 2500; /* Clamp min */
    if (mv > 4500) mv = 4500; /* Clamp max */
    
    /* Round to nearest 100mV */
    uint16_t rounded = (mv + 50) / 100; // e.g. 3855 -> 3905 / 100 = 39 (3.9V)
    
    *major = rounded / 10;      // 39 / 10 = 3
    *minor = rounded % 10;      // 39 % 10 = 9
    
    /* Special Case: If minor is 0, should we blink 10 times or long blink?
       Anduril usually does 0 blinks (long pause) or special fast flicker?
       Let's stick to 0 = 0 blinks (just major, pause, silence).
    */
}
