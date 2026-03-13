/*
 * Copyright (C) 2026 Murilo Freire
 *
 * This file is part of Kommando firmware.
 *
 * Kommando firmware is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kommando firmware is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "zigbee.h"
#include "config.h"
#include "backlight.h"
#include "commands.h"
#include "tiles.h"
#include "ui_panel.h"
#include "led.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_zigbee_core.h"
#include "zboss_api.h"
#include "esp_zigbee_type.h"
#include "nwk/esp_zigbee_nwk.h"
#include "zdo/esp_zigbee_zdo_command.h"
#include "esp_lvgl_port.h"
#include "zcl/zb_zcl_reporting.h"
#include "nvs.h"
#include "display.h"

static const char *TAG = "zigbee";

/* ============================================================
   DEVICE SETTING GLOBALS — extern declared in zigbee.h
   ============================================================ */
uint8_t  g_backlight_level    = BL_LEVEL_DEFAULT;
uint16_t g_screen_timeout_sec = SCREEN_TIMEOUT_DEFAULT_SEC;
uint8_t  g_dim_level          = BL_LEVEL_DIM;
bool     g_night_mode         = false;
uint8_t  g_night_brightness   = BL_LEVEL_NIGHT;
bool     g_deep_sleep_enabled = false;
uint16_t g_sleep_timeout_sec  = SLEEP_TIMEOUT_DEFAULT_SEC;

/* ============================================================
   ZCL ATTRIBUTE STORAGE BUFFERS
   ============================================================ */

/* Setting attribute values — stored as ZCL types */
static uint8_t  s_attr_backlight    = BL_LEVEL_DEFAULT;
static uint16_t s_attr_scr_timeout  = SCREEN_TIMEOUT_DEFAULT_SEC;
static uint8_t  s_attr_dim_level    = BL_LEVEL_DIM;
static uint8_t  s_attr_night_mode   = 0;   /* ZCL BOOLEAN */
static uint8_t  s_attr_night_bl     = BL_LEVEL_NIGHT;
static uint8_t  s_attr_deep_sleep   = 0;   /* ZCL BOOLEAN */
static uint16_t s_attr_sleep_timeout = SLEEP_TIMEOUT_DEFAULT_SEC;

/* Payload / state string buffers (ZCL character string: [len][data]) */
static uint8_t s_cmd_buffer[64]    = {0};
static uint8_t s_action_buffer[16] = {0};

/* Basic cluster strings (ZCL character string: [len][data]) */
static uint8_t s_basic_manufacturer[32] = {0};
static uint8_t s_basic_model[32]        = {0};

/* ============================================================
   INTERNAL STATE
   ============================================================ */
static bool       s_zb_joined    = false;
static bool       s_ready_acked  = false;
static uint16_t   s_ready_report_count = 0;
static bool       s_recovery_factory_reset_pending = false;

static void zb_report_backlight_level_sched(uint8_t level)
{
    zb_report_backlight_level(level);
}

#define READY_RETRY_INTERVAL_MS   10000
#define READY_MAX_REPORTS         1

/* ============================================================
   REPORTING SETUP — run once after joining to ensure ZBOSS
   has a reporting record for ATTR_STATE (0x0001) so that
   esp_zb_zcl_report_attr_cmd_req() actually transmits frames.
   ============================================================ */
static void zb_setup_reporting(uint8_t param)
{
    (void)param;
    /*
     * Use ZBOSS zb_zcl_put_reporting_info() which CREATE/OVERRIDES a record
     * (unlike esp_zb_zcl_update_reporting_info which only updates existing ones
     * and silently fails otherwise — causing ESP_ERR_NOT_FOUND on every send).
     */
    zb_zcl_reporting_info_t rep;
    memset(&rep, 0, sizeof(rep));

    rep.direction    = 0x00;              /* send report */
    rep.ep           = ZB_FIRST_ENDPOINT;
    rep.cluster_id   = ZB_CUSTOM_CLUSTER_ID;
    rep.cluster_role = 0x01;              /* server */
    rep.manuf_code   = 0xFFFF;            /* non-manufacturer-specific */

    /* min=0, max=300s; manual-send only (report_attr_cmd_req) */
    rep.u.send_info.min_interval     = 0;
    rep.u.send_info.max_interval     = 300;
    rep.u.send_info.def_min_interval = 0;
    rep.u.send_info.def_max_interval = 300;

    rep.dst.short_addr = 0x0000;
    rep.dst.endpoint   = 1;
    rep.dst.profile_id = 0x0104;          /* HA profile */

    /* Mark slot as occupied so the lookup in esp_zb_zcl_report_attr_cmd_req finds it */
    rep.flags = ZB_ZCL_REPORTING_SLOT_BUSY;

    rep.attr_id = ZB_ATTR_STATE_ID;
    zb_ret_t ret = zb_zcl_put_reporting_info(&rep, ZB_TRUE);
    ESP_LOGI(TAG, "ATTR_STATE reporting record inserted: %d", (int)ret);

    rep.attr_id = ZB_ATTR_BACKLIGHT_ID;
    ret = zb_zcl_put_reporting_info(&rep, ZB_TRUE);
    ESP_LOGI(TAG, "ATTR_BACKLIGHT reporting record inserted: %d", (int)ret);
}

