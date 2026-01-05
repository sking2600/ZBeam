/*
 * Power Manager (Stub)
 * Handles transitions to low-power states.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "pm_manager.h"

LOG_MODULE_REGISTER(pm_manager, LOG_LEVEL_INF);

static bool is_suspended = false;

void pm_init(void)
{
    LOG_INF("PM Manager Initialized (Stub)");
    is_suspended = false;
}

void pm_suspend(void)
{
    if (is_suspended) return;
    
    LOG_INF("Entering Deep Sleep State (Simulated)...");
    /* In real hardware:
     * - sys_set_power_state(SLEEP);
     * - Disable peripherals (Timers, ADC)
     */
    is_suspended = true;
}

void pm_resume(void)
{
    if (!is_suspended) return;
    
    LOG_INF("Waking up from Sleep...");
    /* Restore peripherals */
    is_suspended = false;
}

bool pm_is_suspended(void)
{
    return is_suspended;
}
