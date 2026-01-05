/**
 * @file key_map.c
 * @brief Anduril-style FSM Graph Implementation.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/reboot.h>
#include <stdlib.h>
#include <string.h>

#include "fsm_engine.h"
#include "batt_check.h"
#include "nvs_manager.h"
#include "thermal_manager.h"
#include "pm_manager.h"
#include "aux_manager.h"
#include "key_map.h"

LOG_MODULE_REGISTER(KeyMap, LOG_LEVEL_INF);

/* ========== Hardware Interface ========== */

static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET_OR(DT_ALIAS(pwm_led0), {0});

/* ========== Brightness State ========== */

static uint8_t current_brightness = 0;
static uint8_t memorized_brightness = 128;

static uint8_t brightness_floor = CONFIG_ZBEAM_BRIGHTNESS_FLOOR;
static uint8_t brightness_ceiling = CONFIG_ZBEAM_BRIGHTNESS_CEILING;

#define BRIGHTNESS_FLOOR   brightness_floor
#define BRIGHTNESS_CEILING brightness_ceiling

static void update_led_hardware(uint8_t level)
{
    if (!device_is_ready(pwm_led.dev)) {
        LOG_ERR("PWM device not ready");
        return;
    }
    
    /* Apply Thermal Throttle */
    uint8_t throttled = thermal_apply_throttle(level);
    
    uint32_t pulse = (pwm_led.period * throttled) / 255;
    pwm_set_pulse_dt(&pwm_led, pulse);
}

/* Thermal Timer */
static struct k_timer thermal_timer;
static void thermal_timer_handler(struct k_timer *timer)
{
    thermal_update(current_brightness);
    /* Re-apply brightness if throttled */
    update_led_hardware(current_brightness);
}

/* ... (keep existing ramping state) ... */

/* ========== Config Menu State ========== */

/* ... (keep existing structs) ... */


/* ... */


/* Ramping state */
static struct k_timer ramp_timer;
static int ramp_direction = 0;
static bool ramp_active = false;
#define RAMP_STEP_SIZE 1

/* ========== Generalized Ramping ========== */

enum control_param {
    PARAM_BRIGHTNESS,
    PARAM_FREQUENCY,
};

static enum control_param active_param = PARAM_BRIGHTNESS;

/* Strobe State */
static struct k_timer strobe_timer;
static uint8_t strobe_frequency = 12;
static bool strobe_on = false;
static bool party_mode = false;

/* Curve: 0=MIN_FREQ, 255=MAX_FREQ */
static uint32_t get_strobe_delay_ms(uint8_t freq_idx)
{
    uint32_t delay_max = 1000 / CONFIG_ZBEAM_STROBE_MIN_FREQ;
    uint32_t delay_min = 1000 / CONFIG_ZBEAM_STROBE_MAX_FREQ;
    uint32_t range = delay_max - delay_min;
    return delay_max - ((range * freq_idx) / 255);
}

/* ========== Config Menu Types ========== */

struct config_item {
    uint8_t nvs_id;
    uint8_t blinks;
    void (*apply_cb)(uint8_t value);
};

/* ========== Forward Declarations ========== */
static void stop_ramping(void);
static void start_ramping(int direction);
static void stop_strobe(void);
static void start_strobe(bool is_party);
static void start_config_menu(const struct config_item *items, uint8_t count);

/* ========== Config Menu State ========== */

#ifdef CONFIG_ZBEAM_NVS_ENABLED
#define CFG_ID_FLOOR   NVS_ID_RAMP_FLOOR
#define CFG_ID_CEILING NVS_ID_RAMP_CEILING
#else
#define CFG_ID_FLOOR   0
#define CFG_ID_CEILING 0
#endif

/* Callbacks for specific items */
static void cb_set_floor(uint8_t val)   { brightness_floor = val; }
static void cb_set_ceiling(uint8_t val) { brightness_ceiling = val; }

static const struct config_item ramp_config_items[] = {
    { .nvs_id = CFG_ID_FLOOR,   .blinks = 1, .apply_cb = cb_set_floor },
    { .nvs_id = CFG_ID_CEILING, .blinks = 2, .apply_cb = cb_set_ceiling },
};

