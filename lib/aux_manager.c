/*
 * AUX LED Manager (Stub)
 * Controls secondary RGB LEDs (e.g. SK6812).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "aux_manager.h"

LOG_MODULE_REGISTER(aux_manager, LOG_LEVEL_INF);

static enum aux_mode {
    AUX_OFF,
    AUX_LOW,
    AUX_HIGH,
    AUX_BLINK,
    AUX_VOLTAGE,
    AUX_RAINBOW,
} current_mode = AUX_OFF;

void aux_init(void)
{
    LOG_INF("AUX Manager Initialized (Stub)");
    current_mode = AUX_OFF;
}

void aux_set_mode(uint8_t mode)
{
    if (mode > AUX_RAINBOW) mode = AUX_OFF;
    current_mode = mode;
    LOG_INF("AUX Mode Set: %d", current_mode);
}

void aux_cycle_mode(void)
{
    current_mode++;
    if (current_mode > AUX_RAINBOW) current_mode = AUX_OFF;
    LOG_INF("AUX Mode Cycled to: %d", current_mode);
}

void aux_update(void)
{
    /* Driven by timer or event loop in real implementation */
    // LOG_DBG("AUX Update (Mode %d)", current_mode);
}
