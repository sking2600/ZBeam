/**
 * @file safety_monitor.c
 * @brief Safety Monitor Thread Implementation.
 *
 * High-priority thread that checks temperature/current/voltage sensors
 * and triggers emergency shutdown if limits are exceeded.
 *
 * Currently uses stub sensor readings - real implementation will use
 * ADC/I2C to read actual sensors.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "safety_monitor.h"
#include "fsm_worker.h"
#include "zbeam_msg.h"

LOG_MODULE_REGISTER(safety_monitor, LOG_LEVEL_INF);

/* Thread priority: highest cooperative priority (-1) or preemptive (negative) */
#define SAFETY_THREAD_PRIORITY  K_PRIO_COOP(0)

/* Safety thresholds from Kconfig */
#define TEMP_WARN_THRESHOLD_C10     CONFIG_ZBEAM_TEMP_WARN_C10
#define TEMP_SHUTDOWN_THRESHOLD_C10 CONFIG_ZBEAM_TEMP_SHUTDOWN_C10
#define CURRENT_SHUTDOWN_MA         CONFIG_ZBEAM_CURRENT_MAX_MA
#define VOLTAGE_MIN_MV              CONFIG_ZBEAM_VOLTAGE_MIN_MV
#define VOLTAGE_MAX_MV              CONFIG_ZBEAM_VOLTAGE_MAX_MV

/* Calculate check interval from configured rate */
#define CHECK_INTERVAL_MS  (1000 / CONFIG_ZBEAM_SAFETY_RATE_HZ)

/* State */
static enum safety_fault current_fault = SAFETY_OK;
static safety_readings_t last_readings;
static bool shutdown_triggered = false;

/* Mock readings pointer for testing */
safety_readings_t *safety_mock_readings = NULL;

/**
 * @brief Read sensors (stub implementation).
 *
 * In real hardware, this would read ADC/I2C sensors.
 * Returns safe defaults or mock values if set.
 */
static safety_readings_t read_sensors(void)
{
    if (safety_mock_readings != NULL) {
        return *safety_mock_readings;
    }

    /* STUB: Safe default values */
    return (safety_readings_t){
        .temperature_c10 = 250,   /* 25.0°C */
        .current_ma = 500,        /* 0.5A */
        .voltage_mv = 3700,       /* 3.7V nominal */
    };
}

/**
 * @brief Safety monitor thread entry point.
 */
static void safety_thread_entry(void *p1, void *p2, void *p3)
{
    LOG_INF("Safety monitor started (rate=%dHz)", CONFIG_ZBEAM_SAFETY_RATE_HZ);

    while (1) {
        last_readings = read_sensors();

        /* Check for shutdown conditions */
        enum safety_fault fault = SAFETY_OK;

        if (last_readings.current_ma > CURRENT_SHUTDOWN_MA) {
            fault = SAFETY_FAULT_OVERCURRENT;
            LOG_ERR("OVERCURRENT: %d mA (limit: %d)", 
                    last_readings.current_ma, CURRENT_SHUTDOWN_MA);
        }
        else if (last_readings.temperature_c10 > TEMP_SHUTDOWN_THRESHOLD_C10) {
            fault = SAFETY_FAULT_OVERTEMP;
            LOG_ERR("OVERTEMP: %d.%d°C (limit: %d.%d)", 
                    last_readings.temperature_c10 / 10,
                    last_readings.temperature_c10 % 10,
                    TEMP_SHUTDOWN_THRESHOLD_C10 / 10,
                    TEMP_SHUTDOWN_THRESHOLD_C10 % 10);
        }
        else if (last_readings.voltage_mv < VOLTAGE_MIN_MV) {
            fault = SAFETY_FAULT_UNDERVOLTAGE;
            LOG_ERR("UNDERVOLTAGE: %d mV (min: %d)", 
                    last_readings.voltage_mv, VOLTAGE_MIN_MV);
        }
        else if (last_readings.voltage_mv > VOLTAGE_MAX_MV) {
            fault = SAFETY_FAULT_OVERVOLTAGE;
            LOG_ERR("OVERVOLTAGE: %d mV (max: %d)", 
                    last_readings.voltage_mv, VOLTAGE_MAX_MV);
        }

        /* Trigger shutdown on fault */
        if (fault != SAFETY_OK && !shutdown_triggered) {
            current_fault = fault;
            safety_emergency_shutdown();
        }
        /* Check for thermal warning (soft limit) */
        else if (last_readings.temperature_c10 > TEMP_WARN_THRESHOLD_C10 &&
                 current_fault == SAFETY_OK) {
            struct zbeam_msg msg = {
                .type = MSG_SAFETY_THERMAL_WARN,
                .severity = (last_readings.temperature_c10 - TEMP_WARN_THRESHOLD_C10) / 10,
            };
            fsm_worker_post_msg(&msg);
        }
        /* Clear fault if conditions normalize */
        else if (fault == SAFETY_OK) {
            current_fault = SAFETY_OK;
        }

        k_msleep(CHECK_INTERVAL_MS);
    }
}

void safety_emergency_shutdown(void)
{
    if (shutdown_triggered) {
        return;  /* Already triggered */
    }

    LOG_WRN("!!! EMERGENCY SHUTDOWN !!!");
    shutdown_triggered = true;

    struct zbeam_msg msg = {
        .type = MSG_SAFETY_SHUTDOWN,
        .severity = 255,
    };
    fsm_worker_post_msg(&msg);
}

enum safety_fault safety_get_status(void)
{
    return current_fault;
}

void safety_get_readings(safety_readings_t *readings)
{
    if (readings != NULL) {
        *readings = last_readings;
    }
}

bool safety_is_shutdown(void)
{
    return shutdown_triggered;
}

/* Define thread statically with highest priority */
K_THREAD_DEFINE(safety_thread_tid,
                CONFIG_ZBEAM_SAFETY_WORKER_STACK_SIZE,
                safety_thread_entry, NULL, NULL, NULL,
                SAFETY_THREAD_PRIORITY, 0, 0);