static struct {
    const struct config_item *items;
    uint8_t count;
    uint8_t current_idx;
    uint8_t stage;
    uint8_t blink_cnt;
    uint8_t click_accum;
    struct k_timer timer;
} cfg_ctx;

enum config_stage {
    STAGE_PRE_BLINK = 0,
    STAGE_BLINK_ON,
    STAGE_BLINK_OFF,
    STAGE_BUZZ_WAIT,
};

static void config_timer_handler(struct k_timer *timer)
{
    struct config_item *item = (struct config_item *)&cfg_ctx.items[cfg_ctx.current_idx];
    
    switch (cfg_ctx.stage) {
    case STAGE_PRE_BLINK:
        update_led_hardware(0);
        cfg_ctx.blink_cnt = 0;
        cfg_ctx.stage = STAGE_BLINK_ON;
        k_timer_start(&cfg_ctx.timer, K_MSEC(500), K_NO_WAIT);
        break;
    case STAGE_BLINK_ON:
        update_led_hardware(100);
        cfg_ctx.stage = STAGE_BLINK_OFF;
        k_timer_start(&cfg_ctx.timer, K_MSEC(100), K_NO_WAIT);
        break;
    case STAGE_BLINK_OFF:
        update_led_hardware(0);
        cfg_ctx.blink_cnt++;
        if (cfg_ctx.blink_cnt < item->blinks) {
            cfg_ctx.stage = STAGE_BLINK_ON; 
            k_timer_start(&cfg_ctx.timer, K_MSEC(200), K_NO_WAIT);
        } else {
            cfg_ctx.stage = STAGE_BUZZ_WAIT;
            update_led_hardware(0); 
            k_timer_start(&cfg_ctx.timer, K_MSEC(3500), K_NO_WAIT);
            LOG_INF("Config: Waiting for input...");
        }
        break;
    case STAGE_BUZZ_WAIT:
            if (cfg_ctx.click_accum > 0) {
                LOG_INF("Config: Saved %d to ID %d", cfg_ctx.click_accum, item->nvs_id);
                nvs_write_byte(item->nvs_id, cfg_ctx.click_accum);
                
                /* Apply via callback */
                if (item->apply_cb) {
                    item->apply_cb(cfg_ctx.click_accum);
                }
            }
        cfg_ctx.click_accum = 0;
        cfg_ctx.current_idx++;
        if (cfg_ctx.current_idx < cfg_ctx.count) {
            cfg_ctx.stage = STAGE_PRE_BLINK;
            k_timer_start(&cfg_ctx.timer, K_MSEC(500), K_NO_WAIT);
        } else {
            fsm_transition_to(&node_off);
        }
        break;
    }
}

static void start_config_menu(const struct config_item *items, uint8_t count)
{
    stop_ramping();
    stop_strobe();
    
    cfg_ctx.items = items;
    cfg_ctx.count = count;
    cfg_ctx.current_idx = 0;
    cfg_ctx.click_accum = 0;
    cfg_ctx.stage = STAGE_PRE_BLINK;
    
    k_timer_start(&cfg_ctx.timer, K_MSEC(100), K_NO_WAIT);
    LOG_INF("Config Menu Started (%d items)", count);
}

/* ========== Ramp Timer Handler ========== */

/* ========== Ramp Timer Handler ========== */

static void ramp_timer_handler(struct k_timer *timer)
{
    uint8_t *target_val = (active_param == PARAM_BRIGHTNESS) ? 
                          &current_brightness : &strobe_frequency;

    if (ramp_direction > 0) {
        if (*target_val < 255 - RAMP_STEP_SIZE) {
            *target_val += RAMP_STEP_SIZE;
        } else {
            *target_val = 255;
            ramp_direction = -1;
        }
    } else if (ramp_direction < 0) {
        if (*target_val > 1 + RAMP_STEP_SIZE) {
            *target_val -= RAMP_STEP_SIZE;
        } else {
            *target_val = 1;
            ramp_direction = 1;
        }
    }
    
    if (active_param == PARAM_BRIGHTNESS) {
        update_led_hardware(current_brightness);
    } else {
        LOG_DBG("Strobe freq idx: %d", strobe_frequency);
    }
}

