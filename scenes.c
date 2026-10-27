/* Objetos sem estilo de tema (lv_obj_remove_style_all): os temas padrão do v8 e do v9 são diferentes,
 * e assim as duas versões desenham exatamente a mesma coisa. */
#include "scenes.h"

#define IMG_W 64

static uint16_t img_px[IMG_W * IMG_W];
#if LVGL_VERSION_MAJOR < 9
static lv_img_dsc_t img = {
    .header.cf = LV_IMG_CF_TRUE_COLOR, .header.w = IMG_W, .header.h = IMG_W,
    .data_size = sizeof(img_px), .data = (const uint8_t *)img_px,
};
#else
static lv_image_dsc_t img = {
    .header.magic = LV_IMAGE_HEADER_MAGIC, .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.w = IMG_W, .header.h = IMG_W, .header.stride = IMG_W * 2,
    .data_size = sizeof(img_px), .data = (const uint8_t *)img_px,
};
#endif

static void (*step_fn)(int frame); // definido pelas cenas animadas

/* Textura RGB565 procedural: degradê em R e G, padrão XOR em B (parece ruído de propósito). */
static void make_img(void)
{
    for (int y = 0; y < IMG_W; y++)
        for (int x = 0; x < IMG_W; x++)
            img_px[y * IMG_W + x] = (x / 2) << 11 | y << 5 | ((x ^ y) & 31);
}

static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    return o;
}

static void sc_fill(lv_obj_t *s) {}

static void sc_gradient(lv_obj_t *s)
{
    lv_obj_set_style_bg_grad_color(s, lv_color_hex(0x0000ff), 0);
    lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_VER, 0);
}

