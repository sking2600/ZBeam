/**
 * @file nvs_manager.h
 * @brief Non-Volatile Storage Manager for ZBeam.
 * 
 * Handles persistence of FSM node configurations and system-wide settings.
 * Uses Zephyr's NVS subsystem on the 'storage_partition'.
 */

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <zephyr/kernel.h>
#include "fsm_engine.h"

#define NVS_SYSTEM_CONFIG_ID 0xFF // Special ID for system config

/* Feature Config IDs (0-100) */
#define NVS_ID_RAMP_FLOOR    1
#define NVS_ID_RAMP_CEILING  2
#define NVS_ID_MEM_BRIGHTNESS 3

#ifdef CONFIG_ZBEAM_NVS_ENABLED
/* Real Prototypes */
int nvs_init_fs(void);
void nvs_wipe_all(void);
int nvs_write_byte(uint16_t id, uint8_t value);
int nvs_read_byte(uint16_t id, uint8_t *value);
#else
/* Stubs for optional NVS */
static inline int nvs_init_fs(void) { return 0; }
static inline void nvs_wipe_all(void) {}
static inline int nvs_write_byte(uint16_t id, uint8_t value) { return 0; }
static inline int nvs_read_byte(uint16_t id, uint8_t *value) { return -1; }
#endif

#endif // NVS_MANAGER_H
