/*
 * PWM Ramp API - Common Interface
 * 
 * Platform-agnostic API for perception-corrected LED brightness ramping.
 * Implementation varies by platform:
 *   - ESP32: LEDC hardware fade with table interpolation
 *   - CH32X035: DMA+Timer (future)
 *   - Generic: CPU loop fallback
 */

#ifndef PWM_RAMP_H
#define PWM_RAMP_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <stdint.h>

/**
 * @brief Initialize the PWM ramp subsystem
 * @param pwm_spec PWM device tree spec
 * @return 0 on success, negative errno on failure
 */
int pwm_ramp_init(const struct pwm_dt_spec *pwm_spec);

/**
 * @brief Set brightness level (0-255)
 * Uses the gamma-corrected ramp table internally.
 * @param brightness 0 (off) to 255 (full)
 */
void pwm_ramp_set_brightness(uint8_t brightness);

/**
 * @brief Start a ramp from current brightness to target
 * @param target_brightness Target brightness (0-255)
 * @param duration_ms Time to complete the ramp
 * @return 0 on success, negative errno on failure
 */
int pwm_ramp_start(uint8_t target_brightness, uint32_t duration_ms);

/**
 * @brief Check if a ramp is currently in progress
 * @return true if ramping, false if idle
 */
bool pwm_ramp_is_active(void);

/**
 * @brief Stop any active ramp and hold current brightness
 */
void pwm_ramp_stop(void);

/**
 * @brief Get the current brightness level
 * @return Current brightness (0-255)
 */
uint8_t pwm_ramp_get_brightness(void);

#endif /* PWM_RAMP_H */
