#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include "nvs_manager.h"

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
        LOG_WRN("NVS Mount failed, attempting wipe mechanism if needed (Error: %d)", rc);
        /* If generic mount fails, usually implies corruption or uninitialized */
        /* Zephyr NVS usually requires cleaner handling, but for now we report error */
        return rc;
    }

    LOG_INF("NVS Mounted. Sector size: %d, Count: %d", fs.sector_size, fs.sector_count);
    return 0;
}

int nvs_write_byte(uint16_t id, uint8_t value)
{
    int rc = nvs_write(&fs, id, &value, sizeof(value));
    if (rc < 0) {
        LOG_ERR("NVS Write ID %d failed: %d", id, rc);
        return rc;
    }
    LOG_DBG("NVS Saved ID %d = %d", id, value);
    return 0;
}

int nvs_read_byte(uint16_t id, uint8_t *value)
{
    int rc = nvs_read(&fs, id, value, sizeof(uint8_t));
    if (rc > 0) {
         /* rc is bytes read */
         return 0;
    }
    if (rc == -ENOENT) {
        return -ENOENT;
    }
    LOG_ERR("NVS Read ID %d failed: %d", id, rc);
    return rc;
}

void nvs_wipe_all(void)
{
    /* Brute force wipe common IDs. 
       Zephyr NVS doesn't have a "format" API easily accessible without re-init,
       so we delete known keys or re-mount with empty.
       Actually, `nvs_clear` is not standard API, only `nvs_delete`.
    */
    for (uint16_t i = 0; i < 20; i++) {
        nvs_delete(&fs, i);
    }
    LOG_INF("NVS Wiped (IDs 0-20)");
}
