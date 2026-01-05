/**
 * @file storage_manager.h
 * @brief Storage Manager API (ZMS-based).
 *
 * Provides wear-leveled key-value storage for user settings.
 * Uses Zephyr Memory Storage (ZMS) which is lighter than NVS.
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <zephyr/kernel.h>
#include <stdint.h>

/* Storage IDs */
#define STORAGE_ID_SYSTEM_CONFIG    0x01
#define STORAGE_ID_PWM_LEVELS       0x02
#define STORAGE_ID_USER_PREFS       0x03

/**
 * @brief System configuration settings.
 */
struct system_config {
    uint32_t click_timeout_ms;
    uint32_t hold_duration_ms;
    uint8_t last_brightness;
    uint8_t reserved[3];  /* Alignment padding */
};

/**
 * @brief Initialize storage subsystem.
 * @return 0 on success, negative errno on failure.
 */
int storage_init(void);

/**
 * @brief Save system configuration.
 * @param config Pointer to config struct.
 * @return 0 on success, negative errno on failure.
 */
int storage_save_system_config(const struct system_config *config);

/**
 * @brief Load system configuration.
 * @param config Pointer to struct to populate.
 * @return 0 on success, -ENOENT if not found.
 */
int storage_load_system_config(struct system_config *config);

/**
 * @brief Save raw data by ID.
 * @param id Storage ID.
 * @param data Pointer to data.
 * @param len Length of data.
 * @return Bytes written on success, negative errno on failure.
 */
int storage_write(uint32_t id, const void *data, size_t len);

/**
 * @brief Read raw data by ID.
 * @param id Storage ID.
 * @param data Buffer to read into.
 * @param len Maximum length to read.
 * @return Bytes read on success, negative errno on failure.
 */
int storage_read(uint32_t id, void *data, size_t len);

/**
 * @brief Wipe all stored data (factory reset).
 */
void storage_wipe_all(void);

#endif /* STORAGE_MANAGER_H */