static void start_ramping(int direction)
{
    ramp_direction = direction;
    ramp_active = true;
    
    uint32_t duration_ms = (active_param == PARAM_BRIGHTNESS) ?
                           CONFIG_ZBEAM_BRIGHTNESS_SWEEP_DURATION_MS :
                           CONFIG_ZBEAM_STROBE_SWEEP_DURATION_MS;

    uint32_t step_ms = duration_ms / 255;
    if (step_ms < 1) step_ms = 1;
    
    k_timer_start(&ramp_timer, K_MSEC(step_ms), K_MSEC(step_ms));
    LOG_INF("Ramp start: dir=%d duration=%dms", direction, duration_ms);
}

static void stop_ramping(void)
{
    k_timer_stop(&ramp_timer);
    ramp_direction = 0;
    
    if (ramp_active && active_param == PARAM_BRIGHTNESS) {
        memorized_brightness = current_brightness;
        /* Persist Brightness */
#ifdef CONFIG_ZBEAM_NVS_ENABLED
        nvs_write_byte(NVS_ID_MEM_BRIGHTNESS, memorized_brightness);
#endif
    }
    ramp_active = false;
    LOG_INF("Ramp stop.");
}

/* ========== Strobe Handler ========== */

/* Sawtooth State */
static uint8_t sawtooth_step = 0;
#define SAWTOOTH_MIN_STEP_MS 5

static void strobe_timer_handler(struct k_timer *timer)
{
#ifdef CONFIG_ZBEAM_STROBE_WAVEFORM_SAWTOOTH
    /* Sawtooth Logic */
    uint32_t period_ms = get_strobe_delay_ms(strobe_frequency) * 2; /* Full period */
    
    /* Dynamic Resolution: Keep step size >= MIN_STEP_MS */
    uint8_t steps = period_ms / SAWTOOTH_MIN_STEP_MS;
    if (steps < 4) steps = 4; /* Minimum 4 steps for shape */
    if (steps > 32) steps = 32; /* Max 32 steps for resolution */
    
    uint32_t step_delay = period_ms / steps;
    
    /* Calculate brightness */
    uint16_t level = (sawtooth_step * 255) / steps;
    
    /* Apply Floor/Ceiling or ALC */
    uint8_t target_max = 255;
#ifdef CONFIG_ZBEAM_STROBE_USE_ALC_BRIGHTNESS
    target_max = memorized_brightness;
    if (target_max < BRIGHTNESS_FLOOR) target_max = BRIGHTNESS_FLOOR;
#endif
    level = (level * target_max) / 255;
    
    update_led_hardware((uint8_t)level);
    
    /* Advance Step */
    sawtooth_step++;
    if (sawtooth_step >= steps) {
        sawtooth_step = 0;
    }
    
    k_timer_start(&strobe_timer, K_MSEC(step_delay), K_NO_WAIT);

#else    
    /* Square Wave Logic */
    strobe_on = !strobe_on;
    
    uint8_t target_level = 255;
#ifdef CONFIG_ZBEAM_STROBE_USE_ALC_BRIGHTNESS
    target_level = memorized_brightness;
    if (target_level < BRIGHTNESS_FLOOR) target_level = BRIGHTNESS_FLOOR;
#endif

    if (strobe_on) {
        update_led_hardware(target_level); 
    } else {
        update_led_hardware(0);
    }
    
    /* Re-arm One-Shot */
    uint32_t delay = get_strobe_delay_ms(strobe_frequency);
    k_timer_start(&strobe_timer, K_MSEC(delay), K_NO_WAIT);
#endif
}

static void start_strobe(bool is_party)
{
    stop_ramping();
    
    party_mode = is_party;
    active_param = PARAM_FREQUENCY; 
    
    uint32_t delay = get_strobe_delay_ms(strobe_frequency);
    k_timer_start(&strobe_timer, K_MSEC(delay), K_NO_WAIT);
    
    uint8_t start_level = 255;
#ifdef CONFIG_ZBEAM_STROBE_USE_ALC_BRIGHTNESS
    start_level = memorized_brightness;
#endif
    update_led_hardware(start_level);
    strobe_on = true;
    
    LOG_INF("Strobe START (Party=%d FreqIdx=%d)", is_party, strobe_frequency);
}

