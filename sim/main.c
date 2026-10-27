/* Visual reference on the PC (LVGL 9 + SDL2), at the board resolution scaled 3x.
 *   lvgl_bench_sim          cycles through the benchmark scenes every 2 s (name in the title bar)
 *   lvgl_bench_sim --demo   runs the official lv_demo_benchmark */
#include <string.h>
#include "scenes.h"
#include "lv_demos.h"

static lv_display_t *display;
static int current_scene, frame;

static void next_scene(lv_timer_t *timer)
{
    scene_load(&scenes[current_scene]);
    lv_sdl_window_set_title(display, scenes[current_scene].name);
    current_scene = (current_scene + 1) % scene_count;
}

static void step(lv_timer_t *timer)
{
    scene_step(frame++);
}

int main(int argc, char **argv)
{
    lv_init();
    display = lv_sdl_window_create(320, 170);
    lv_sdl_window_set_zoom(display, 3);

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
