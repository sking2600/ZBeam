/*
 * AUX Manager Header
 */

#ifndef AUX_MANAGER_H
#define AUX_MANAGER_H

#include <stdint.h>

typedef enum {
    AUX_OFF = 0,
    AUX_LOW,
    AUX_HIGH,
    AUX_BLINK,
    AUX_SINE, // New Breathing Mode
    AUX_MODE_COUNT
} aux_mode_t;

void aux_init(void);
void aux_set_mode(uint8_t mode);
void aux_cycle_mode(void);
void aux_update(void);

#endif