static void stop_strobe(void)
{
    k_timer_stop(&strobe_timer);
    update_led_hardware(0);
    active_param = PARAM_BRIGHTNESS;
}

/* ========== Node Routines ========== */

static void routine_off(void) 
{ 
    stop_ramping(); 
    update_led_hardware(0); 
    k_timer_stop(&thermal_timer); /* Stop thermal check */
    pm_suspend(); 
    LOG_INF("OFF"); 
}

static void routine_on(void) 
{ 
    pm_resume(); 
    k_timer_start(&thermal_timer, K_SECONDS(1), K_SECONDS(1));
    stop_ramping(); 
    current_brightness = memorized_brightness; 
    update_led_hardware(current_brightness); 
    LOG_INF("ON"); 
}

static void routine_moon(void) 
{ 
    pm_resume();
    k_timer_start(&thermal_timer, K_SECONDS(1), K_SECONDS(1));
    stop_ramping(); 
    current_brightness = BRIGHTNESS_FLOOR; 
    update_led_hardware(current_brightness); 
    LOG_INF("MOON"); 
}

static void routine_turbo(void) 
{ 
    pm_resume();
    k_timer_start(&thermal_timer, K_SECONDS(1), K_SECONDS(1));
    stop_ramping(); 
    current_brightness = BRIGHTNESS_CEILING; 
    update_led_hardware(current_brightness); 
    LOG_INF("TURBO"); 
}
static void routine_ramp(void) { LOG_INF("RAMP"); }
static void routine_lockout(void) { stop_ramping(); update_led_hardware(0); LOG_INF("LOCKOUT"); }
static void routine_strobe(void) { start_strobe(false); }
static void routine_party_strobe(void) { start_strobe(true); }
static void routine_blink(void) { stop_strobe(); update_led_hardware(100); k_msleep(50); update_led_hardware(0); }
static void routine_config_ramp(void) { start_config_menu(ramp_config_items, ARRAY_SIZE(ramp_config_items)); }

static void routine_battcheck(void)
{
    stop_ramping();
    LOG_INF("BATTCHECK");
    uint16_t mv = batt_read_voltage_mv();
    uint8_t major, minor;
    batt_calculate_blinks(mv, &major, &minor);
    LOG_INF("Voltage: %dmV", mv);
    for (int i = 0; i < major; i++) { update_led_hardware(100); k_msleep(100); update_led_hardware(0); k_msleep(300); }
    k_msleep(800);
    for (int i = 0; i < minor; i++) { update_led_hardware(100); k_msleep(100); update_led_hardware(0); k_msleep(300); }
}

static void routine_factory_reset(void)
{
    stop_ramping();
    LOG_WRN("FACTORY RESET");
#ifdef CONFIG_ZBEAM_NVS_ENABLED
    nvs_wipe_all();
#endif
    k_msleep(100);
    sys_reboot(SYS_REBOOT_COLD);
}

static void routine_aux_config(void)
{
    stop_ramping();
    aux_cycle_mode();
    /* Blink to confirm */
    update_led_hardware(50);
    k_msleep(100);
    update_led_hardware(0);
    fsm_transition_to(&node_off);
}

/* ========== Callbacks ========== */

static struct fsm_node* cb_hold_from_off(struct fsm_node *self) { current_brightness = BRIGHTNESS_FLOOR; update_led_hardware(current_brightness); start_ramping(1); return NULL; }
static struct fsm_node* cb_hold_ramp_up(struct fsm_node *self) { start_ramping(1); return NULL; }
static struct fsm_node* cb_hold_ramp_down(struct fsm_node *self) { start_ramping(-1); return NULL; }
static struct fsm_node* cb_ramp_release(struct fsm_node *self) { stop_ramping(); return &node_on; }
static struct fsm_node* cb_lockout_momentary(struct fsm_node *self) { update_led_hardware(BRIGHTNESS_FLOOR); return NULL; }
static struct fsm_node* cb_lockout_release(struct fsm_node *self) { update_led_hardware(0); return NULL; }
static struct fsm_node* cb_hold_strobe_fast(struct fsm_node *self) { start_ramping(1); return NULL; }
static struct fsm_node* cb_hold_strobe_slow(struct fsm_node *self) { start_ramping(-1); return NULL; }
static struct fsm_node* cb_strobe_release(struct fsm_node *self) { stop_ramping(); return NULL; }