/* ============================================================
   REPORTING REFRESH — wake up coordinator forwarding path
   ============================================================ */
static void zb_refresh_reporting(uint8_t param)
{
    (void)param;

    /* Refresh local ZBOSS reporting records after steering to ensure they're current */
    zb_setup_reporting(0);

    /* Send a dummy attribute report to wake up coordinator's forwarding path.
     * This forces the coordinator to rebuild its receive tables for our custom cluster. */
    const char *test_str = "REFRESH";
    uint8_t buf[16];
    buf[0] = strlen(test_str);
    memcpy(&buf[1], test_str, buf[0]);

    esp_zb_zcl_set_attribute_val(ZB_FIRST_ENDPOINT,
                                   ZB_CUSTOM_CLUSTER_ID,
                                   ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                   ZB_ATTR_STATE_ID,
                                   buf,
                                   false);

    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint          = 1,
            .src_endpoint          = ZB_FIRST_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID    = ZB_CUSTOM_CLUSTER_ID,
        .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .attributeID  = ZB_ATTR_STATE_ID,
    };
    esp_zb_zcl_report_attr_cmd_req(&report_cmd);
    ESP_LOGI(TAG, "Sent REFRESH report to wake up coordinator forwarding path");
}

/* ============================================================
   BASIC CLUSTER ANNOUNCE — helps Z2M recognize the device
   ============================================================ */
static void zb_report_basic_info(uint8_t param)
{
    (void)param;

    /* Push manufacturer + model to coordinator (attribute reports). */
    esp_zb_zcl_set_attribute_val(
        ZB_FIRST_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
        s_basic_manufacturer,
        false);

    esp_zb_zcl_set_attribute_val(
        ZB_FIRST_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
        s_basic_model,
        false);

    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint          = 1,
            .src_endpoint          = ZB_FIRST_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID    = ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .attributeID  = ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
    };
    esp_zb_zcl_report_attr_cmd_req(&report_cmd);

    report_cmd.attributeID = ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID;
    esp_zb_zcl_report_attr_cmd_req(&report_cmd);

    ESP_LOGI(TAG, "Reported BASIC info (manufacturer/model)");
}

/* ============================================================
   ZDO DEVICE ANNOUNCE — force coordinator rediscovery
   ============================================================ */
static void zb_send_device_announce(uint8_t param)
{
    (void)param;
    esp_zb_zdo_device_announcement_req();
    ESP_LOGI(TAG, "Sent ZDO device announcement");
}

static void zb_log_network_identity(void)
{
    uint8_t channel = esp_zb_get_current_channel();
    uint16_t pan_id = esp_zb_get_pan_id();
    uint16_t short_addr = esp_zb_get_short_address();
    esp_zb_ieee_addr_t ext_pan = {0};
    esp_zb_get_extended_pan_id(ext_pan);

    ESP_LOGI(TAG,
             "NWK identity: ch=%u pan=0x%04X short=0x%04X extpan=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             channel,
             pan_id,
             short_addr,
             ext_pan[7], ext_pan[6], ext_pan[5], ext_pan[4],
             ext_pan[3], ext_pan[2], ext_pan[1], ext_pan[0]);
}

static bool zb_network_identity_valid(void)
{
    const uint8_t channel = esp_zb_get_current_channel();
    const uint16_t pan_id = esp_zb_get_pan_id();
    const uint16_t short_addr = esp_zb_get_short_address();

    if (channel < 11 || channel > 26) return false;
    if (pan_id == 0x0000 || pan_id == 0xFFFF) return false;
    if (short_addr == 0xFFFF || short_addr == 0x0000) return false;
    return true;
}

static void zb_load_recovery_marker(void)
{
    nvs_handle_t nvs;
    uint8_t done = 0;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        if (nvs_get_u8(nvs, NVS_KEY_ZB_RECOVERY_DONE, &done) != ESP_OK) {
            done = 0;
        }
        nvs_close(nvs);
    }

    s_recovery_factory_reset_pending = (done == 0);
    ESP_LOGI(TAG, "Recovery marker: %s",
             s_recovery_factory_reset_pending ? "pending one-time factory reset" : "already completed");
}

