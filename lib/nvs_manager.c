#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include "nvs_manager.h"
#include "key_map.h" // Needs to know about all_nodes and NODE_COUNT

LOG_MODULE_REGISTER(NVS_Manager, LOG_LEVEL_INF);

static struct nvs_fs fs;

#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

int nvs_init_fs(void)
{
    struct flash_pages_info info;
    int rc = 0;

    fs.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("Flash device %s is not ready", fs.flash_device->name);
        return -ENODEV;
    }

    fs.offset = NVS_PARTITION_OFFSET;
    rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (rc) {
        LOG_ERR("Unable to get page info");
        return rc;
    }
    fs.sector_size = info.size;
    fs.sector_count = 4U; // 4 sectors of 4KB = 16KB. Partition is 16KB.

    rc = nvs_mount(&fs);
    if (rc) {
        LOG_ERR("Flash Init failed");
        return rc;
    }

    LOG_INF("NVS Mounted. Sector size: %d, Count: %d", fs.sector_size, fs.sector_count);
    return 0;
}

void nvs_save_node_config(uint8_t node_id, const struct node_config_data *config)
{
    int rc = nvs_write(&fs, node_id, config, sizeof(struct node_config_data));
    if (rc < 0) {
        LOG_ERR("Failed to write config for node %d (err: %d)", node_id, rc);
    } else {
        LOG_INF("Saved config for node %d", node_id);
    }
}

void nvs_load_runtime_config(void)
{
    struct node_config_data config;
    int rc;

    for (int i = 0; i < NODE_COUNT; i++) {
        rc = nvs_read(&fs, i, &config, sizeof(config));
        if (rc > 0) {
            LOG_INF("Loading config for Node %d", i);
            struct fsm_node *node = all_nodes[i];
            if (!node) continue; 
            
            node->timeout_ms = config.timeout_ms;
            
            // Link pointers based on saved IDs
            for (int k = 0; k < MAX_NAV_SLOTS; k++) {
                // Resolve Click Map
                uint8_t target = config.target_click_ids[k];
                if (target < NODE_COUNT) {
                    node->click_map[k] = all_nodes[target];
                } else {
                    node->click_map[k] = NULL;
                }

                // Resolve Hold Map
                target = config.target_hold_ids[k];
                if (target < NODE_COUNT) {
                    node->hold_map[k] = all_nodes[target];
                } else {
                    node->hold_map[k] = NULL;
                }
            }
        }
    }
}

void nvs_save_system_config(const struct system_config *config)
{
    int rc = nvs_write(&fs, NVS_SYSTEM_CONFIG_ID, config, sizeof(struct system_config));
    if (rc < 0) {
        LOG_ERR("Failed to save System Config (err: %d)", rc);
    } else {
        LOG_INF("Saved System Config");
    }
}

int nvs_load_system_config(struct system_config *config)
{
    int rc = nvs_read(&fs, NVS_SYSTEM_CONFIG_ID, config, sizeof(struct system_config));
    if (rc > 0) {
        LOG_INF("System Config Loaded");
        return 0; // Success
    }
    return -ENOENT; // Not found
}

void nvs_wipe_all(void)
{
    // Wipe all node configs
    for (int i = 0; i < NODE_COUNT; i++) {
        nvs_delete(&fs, i);
    }
    // Wipe system config
    nvs_delete(&fs, NVS_SYSTEM_CONFIG_ID);
    
    LOG_INF("NVS Wiped (Factory Reset)");
}
