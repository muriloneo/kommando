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

static bool s_rgb_override = false;
static uint8_t s_rgb_r = 0;
static uint8_t s_rgb_g = 0;
static uint8_t s_rgb_b = 0;

#define MIN_LED_TASK_DELAY_MS 20 // Always yield at least 20ms to avoid watchdog

static bool s_led_enabled;
static gpio_num_t s_led_gpio = GPIO_NUM_NC;
static bool s_led_active_high;
static bool s_led_ws2812;
static led_pattern_t s_pattern = LED_PATTERN_OFF;
static bool s_led_state_on;
static int64_t s_last_toggle_us;
static led_strip_handle_t s_led_strip;

static esp_err_t led_ws2812_apply(bool on)
{
  const uint8_t red = on ? 16 : 0;
  const uint8_t green = on ? 64 : 0;
  const uint8_t blue = on ? 255 : 0;

  ESP_RETURN_ON_ERROR(led_strip_set_pixel(s_led_strip, 0, red, green, blue), TAG, "WS2812 set pixel failed");
  ESP_RETURN_ON_ERROR(led_strip_refresh(s_led_strip), TAG, "WS2812 refresh failed");
  return ESP_OK;
}

static void led_apply_output(bool on)
{
  if (s_led_ws2812)
  {
    if (led_ws2812_apply(on) != ESP_OK)
    {
      ESP_LOGE(TAG, "WS2812 update failed, falling back to GPIO");
      s_led_ws2812 = false;
      // Try to configure GPIO output if not already done
      gpio_config_t io_cfg = {
          .pin_bit_mask = (1ULL << s_led_gpio),
          .mode = GPIO_MODE_OUTPUT,
          .pull_down_en = GPIO_PULLDOWN_DISABLE,
          .pull_up_en = GPIO_PULLUP_DISABLE,
          .intr_type = GPIO_INTR_DISABLE,
      };
      gpio_config(&io_cfg);
      // Continue to GPIO below
    }
    else
    {
      s_led_state_on = on;
      return;
    }
  }
  // Always allow GPIO fallback
  const int level = on ? (s_led_active_high ? 1 : 0) : (s_led_active_high ? 0 : 1);
  gpio_set_level(s_led_gpio, level);
  s_led_state_on = on;
}

static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
  uint8_t region = h / 43; // 256 / 6 ≈ 43
  uint8_t remainder = (h - (region * 43)) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region)
  {
  case 0:
    *r = v;
    *g = t;
    *b = p;
    break;
  case 1:
    *r = q;
    *g = v;
    *b = p;
    break;
  case 2:
    *r = p;
    *g = v;
    *b = t;
    break;
  case 3:
    *r = p;
    *g = q;
    *b = v;
    break;
  case 4:
    *r = t;
    *g = p;
    *b = v;
    break;
  default:
    *r = v;
    *g = p;
    *b = q;
    break;
  }
}

static int s_breath_speed_ms = 60; // Default breathing speed

void led_set_breath_speed(int ms)
{
  s_breath_speed_ms = ms;
}

