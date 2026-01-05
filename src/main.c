#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include "multi_tap_input.h"

LOG_MODULE_REGISTER(ButtonApp, LOG_LEVEL_INF);

#define MONITOR_KEY_CODE 30 // 'a'

static void app_action_handler(const struct multi_tap_input_action *action)
{
    if (action->is_hold) {
        LOG_INF("APP ACTION: Mode change detected: %d Clicks + HOLD.", action->count);
    } else {
        switch (action->count) {
            case 1:
                LOG_INF("APP ACTION: Single Tap executed (Light ON/OFF).");
                break;
            case 2:
                LOG_INF("APP ACTION: Double Tap executed (Dimmer setting).");
                break;
            default:
                LOG_INF("APP ACTION: %d Clicks executed (Advanced feature).", action->count);
                break;
        }
    }
}

static void raw_input_cb(struct input_event *evt, void *user_data)
{
    if (evt->type != INPUT_EV_KEY) {
        return;
    }
    
    if (evt->code != MONITOR_KEY_CODE) {
        return;
    }

    multi_tap_input_process_key(evt->value);
}

INPUT_CALLBACK_DEFINE(NULL, raw_input_cb, NULL);

int main(void)
{
    multi_tap_input_init(app_action_handler);

    LOG_INF("Application Ready. Waiting for key code %d ('a') press and release cycles.", MONITOR_KEY_CODE);
    return 0;
}