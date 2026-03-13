#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "esp_zigbee_core.h"
#include "esp_zigbee_attribute.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zcl/esp_zigbee_zcl_identify.h"
#include "zboss_api.h"

#include "led.h"

#include <math.h>

#define LIGHT_ENDPOINT 1

static const char *TAG = "ZB_LIGHT";

/* -----------------------------------------------------------
   STATE
----------------------------------------------------------- */
static bool     light_on = false;
static uint8_t  level    = 254;
static uint16_t color_x  = 32768;
static uint16_t color_y  = 32768;

/* -----------------------------------------------------------
   LED UPDATE  (XY → sRGB)
----------------------------------------------------------- */
static void update_led(void)
{
    if (!light_on) {
        led_set_rgb(0, 0, 0);
        return;
    }

    float x = color_x / 65535.0f;
    float y = color_y / 65535.0f;
    if (y == 0.0f) return;

    float z = 1.0f - x - y;
    float Y = level / 254.0f;
    float X = (Y / y) * x;
    float Z = (Y / y) * z;

    float r =  3.2406f * X - 1.5372f * Y - 0.4986f * Z;
    float g = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
    float b =  0.0557f * X - 0.2040f * Y + 1.0570f * Z;

    r = r <= 0.0031308f ? 12.92f * r : 1.055f * powf(r, 1.0f / 2.4f) - 0.055f;
    g = g <= 0.0031308f ? 12.92f * g : 1.055f * powf(g, 1.0f / 2.4f) - 0.055f;
    b = b <= 0.0031308f ? 12.92f * b : 1.055f * powf(b, 1.0f / 2.4f) - 0.055f;

    r = r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
    g = g < 0.0f ? 0.0f : (g > 1.0f ? 1.0f : g);
    b = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);

    led_set_rgb((uint8_t)(r * 255),
                (uint8_t)(g * 255),
                (uint8_t)(b * 255));
}

/* -----------------------------------------------------------
   ATTRIBUTE HANDLER
----------------------------------------------------------- */
static esp_err_t attribute_handler(
    const esp_zb_zcl_set_attr_value_message_t *msg)
{
    if (!msg || msg->info.status != ESP_ZB_ZCL_STATUS_SUCCESS)
        return ESP_OK;

    ESP_LOGI(TAG, "ATTR cluster=0x%04X attr=0x%04X",
             msg->info.cluster, msg->attribute.id);

    switch (msg->info.cluster) {
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
        light_on = *(bool *)msg->attribute.data.value;
        update_led();
        break;

    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
        level = *(uint8_t *)msg->attribute.data.value;
        update_led();
        break;

    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY: {
        uint16_t t = *(uint16_t *)msg->attribute.data.value;
        ESP_LOGI(TAG, "IdentifyTime=%u", t);
        led_set_pattern(t > 0 ? LED_PATTERN_PAIRING : LED_PATTERN_SOLID_ON);
        break;
    }

    case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
        if (msg->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID)
            color_x = *(uint16_t *)msg->attribute.data.value;
        if (msg->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID)
            color_y = *(uint16_t *)msg->attribute.data.value;
        update_led();
        break;
    }

    return ESP_OK;
}

/* -----------------------------------------------------------
   IDENTIFY NOTIFY
----------------------------------------------------------- */
static void identify_notify_cb(uint8_t identify_on)
{
    ESP_LOGI(TAG, "Identify notify: %u", identify_on);
    led_set_pattern(identify_on ? LED_PATTERN_PAIRING : LED_PATTERN_SOLID_ON);
}

/* -----------------------------------------------------------
   RAW COMMAND HANDLER
----------------------------------------------------------- */
static bool zb_raw_cmd_handler(zb_uint8_t bufid)
{
    zb_zcl_parsed_hdr_t *h = ZB_BUF_GET_PARAM(bufid, zb_zcl_parsed_hdr_t);
    if (!h) return false;

    uint8_t *p   = (uint8_t *)zb_buf_begin(bufid);
    uint8_t  len = (uint8_t)zb_buf_len(bufid);

    ESP_LOGI(TAG, "RAW cluster=0x%04X cmd=0x%02X len=%u",
             h->cluster_id, h->cmd_id, len);
    for (int i = 0; i < len; i++)
        ESP_LOGI(TAG, "  [%02d] 0x%02X", i, p[i]);

    switch (h->cluster_id) {

    /* ── ON / OFF ── */
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
        switch (h->cmd_id) {
        case 0x00: light_on = false;      ESP_LOGI(TAG, "OFF");    break;
        case 0x01: light_on = true;       ESP_LOGI(TAG, "ON");     break;
        case 0x02: light_on = !light_on;  ESP_LOGI(TAG, "TOGGLE"); break;
        }
        update_led();
        break;

    /* ── LEVEL CONTROL ── */
    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
        if ((h->cmd_id == 0x00 || h->cmd_id == 0x04) && len >= 1) {
            level = p[0];
            ESP_LOGI(TAG, "Level -> %u", level);
            update_led();
        }
        break;

    /* ── IDENTIFY / TRIGGER EFFECT ── */
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
        if (h->cmd_id == 0x40 && len >= 2) {
            uint8_t effect  = p[0];
            uint8_t variant = p[1];
            ESP_LOGI(TAG, "Trigger Effect: effect=0x%02X variant=0x%02X",
                     effect, variant);
            switch (effect) {
            case 0x00: /* Blink */
                led_set_pattern(LED_PATTERN_PAIRING);
                break;
            case 0x01: /* Breathe */
                led_set_pattern(LED_PATTERN_IDLE_BREATHING);
                break;
            case 0x02: /* Okay */
            case 0x0B: /* Channel change */
                led_set_pattern(LED_PATTERN_SOLID_ON);
                update_led();
                break;
            case 0xFE: /* Stop effect */
            case 0xFF: /* Finish */
                if (light_on) {
                    led_set_pattern(LED_PATTERN_SOLID_ON);
                    update_led();
                } else {
                    led_set_rgb(0, 0, 0);
                }
                break;
            default:
                ESP_LOGW(TAG, "Unknown effect 0x%02X", effect);
                break;
            }
        }
        break;
    }

    return false;
}