static void led_task(void *arg)
{
  (void)arg;

  uint8_t breath_brightness = 0;
  int breath_direction = 1;
  const uint8_t breath_max = 64;
  const uint8_t breath_min = 4;
  const uint8_t breath_step = 2;
  while (true)
  {
    const int64_t now_us = esp_timer_get_time();
    if (s_rgb_override && s_led_ws2812)
    {
      led_strip_set_pixel(s_led_strip, 0, s_rgb_r, s_rgb_g, s_rgb_b);
      led_strip_refresh(s_led_strip);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (!s_led_enabled && !s_led_ws2812)
    {
      // If both are disabled, just yield
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    switch (s_pattern)
    {
    case LED_PATTERN_ERROR:
      if (s_led_ws2812)
      {
        led_strip_set_pixel(s_led_strip, 0, 64, 0, 0);
        led_strip_refresh(s_led_strip);
      }
      else
      {
        led_apply_output(true);
      }
      break;
    case LED_PATTERN_IDLE_BREATHING:
      if (s_led_ws2812)
      {
        breath_brightness += breath_direction * breath_step;
        if (breath_brightness >= breath_max)
        {
          breath_brightness = breath_max;
          breath_direction = -1;
        }
        else if (breath_brightness <= breath_min)
        {
          breath_brightness = breath_min;
          breath_direction = 1;
        }
        static uint8_t hue = 0;
        hue++;
        uint8_t r, g, b;
        hsv_to_rgb(hue, 255, breath_brightness, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
      }
      else
      {
        led_apply_output(true);
      }
      break;
    case LED_PATTERN_PAIRING:
      if ((now_us - s_last_toggle_us) >= 120000)
      {
        if (s_led_ws2812)
        {
          static bool pulse_on = false;
          pulse_on = !pulse_on;
          if (pulse_on)
          {
            led_strip_set_pixel(s_led_strip, 0, 64, 64, 64);
          }
          else
          {
            led_strip_set_pixel(s_led_strip, 0, 0, 0, 0);
          }
          led_strip_refresh(s_led_strip);
        }
        else
        {
          led_apply_output(!s_led_state_on);
        }
        s_last_toggle_us = now_us;
      }
      break;
    case LED_PATTERN_OFF:
      led_apply_output(false);
      break;
    case LED_PATTERN_SOLID_ON:
      led_set_rgb(50, 50, 50);
      led_apply_output(true);
      break;
    default:
      break;
    }
    // Always yield at least MIN_LED_TASK_DELAY_MS
    int delay_ms = s_breath_speed_ms < MIN_LED_TASK_DELAY_MS ? MIN_LED_TASK_DELAY_MS : s_breath_speed_ms;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

esp_err_t led_init(void)
{
  s_led_gpio = PIN_STATUS_LED;
  s_led_active_high = true;  /* Active high for WS2812 */
  s_led_enabled = true;
  s_led_ws2812 = false;
  s_led_strip = NULL;
  s_led_state_on = false;
  s_last_toggle_us = esp_timer_get_time();

  /* Initialize WS2812 RGB LED */
  led_strip_config_t strip_cfg = {
      .strip_gpio_num = s_led_gpio,
      .max_leds = 1,
  };
  led_strip_rmt_config_t rmt_cfg = {
      .resolution_hz = 10 * 1000 * 1000,
  };
  esp_err_t ws_err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_strip);
  if (ws_err == ESP_OK)
  {
    s_led_ws2812 = true;
    ESP_LOGI(TAG, "Status LED ready on WS2812 GPIO %d", (int)s_led_gpio);
  }
  else
  {
    ESP_LOGW(TAG, "WS2812 init failed (%s), falling back to GPIO mode", esp_err_to_name(ws_err));
  }

  if (!s_led_ws2812)
  {
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << s_led_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), TAG, "Failed to configure status LED GPIO");
    ESP_LOGI(TAG, "Status LED ready on GPIO %d", (int)s_led_gpio);
  }

  led_apply_output(false);

  BaseType_t task_ok = xTaskCreate(led_task, "remote_led", 2048, NULL, 4, NULL);
  ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_FAIL, TAG, "Failed to create LED task");
  return ESP_OK;
}

void led_set_pattern(led_pattern_t pattern)
{
  // Only update the pattern variable here. Do not perform hardware
  // operations from caller tasks because those may run concurrently
  // with the LED driver and can trigger RMT errors (channel not in
  // init state). The led_task is the sole owner of the LED hardware
  // and will apply the new pattern on its next loop iteration.
  s_rgb_override = false; // ← ADD THIS
  s_pattern = pattern;
  s_last_toggle_us = esp_timer_get_time();
}

bool led_is_available(void)
{
  return s_led_enabled;
}

void led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    // Cap brightness to ~15% to prevent screen brownouts
    r = r > 40 ? 40 : r;
    g = g > 40 ? 40 : g;
    b = b > 40 ? 40 : b;
    
    // Use the actual variable name (e.g. s_led_strip)
    led_strip_set_pixel(s_led_strip, 0, r, g, b);
    led_strip_refresh(s_led_strip);
}

void led_clear_rgb_override(void)
{
  s_rgb_override = false;
}