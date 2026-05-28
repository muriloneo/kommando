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

/*
 * Status LED driver.
 *
 * Purpose: visual feedback for Zigbee pairing / connection / error states,
 * plus optional arbitrary RGB override (e.g. tile-driven color).
 *
 * Concurrency model: led_task is the SOLE owner of the LED hardware (RMT
 * channel for WS2812, or plain GPIO fallback). All public setters only
 * mutate state + raise a dirty flag; the task writes pixels on its next
 * tick. This avoids cross-task RMT races that were observed previously.
 *
 * Power model: for static patterns (OFF / SOLID_ON / ERROR / RGB override)
 * the task only re-emits pixels when state actually changed. Animated
 * patterns (PAIRING / IDLE_BREATHING) re-emit every tick.
 */

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "config.h"
#include "led.h"

static const char *TAG = "led";

/* ---- Tunables ---------------------------------------------------------- */

#define LED_TASK_TICK_MS        20     /* base tick; also feeds the watchdog */
#define LED_TASK_IDLE_TICK_MS  100     /* when both WS2812 and GPIO disabled */
#define LED_PAIRING_PERIOD_US  120000  /* 120 ms toggle */
#define LED_BREATH_MIN           4
#define LED_BREATH_MAX          64
#define LED_BREATH_STEP          2
#define LED_BRIGHTNESS_CAP      40     /* ~15%, prevents screen brownout */

/* Pattern color presets (post brightness-cap is not applied here; these
 * are already chosen safe values). */
#define LED_SOLID_R 16
#define LED_SOLID_G 64
#define LED_SOLID_B 255
#define LED_ERROR_R 64
#define LED_PAIRING_LEVEL 64

/* ---- State (owned by led_task except where noted) ---------------------- */

static gpio_num_t       s_gpio          = GPIO_NUM_NC;
static bool             s_active_high   = true;
static bool             s_ws2812        = false;
static bool             s_enabled       = false;
static led_strip_handle_t s_strip       = NULL;

static led_pattern_t    s_pattern       = LED_PATTERN_OFF;
static bool             s_rgb_override  = false;
static uint8_t          s_rgb_r         = 0;
static uint8_t          s_rgb_g         = 0;
static uint8_t          s_rgb_b         = 0;
static volatile bool    s_dirty         = true;   /* force first paint */

static uint32_t         s_breath_speed_ms = 60;
static int64_t          s_last_toggle_us  = 0;
static bool             s_output_on       = false;

/* ---- Helpers ----------------------------------------------------------- */

static inline uint8_t cap(uint8_t v) { return v > LED_BRIGHTNESS_CAP ? LED_BRIGHTNESS_CAP : v; }

static void write_rgb(uint8_t r, uint8_t g, uint8_t b)
{
  if (s_ws2812 && s_strip) {
    if (led_strip_set_pixel(s_strip, 0, r, g, b) == ESP_OK) {
      led_strip_refresh(s_strip);
      s_output_on = (r || g || b);
      return;
    }
    ESP_LOGE(TAG, "WS2812 write failed, falling back to GPIO");
    s_ws2812 = false;
  }
  if (s_gpio != GPIO_NUM_NC) {
    const bool on = (r || g || b);
    gpio_set_level(s_gpio, on ? (s_active_high ? 1 : 0) : (s_active_high ? 0 : 1));
    s_output_on = on;
  }
}

static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
  const uint8_t region    = h / 43;                 /* 256 / 6 ≈ 43 */
  const uint8_t remainder = (h - (region * 43)) * 6;
  const uint8_t p = (v * (255 - s)) >> 8;
  const uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  const uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0:  *r = v; *g = t; *b = p; break;
    case 1:  *r = q; *g = v; *b = p; break;
    case 2:  *r = p; *g = v; *b = t; break;
    case 3:  *r = p; *g = q; *b = v; break;
    case 4:  *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}

/* ---- Per-pattern step (returns true if pattern is animated this tick) -- */

