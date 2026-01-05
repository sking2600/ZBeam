#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H

#include <stdint.h>
#include <zephyr/drivers/pwm.h>

typedef enum {
    CHANNEL_MODE_SINGLE = 0,    /* One emitter only */
    CHANNEL_MODE_50_50,         /* Even mix */
    CHANNEL_MODE_COLD,          /* Only emitter 0 */
    CHANNEL_MODE_WARM,          /* Only emitter 1 */
    CHANNEL_MODE_AUTO_TINT,     /* Shifts CW->WW with brightness */
    CHANNEL_MODE_SEQUENTIAL,    /* Emitters turn on in sequence (Continuum) */
    CHANNEL_MODE_COUNT
} channel_mode_t;

/**
 * @brief Initialize emitters from DeviceTree
 */
void channel_init(void);

/**
 * @brief Apply brightness level to all mapped channels based on current mode
 * @param master_level 0-255 brightness level
 */
void channel_apply_mix(uint8_t master_level);

/**
 * @brief Switch to next available channel mode
 */
void channel_cycle_mode(void);

#endif
