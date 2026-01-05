/**
 * @file storage_manager.c
 * @brief Storage Manager Implementation (ZMS-based).
 *
 * Uses Zephyr Memory Storage (ZMS) for wear-leveled key-value storage.
 * ZMS is lighter than NVS and designed for flash longevity.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/zms.h>
#include "storage_manager.h"

LOG_MODULE_REGISTER(storage, LOG_LEVEL_INF);

/* Use the storage partition */
#define STORAGE_PARTITION       storage_partition
#define STORAGE_PARTITION_DEV   FIXED_PARTITION_DEVICE(STORAGE_PARTITION)
#define STORAGE_PARTITION_OFF   FIXED_PARTITION_OFFSET(STORAGE_PARTITION)

static struct zms_fs fs;
static bool initialized = false;

int storage_init(void)
{
    struct flash_pages_info info;
    int rc;

    fs.flash_device = STORAGE_PARTITION_DEV;
    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("Flash device not ready");
        return -ENODEV;
    }

    fs.offset = STORAGE_PARTITION_OFF;
    rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (rc) {
        LOG_ERR("Failed to get page info: %d", rc);
        return rc;
    }

    fs.sector_size = info.size;
    fs.sector_count = 2;  /* Minimal: 2 sectors for wear leveling */

    rc = zms_mount(&fs);
    if (rc) {
        LOG_ERR("ZMS mount failed: %d", rc);
        return rc;
    }

    initialized = true;
    LOG_INF("ZMS mounted: %d sectors x %d bytes", 
            fs.sector_count, fs.sector_size);
    return 0;
}

int storage_write(uint32_t id, const void *data, size_t len)
{
    if (!initialized) return -ENODEV;
    
    ssize_t ret = zms_write(&fs, id, data, len);
    if (ret < 0) {
        LOG_ERR("Write failed id=%d: %d", id, (int)ret);
    }
    return (int)ret;
}

int storage_read(uint32_t id, void *data, size_t len)
{
    if (!initialized) return -ENODEV;
    
    ssize_t ret = zms_read(&fs, id, data, len);
    if (ret < 0 && ret != -ENOENT) {
        LOG_ERR("Read failed id=%d: %d", id, (int)ret);
    }
    return (int)ret;
}

int storage_save_system_config(const struct system_config *config)
{
    return storage_write(STORAGE_ID_SYSTEM_CONFIG, config, sizeof(*config));
}

int storage_load_system_config(struct system_config *config)
{
    int ret = storage_read(STORAGE_ID_SYSTEM_CONFIG, config, sizeof(*config));
    return (ret > 0) ? 0 : -ENOENT;
}

void storage_wipe_all(void)
{
    if (!initialized) return;
    
    zms_delete(&fs, STORAGE_ID_SYSTEM_CONFIG);
    zms_delete(&fs, STORAGE_ID_PWM_LEVELS);
    zms_delete(&fs, STORAGE_ID_USER_PREFS);
    
    LOG_INF("Storage wiped (factory reset)");
}
