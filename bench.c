/* Benchmark de renderização LVGL 8.4 x 9.x no LilyGO T-Display-S3 (ST7789 320x170, i80 8 bits).
 * O mesmo arquivo compila nas duas versões; as diferenças de API ficam nos #if e em scenes.h. */
#include <stdio.h>
#include <stdatomic.h>
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
#define BUF_LINES 40      // buffer parcial, dois buffers, igual nas duas versões
#define FRAMES    60      // quadros medidos por cena
#ifndef PCLK_HZ
#define PCLK_HZ 6528000 // clock do LCD do exemplo da LilyGO; o S3 só gera 80 MHz / n, então vira 6,67 MHz. Mude com "bench.py run v9@20"
#endif

static esp_lcd_panel_handle_t panel;
static lv_display_t *disp;
static bool null_flush;    // true: descarta os pixels, mede só a CPU renderizando
static atomic_int pending; // transferências DMA ainda em andamento
static int flushes;

#if LVGL_VERSION_MAJOR < 9
static lv_disp_drv_t drv;
#define FLUSH_READY() lv_disp_flush_ready(&drv)
static void flush_cb(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *px)
#else
#define FLUSH_READY() lv_display_flush_ready(disp)
static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
#endif
{
    flushes++;
    if (null_flush) {
        FLUSH_READY();
        return;
    }
    pending++;
    esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
}

static bool on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    pending--;
    FLUSH_READY();
    return false;
}

/* Sequência da LilyGO para o lote novo de telas (LCD_MODULE_CMD_1 nos exemplos deles);
 * sem ela aparece lixo em partes da tela. */
static const struct {
    uint8_t cmd, len, data[14];
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
    gpio_config_t out = { .pin_bit_mask = 1ULL << 15 | 1ULL << 9 | 1ULL << 38, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(15, 1); // alimentação do LCD (necessária na bateria)
    gpio_set_level(9, 1);  // RD inativo

    esp_lcd_i80_bus_handle_t bus;
    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num = 7, .wr_gpio_num = 8, .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = { 39, 40, 41, 42, 45, 46, 47, 48 },
        .bus_width = 8, .max_transfer_bytes = H_RES * BUF_LINES * 2,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &bus));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = 6, .pclk_hz = PCLK_HZ, .trans_queue_depth = 10,
        .on_color_trans_done = on_trans_done,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .dc_levels = { .dc_data_level = 1 },
        .flags.swap_color_bytes = 1, // ST7789 quer RGB565 big-endian; o LCD_CAM troca em hardware, custo zero nas duas versões
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));

    esp_lcd_panel_dev_config_t pcfg = { .reset_gpio_num = 5, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &pcfg, &panel));
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_swap_xy(panel, true);
    esp_lcd_panel_mirror(panel, false, true);
    esp_lcd_panel_set_gap(panel, 0, 35);
    for (int i = 0; i < sizeof(lilygo_init) / sizeof(lilygo_init[0]); i++)
        esp_lcd_panel_io_tx_param(io, lilygo_init[i].cmd, lilygo_init[i].data, lilygo_init[i].len);
    esp_lcd_panel_disp_on_off(panel, true);
    gpio_set_level(38, 1); // backlight
}

static void lvgl_init(void)
{
    lv_init();
    size_t sz = H_RES * BUF_LINES * sizeof(uint16_t);
    void *b1 = heap_caps_malloc(sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *b2 = heap_caps_malloc(sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(b1 && b2 ? ESP_OK : ESP_ERR_NO_MEM);
#if LVGL_VERSION_MAJOR < 9
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, b1, b2, H_RES * BUF_LINES);
    lv_disp_drv_init(&drv);
    drv.hor_res = H_RES;
    drv.ver_res = V_RES;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &draw_buf;
    disp = lv_disp_drv_register(&drv);
#else
    disp = lv_display_create(H_RES, V_RES);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, b1, b2, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
}

#if LV_USE_DEMO_BENCHMARK
static void tick_cb(void *arg)
{
    lv_tick_inc(1);
}
#endif

static int rows;

/* Redesenha a tela inteira FRAMES vezes e manda o tempo de cada quadro (us) pela USB depois da medida. */
static void run(const scene_t *sc, bool to_lcd)
{
    static int32_t us[FRAMES];
    scene_load(sc);
    null_flush = !to_lcd;
    lv_refr_now(disp); // primeiro quadro fora da medida (layout, caches)
    while (pending) {}

    flushes = 0;
    for (int f = 0; f < FRAMES; f++) {
        scene_step(f);
        lv_obj_invalidate(lv_screen_active());
        int64_t t0 = esp_timer_get_time();
        lv_refr_now(disp);
        while (pending) {}
        us[f] = esp_timer_get_time() - t0;
    }
    // sem isso um invalidate que não gera desenho daria um tempo absurdo de baixo
    ESP_ERROR_CHECK(flushes >= FRAMES ? ESP_OK : ESP_FAIL);
    for (int f = 0; f < FRAMES; f++)
        printf("F,%s,%s,%d,%ld\n", sc->name, to_lcd ? "lcd" : "render", f, (long)us[f]);
    rows += FRAMES;
}

void app_main(void)
{
    lcd_init();
    lvgl_init();

#if LV_USE_DEMO_BENCHMARK
    // build "demo": roda o lv_demo_benchmark oficial, que imprime o próprio resumo pela USB ao terminar.
    // Ele mede com o tick do LVGL, que precisa andar durante o desenho: esp_timer de 1 ms, igual nas duas versões.
    esp_timer_handle_t tick;
    const esp_timer_create_args_t tick_args = { .callback = tick_cb, .name = "lv_tick" };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, 1000));
#if LVGL_VERSION_MAJOR < 9
    lv_demo_benchmark_set_max_speed(true);
#endif
    lv_demo_benchmark();
    while (1)
        lv_timer_handler();
#endif

#if LVGL_VERSION_MAJOR >= 9
    int units = LV_DRAW_SW_DRAW_UNIT_CNT;
#else
    int units = 1;
#endif
    // repete para sempre: o bench.py no PC pega o próximo ciclo completo, sem precisar resetar a placa
    while (1) {
        printf("B,%d.%d.%d,%d\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, units);
        rows = 0;
        for (int i = 0; i < scene_count; i++) {
            run(&scenes[i], false);
            run(&scenes[i], true);
        }
        printf("E,%d\n", rows);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