/* -----------------------------------------------------------
   CORE ACTION HANDLER
----------------------------------------------------------- */
static esp_err_t zb_action_handler(
    esp_zb_core_action_callback_id_t id,
    const void *message)
{
    if (id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID)
        return attribute_handler(message);
    return ESP_OK;
}

/* -----------------------------------------------------------
   SIGNAL HANDLER
----------------------------------------------------------- */
static void start_steering_cb(uint8_t param)
{
    esp_zb_bdb_start_top_level_commissioning(param);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal)
{
    uint32_t  *sig        = signal->p_app_signal;
    esp_err_t  err_status = signal->esp_err_status;

    ESP_LOGI(TAG, "ZB signal=%lu (%s) status=%s",
             (unsigned long)*sig,
             esp_zb_zdo_signal_to_string(*sig),
             esp_err_to_name(err_status));

    switch (*sig) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        esp_zb_bdb_start_top_level_commissioning(
            ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        ESP_LOGI(TAG, "No network stored, steering...");
        led_set_pattern(LED_PATTERN_PAIRING);
        esp_zb_bdb_start_top_level_commissioning(
            ESP_ZB_BDB_MODE_NETWORK_STEERING);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Rejoined network after reboot");
            led_set_pattern(LED_PATTERN_SOLID_ON);
        } else {
            ESP_LOGW(TAG, "Rejoin failed, steering...");
            led_set_pattern(LED_PATTERN_PAIRING);
            esp_zb_bdb_start_top_level_commissioning(
                ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Joined network successfully");
            led_set_pattern(LED_PATTERN_SOLID_ON);
        } else {
            ESP_LOGW(TAG, "Steering failed, retrying in 5s...");
            led_set_pattern(LED_PATTERN_PAIRING);
            esp_zb_scheduler_alarm(
                start_steering_cb,
                ESP_ZB_BDB_MODE_NETWORK_STEERING,
                5000);
        }
        break;

    default:
        break;
    }
}

/* -----------------------------------------------------------
   ENDPOINT
----------------------------------------------------------- */
static void create_endpoint(void)
{
    esp_zb_color_dimmable_light_cfg_t cfg =
        ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();

    esp_zb_ep_list_t *ep =
        esp_zb_color_dimmable_light_ep_create(LIGHT_ENDPOINT, &cfg);

    ESP_ERROR_CHECK(esp_zb_device_register(ep));
    esp_zb_identify_notify_handler_register(LIGHT_ENDPOINT, identify_notify_cb);
}

/* -----------------------------------------------------------
   ZIGBEE TASK
----------------------------------------------------------- */
static void zigbee_task(void *pv)
{
    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER
    };

    esp_zb_init(&zb_cfg);
    create_endpoint();

    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_raw_command_handler_register(zb_raw_cmd_handler);

    esp_zb_set_trace_level_mask(ESP_ZB_TRACE_LEVEL_INFO,
                                ESP_ZB_TRACE_SUBSYSTEM_ZCL);

    ESP_LOGI(TAG, "Starting Zigbee stack");
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

/* -----------------------------------------------------------
   PUBLIC ENTRY — called from app_main
   NOTE: nvs_flash_init() is handled in main.c, NOT here.
----------------------------------------------------------- */
esp_err_t esp_zb_app_start(void)
{
    esp_zb_platform_config_t platform = {0};
    platform.radio_config.radio_mode           = ZB_RADIO_MODE_NATIVE;
    platform.host_config.host_connection_mode  = ZB_HOST_CONNECTION_MODE_NONE;
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform));

    xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);

    return ESP_OK;
}
