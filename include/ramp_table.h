/*
 * PWM Ramp Table - Modular Selector
 * 
 * Includes the appropriate ramp/sine table based on Kconfig settings.
 * Tables include gamma value in naming for color-specific optimization.
 */

#ifndef RAMP_TABLE_H
#define RAMP_TABLE_H

#include <stdint.h>

/* Default: Include 13-bit gamma 2.8 tables (blue LED optimized) */
#if defined(CONFIG_PWM_RAMP_RESOLUTION_13BIT) || !defined(CONFIG_PWM_RAMP_RESOLUTION)

/* Linear ramp table */
#include "ramp_table_13bit_g28.h"
#define pwm_ramp_table      pwm_ramp_table_13bit_g28
#define RAMP_TABLE_SIZE     PWM_RAMP_13BIT_G28_SIZE
#define RAMP_TABLE_MAX_DUTY PWM_RAMP_13BIT_G28_MAX_DUTY

/* Sine wave table */
#include "ramp_sine_13bit_g28.h"
#define pwm_sine_table      pwm_sine_table_13bit_g28
#define SINE_TABLE_SIZE     PWM_SINE_13BIT_G28_SIZE
#define SINE_TABLE_MAX_DUTY PWM_SINE_13BIT_G28_MAX_DUTY

#elif defined(CONFIG_PWM_RAMP_RESOLUTION_10BIT)
/* TODO: Generate 10-bit tables when needed */
#error "10-bit tables not yet generated. Run: python3 scripts/generate_ramp_table.py --bits 10 --gamma 2.8"

#elif defined(CONFIG_PWM_RAMP_RESOLUTION_8BIT)
/* TODO: Generate 8-bit tables when needed */
#error "8-bit tables not yet generated. Run: python3 scripts/generate_ramp_table.py --bits 8 --gamma 2.8"

#endif

#endif /* RAMP_TABLE_H */
