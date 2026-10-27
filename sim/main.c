/* Referência visual no PC (LVGL 9 + SDL2), na resolução da placa ampliada 3x.
 *   lvgl_bench_sim          alterna as cenas do benchmark a cada 2 s (nome na barra de título)
 *   lvgl_bench_sim --demo   roda o lv_demo_benchmark oficial */
#include <string.h>
#include "scenes.h"
#include "lv_demos.h"

static lv_display_t *disp;
static int cur, frame;

static void next_scene(lv_timer_t *t)
{
    scene_load(&scenes[cur]);
    lv_sdl_window_set_title(disp, scenes[cur].name);
    cur = (cur + 1) % scene_count;
}

static void step(lv_timer_t *t)
{
    scene_step(frame++);
}

int main(int argc, char **argv)
{
    lv_init();
    disp = lv_sdl_window_create(320, 170);
    lv_sdl_window_set_zoom(disp, 3);

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        lv_demo_benchmark();
    } else {
        next_scene(NULL);
        lv_timer_create(next_scene, 2000, NULL);
        lv_timer_create(step, 16, NULL);
    }
    while (1)
        lv_delay_ms(lv_timer_handler());
}