static void zb_mark_recovery_done(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, NVS_KEY_ZB_RECOVERY_DONE, 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

/* ============================================================
   TILE ACTION REPORTING
   ============================================================ */

/*
 * Report "READY" via attr 0x0001 after joining the network.
 * HA automation can listen for tile_action == "READY" and push
 * current tile states back via S: commands.
 */
static void zb_report_ready(uint8_t param)
{
    (void)param;
    if (s_ready_acked) {
        s_ready_report_count = 0;
        return;   /* HA already responded — stop retries */
    }

    s_ready_report_count++;
    if (s_ready_report_count > READY_MAX_REPORTS) {
        ESP_LOGI(TAG, "READY sent %u times — stopping retries", READY_MAX_REPORTS);
        s_ready_acked = true;
        s_ready_report_count = 0;
        return;
    }

    const char *msg = "READY";
    uint8_t payload[8] = {0};
    payload[0] = (uint8_t)strlen(msg);
    memcpy(payload + 1, msg, payload[0]);

    esp_zb_zcl_set_attribute_val(
        ZB_FIRST_ENDPOINT,
        ZB_CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ZB_ATTR_STATE_ID,
        payload,
        false);

    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint          = 1,
            .src_endpoint          = ZB_FIRST_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID    = ZB_CUSTOM_CLUSTER_ID,
        .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .attributeID  = ZB_ATTR_STATE_ID,
    };
    esp_err_t ready_err = esp_zb_zcl_report_attr_cmd_req(&report_cmd);
    if (ready_err != ESP_OK) {
        ESP_LOGW(TAG, "READY report failed: %s", esp_err_to_name(ready_err));
    } else {
        ESP_LOGI(TAG, "Reported READY to coordinator (%u/%u)",
                 s_ready_report_count, READY_MAX_REPORTS);
    }

    if (s_ready_report_count < READY_MAX_REPORTS) {
        esp_zb_scheduler_alarm(zb_report_ready, 0, READY_RETRY_INTERVAL_MS);
    } else {
        ESP_LOGI(TAG, "READY single-shot complete; no further retries");
    }
}

/*
 * Update custom cluster attr 0x0001 and send a ZCL report to the coordinator.
 * Called from zb_send_tile_state().
 */
static void zb_report_tile_action(const zb_tile_evt_t *evt)
{
    /* Build ZCL char string: [len][ascii], formats:
     *  TAP:  T:<id>:<0|1>
     *  HOLD: H:<id>
     *  DIM:  D:<id>:<0..100>
     */
    int len = 0;

    if (evt->type == TILE_EVT_TAP) {
        len = snprintf((char *)s_action_buffer + 1, sizeof(s_action_buffer) - 1, "T:%d:%d", evt->id, evt->state ? 1 : 0);
    } else if (evt->type == TILE_EVT_HOLD) {
        len = snprintf((char *)s_action_buffer + 1, sizeof(s_action_buffer) - 1, "H:%d", evt->id);
    } else {
        len = snprintf((char *)s_action_buffer + 1, sizeof(s_action_buffer) - 1, "D:%d:%d", evt->id, evt->value);
    }
    if (len < 0) len = 0;
    s_action_buffer[0] = (uint8_t)len;

    esp_zb_zcl_set_attribute_val(
        ZB_FIRST_ENDPOINT,
        ZB_CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ZB_ATTR_STATE_ID,
        s_action_buffer,
        false);

    /* Best-effort attribute report (may not reach Z2M for custom clusters) */
    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint          = 1,
            .src_endpoint          = ZB_FIRST_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID    = ZB_CUSTOM_CLUSTER_ID,
        .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .attributeID  = ZB_ATTR_STATE_ID,
    };
    esp_err_t report_err = esp_zb_zcl_report_attr_cmd_req(&report_cmd);
    if (report_err != ESP_OK) {
        ESP_LOGW(TAG, "Tile[%d] state report failed: %s", evt->id, esp_err_to_name(report_err));
    }

    /* For DIM events, ALSO send a standard genLevelCtrl moveToLevel command.
     * Level 0-100 → 0-254 for Zigbee.
     *
     * NOTE: We intentionally use transition_time=2 for DIM and transition_time=0
     * for TAP fallback (below), so the converter can disambiguate them. */
    if (evt->type == TILE_EVT_DIM) {
        uint8_t tile_ep  = (uint8_t)(ZB_FIRST_ENDPOINT + evt->id);
        uint8_t zb_level = (uint8_t)(evt->value > 100 ? 254 : (evt->value * 254 / 100));
        
        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zcl_move_to_level_cmd_t lvl_cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint          = 1,
                .src_endpoint          = tile_ep,
            },
            .address_mode   = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .level          = zb_level,
            .transition_time = 2,
        };
        esp_zb_zcl_level_move_to_level_cmd_req(&lvl_cmd);
        esp_zb_lock_release();
        
        ESP_LOGI(TAG, "Tile[%d] DIM reported -> %u (zb_level=%u ep=%d)",
                 evt->id, evt->value, zb_level, tile_ep);
    } else if (evt->type == TILE_EVT_TAP) {
        /* Backup path for coordinators/converters that miss custom-cluster reports:
         * emit moveToLevel with transition_time=0 and level 0/254, which converter
         * maps back to ON/OFF for the specific tile endpoint. */
        uint8_t tile_ep  = (uint8_t)(ZB_FIRST_ENDPOINT + evt->id);
        uint8_t zb_level = evt->state ? 254 : 0;
        
        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zcl_move_to_level_cmd_t lvl_cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint          = 1,
                .src_endpoint          = tile_ep,
            },
            .address_mode   = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .level          = zb_level,
            .transition_time = 0,
        };
        esp_err_t tap_lvl_err = esp_zb_zcl_level_move_to_level_cmd_req(&lvl_cmd);
        esp_zb_lock_release();
        
        if (tap_lvl_err != ESP_OK) {
            ESP_LOGW(TAG, "Tile[%d] TAP level fallback send failed: %s", evt->id, esp_err_to_name(tap_lvl_err));
        }
        ESP_LOGI(TAG, "Tile[%d] TAP reported -> %s", evt->id, evt->state ? "ON" : "OFF");
    } else if (evt->type == TILE_EVT_HOLD) {
        ESP_LOGI(TAG, "Tile[%d] HOLD reported", evt->id);
    }
}