static struct fsm_node* cb_config_click(struct fsm_node *self)
{
    if (cfg_ctx.stage == STAGE_BUZZ_WAIT) {
        cfg_ctx.click_accum++;
        k_timer_start(&cfg_ctx.timer, K_MSEC(3000), K_NO_WAIT);
        update_led_hardware(0);
        k_busy_wait(10000);
    }
    return NULL;
}

/* ========== Nodes ========== */

struct fsm_node node_off, node_on, node_ramp, node_moon, node_turbo, node_lockout;
struct fsm_node node_battcheck, node_blink, node_factory_reset;
struct fsm_node node_strobe, node_party_strobe, node_config_ramp;

struct fsm_node node_off = {
    .id = NODE_OFF, .name = "OFF", .action_routine = routine_off,
    .click_map = { [0]=&node_on, [1]=&node_turbo, [2]=&node_battcheck, [3]=&node_lockout, [4]=&node_factory_reset, [6]=&node_aux_config },
    .hold_map = { [0]=&node_ramp, [2]=&node_strobe },
    .hold_callbacks = { [0]=cb_hold_from_off },
};
struct fsm_node node_on = {
    .id = NODE_ON, .name = "ON", .action_routine = routine_on,
    .click_map = { [0]=&node_off, [1]=&node_turbo, [3]=&node_lockout },
    .hold_map = { [0]=&node_ramp, [1]=&node_ramp, [4]=&node_config_ramp },
    .hold_callbacks = { [0]=cb_hold_ramp_up, [1]=cb_hold_ramp_down },
};
struct fsm_node node_ramp = {
    .id = NODE_RAMP, .name = "RAMP", .action_routine = routine_ramp,
    .release_callback = cb_ramp_release, .click_map = { [0]=&node_off },
};
struct fsm_node node_moon = {
    .id = NODE_MOON, .name = "MOON", .action_routine = routine_moon,
    .click_map = { [0]=&node_off }, .hold_map = { [0]=&node_ramp }, .hold_callbacks = { [0]=cb_hold_ramp_up },
};
struct fsm_node node_turbo = {
    .id = NODE_TURBO, .name = "TURBO", .action_routine = routine_turbo,
    .click_map = { [0]=&node_off, [1]=&node_on },
};
struct fsm_node node_lockout = {
    .id = NODE_LOCKOUT, .name = "LOCKOUT", .action_routine = routine_lockout,
    .click_map = { [3]=&node_off }, .hold_callbacks = { [0]=cb_lockout_momentary }, .release_callback = cb_lockout_release,
};
struct fsm_node node_battcheck = {
    .id = NODE_BATTCHECK, .name = "BATTCHECK", .action_routine = routine_battcheck, .timeout_ms = 2000, .click_map = { [0]=&node_off },
};
struct fsm_node node_blink = {
    .id = NODE_BLINK, .name = "BLINK", .action_routine = routine_blink, .timeout_ms = 1000, .timeout_reverts = true,
};
struct fsm_node node_factory_reset = {
    .id = NODE_FACTORY_RESET, .name = "FACTORY_RESET", .action_routine = routine_factory_reset,
};
struct fsm_node node_strobe = {
    .id = NODE_STROBE, .name = "STROBE", .action_routine = routine_strobe,
    .click_map = { [0]=&node_off, [1]=&node_party_strobe },
    .hold_callbacks = { [0]=cb_hold_strobe_fast, [1]=cb_hold_strobe_slow },
    .release_callback = cb_strobe_release,
};
struct fsm_node node_party_strobe = {
    .id = NODE_PARTY_STROBE, .name = "PARTY_STROBE", .action_routine = routine_party_strobe,
    .click_map = { [0]=&node_off, [1]=&node_strobe },
    .hold_callbacks = { [0]=cb_hold_strobe_fast, [1]=cb_hold_strobe_slow },
    .release_callback = cb_strobe_release,
};
struct fsm_node node_config_ramp = {
    .id = NODE_CONFIG_RAMP, .name = "CONFIG_RAMP", .action_routine = routine_config_ramp,
    .click_callbacks = { [0]=cb_config_click, [1]=cb_config_click, [2]=cb_config_click, [3]=cb_config_click, [4]=cb_config_click },
};
struct fsm_node node_aux_config = {
    .id = NODE_AUX_CONFIG, .name = "AUX_CONFIG", .action_routine = routine_aux_config,
};
struct fsm_node *all_nodes[NODE_COUNT] = {
    [NODE_OFF]=&node_off, [NODE_ON]=&node_on, [NODE_RAMP]=&node_ramp, [NODE_MOON]=&node_moon, [NODE_TURBO]=&node_turbo,
    [NODE_LOCKOUT]=&node_lockout, [NODE_BATTCHECK]=&node_battcheck, [NODE_BLINK]=&node_blink, [NODE_FACTORY_RESET]=&node_factory_reset,
    [NODE_STROBE]=&node_strobe, [NODE_PARTY_STROBE]=&node_party_strobe, [NODE_CONFIG_RAMP]=&node_config_ramp, [NODE_AUX_CONFIG]=&node_aux_config,
};

