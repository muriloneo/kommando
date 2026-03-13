#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"

#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "lcd_demo.h"

/* -----------------------------------------------------------
   PINS (From your known-good configuration)
----------------------------------------------------------- */
#define LCD_GPIO_BL    GPIO_NUM_2
#define LCD_GPIO_RST   GPIO_NUM_3
#define LCD_GPIO_DC    GPIO_NUM_11
#define LCD_GPIO_CS    GPIO_NUM_10
#define LCD_GPIO_CLK   GPIO_NUM_6
#define LCD_GPIO_MOSI  GPIO_NUM_7

#define TOUCH_GPIO_SDA GPIO_NUM_0
#define TOUCH_GPIO_SCL GPIO_NUM_1
#define TOUCH_GPIO_RST GPIO_NUM_20
/* We will NOT use the IRQ pin, we will poll via I2C only */
#define TOUCH_GPIO_IRQ -1

/* -----------------------------------------------------------
   DISPLAY CONFIG
----------------------------------------------------------- */
#define LCD_H_RES          172
#define LCD_V_RES          320
#define LCD_SPI_HOST       SPI2_HOST
#define LCD_SPI_CLK_HZ     (40 * 1000 * 1000)
#define LCD_CMD_BITS       8
#define LCD_PARAM_BITS     8
#define LCD_DRAW_BUF_LINES 40

/* Crucial for 172x320 ST7789 panels to avoid the "cut off" / noise strips */
#define LCD_OFFSET_X       34
#define LCD_OFFSET_Y       0

/* -----------------------------------------------------------
   TOUCH CONFIG
----------------------------------------------------------- */
#define TOUCH_I2C_CLK_HZ 400000

static const char *TAG = "LCD_DEMO";

static lv_display_t *s_disp = NULL;
static lv_indev_t *s_indev = NULL;
static esp_lcd_touch_handle_t s_tp = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static lv_obj_t *s_circle = NULL;

/* -----------------------------------------------------------
   LVGL EVENT
----------------------------------------------------------- */
static void circle_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "Circle tapped");
    }
}

/* -----------------------------------------------------------
   SIMPLE UI
----------------------------------------------------------- */
static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_set_style_bg_color(scr, lv_color_make(0x10, 0x18, 0x2A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Circle in the center */
    s_circle = lv_obj_create(scr);
    lv_obj_set_size(s_circle, 80, 80);
    lv_obj_align(s_circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_circle, lv_color_make(0xFF, 0x5E, 0x57), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_circle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_circle, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_circle, circle_event_cb, LV_EVENT_CLICKED, NULL);

    /* Text above */
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Tap the circle");
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -70);
}

/* -----------------------------------------------------------
   LCD TASK
----------------------------------------------------------- */
static void lcd_task(void *arg)
{
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << LCD_GPIO_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));

    /* Backlight ON (Level 0 for your active-low board) */
    gpio_set_level(LCD_GPIO_BL, 0);
    ESP_LOGI(TAG, "Backlight ON (Level 0)");

    /* SPI bus */
    spi_bus_config_t bus = {
        .mosi_io_num = LCD_GPIO_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_GPIO_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUF_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    /* LCD panel IO */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_GPIO_DC,
        .cs_gpio_num = LCD_GPIO_CS,
        .pclk_hz = LCD_SPI_CLK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 2, /* Mode 2/3 is often more stable for ST7789 */
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    /* ST7789 panel */
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_GPIO_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    
    /* Display orientation and offsets */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, LCD_OFFSET_X, LCD_OFFSET_Y));
    
    // Set to native orientation at driver level
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    
    ESP_LOGI(TAG, "LCD ready %dx%d", LCD_H_RES, LCD_V_RES);

    /* I2C master bus */
    i2c_master_bus_config_t i2c_bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = TOUCH_GPIO_SCL,
        .sda_io_num = TOUCH_GPIO_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &s_i2c_bus));

    /* CST816S touch, POLLING ONLY */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg =
        ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = TOUCH_I2C_CLK_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_GPIO_RST,
        .int_gpio_num = TOUCH_GPIO_IRQ,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0}, // Native
        .interrupt_callback = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &s_tp));
    ESP_LOGI(TAG, "Touch ready");

    /* LVGL */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * LCD_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false, // Keep everything native, we adjust if it's upside down later
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };

    s_disp = lvgl_port_add_disp(&disp_cfg);
    configASSERT(s_disp);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = s_disp,
        .handle = s_tp,
    };
    s_indev = lvgl_port_add_touch(&touch_cfg);
    configASSERT(s_indev);

    ESP_LOGI(TAG, "LVGL ready — building UI");
    vTaskDelay(pdMS_TO_TICKS(100));

    if (lvgl_port_lock(portMAX_DELAY))
    {
        /* Clear any potential GRAM noise before building UI */
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
        lv_refr_now(s_disp);
        
        build_ui();
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "UI ready — tap circle to log");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -----------------------------------------------------------
   PUBLIC ENTRY — called from app_main in main.c
----------------------------------------------------------- */
void lcd_demo_start(void)
{
    xTaskCreate(lcd_task, "lcd_task", 8192, NULL, 5, NULL);
}
