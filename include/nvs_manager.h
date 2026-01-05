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

/**
 * @brief Global system configuration settings.
 */
struct system_config {
    uint32_t click_timeout_ms;   ///< Click detection timeout.
    uint32_t hold_duration_ms;   ///< Hold detection duration.
    uint16_t monitor_key_code;   ///< Input event code to monitor.
};

/**
 * @brief Initialize the NVS file system.
 * @return 0 on success, negative error code otherwise.
 */
int nvs_init_fs(void);

/**
 * @brief Save a single node's configuration to NVS.
 * @param node_id The unique enum ID of the node.
 * @param config Pointer to the configuration data.
 */
void nvs_save_node_config(uint8_t node_id, const struct node_config_data *config);

/**
 * @brief Load all node configurations from NVS and update runtime pointers.
 * 
 * Iterates through all nodes. If NVS data exists, updates the node's
 * timeout and navigation maps (click_map/hold_map).
 */
void nvs_load_runtime_config(void);

// System Config & Reset

/**
 * @brief Save system-wide settings.
 * @param config Pointer to system config struct.
 */
void nvs_save_system_config(const struct system_config *config);

/**
 * @brief Load system-wide settings.
 * @param config Pointer to struct to populate.
 * @return 0 on success, -ENOENT if not found.
 */
int nvs_load_system_config(struct system_config *config);

/**
 * @brief Wipe all data from NVS (Factory Reset).
 * 
 * Deletes all node configs and system config.
 */
void nvs_wipe_all(void);

#endif // NVS_MANAGER_H