void zb_report_backlight_level(uint8_t level)
{
    /* Update local state + persist */
    g_backlight_level = level;
    s_attr_backlight  = level;
    backlight_set(level);
    save_setting_u8(NVS_KEY_BACKLIGHT, level);

    /* Update attribute value and report to coordinator */
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(
        ZB_FIRST_ENDPOINT,
        ZB_CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ZB_ATTR_BACKLIGHT_ID,
        &s_attr_backlight,
        false);

    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint          = 1,
            .src_endpoint          = ZB_FIRST_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID    = ZB_CUSTOM_CLUSTER_ID,
        .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .attributeID  = ZB_ATTR_BACKLIGHT_ID,
    };
    esp_err_t bl_rep_err = esp_zb_zcl_report_attr_cmd_req(&report_cmd);
    if (bl_rep_err == ESP_OK) {
        ESP_LOGI(TAG, "Backlight report sent (attr 0x%04X)", ZB_ATTR_BACKLIGHT_ID);
    } else if (bl_rep_err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Backlight report not supported by stack");
    } else {
        ESP_LOGW(TAG, "Backlight report failed: %s", esp_err_to_name(bl_rep_err));
    }
    esp_zb_lock_release();

    /* Also emit a custom state string so Z2M always publishes */
    {
        int len = snprintf((char *)s_action_buffer + 1, sizeof(s_action_buffer) - 1, "B:%u", level);
        if (len < 0) len = 0;
        if (len > (int)(sizeof(s_action_buffer) - 1)) len = sizeof(s_action_buffer) - 1;
        s_action_buffer[0] = (uint8_t)len;

        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zcl_set_attribute_val(
            ZB_FIRST_ENDPOINT,
            ZB_CUSTOM_CLUSTER_ID,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ZB_ATTR_STATE_ID,
            s_action_buffer,
            false);

        esp_zb_zcl_report_attr_cmd_t report_state_cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint          = 1,
                .src_endpoint          = ZB_FIRST_ENDPOINT,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .clusterID    = ZB_CUSTOM_CLUSTER_ID,
            .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
            .attributeID  = ZB_ATTR_STATE_ID,
        };
        esp_err_t state_rep_err = esp_zb_zcl_report_attr_cmd_req(&report_state_cmd);
        if (state_rep_err == ESP_OK) {
            ESP_LOGI(TAG, "Backlight state-string report sent (attr 0x%04X)", ZB_ATTR_STATE_ID);
        } else {
            ESP_LOGW(TAG, "Backlight state-string report failed: %s", esp_err_to_name(state_rep_err));
        }
        esp_zb_lock_release();
    }

    /* Fallback: emit a standard genLevelCtrl moveToLevel so Z2M sees a message
     * even if the coordinator ignores custom-cluster reports. Signature:
     * ep=1, transition_time=7 (handled by converter as backlight update). */
    {
        uint8_t zb_level = (uint8_t)((level * 254) / 255);
        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zcl_move_to_level_cmd_t lvl_cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint          = 1,
                .src_endpoint          = ZB_FIRST_ENDPOINT,
            },
            .address_mode   = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .level          = zb_level,
            .transition_time = 7,
        };
        esp_err_t lvl_err = esp_zb_zcl_level_move_to_level_cmd_req(&lvl_cmd);
        esp_zb_lock_release();
        if (lvl_err != ESP_OK) {
            ESP_LOGW(TAG, "Backlight fallback moveToLevel failed: %s", esp_err_to_name(lvl_err));
        } else {
            ESP_LOGI(TAG, "Backlight fallback moveToLevel sent (level=%u)", zb_level);
        }
    }

    ESP_LOGI(TAG, "Backlight (UI) -> %u", level);
}