static bool step_pattern(int64_t now_us)
{
  /* RGB override wins over pattern. */
  if (s_rgb_override) {
    if (s_dirty) { write_rgb(s_rgb_r, s_rgb_g, s_rgb_b); s_dirty = false; }
    return false;
  }

  switch (s_pattern) {
    case LED_PATTERN_OFF:
      if (s_dirty) { write_rgb(0, 0, 0); s_dirty = false; }
      return false;

    case LED_PATTERN_SOLID_ON:
      if (s_dirty) { write_rgb(LED_SOLID_R, LED_SOLID_G, LED_SOLID_B); s_dirty = false; }
      return false;

    case LED_PATTERN_ERROR:
      if (s_dirty) { write_rgb(LED_ERROR_R, 0, 0); s_dirty = false; }
      return false;

    case LED_PATTERN_PAIRING:
      if ((now_us - s_last_toggle_us) >= LED_PAIRING_PERIOD_US) {
        const uint8_t v = s_output_on ? 0 : LED_PAIRING_LEVEL;
        write_rgb(v, v, v);
        s_last_toggle_us = now_us;
      }
      return true;

    case LED_PATTERN_IDLE_BREATHING: {
      static uint8_t hue        = 0;
      static uint8_t brightness = LED_BREATH_MIN;
      static int8_t  direction  = 1;

      brightness = (uint8_t)(brightness + direction * LED_BREATH_STEP);
      if (brightness >= LED_BREATH_MAX) { brightness = LED_BREATH_MAX; direction = -1; }
      else if (brightness <= LED_BREATH_MIN) { brightness = LED_BREATH_MIN; direction =  1; }
      hue++;

      uint8_t r, g, b;
      hsv_to_rgb(hue, 255, brightness, &r, &g, &b);
      write_rgb(r, g, b);
      return true;
    }

    default:
      return false;
  }
}

/* ---- Task -------------------------------------------------------------- */

static void led_task(void *arg)
{
  (void)arg;
  while (true) {
    if (!s_enabled && !s_ws2812) {
      vTaskDelay(pdMS_TO_TICKS(LED_TASK_IDLE_TICK_MS));
      continue;
    }
    const int64_t now_us = esp_timer_get_time();
    const bool animated = step_pattern(now_us);

    uint32_t delay_ms = animated ? s_breath_speed_ms : LED_TASK_TICK_MS;
    if (delay_ms < LED_TASK_TICK_MS) delay_ms = LED_TASK_TICK_MS;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

/* ---- Public API -------------------------------------------------------- */

esp_err_t led_init(void)
{
  s_gpio           = PIN_STATUS_LED;
  s_active_high    = true;
  s_enabled        = true;
  s_ws2812         = false;
  s_strip          = NULL;
  s_output_on      = false;
  s_last_toggle_us = esp_timer_get_time();
  s_dirty          = true;

  const led_strip_config_t strip_cfg = { .strip_gpio_num = s_gpio, .max_leds = 1 };
  const led_strip_rmt_config_t rmt_cfg = { .resolution_hz = 10 * 1000 * 1000 };
  const esp_err_t ws_err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
  if (ws_err == ESP_OK) {
    s_ws2812 = true;
    ESP_LOGI(TAG, "Status LED ready on WS2812 GPIO %d", (int)s_gpio);
  } else {
    ESP_LOGW(TAG, "WS2812 init failed (%s), falling back to GPIO", esp_err_to_name(ws_err));
    const gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << s_gpio),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), TAG, "Failed to configure status LED GPIO");
    ESP_LOGI(TAG, "Status LED ready on GPIO %d", (int)s_gpio);
  }

  /* Paint OFF immediately so the LED is in a known state. */
  write_rgb(0, 0, 0);

  const BaseType_t task_ok = xTaskCreate(led_task, "remote_led", 2048, NULL, 4, NULL);
  ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_FAIL, TAG, "Failed to create LED task");
  return ESP_OK;
}

void led_set_pattern(led_pattern_t pattern)
{
  /* Setters never touch hardware — only mutate state + flag dirty. */
  s_rgb_override   = false;
  s_pattern        = pattern;
  s_last_toggle_us = esp_timer_get_time();
  s_dirty          = true;
}

void led_set_breath_speed(int ms)
{
  if (ms < 0) ms = 0;
  s_breath_speed_ms = (uint32_t)ms;
}

bool led_is_available(void)
{
  return s_enabled;
}

void led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
  s_rgb_r        = cap(r);
  s_rgb_g        = cap(g);
  s_rgb_b        = cap(b);
  s_rgb_override = true;
  s_dirty        = true;
}

void led_clear_rgb_override(void)
{
  s_rgb_override = false;
  s_dirty        = true;
}
