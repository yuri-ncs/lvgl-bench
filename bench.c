/* LVGL 8.4 vs 9.x rendering benchmark on the LilyGO T-Display-S3 (ST7789 320x170, 8-bit i80).
 * The same file builds on both versions; API differences live in the #if blocks and in scenes.h. */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "scenes.h"
#if LV_USE_DEMO_BENCHMARK
#include "lv_demos.h"
#endif

#define H_RES     320
#define V_RES     170
#define BUF_LINES 40      // partial buffer, two buffers, same on both versions
#define FRAMES    60      // frames measured per scene
#ifndef PCLK_HZ
#define PCLK_HZ 6528000 // LCD clock from the LilyGO example; the S3 only generates 80 MHz / n, so it becomes 6.67 MHz. Change with "bench.py run v9@20"
#endif
#define PIN_LCD_POWER 15      // LCD power (required on battery)
#define PIN_LCD_RD    9       // RD inactive
#define PIN_BACKLIGHT 38      // backlight
#define PIN_LCD_DC    7
#define PIN_LCD_WR    8
#define PIN_LCD_CS    6
#define PIN_LCD_RESET 5
#define PIN_LCD_DATA  { 39, 40, 41, 42, 45, 46, 47, 48 } // D0..D7

static esp_lcd_panel_handle_t panel;
static lv_display_t *lvgl_display;
static bool null_flush;            // true: discards the pixels, measures only the CPU rendering
static volatile uint32_t dma_sent; // only the task writes (flush_cb)
static volatile uint32_t dma_done; // only the DMA ISR writes (on_trans_done)
static int flush_count;

#if LVGL_VERSION_MAJOR < 9
static lv_disp_drv_t display_driver;
#define FLUSH_READY() lv_disp_flush_ready(&display_driver)
static void flush_cb(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
#else
#define FLUSH_READY() lv_display_flush_ready(lvgl_display)
static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
#endif
{
    flush_count++;
    if (null_flush) {
        FLUSH_READY();
        return;
    }
    dma_sent++;
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
}

static bool on_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *event_data, void *user_ctx)
{
    dma_done++;
    FLUSH_READY();
    return false;
}

/* LilyGO sequence for the new batch of screens (LCD_MODULE_CMD_1 in their examples);
 * without it garbage shows up on parts of the screen. */
static const struct {
    uint8_t command, length, data[14];
} lilygo_init[] = {
    { 0x3A, 1, { 0x05 } },
    { 0xB2, 5, { 0x0B, 0x0B, 0x00, 0x33, 0x33 } },
    { 0xB7, 1, { 0x75 } },
    { 0xBB, 1, { 0x28 } },
    { 0xC0, 1, { 0x2C } },
    { 0xC2, 1, { 0x01 } },
    { 0xC3, 1, { 0x1F } },
    { 0xC6, 1, { 0x13 } },
    { 0xD0, 1, { 0xA7 } },
    { 0xD0, 2, { 0xA4, 0xA1 } },
    { 0xD6, 1, { 0xA1 } },
    { 0xE0, 14, { 0xF0, 0x05, 0x0A, 0x06, 0x06, 0x03, 0x2B, 0x32, 0x43, 0x36, 0x11, 0x10, 0x2B, 0x32 } },
    { 0xE1, 14, { 0xF0, 0x08, 0x0C, 0x0B, 0x09, 0x24, 0x2B, 0x22, 0x43, 0x38, 0x15, 0x16, 0x2F, 0x37 } },
};

static void lcd_init(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << PIN_LCD_POWER | 1ULL << PIN_LCD_RD | 1ULL << PIN_BACKLIGHT, .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));
    gpio_set_level(PIN_LCD_POWER, 1);
    gpio_set_level(PIN_LCD_RD, 1);

    esp_lcd_i80_bus_handle_t bus;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = PIN_LCD_DC, .wr_gpio_num = PIN_LCD_WR, .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = PIN_LCD_DATA,
        .bus_width = 8, .max_transfer_bytes = H_RES * BUF_LINES * 2,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &bus));

    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_io_i80_config_t panel_io_config = {
        .cs_gpio_num = PIN_LCD_CS, .pclk_hz = PCLK_HZ, .trans_queue_depth = 10,
        .on_color_trans_done = on_trans_done,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .dc_levels = { .dc_data_level = 1 },
        .flags.swap_color_bytes = 1, // ST7789 wants big-endian RGB565; the LCD_CAM swaps in hardware, zero cost on both versions
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &panel_io_config, &panel_io));

    esp_lcd_panel_dev_config_t panel_config = { .reset_gpio_num = PIN_LCD_RESET, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_swap_xy(panel, true);
    esp_lcd_panel_mirror(panel, false, true);
    esp_lcd_panel_set_gap(panel, 0, 35);
    for (int i = 0; i < sizeof(lilygo_init) / sizeof(lilygo_init[0]); i++)
        esp_lcd_panel_io_tx_param(panel_io, lilygo_init[i].command, lilygo_init[i].data, lilygo_init[i].length);
    esp_lcd_panel_disp_on_off(panel, true);
    gpio_set_level(PIN_BACKLIGHT, 1);
}