void zb_report_backlight_level_async(uint8_t level)
{
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_scheduler_alarm(zb_report_backlight_level_sched, level, 0);
    esp_zb_lock_release();
}

void zb_send_tile_state(uint8_t param)
{
    zb_tile_evt_t *evt    = &s_zb_evt_buf[param];

    /* Report tile action string via custom cluster attr 0x0001 */
    zb_report_tile_action(evt);

    /* IMPORTANT:
     * Do not send direct genOnOff commands to coordinator here.
     * In this deployment they are intermittently rejected (0x14/0x17),
     * causing noisy logs and unreliable tap behavior.
     *
     * We rely on the custom ATTR_STATE report (T:<id>:<state>) path,
     * which preserves tile index and is consumed by the external converter. */
}

/* ============================================================
   ACTION HANDLER — receives writes from Z2M / HA
   ============================================================ */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t cb_id, const void *msg)
{
    if (cb_id != ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
        ESP_LOGD(TAG, "Unhandled cb_id=0x%04x", cb_id);
        return ESP_OK;
    }

    const esp_zb_zcl_set_attr_value_message_t *m =
        (const esp_zb_zcl_set_attr_value_message_t *)msg;

    ESP_LOGI(TAG, "SET_ATTR cluster=0x%04X attr=0x%04X", m->info.cluster, m->attribute.id);

    if (m->info.cluster != ZB_CUSTOM_CLUSTER_ID) return ESP_OK;

    /* Any successful write from HA/Z2M means link/path is alive.
     * Stop READY retries to avoid noisy 3/3 logs when attr 0x0000 ack isn't used. */
    s_ready_acked = true;
    s_ready_report_count = 0;

    /* ---- Command payload attr (0x0000) ---- */
    if (m->attribute.id == ZB_ATTR_PAYLOAD_ID) {
        uint8_t *data = (uint8_t *)m->attribute.data.value;
        uint8_t  len  = data[0];
        if (len > 63) len = 63;

        static char buf[64];
        memcpy(buf, data + 1, len);
        buf[len] = '\0';
        ESP_LOGI(TAG, "<<< Write: [%s]", buf);

        /* READY ack from Z2M — no action besides stopping retries */
        if (strcmp(buf, "A:READY") == 0 || strcmp(buf, "READY_ACK") == 0) {
            s_ready_report_count = 0;
            return ESP_OK;
        }

        if (should_drop_duplicate_cfg(buf)) return ESP_OK;

        /* Persist C: tile config to NVS */
        char tmp[64];
        strncpy(tmp, buf, sizeof(tmp) - 1);
        char *sp, *cmd = strtok_r(tmp, ":", &sp), *id_str = strtok_r(NULL, ":", &sp);
        if (cmd && id_str && strcmp(cmd, "C") == 0) {
            int tid = atoi(id_str);
            if (tid >= 0 && tid < MAX_TILES) save_tile_to_nvs(tid, buf);
        }

        if (!g_lvgl_ready) {
            ESP_LOGW(TAG, "LVGL not ready — processing without UI: [%s]", buf);
        }
        process_ha_command(buf);
        return ESP_OK;
    }

    /* ---- Backlight level (0x0010) — UINT8 ---- */
    if (m->attribute.id == ZB_ATTR_BACKLIGHT_ID) {
        uint8_t val = *(uint8_t *)m->attribute.data.value;
        g_backlight_level = val;
        s_attr_backlight  = val;
        backlight_set(val);
        save_setting_u8(NVS_KEY_BACKLIGHT, val);
        ESP_LOGI(TAG, "Backlight → %d", val);
        return ESP_OK;
    }

    /* ---- Screen timeout (0x0011) — UINT16 ---- */
    if (m->attribute.id == ZB_ATTR_SCR_TIMEOUT_ID) {
        uint16_t val = *(uint16_t *)m->attribute.data.value;
        g_screen_timeout_sec = val;
        s_attr_scr_timeout   = val;
        save_setting_u16(NVS_KEY_SCREEN_TIMEOUT, val);
        ESP_LOGI(TAG, "Screen timeout → %ds", val);
        return ESP_OK;
    }

    /* ---- Dim level (0x0012) — UINT8 ---- */
    if (m->attribute.id == ZB_ATTR_DIM_LEVEL_ID) {
        uint8_t val = *(uint8_t *)m->attribute.data.value;
        g_dim_level      = val;
        s_attr_dim_level = val;
        save_setting_u8(NVS_KEY_DIM_LEVEL, val);
        ESP_LOGI(TAG, "Dim level → %d", val);
        return ESP_OK;
    }

    /* ---- Night mode (0x0013) — BOOLEAN ---- */
    if (m->attribute.id == ZB_ATTR_NIGHT_MODE_ID) {
        uint8_t val = *(uint8_t *)m->attribute.data.value;
        g_night_mode    = (bool)val;
        s_attr_night_mode = val;
        save_setting_u8(NVS_KEY_NIGHT_MODE, val);

        /* Apply brightness change immediately */
        uint8_t level = g_night_mode ? g_night_brightness : g_backlight_level;
        backlight_set(level);

        /* Repaint tile "on" colours */
        if (lvgl_port_lock(0)) {
            ui_panel_set_night_mode(g_night_mode);
            lvgl_port_unlock();
        }
        ESP_LOGI(TAG, "Night mode → %s", g_night_mode ? "ON" : "OFF");
        return ESP_OK;
    }

    /* ---- Night brightness (0x0014) — UINT8 ---- */
    if (m->attribute.id == ZB_ATTR_NIGHT_BL_ID) {
        uint8_t val = *(uint8_t *)m->attribute.data.value;
        g_night_brightness = val;
        s_attr_night_bl    = val;
        save_setting_u8(NVS_KEY_NIGHT_BL, val);
        if (g_night_mode) backlight_set(val);
        ESP_LOGI(TAG, "Night brightness → %d", val);
        return ESP_OK;
    }

    /* ---- Deep sleep enable (0x0015) — BOOLEAN ---- */
    if (m->attribute.id == ZB_ATTR_DEEP_SLEEP_ID) {
        uint8_t val = *(uint8_t *)m->attribute.data.value;
        g_deep_sleep_enabled = (bool)val;
        s_attr_deep_sleep    = val;
        save_setting_u8(NVS_KEY_DEEP_SLEEP_EN, val);
        ESP_LOGI(TAG, "Deep sleep enable → %s", val ? "ON" : "OFF");
        return ESP_OK;
    }

    /* ---- Sleep timeout (0x0016) — UINT16 ---- */
    if (m->attribute.id == ZB_ATTR_SLEEP_TIMEOUT_ID) {
        uint16_t val = *(uint16_t *)m->attribute.data.value;
        g_sleep_timeout_sec   = val;
        s_attr_sleep_timeout  = val;
        save_setting_u16(NVS_KEY_SLEEP_TIMEOUT, val);
        ESP_LOGI(TAG, "Sleep timeout → %ds", val);
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Unhandled attr 0x%04X on cluster 0x%04X", m->attribute.id, m->info.cluster);
    return ESP_OK;
}

/* ============================================================
   NETWORK STEERING HELPER
   ============================================================ */
static void zb_start_steering(uint8_t param)
{
    ESP_LOGI(TAG, "Starting network steering...");
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
}

/* ============================================================
   SIGNAL HANDLER (called by the Zigbee stack)
   ============================================================ */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    esp_err_t                err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type   = *signal_struct->p_app_signal;
    const bool joined_now = esp_zb_bdb_dev_joined();

    ESP_LOGI(TAG,
             "ZB signal=%u (%s) status=%s joined=%d",
             (unsigned)sig_type,
             esp_zb_zdo_signal_to_string(sig_type),
             esp_err_to_name(err_status),
             joined_now ? 1 : 0);

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Skip startup → init");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        ESP_LOGI(TAG, "First start → steering");
        s_zb_joined = false;
        s_ready_report_count = 0;
        led_set_pattern(LED_PATTERN_PAIRING);
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK && joined_now && zb_network_identity_valid()) {
            if (s_recovery_factory_reset_pending) {
                ESP_LOGW(TAG, "Recovery: forcing one-time Zigbee factory reset for clean coordinator rejoin");
                s_recovery_factory_reset_pending = false;
                s_zb_joined = false;
                s_ready_report_count = 0;
                led_set_pattern(LED_PATTERN_PAIRING);
                zb_mark_recovery_done();
                esp_zb_factory_reset();
                break;
            }

            ESP_LOGI(TAG, "Reboot — joined network restored");
            s_zb_joined = true;
            s_ready_acked = false;
            s_ready_report_count = 0;
            led_set_pattern(LED_PATTERN_IDLE_BREATHING);
            esp_zb_scheduler_alarm(zb_setup_reporting, 0, 500);
            esp_zb_scheduler_alarm(zb_send_device_announce, 0, 1200);
            esp_zb_scheduler_alarm(zb_report_ready, 0, 2200);
            zb_log_network_identity();
        } else {
            ESP_LOGW(TAG, "Reboot — network not restored/invalid, scheduling steering in 1s");
            s_ready_report_count = 0;
            s_zb_joined = false;
            led_set_pattern(LED_PATTERN_PAIRING);
            esp_zb_scheduler_alarm(zb_start_steering, 0, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            s_zb_joined = true;
            s_ready_acked = false;
            s_ready_report_count = 0;
            led_set_pattern(LED_PATTERN_IDLE_BREATHING);
            /* Force reporting refresh to rebuild coordinator forwarding tables */
            esp_zb_scheduler_alarm(zb_refresh_reporting, 0, 500);
            esp_zb_scheduler_alarm(zb_report_basic_info, 0, 700);
            esp_zb_scheduler_alarm(zb_send_device_announce, 0, 1200);
            esp_zb_scheduler_alarm(zb_report_ready, 0, 2200);
            esp_zb_ieee_addr_t addr;
            esp_zb_get_long_address(addr);
            ESP_LOGI(TAG,
                "Joined! IEEE: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                addr[7], addr[6], addr[5], addr[4],
                addr[3], addr[2], addr[1], addr[0]);
            zb_log_network_identity();
        } else {
            s_zb_joined = false;
            led_set_pattern(LED_PATTERN_PAIRING);
            ESP_LOGW(TAG, "Steering failed (%s) — retry in 2s", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm(zb_start_steering, 0, 2000);
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        s_zb_joined = false;
        led_set_pattern(LED_PATTERN_PAIRING);
        ESP_LOGI(TAG, "Left network — rejoining in 3s");
        esp_zb_scheduler_alarm(zb_start_steering, 0, 3000);
        break;

    default:
        ESP_LOGD(TAG, "Unhandled signal 0x%02x", sig_type);
        break;
    }
}

/* ============================================================
   ZIGBEE INIT — registers all endpoints + clusters
   ============================================================ */
static void zigbee_init(void)
{
    ESP_LOGI(TAG, "Initializing...");

    zb_load_recovery_marker();

    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role          = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy  = false,
        .nwk_cfg.zczr_cfg.max_children = 10,
    };
    esp_zb_init(&zb_cfg);

    /* Device identity strings (ZCL char string: [len][data]) */
    s_basic_manufacturer[0] = (uint8_t)strlen(ZB_MANUFACTURER_NAME);
    memcpy(s_basic_manufacturer + 1, ZB_MANUFACTURER_NAME, s_basic_manufacturer[0]);
    s_basic_model[0] = (uint8_t)strlen(ZB_MODEL_ID);
    memcpy(s_basic_model + 1, ZB_MODEL_ID, s_basic_model[0]);

    uint8_t zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
    uint8_t power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    /* ---- Endpoint 1: UI control + tile[0] action source ---- */
    {
        esp_zb_attribute_list_t *basic = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID,        &zcl_version);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID,       &power_source);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,  s_basic_manufacturer);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,   s_basic_model);

        esp_zb_attribute_list_t *identify      = esp_zb_identify_cluster_create(NULL);
        esp_zb_attribute_list_t *on_off_srv    = esp_zb_on_off_cluster_create(NULL);
        esp_zb_attribute_list_t *on_off_cli    = esp_zb_on_off_cluster_create(NULL);
        esp_zb_attribute_list_t *level_ctrl_srv = esp_zb_level_cluster_create(NULL);
        esp_zb_attribute_list_t *level_ctrl_cli = esp_zb_level_cluster_create(NULL);

        /* Custom cluster 0xFC11 */
        esp_zb_attribute_list_t *ui = esp_zb_zcl_attr_list_create(ZB_CUSTOM_CLUSTER_ID);

        /* 0x0000 — command payload (HA → ESP32, write-only) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_PAYLOAD_ID,
            ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
            ESP_ZB_ZCL_ATTR_ACCESS_WRITE_ONLY,
            s_cmd_buffer);

        /* 0x0001 — state report (ESP32 → HA, read + report) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_STATE_ID,
            ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
            s_action_buffer);

        /* 0x0010 — backlight level (UINT8, read+write, reporting) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_BACKLIGHT_ID,
            ESP_ZB_ZCL_ATTR_TYPE_U8,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
            &s_attr_backlight);

        /* 0x0011 — screen timeout seconds (UINT16, read+write) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_SCR_TIMEOUT_ID,
            ESP_ZB_ZCL_ATTR_TYPE_U16,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
            &s_attr_scr_timeout);

        /* 0x0012 — dim level (UINT8, read+write) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_DIM_LEVEL_ID,
            ESP_ZB_ZCL_ATTR_TYPE_U8,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
            &s_attr_dim_level);

        /* 0x0013 — night mode (BOOLEAN, read+write) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_NIGHT_MODE_ID,
            ESP_ZB_ZCL_ATTR_TYPE_BOOL,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
            &s_attr_night_mode);

        /* 0x0014 — night brightness (UINT8, read+write) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_NIGHT_BL_ID,
            ESP_ZB_ZCL_ATTR_TYPE_U8,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
            &s_attr_night_bl);

        /* 0x0015 — deep sleep enable (BOOLEAN, read+write) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_DEEP_SLEEP_ID,
            ESP_ZB_ZCL_ATTR_TYPE_BOOL,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
            &s_attr_deep_sleep);

        /* 0x0016 — sleep timeout seconds (UINT16, read+write) */
        esp_zb_cluster_add_attr(ui, ZB_CUSTOM_CLUSTER_ID, ZB_ATTR_SLEEP_TIMEOUT_ID,
            ESP_ZB_ZCL_ATTR_TYPE_U16,
            ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
            &s_attr_sleep_timeout);

        esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();
        esp_zb_cluster_list_add_basic_cluster(clusters,  basic,      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_identify_cluster(clusters, identify,       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_on_off_cluster(clusters, on_off_srv,       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_on_off_cluster(clusters, on_off_cli,       ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
        esp_zb_cluster_list_add_level_cluster(clusters,  level_ctrl_srv,   ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_level_cluster(clusters,  level_ctrl_cli,   ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
        esp_zb_cluster_list_add_custom_cluster(clusters, ui,         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

        esp_zb_endpoint_config_t ep_cfg = {
            .endpoint         = ZB_FIRST_ENDPOINT,
            .app_profile_id   = ESP_ZB_AF_HA_PROFILE_ID,
            .app_device_id    = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
            .app_device_version = 1,
        };
        esp_zb_ep_list_add_ep(ep_list, clusters, ep_cfg);
    }

    /* ---- Endpoints 2–6: action sources for tiles 1–5 ---- */
    for (uint8_t ep = ZB_FIRST_ENDPOINT + 1; ep < ZB_FIRST_ENDPOINT + MAX_TILES; ep++) {
        esp_zb_attribute_list_t *basic = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID,       &zcl_version);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID,      &power_source);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, s_basic_manufacturer);
        esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,  s_basic_model);

        esp_zb_attribute_list_t *identify      = esp_zb_identify_cluster_create(NULL);
        esp_zb_attribute_list_t *on_off_srv    = esp_zb_on_off_cluster_create(NULL);
        esp_zb_attribute_list_t *on_off_cli    = esp_zb_on_off_cluster_create(NULL);
        esp_zb_attribute_list_t *level_ctrl_srv = esp_zb_level_cluster_create(NULL);
        esp_zb_attribute_list_t *level_ctrl_cli = esp_zb_level_cluster_create(NULL);

        esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();
        esp_zb_cluster_list_add_basic_cluster(clusters,  basic,      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_identify_cluster(clusters, identify,      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_on_off_cluster(clusters, on_off_srv,      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_on_off_cluster(clusters, on_off_cli,      ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
        esp_zb_cluster_list_add_level_cluster(clusters,  level_ctrl_srv,  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_level_cluster(clusters,  level_ctrl_cli,  ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

        esp_zb_endpoint_config_t ep_cfg = {
            .endpoint           = ep,
            .app_profile_id     = ESP_ZB_AF_HA_PROFILE_ID,
            .app_device_id      = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
            .app_device_version = 1,
        };
        esp_zb_ep_list_add_ep(ep_list, clusters, ep_cfg);
    }

    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    ESP_LOGI(TAG, "Registered endpoints %d–%d, custom cluster 0x%04X",
             ZB_FIRST_ENDPOINT, ZB_FIRST_ENDPOINT + MAX_TILES - 1, ZB_CUSTOM_CLUSTER_ID);
}

/* ============================================================
   ZIGBEE TASK
   ============================================================ */
void zigbee_task(void *arg)
{
    ESP_LOGI(TAG, "Task started");
    zigbee_init();
    esp_zb_set_tx_power(ZB_TX_POWER);
    esp_zb_start(false);
    ESP_LOGI(TAG, "Scheduler loop running");
    while (1) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        esp_zb_main_loop_iteration();
#pragma GCC diagnostic pop
    }
}
