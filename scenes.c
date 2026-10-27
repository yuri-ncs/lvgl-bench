/* Objects without theme style (lv_obj_remove_style_all): the default themes of v8 and v9 differ,
 * so both versions draw exactly the same thing. */
#include "scenes.h"

#define IMAGE_SIZE 64

static uint16_t texture_pixels[IMAGE_SIZE * IMAGE_SIZE];
#if LVGL_VERSION_MAJOR < 9
static lv_img_dsc_t texture = {
    .header.cf = LV_IMG_CF_TRUE_COLOR, .header.w = IMAGE_SIZE, .header.h = IMAGE_SIZE,
    .data_size = sizeof(texture_pixels), .data = (const uint8_t *)texture_pixels,
};
#else
static lv_image_dsc_t texture = {
    .header.magic = LV_IMAGE_HEADER_MAGIC, .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.w = IMAGE_SIZE, .header.h = IMAGE_SIZE, .header.stride = IMAGE_SIZE * 2,
    .data_size = sizeof(texture_pixels), .data = (const uint8_t *)texture_pixels,
};
#endif

static void (*step_callback)(int frame); // set by the animated scenes

/* Procedural RGB565 texture: gradient in R and G, XOR pattern in B (deliberately looks like noise). */
static void make_texture(void)
{
    for (int y = 0; y < IMAGE_SIZE; y++)
        for (int x = 0; x < IMAGE_SIZE; x++)
            texture_pixels[y * IMAGE_SIZE + x] = (x / 2) << 11 | y << 5 | ((x ^ y) & 31);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int width, int height, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    return obj;
}

static void create_fill(lv_obj_t *screen) {}

static void create_gradient(lv_obj_t *screen)
{
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x0000ff), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
}

static void rects(lv_obj_t *screen, lv_opa_t opa)
{
    for (int i = 0; i < 24; i++) {
        lv_obj_t *obj = box(screen, (i % 6) * 52 + 4, (i / 6) * 42 + 2, 60, 50, lv_palette_main(i % LV_PALETTE_GREY));
        lv_obj_set_style_bg_opa(obj, opa, 0);
        lv_obj_set_style_radius(obj, 12, 0);
        lv_obj_set_style_border_width(obj, 3, 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_border_opa(obj, opa, 0);
    }
}
static void create_rects(lv_obj_t *screen) { rects(screen, LV_OPA_COVER); }
static void create_rects_opa(lv_obj_t *screen) { rects(screen, LV_OPA_50); }

static void create_shadow(lv_obj_t *screen)
{
    for (int i = 0; i < 6; i++) {
        lv_obj_t *obj = box(screen, (i % 3) * 100 + 20, (i / 3) * 80 + 20, 70, 50, lv_palette_main(i));
        lv_obj_set_style_radius(obj, 10, 0);
        lv_obj_set_style_shadow_width(obj, 25, 0);
        lv_obj_set_style_shadow_spread(obj, 4, 0);
    }
}

static void create_text(lv_obj_t *screen)
{
    lv_obj_t *paragraph = lv_label_create(screen);
    lv_obj_remove_style_all(paragraph);
    lv_obj_set_width(paragraph, lv_pct(100));
    lv_obj_set_style_text_font(paragraph, &lv_font_montserrat_14, 0);
    lv_label_set_text(paragraph, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
                         "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                         "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
                         "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
                         "cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. "
                         "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz");
    lv_obj_t *title = lv_label_create(screen);
    lv_obj_remove_style_all(title);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "LVGL benchmark 28px");
    lv_obj_set_pos(title, 10, 130);
}

static void images(lv_obj_t *screen, lv_opa_t opa)
{
    make_texture();
    for (int i = 0; i < 15; i++) {
        lv_obj_t *image = lv_image_create(screen);
        lv_image_set_src(image, &texture);
        lv_obj_set_pos(image, (i % 5) * IMAGE_SIZE, (i / 5) * IMAGE_SIZE);
        lv_obj_set_style_image_opa(image, opa, 0);
    }
}
static void create_image(lv_obj_t *screen) { images(screen, LV_OPA_COVER); }
static void create_image_opa(lv_obj_t *screen) { images(screen, LV_OPA_50); }

static lv_obj_t *rotating_image;

static void rotate_step(int frame)
{
    lv_image_set_rotation(rotating_image, frame * 37 % 3600);
}

static void create_rotate(lv_obj_t *screen)
{
    make_texture();
    rotating_image = lv_image_create(screen);
    lv_image_set_src(rotating_image, &texture);
    lv_obj_center(rotating_image);
    lv_image_set_scale(rotating_image, 512); // 2x
    step_callback = rotate_step;
}

static void create_arc(lv_obj_t *screen)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *arc = lv_arc_create(screen);
        lv_obj_remove_style_all(arc);
        lv_obj_set_size(arc, 160 - i * 36, 160 - i * 36);
        lv_obj_center(arc);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_angles(arc, 0, 270 - i * 40);
        lv_obj_set_style_arc_width(arc, 14, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_palette_main(i), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    }
}