void key_map_init(void)
{
    k_timer_init(&ramp_timer, ramp_timer_handler, NULL);
    k_timer_init(&strobe_timer, strobe_timer_handler, NULL);
    k_timer_init(&cfg_ctx.timer, config_timer_handler, NULL);
    k_timer_init(&thermal_timer, thermal_timer_handler, NULL);
    
    /* Initialize Thermal Manager */
    thermal_init();
    /* Don't start thermal timer yet, we start in OFF */
    
    /* Initialize PM */
    pm_init();
    
    /* Initialize AUX */
    aux_init();
    
    memorized_brightness = 128;
    
#ifdef CONFIG_ZBEAM_NVS_ENABLED
    /* Initialize NVS */
    nvs_init_fs();
    
    /* Load Persistence */
    nvs_read_byte(NVS_ID_MEM_BRIGHTNESS, &memorized_brightness);
    nvs_read_byte(NVS_ID_RAMP_FLOOR, &brightness_floor);
    nvs_read_byte(NVS_ID_RAMP_CEILING, &brightness_ceiling);
#endif
    
    /* Validate Limits */
    if (memorized_brightness < brightness_floor) memorized_brightness = brightness_floor;
    if (memorized_brightness > brightness_ceiling) memorized_brightness = brightness_ceiling;
    
    current_brightness = 0;
    
    {
        uint32_t min_hz = CONFIG_ZBEAM_STROBE_MIN_FREQ;
        uint32_t max_hz = CONFIG_ZBEAM_STROBE_MAX_FREQ;
        uint32_t def_hz = CONFIG_ZBEAM_STROBE_DEFAULT_FREQ;
        
        if (max_hz > min_hz) {
            if (def_hz < min_hz) def_hz = min_hz;
            if (def_hz > max_hz) def_hz = max_hz;
            strobe_frequency = (uint8_t)(((def_hz - min_hz) * 255) / (max_hz - min_hz));
        } else {
            strobe_frequency = 128;
        }
        
#ifdef CONFIG_ZBEAM_NVS_ENABLED
        /* Load Strobe Freq from NVS if available (using 10 as ID placeholder) */
        /* nvs_read_byte(NVS_ID_STROBE_FREQ, &strobe_frequency); */
#endif
    }
    LOG_INF("KeyMap Init. MemBright=%d Floor=%d Ceiling=%d", memorized_brightness, brightness_floor, brightness_ceiling);
}

struct fsm_node *get_start_node(void) { return &node_off; }
uint8_t key_map_get_current_pwm(void) { return current_brightness; }
uint8_t key_map_get_strobe_freq(void) { return strobe_frequency; }
int key_map_get_brightness_index(void) { return current_brightness; }