static void lvgl_init(void)
{
    lv_init();
    size_t buffer_size = H_RES * BUF_LINES * sizeof(uint16_t);
    void *buffer1 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buffer2 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(buffer1 && buffer2 ? ESP_OK : ESP_ERR_NO_MEM);
#if LVGL_VERSION_MAJOR < 9
    static lv_disp_draw_buf_t draw_buffer;
    lv_disp_draw_buf_init(&draw_buffer, buffer1, buffer2, H_RES * BUF_LINES);
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = H_RES;
    display_driver.ver_res = V_RES;
    display_driver.flush_cb = flush_cb;
    display_driver.draw_buf = &draw_buffer;
    lvgl_display = lv_disp_drv_register(&display_driver);
#else
    lvgl_display = lv_display_create(H_RES, V_RES);
    lv_display_set_flush_cb(lvgl_display, flush_cb);
    lv_display_set_buffers(lvgl_display, buffer1, buffer2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
}

#if LV_USE_DEMO_BENCHMARK
static void tick_cb(void *arg)
{
    lv_tick_inc(1);
}
#endif

static int row_count;

/* Redraws the whole screen FRAMES times and sends each frame's time (us) over USB after the measurement. */
static void run(const scene_t *scene, bool to_lcd)
{
    static int32_t frame_us[FRAMES];
    scene_load(scene);
    null_flush = !to_lcd;
    lv_refr_now(lvgl_display); // first frame outside the measurement (layout, caches)
    while (dma_done != dma_sent) {}

    flush_count = 0;
    for (int frame = 0; frame < FRAMES; frame++) {
        scene_step(frame);
        lv_obj_invalidate(lv_screen_active());
        int64_t start_us = esp_timer_get_time();
        lv_refr_now(lvgl_display);
        while (dma_done != dma_sent) {}
        frame_us[frame] = esp_timer_get_time() - start_us;
    }
    // without this, an invalidate that produces no drawing would give an absurdly low time
    ESP_ERROR_CHECK(flush_count >= FRAMES ? ESP_OK : ESP_FAIL);
    for (int frame = 0; frame < FRAMES; frame++)
        printf("F,%s,%s,%d,%ld\n", scene->name, to_lcd ? "lcd" : "render", frame, (long)frame_us[frame]);
    row_count += FRAMES;
}

void app_main(void)
{
    lcd_init();
    lvgl_init();

#if LV_USE_DEMO_BENCHMARK
    // "demo" build: runs the official lv_demo_benchmark, which prints its own summary over USB when done.
    // It measures with the LVGL tick, which must advance while drawing: 1 ms esp_timer, same on both versions.
    esp_timer_handle_t tick_timer;
    const esp_timer_create_args_t tick_args = { .callback = tick_cb, .name = "lv_tick" };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));
#if LVGL_VERSION_MAJOR < 9
    lv_demo_benchmark_set_max_speed(true);
#endif
    lv_demo_benchmark();
    while (1)
        lv_timer_handler();
#endif

#if LVGL_VERSION_MAJOR >= 9
    int draw_units = LV_DRAW_SW_DRAW_UNIT_CNT;
#else
    int draw_units = 1;
#endif
    // repeats forever: bench.py on the PC picks up the next complete cycle, no board reset needed
    while (1) {
        printf("B,%d.%d.%d,%d\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, draw_units);
        row_count = 0;
        for (int i = 0; i < scene_count; i++) {
            run(&scenes[i], false);
            run(&scenes[i], true);
        }
        printf("E,%d\n", row_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
