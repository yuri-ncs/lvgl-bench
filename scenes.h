/* Benchmark scenes. The same calls build on LVGL 8 and 9; the names used are the v9 ones. */
#pragma once
#include "lvgl.h"

#if LVGL_VERSION_MAJOR < 9
#define lv_display_t               lv_disp_t
#define lv_screen_active           lv_scr_act
#define lv_image_create            lv_img_create
#define lv_image_set_src           lv_img_set_src
#define lv_image_set_rotation      lv_img_set_angle
#define lv_image_set_scale         lv_img_set_zoom
#define lv_obj_set_style_image_opa lv_obj_set_style_img_opa
#define lv_list_add_button         lv_list_add_btn
#define lv_obj_set_style_transform_rotation lv_obj_set_style_transform_angle
#define lv_obj_set_scrollable(obj, enable) ((enable) ? lv_obj_add_flag : lv_obj_clear_flag)(obj, LV_OBJ_FLAG_SCROLLABLE)
#endif

typedef struct {
    const char *name;
    void (*create)(lv_obj_t *screen);
} scene_t;

extern const scene_t scenes[];
extern const int scene_count;

/* Clears the active screen and builds the scene. */
void scene_load(const scene_t *scene);

/* Per-frame change; only "rotate", "grad_rot" and "ui" animate. */
void scene_step(int frame);
