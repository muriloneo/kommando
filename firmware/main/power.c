/*
 * Copyright (C) 2026 Murilo Freire
 * power.c — ESP32-C6 deep-sleep prep + wake-cause helpers.
 *
 * KEY ESP32-C6 NOTES:
 *  - LP_IO range for deep-sleep wake is GPIO0..GPIO7. PIN_TOUCH_INT=GPIO4 ✓
 *  - Backlight (GPIO2) is active-low: must be driven HIGH and HELD across
 *    sleep, otherwise LEDC stops and the pin floats causing wake-flash.
 *  - SPI/DC/CS/RST pins are NOT LP_IO; they go HiZ in deep sleep. We
 *    leave the LCD in SLPIN+DISPOFF (~10 µA) so HiZ on SPI is acceptable.
 *  - C6 has SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP, so per-pin
 *    gpio_hold_en() survives deep sleep without a global hold call.
 */

#include "power.h"
#include "config.h"
#include "led.h"

#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

static const char *TAG = "power";

esp_sleep_wakeup_cause_t g_wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;

bool power_woke_from_deep_sleep(void)
{
    switch (g_wakeup_cause) {
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
    case ESP_SLEEP_WAKEUP_TIMER:
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
    case ESP_SLEEP_WAKEUP_ULP:
    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_UART:
        return true;
    default:
        return false;
    }
}

void power_log_wakeup_cause(void)
{
    /* IDF v6 returns a bitmap; convert to first matching cause for logging. */
    uint32_t causes = esp_sleep_get_wakeup_causes();
    if (causes & (1U << ESP_SLEEP_WAKEUP_EXT1))          g_wakeup_cause = ESP_SLEEP_WAKEUP_EXT1;
    else if (causes & (1U << ESP_SLEEP_WAKEUP_EXT0))     g_wakeup_cause = ESP_SLEEP_WAKEUP_EXT0;
    else if (causes & (1U << ESP_SLEEP_WAKEUP_GPIO))     g_wakeup_cause = ESP_SLEEP_WAKEUP_GPIO;
    else if (causes & (1U << ESP_SLEEP_WAKEUP_TIMER))    g_wakeup_cause = ESP_SLEEP_WAKEUP_TIMER;
    else if (causes & (1U << ESP_SLEEP_WAKEUP_TOUCHPAD)) g_wakeup_cause = ESP_SLEEP_WAKEUP_TOUCHPAD;
    else if (causes & (1U << ESP_SLEEP_WAKEUP_UART))     g_wakeup_cause = ESP_SLEEP_WAKEUP_UART;
    else if (causes & (1U << ESP_SLEEP_WAKEUP_ULP))      g_wakeup_cause = ESP_SLEEP_WAKEUP_ULP;
    else                                                 g_wakeup_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
    const char *name = "POWER_ON / RESET";
    switch (g_wakeup_cause) {
    case ESP_SLEEP_WAKEUP_EXT0:     name = "EXT0";     break;
    case ESP_SLEEP_WAKEUP_EXT1:     name = "EXT1";     break;
    case ESP_SLEEP_WAKEUP_TIMER:    name = "TIMER";    break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: name = "TOUCHPAD"; break;
    case ESP_SLEEP_WAKEUP_ULP:      name = "ULP";      break;
    case ESP_SLEEP_WAKEUP_GPIO:     name = "GPIO (LP_IO)"; break;
    case ESP_SLEEP_WAKEUP_UART:     name = "UART";     break;
    default: break;
    }
    ESP_LOGI(TAG, "Wakeup cause: %s", name);
}

static void hold_pin_at(gpio_num_t pin, int level)
{
    /* Drop any driver attachment so we own the pin. */
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);
    gpio_hold_en(pin);
}

void power_prepare_deep_sleep(void)
{
    ESP_LOGI(TAG, "Preparing deep sleep (hold backlight OFF, blank LED, pull-up touch INT)");

    /* 1. Blank the WS2812 so it doesn't latch on while we sleep.
     *    led_set_pattern(OFF) lets the RMT driver send a black frame. */
    led_set_pattern(LED_PATTERN_OFF);

    /* 2. Drive backlight pin HIGH (=off, circuit is active-low) and hold.
     *    Prevents the brief wake-flash when LEDC re-initialises. C6's
     *    per-pin hold survives deep sleep without a global hold call. */
    hold_pin_at(PIN_LCD_BL, 1);

    /* 3. Internal pull-up on touch INT so it does not float while CST816
     *    is in low-power and the line is between assert pulses. */
    gpio_set_pull_mode(PIN_TOUCH_INT, GPIO_PULLUP_ONLY);

#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    /* On SoCs without per-pin deep-sleep hold (not C6), enable the
     * global deep-sleep hold so the per-pin holds survive. */
    gpio_deep_sleep_hold_en();
#endif
}

bool power_enable_touch_wakeup(void)
{
    if (!esp_sleep_is_valid_wakeup_gpio(PIN_TOUCH_INT)) {
        ESP_LOGE(TAG, "GPIO%d is not valid as a deep-sleep wake source on this SoC",
                 (int)PIN_TOUCH_INT);
        return false;
    }

    /* On C6 in IDF v6.0, ext1 (LP_IO) is the public path for deep-sleep
     * GPIO wake. The level-low option corresponds to CST816 INT pulse. */
    esp_err_t err = esp_sleep_enable_ext1_wakeup((1ULL << PIN_TOUCH_INT),
                                                 ESP_EXT1_WAKEUP_ANY_LOW);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wake source: ext1 GPIO%d ANY_LOW", (int)PIN_TOUCH_INT);
        return true;
    }
    ESP_LOGE(TAG, "Failed to register wake source: %s", esp_err_to_name(err));
    return false;
}
