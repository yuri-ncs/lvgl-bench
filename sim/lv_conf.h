/* Só o que difere do padrão do LVGL 9; o resto vem de lv_conf_internal.h. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16 /* igual ao ST7789 da placa */
#define LV_MEM_SIZE    (256 * 1024)

#define LV_USE_SDL 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_28 1

#define LV_USE_DEMO_WIDGETS   1
#define LV_USE_DEMO_BENCHMARK 1
#define LV_USE_SYSMON         1
#define LV_USE_PERF_MONITOR   1

#define LV_USE_LOG    1
#define LV_LOG_PRINTF 1

#endif
