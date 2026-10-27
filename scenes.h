/* Cenas do benchmark. As mesmas chamadas compilam no LVGL 8 e no 9; os nomes usados são os do v9. */
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
#define lv_obj_set_scrollable(o, en) ((en) ? lv_obj_add_flag : lv_obj_clear_flag)(o, LV_OBJ_FLAG_SCROLLABLE)
#endif

typedef struct {
    const char *name;
    void (*create)(lv_obj_t *s);
} scene_t;

extern const scene_t scenes[];
extern const int scene_count;

/* Limpa a tela ativa e monta a cena. */
void scene_load(const scene_t *sc);

/* Mudança por quadro; só "rotate", "grad_rot" e "ui" animam. */
void scene_step(int frame);
