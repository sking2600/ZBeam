/*
 * AUX Manager Header
 */

#ifndef AUX_MANAGER_H
#define AUX_MANAGER_H

#include <stdint.h>

void aux_init(void);
void aux_set_mode(uint8_t mode);
void aux_cycle_mode(void);
void aux_update(void);

#endif