static void rects(lv_obj_t *s, lv_opa_t opa)
{
    for (int i = 0; i < 24; i++) {
        lv_obj_t *o = box(s, (i % 6) * 52 + 4, (i / 6) * 42 + 2, 60, 50, lv_palette_main(i % LV_PALETTE_GREY));
        lv_obj_set_style_bg_opa(o, opa, 0);
        lv_obj_set_style_radius(o, 12, 0);
        lv_obj_set_style_border_width(o, 3, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_border_opa(o, opa, 0);
    }
}
static void sc_rects(lv_obj_t *s) { rects(s, LV_OPA_COVER); }
static void sc_rects_opa(lv_obj_t *s) { rects(s, LV_OPA_50); }

static void sc_shadow(lv_obj_t *s)
{
    for (int i = 0; i < 6; i++) {
        lv_obj_t *o = box(s, (i % 3) * 100 + 20, (i / 3) * 80 + 20, 70, 50, lv_palette_main(i));
        lv_obj_set_style_radius(o, 10, 0);
        lv_obj_set_style_shadow_width(o, 25, 0);
        lv_obj_set_style_shadow_spread(o, 4, 0);
    }
}

static void sc_text(lv_obj_t *s)
{
    lv_obj_t *l = lv_label_create(s);
    lv_obj_remove_style_all(l);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_label_set_text(l, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
                         "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                         "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
                         "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
                         "cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. "
                         "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz");
    lv_obj_t *big = lv_label_create(s);
    lv_obj_remove_style_all(big);
    lv_obj_set_style_text_font(big, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(big, lv_color_hex(0xffffff), 0);
    lv_label_set_text(big, "LVGL benchmark 28px");
    lv_obj_set_pos(big, 10, 130);
}

static void images(lv_obj_t *s, lv_opa_t opa)
{
    make_img();
    for (int i = 0; i < 15; i++) {
        lv_obj_t *im = lv_image_create(s);
        lv_image_set_src(im, &img);
        lv_obj_set_pos(im, (i % 5) * IMG_W, (i / 5) * IMG_W);
        lv_obj_set_style_image_opa(im, opa, 0);
    }
}
static void sc_image(lv_obj_t *s) { images(s, LV_OPA_COVER); }
static void sc_image_opa(lv_obj_t *s) { images(s, LV_OPA_50); }

static lv_obj_t *rot_img;

static void rotate_step(int f)
{
    lv_image_set_rotation(rot_img, f * 37 % 3600);
}

static void sc_rotate(lv_obj_t *s)
{
    make_img();
    rot_img = lv_image_create(s);
    lv_image_set_src(rot_img, &img);
    lv_obj_center(rot_img);
    lv_image_set_scale(rot_img, 512); // 2x
    step_fn = rotate_step;
}

static void sc_arc(lv_obj_t *s)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *a = lv_arc_create(s);
        lv_obj_remove_style_all(a);
        lv_obj_set_size(a, 160 - i * 36, 160 - i * 36);
        lv_obj_center(a);
        lv_arc_set_bg_angles(a, 0, 360);
        lv_arc_set_angles(a, 0, 270 - i * 40);
        lv_obj_set_style_arc_width(a, 14, LV_PART_MAIN);
        lv_obj_set_style_arc_width(a, 14, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(a, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, lv_palette_main(i), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    }
}

/* Círculos com degradê girando: com rotação, cada objeto é desenhado numa camada intermediária (ARGB)
 * que depois é transformada; das operações mais caras do LVGL. */
static lv_obj_t *grad_circ[8];

static void grad_rot_step(int f)
{
    for (int i = 0; i < 8; i++) // +1: com ângulo 0 o LVGL pula a camada e o quadro sai barato
        lv_obj_set_style_transform_rotation(grad_circ[i], (i * 450 + f * (25 + i * 10)) % 3599 + 1, 0);
}

static void sc_grad_rot(lv_obj_t *s)
{
    for (int i = 0; i < 8; i++) {
        lv_obj_t *o = box(s, (i % 4) * 78 + 9, (i / 4) * 82 + 8, 70, 70, lv_palette_main(i * 2));
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_grad_color(o, lv_palette_main(i * 2 + 1), 0);
        lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_transform_pivot_x(o, 35, 0);
        lv_obj_set_style_transform_pivot_y(o, 35, 0);
        grad_circ[i] = o;
    }
    grad_rot_step(0);
    step_fn = grad_rot_step;
}

/* Interface "de verdade": widgets com o tema padrão de cada versão, como num app migrado do v8 para o v9.
 * Diferente das outras cenas, aqui o v8 e o v9 desenham com estilos um pouco diferentes (temas diferentes). */
static lv_obj_t *ui_chart, *ui_arc, *ui_arc_label, *ui_bar, *ui_slider, *ui_list;
static lv_chart_series_t *ui_ser;

static void ui_step(int f)
{
    lv_chart_set_next_value(ui_chart, ui_ser, (f * 37) % 80 + 10);
    lv_arc_set_value(ui_arc, f % 100);
    lv_label_set_text_fmt(ui_arc_label, "%d%%", f % 100);
    lv_bar_set_value(ui_bar, (f * 3) % 100, LV_ANIM_OFF);
    lv_slider_set_value(ui_slider, 100 - f % 100, LV_ANIM_OFF);
    lv_obj_scroll_to_y(ui_list, (f * 4) % 200, LV_ANIM_OFF);
}

static void sc_ui(lv_obj_t *s)
{
    ui_chart = lv_chart_create(s);
    lv_obj_set_pos(ui_chart, 4, 4);
    lv_obj_set_size(ui_chart, 190, 96);
    lv_obj_set_style_shadow_width(ui_chart, 15, 0);
    lv_chart_set_point_count(ui_chart, 40);
    ui_ser = lv_chart_add_series(ui_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 40; i++)
        lv_chart_set_next_value(ui_chart, ui_ser, (i * 37) % 80 + 10);

    ui_arc = lv_arc_create(s);
    lv_obj_set_pos(ui_arc, 4, 104);
    lv_obj_set_size(ui_arc, 62, 62);
    ui_arc_label = lv_label_create(ui_arc);
    lv_obj_center(ui_arc_label);

    ui_bar = lv_bar_create(s);
    lv_obj_set_pos(ui_bar, 76, 112);
    lv_obj_set_size(ui_bar, 116, 12);

    ui_slider = lv_slider_create(s);
    lv_obj_set_pos(ui_slider, 80, 136);
    lv_obj_set_size(ui_slider, 64, 10);

    lv_obj_t *sw = lv_switch_create(s);
    lv_obj_set_pos(sw, 152, 130);
    lv_obj_set_size(sw, 40, 22);
    lv_obj_add_state(sw, LV_STATE_CHECKED);

    ui_list = lv_list_create(s);
    lv_obj_set_pos(ui_list, 200, 4);
    lv_obj_set_size(ui_list, 116, 162);
    static const char *icons[] = { LV_SYMBOL_FILE, LV_SYMBOL_IMAGE, LV_SYMBOL_AUDIO, LV_SYMBOL_WIFI, LV_SYMBOL_GPS };
    for (int i = 0; i < 15; i++) {
        char txt[16];
        lv_snprintf(txt, sizeof(txt), "Item %d", i + 1);
        lv_list_add_button(ui_list, icons[i % 5], txt);
    }
    step_fn = ui_step;
}

const scene_t scenes[] = {
    { "fill", sc_fill },           { "gradient", sc_gradient }, { "rects", sc_rects },
    { "rects_opa", sc_rects_opa }, { "shadow", sc_shadow },     { "text", sc_text },
    { "image", sc_image },         { "image_opa", sc_image_opa }, { "rotate", sc_rotate },
    { "arc", sc_arc },             { "grad_rot", sc_grad_rot }, { "ui", sc_ui },
};
const int scene_count = sizeof(scenes) / sizeof(scenes[0]);

void scene_load(const scene_t *sc)
{
    lv_obj_t *s = lv_screen_active();
    lv_obj_clean(s);
    lv_obj_remove_style_all(s);
    lv_obj_set_scrollable(s, false);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s, lv_color_hex(0x808080), 0);
    step_fn = NULL;
    sc->create(s);
}

void scene_step(int frame)
{
    if (step_fn) step_fn(frame);
}