/* Rotating gradient circles: with rotation, each object is drawn into an intermediate (ARGB) layer
 * that is then transformed; one of the most expensive LVGL operations. */
static lv_obj_t *gradient_circles[8];

static void grad_rot_step(int frame)
{
    for (int i = 0; i < 8; i++) // +1: at angle 0 LVGL skips the layer and the frame comes out cheap
        lv_obj_set_style_transform_rotation(gradient_circles[i], (i * 450 + frame * (25 + i * 10)) % 3599 + 1, 0);
}

static void create_grad_rot(lv_obj_t *screen)
{
    for (int i = 0; i < 8; i++) {
        lv_obj_t *obj = box(screen, (i % 4) * 78 + 9, (i / 4) * 82 + 8, 70, 70, lv_palette_main(i * 2));
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_grad_color(obj, lv_palette_main(i * 2 + 1), 0);
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_transform_pivot_x(obj, 35, 0);
        lv_obj_set_style_transform_pivot_y(obj, 35, 0);
        gradient_circles[i] = obj;
    }
    grad_rot_step(0);
    step_callback = grad_rot_step;
}

/* "Real" interface: widgets with each version's default theme, as in an app migrated from v8 to v9.
 * Unlike the other scenes, here v8 and v9 draw with slightly different styles (different themes). */
static lv_obj_t *ui_chart, *ui_arc, *ui_arc_label, *ui_bar, *ui_slider, *ui_list;
static lv_chart_series_t *ui_series;

static void ui_step(int frame)
{
    lv_chart_set_next_value(ui_chart, ui_series, (frame * 37) % 80 + 10);
    lv_arc_set_value(ui_arc, frame % 100);
    lv_label_set_text_fmt(ui_arc_label, "%d%%", frame % 100);
    lv_bar_set_value(ui_bar, (frame * 3) % 100, LV_ANIM_OFF);
    lv_slider_set_value(ui_slider, 100 - frame % 100, LV_ANIM_OFF);
    lv_obj_scroll_to_y(ui_list, (frame * 4) % 200, LV_ANIM_OFF);
}

static void create_ui(lv_obj_t *screen)
{
    ui_chart = lv_chart_create(screen);
    lv_obj_set_pos(ui_chart, 4, 4);
    lv_obj_set_size(ui_chart, 190, 96);
    lv_obj_set_style_shadow_width(ui_chart, 15, 0);
    lv_chart_set_point_count(ui_chart, 40);
    ui_series = lv_chart_add_series(ui_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < 40; i++)
        lv_chart_set_next_value(ui_chart, ui_series, (i * 37) % 80 + 10);

    ui_arc = lv_arc_create(screen);
    lv_obj_set_pos(ui_arc, 4, 104);
    lv_obj_set_size(ui_arc, 62, 62);
    ui_arc_label = lv_label_create(ui_arc);
    lv_obj_center(ui_arc_label);

    ui_bar = lv_bar_create(screen);
    lv_obj_set_pos(ui_bar, 76, 112);
    lv_obj_set_size(ui_bar, 116, 12);

    ui_slider = lv_slider_create(screen);
    lv_obj_set_pos(ui_slider, 80, 136);
    lv_obj_set_size(ui_slider, 64, 10);

    lv_obj_t *ui_switch = lv_switch_create(screen);
    lv_obj_set_pos(ui_switch, 152, 130);
    lv_obj_set_size(ui_switch, 40, 22);
    lv_obj_add_state(ui_switch, LV_STATE_CHECKED);

    ui_list = lv_list_create(screen);
    lv_obj_set_pos(ui_list, 200, 4);
    lv_obj_set_size(ui_list, 116, 162);
    static const char *icons[] = { LV_SYMBOL_FILE, LV_SYMBOL_IMAGE, LV_SYMBOL_AUDIO, LV_SYMBOL_WIFI, LV_SYMBOL_GPS };
    for (int i = 0; i < 15; i++) {
        char text[16];
        lv_snprintf(text, sizeof(text), "Item %d", i + 1);
        lv_list_add_button(ui_list, icons[i % 5], text);
    }
    step_callback = ui_step;
}

const scene_t scenes[] = {
    { "fill", create_fill },           { "gradient", create_gradient },   { "rects", create_rects },
    { "rects_opa", create_rects_opa }, { "shadow", create_shadow },       { "text", create_text },
    { "image", create_image },         { "image_opa", create_image_opa }, { "rotate", create_rotate },
    { "arc", create_arc },             { "grad_rot", create_grad_rot },   { "ui", create_ui },
};
const int scene_count = sizeof(scenes) / sizeof(scenes[0]);

void scene_load(const scene_t *scene)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x808080), 0);
    step_callback = NULL;
    scene->create(screen);
}

void scene_step(int frame)
{
    if (step_callback)
        step_callback(frame);
}
