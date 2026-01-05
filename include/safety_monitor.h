/**
 * @file safety_monitor.h
 * @brief Safety Monitor API.
 *
 * High-priority thread that monitors temperature, current, and voltage.
 * Can trigger emergency shutdown if limits exceeded.
 */

#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Safety fault codes.
 */
enum safety_fault {
    SAFETY_OK = 0,
    SAFETY_FAULT_OVERCURRENT,
    SAFETY_FAULT_OVERTEMP,
    SAFETY_FAULT_UNDERVOLTAGE,
    SAFETY_FAULT_OVERVOLTAGE,
};

/**
 * @brief Sensor readings structure.
 */
typedef struct {
    int16_t temperature_c10;  /**< Temperature in 0.1°C (250 = 25.0°C) */
    uint16_t current_ma;      /**< Current in milliamps */
    uint16_t voltage_mv;      /**< Voltage in millivolts */
} safety_readings_t;

/**
 * @brief Get the current fault status.
 * @return Current fault code, or SAFETY_OK if no fault.
 */
enum safety_fault safety_get_status(void);

/**
 * @brief Get the last sensor readings.
 * @param readings Pointer to structure to populate.
 */
void safety_get_readings(safety_readings_t *readings);

/**
 * @brief Trigger manual emergency shutdown.
 *
 * Posts MSG_SAFETY_SHUTDOWN to FSM worker.
 */
void safety_emergency_shutdown(void);

/**
 * @brief Check if system is in shutdown state.
 * @return true if emergency shutdown has been triggered.
 */
bool safety_is_shutdown(void);

/**
 * @brief Mock readings for testing.
 *
 * Set to non-NULL to override real sensor readings.
 * Set to NULL to use real hardware (stub returns safe defaults).
 */
extern safety_readings_t *safety_mock_readings;

#endif /* SAFETY_MONITOR_H */
