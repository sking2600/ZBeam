#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "nvs_manager.h"

LOG_MODULE_REGISTER(nvs_test, LOG_LEVEL_INF);

/* Mock key_map stuff referenced by nvs_manager if any kept */
struct fsm_node;
struct fsm_node *all_nodes[1]; 

/* Setup (once) */
static void *setup(void)
{
    int rc = nvs_init_fs();
    zassert_equal(rc, 0, "NVS Init failed: %d", rc);
    return NULL;
}

/* Before (per test) */
static void before(void *data)
{
    ARG_UNUSED(data);
    nvs_wipe_all();
}

ZTEST_SUITE(nvs_suite, NULL, setup, before, NULL, NULL);

/* Tests */

ZTEST(nvs_suite, test_rw_byte)
{
    int rc;
    uint8_t val;
    
    /* Read non-existent */
    rc = nvs_read_byte(1, &val);
    zassert_equal(rc, -ENOENT, "Should fail reading non-existent ID");
    
    /* Write value */
    rc = nvs_write_byte(1, 42);
    zassert_equal(rc, 0, "Write failed");
    
    /* Read back */
    rc = nvs_read_byte(1, &val);
    zassert_equal(rc, 0, "Read failed");
    zassert_equal(val, 42, "Value mismatch");
    
    /* Update value */
    rc = nvs_write_byte(1, 100);
    zassert_equal(rc, 0, "Update failed");
    
    /* Read back updated */
    rc = nvs_read_byte(1, &val);
    zassert_equal(val, 100, "Update value mismatch");
}

ZTEST(nvs_suite, test_multiple_ids)
{
    nvs_write_byte(1, 10);
    nvs_write_byte(2, 20);
    nvs_write_byte(3, 30);
    
    uint8_t v1, v2, v3;
    nvs_read_byte(1, &v1);
    nvs_read_byte(2, &v2);
    nvs_read_byte(3, &v3);
    
    zassert_equal(v1, 10, "ID 1 mismatch");
    zassert_equal(v2, 20, "ID 2 mismatch");
    zassert_equal(v3, 30, "ID 3 mismatch");
}
