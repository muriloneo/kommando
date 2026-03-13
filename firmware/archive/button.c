#include "button.h"
#include "main.h"
#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "button";

// Array of pulsation speeds in ms (small set; index cycles)
static const int pulsation_speeds[4] = {5, 10, 15, 30};
static int pulsation_index = 0;

extern void led_set_breath_speed(int ms);

// Button handling configuration
static const int LONG_PRESS_MS = 600;
static const int POLL_MS = 50;

// Button modes for clear dispatch
typedef enum {
    BTN_MODE_BREATHING = 0,
    BTN_MODE_SOLID_ON,
    BTN_MODE_SOLID_OFF,
} btn_mode_t;

// Internal state (owned by button task)
static btn_mode_t s_mode = BTN_MODE_BREATHING;

// Helpers — small focused functions for each action
static void enter_breathing_mode(void)
{
    s_mode = BTN_MODE_BREATHING;
    led_set_pattern(LED_PATTERN_IDLE_BREATHING);
}

static void enter_solid_on_mode(void)
{
    s_mode = BTN_MODE_SOLID_ON;
    led_set_pattern(LED_PATTERN_SOLID_ON);
}

static void enter_solid_off_mode(void)
{
    s_mode = BTN_MODE_SOLID_OFF;
    led_set_pattern(LED_PATTERN_OFF);
}

static void cycle_breath_speed(void)
{
    pulsation_index = (pulsation_index + 1) % (int)(sizeof(pulsation_speeds) / sizeof(pulsation_speeds[0]));
    int speed = pulsation_speeds[pulsation_index];
    ESP_LOGI(TAG, "Cycle breath speed -> %d ms (index %d)", speed, pulsation_index);
    led_set_breath_speed(speed);
    // ensure breathing pattern is active
    enter_breathing_mode();
}

static void toggle_solid(void)
{
    if (s_mode == BTN_MODE_SOLID_ON) {
        ESP_LOGI(TAG, "Short press: Solid OFF");
        enter_solid_off_mode();
    } else {
        ESP_LOGI(TAG, "Short press: Solid ON");
        enter_solid_on_mode();
    }
}

static void handle_short_press(void)
{
    switch (s_mode) {
    case BTN_MODE_BREATHING:
        cycle_breath_speed();
        break;
    case BTN_MODE_SOLID_ON:
    case BTN_MODE_SOLID_OFF:
        toggle_solid();
        break;
    default:
        // fallback: ensure breathing
        enter_breathing_mode();
        break;
    }
}

static void handle_long_press(void)
{
    // Toggle between breathing and solid (default to solid ON)
    if (s_mode == BTN_MODE_BREATHING) {
        ESP_LOGI(TAG, "Long press: switch to SOLID (default ON)");
        enter_solid_on_mode();
    } else {
        ESP_LOGI(TAG, "Long press: switch to BREATHING");
        enter_breathing_mode();
    }
}

static void button_task(void *arg)
{
    (void)arg;
    int press_time = 0;
    bool was_pressed = false;

    // Start with breathing mode (consistent with app_main)
    enter_breathing_mode();

    while (1) {
        bool pressed = gpio_get_level(REMOTE_BUTTON_GPIO) == REMOTE_BUTTON_ACTIVE_LEVEL;
        if (pressed) {
            press_time += POLL_MS;
        } else {
            if (was_pressed) {
                ESP_LOGI(TAG, "Button released after %d ms", press_time);
                if (press_time >= LONG_PRESS_MS) {
                    handle_long_press();
                } else if (press_time > 0) {
                    handle_short_press();
                }
            }
            press_time = 0;
        }
        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void button_init(void)
{
    // Configure input; keep existing minimal setup but ensure no accidental
    // pull-ups are forced here (board wiring may provide one).
    gpio_set_direction(REMOTE_BUTTON_GPIO, GPIO_MODE_INPUT);
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
}
