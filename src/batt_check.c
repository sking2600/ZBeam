/**
 * @file batt_check.c
 * @brief Battery monitor implementation using DeviceTree for ADC configuration.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "batt_check.h"
#include "nvs_manager.h"

LOG_MODULE_REGISTER(batt_check, LOG_LEVEL_INF);

/* Get ADC Spec from DeviceTree (zephyr,user -> io-channels) */
static const struct adc_dt_spec adc_chan = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
static uint8_t batt_cal_offset = 100; // Cached offset (0.1V units, 100=0V)

void batt_init(void)
{
    if (!adc_is_ready_dt(&adc_chan)) {
        LOG_ERR("ADC device not ready");
        return;
    }

    int err = adc_channel_setup_dt(&adc_chan);
    if (err) {
        LOG_ERR("ADC channel setup failed: %d", err);
    }
    
    // Load calibration
    nvs_read_byte(NVS_ID_BATT_CALIB_OFFSET, &batt_cal_offset);
}

uint16_t batt_read_voltage_mv(void)
{
    if (!adc_is_ready_dt(&adc_chan)) {
        return 3800; // Fallback
    }

    uint16_t buf;
    struct adc_sequence seq = {
        .buffer      = &buf,
        .buffer_size = sizeof(buf),
    };

    // Initialize sequence from DT (resolution, channels, etc.)
    adc_sequence_init_dt(&adc_chan, &seq);

    int err = adc_read_dt(&adc_chan, &seq);
    if (err) {
        LOG_ERR("ADC read failed: %d", err);
        return 3800;
    }

    int32_t val_mv = buf;
    
    /* Convert raw to mV using driver internal Vref logic */
    adc_raw_to_millivolts_dt(&adc_chan, &val_mv);

    /* Apply Divider Factor from DeviceTree overlay */
    /* V_batt = V_adc * DividerFactor / 1000 */
    uint32_t divider = DT_PROP(DT_PATH(zephyr_user), zbeam_battery_divider_factor);
    uint32_t batt_mv = (uint32_t)val_mv * divider / 1000;

    /* Apply Calibration Offset from NVS (Cached) */
    /* Stored as byte. 100 = 0V offset. Units of 0.1V (100mV). */
    int32_t offset_mv = ((int32_t)batt_cal_offset - 100) * 100;
    int32_t final_mv = (int32_t)batt_mv + offset_mv;

    if (final_mv < 0) final_mv = 0;
    
    return (uint16_t)final_mv;
}

void batt_calculate_blinks(uint16_t mv, uint8_t *major, uint8_t *minor)
{
    if (mv < 2500) mv = 2500; 
    if (mv > 4500) mv = 4500; 
    
    uint16_t rounded = (mv + 50) / 100; 
    
    *major = rounded / 10;      
    *minor = rounded % 10;      
}

void batt_calibrate_voltage(uint16_t actual_mv)
{
    // 1. Get raw reading (temporarily assume 0 offset)
    int32_t current_offset_mv = ((int32_t)batt_cal_offset - 100) * 100;
    int32_t measured_calibrated = batt_read_voltage_mv();
    int32_t measured_raw = measured_calibrated - current_offset_mv;
    
    // 2. Calculate new offset needed
    int32_t needed_offset_mv = (int32_t)actual_mv - measured_raw;
    
    // 3. Convert to storage units (100mV)
    // Round to nearest 100mV
    int32_t units = (needed_offset_mv + 50) / 100;
    int32_t stored = units + 100;
    
    // 4. Clamp and Store
    if (stored < 0) stored = 0;
    if (stored > 255) stored = 255;
    
    batt_cal_offset = (uint8_t)stored;
    nvs_write_byte(NVS_ID_BATT_CALIB_OFFSET, batt_cal_offset);
    
    LOG_INF("Batt Calibrated. Act: %d, Raw: %d, Off: %d", actual_mv, measured_raw, units*100);
}
