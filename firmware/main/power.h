/*
 * Copyright (C) 2026 Murilo Freire
 * power.h — pre-sleep ritual + wake-cause helpers (ESP32-C6).
 */
#pragma once

#include <stdbool.h>
#include "esp_sleep.h"

/* Latched at boot from esp_sleep_get_wakeup_cause().
 * Useful to skip cosmetic startup (pairing LED, splash) on wake. */
extern esp_sleep_wakeup_cause_t g_wakeup_cause;

/* True if this boot was caused by a deep-sleep wake (any source). */
bool power_woke_from_deep_sleep(void);

/* Log the wake cause once. Call early in app_main. */
void power_log_wakeup_cause(void);

/* Configure GPIO holds + pull-ups so peripherals stay in a safe state
 * (LCD blanked, backlight off, touch INT pulled up) across deep sleep,
 * then enables deep_sleep_hold so holds survive the sleep transition.
 * Call IMMEDIATELY before esp_deep_sleep_start(). */
void power_prepare_deep_sleep(void);

/* Enable touch INT as the wake source. Returns false if the pin is not
 * a valid LP-IO wake on this SoC. Uses the C6-native LP-IO API first
 * with an ext1 fallback for older IDFs. */
bool power_enable_touch_wakeup(void);
