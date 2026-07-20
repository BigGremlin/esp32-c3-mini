
// Hand-authored watchface (not bin2lvgl-generated): a procedurally-drawn
// classic analog dial, ported from the user's own AnalogFace app in the
// OLEDS3Watch-joaquim project (esp-brookesia, same physical hardware -
// Waveshare ESP32-S3-Touch-AMOLED-2.06). Drawn entirely with LVGL canvas/line
// primitives rather than pre-rasterized sprite images, so unlike the other
// _410 faces there are no per-digit/per-frame image assets here at all.
// Watchface: classic_410

#include <math.h>
#include <string.h>
#include "classic_410.h"

#ifdef ENABLE_FACE_CLASSIC_410

lv_obj_t *face_classic_410;

static void *canvas_buf  = NULL;
static lv_obj_t *dial_canvas = NULL;
static lv_obj_t *hour_hand   = NULL;
static lv_obj_t *min_hand    = NULL;
static lv_obj_t *sec_hand    = NULL;
static lv_obj_t *hour_stripe = NULL;
static lv_obj_t *min_stripe  = NULL;
static lv_obj_t *date_box    = NULL;
static lv_obj_t *date_label  = NULL;

static lv_point_precise_t hour_pts[2];
static lv_point_precise_t min_pts[2];
static lv_point_precise_t sec_pts[2];

// Source dial was authored at 410x462 for this exact screen (410-wide AMOLED).
// This board reports 410x494, so the dial is vertically centered with a
// small black letterbox top/bottom rather than stretched to fill the extra
// height.
#define CW 410
#define CH 462
#define SCREEN_H 494
#define Y_OFFSET ((SCREEN_H - CH) / 2)
#define CANVAS_CX (CW / 2)
#define CANVAS_CY (CH / 2)
#define SCREEN_CX (CW / 2)
#define SCREEN_CY (Y_OFFSET + CH / 2)
#define PI_F 3.14159265f

static const char *const DAY_NAMES[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

#endif

void init_face_classic_410(void (*callback)(const char*, const lv_img_dsc_t *, lv_obj_t **, lv_obj_t **)){
#ifdef ENABLE_FACE_CLASSIC_410
    face_classic_410 = lv_obj_create(NULL);
    lv_obj_remove_flag(face_classic_410, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(face_classic_410, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(face_classic_410, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(face_classic_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(face_classic_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(face_classic_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(face_classic_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(face_classic_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(face_classic_410, onFaceEvent, LV_EVENT_ALL, NULL);

    canvas_buf = lv_malloc(CW * CH * 2);
    if (!canvas_buf) {
        callback("Classic", &face_classic_410_dial_img_preview_0, &face_classic_410, NULL);
        return;
    }
    memset(canvas_buf, 0, CW * CH * 2);

    dial_canvas = lv_canvas_create(face_classic_410);
    lv_obj_add_flag(dial_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_buffer(dial_canvas, canvas_buf, CW, CH, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(dial_canvas, 0, Y_OFFSET);

    lv_layer_t layer;

    /* ---- Pass 1: red border arc + 12 major ticks ---- */
    lv_canvas_init_layer(dial_canvas, &layer);

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = lv_color_hex(0xFF0000); arc.width = 8;
    arc.center.x = CANVAS_CX; arc.center.y = CANVAS_CY;
    arc.radius = 192; arc.start_angle = 0; arc.end_angle = 360;
    arc.opa = LV_OPA_COVER;
    lv_draw_arc(&layer, &arc);

    for (int i = 0; i < 12; i++) {
        float a = i * 30.0f * PI_F / 180.0f;
        lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
        line.color = lv_color_white(); line.width = 4; line.opa = LV_OPA_COVER;
        line.round_start = line.round_end = 1;
        line.p1.x = (int32_t)(CANVAS_CX + sinf(a) * 165); line.p1.y = (int32_t)(CANVAS_CY - cosf(a) * 165);
        line.p2.x = (int32_t)(CANVAS_CX + sinf(a) * 184); line.p2.y = (int32_t)(CANVAS_CY - cosf(a) * 184);
        lv_draw_line(&layer, &line);
    }
    lv_canvas_finish_layer(dial_canvas, &layer);

    /* ---- Pass 2: 48 minor ticks ---- */
    lv_canvas_init_layer(dial_canvas, &layer);
    for (int i = 0; i < 60; i++) {
        if (i % 5 == 0) continue;
        float a = i * 6.0f * PI_F / 180.0f;
        lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
        line.color = lv_color_white(); line.width = 2; line.opa = LV_OPA_COVER;
        line.round_start = line.round_end = 1;
        line.p1.x = (int32_t)(CANVAS_CX + sinf(a) * 176); line.p1.y = (int32_t)(CANVAS_CY - cosf(a) * 176);
        line.p2.x = (int32_t)(CANVAS_CX + sinf(a) * 184); line.p2.y = (int32_t)(CANVAS_CY - cosf(a) * 184);
        lv_draw_line(&layer, &line);
    }
    lv_canvas_finish_layer(dial_canvas, &layer);

    /* ---- Pass 3: 11 hour numerals (skip 3 o'clock = date window) ---- */
    static const char *hour_nums[12] = {
        "12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"
    };
    lv_canvas_init_layer(dial_canvas, &layer);
    for (int i = 0; i < 12; i++) {
        if (i == 3) continue;
        float a = i * 30.0f * PI_F / 180.0f;
        int32_t nx = CANVAS_CX + (int32_t)(sinf(a) * 143.0f);
        int32_t ny = CANVAS_CY - (int32_t)(cosf(a) * 143.0f);
        lv_draw_label_dsc_t lbl; lv_draw_label_dsc_init(&lbl);
        lbl.font = &lv_font_montserrat_36; lbl.color = lv_color_white();
        lbl.align = LV_TEXT_ALIGN_CENTER; lbl.opa = LV_OPA_COVER;
        lbl.text = hour_nums[i];
        lv_area_t area = { nx - 30, ny - 22, nx + 30, ny + 22 };
        lv_draw_label(&layer, &lbl, &area);
    }
    lv_canvas_finish_layer(dial_canvas, &layer);

    lv_obj_clear_flag(dial_canvas, LV_OBJ_FLAG_HIDDEN);

    /* ---- Date window - 3 o'clock ---- */
    date_box = lv_obj_create(face_classic_410);
    lv_obj_set_size(date_box, 136, 44);
    lv_obj_set_pos(date_box, SCREEN_CX + 37, SCREEN_CY - 22);
    lv_obj_set_style_bg_color(date_box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(date_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(date_box, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(date_box, 2, 0);
    lv_obj_set_style_radius(date_box, 8, 0);
    lv_obj_set_style_pad_all(date_box, 4, 0);
    lv_obj_remove_flag(date_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(date_box, LV_OBJ_FLAG_CLICKABLE);

    date_label = lv_label_create(date_box);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(date_label, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(date_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_label, 0, 0);
    lv_label_set_text(date_label, "??? --");
    lv_obj_center(date_label);

    /* ---- Clock hands ---- */
    hour_hand = lv_line_create(face_classic_410);
    lv_obj_set_style_line_color(hour_hand, lv_color_white(), 0);
    lv_obj_set_style_line_width(hour_hand, 7, 0);
    lv_obj_set_style_line_rounded(hour_hand, true, 0);

    min_hand = lv_line_create(face_classic_410);
    lv_obj_set_style_line_color(min_hand, lv_color_white(), 0);
    lv_obj_set_style_line_width(min_hand, 4, 0);
    lv_obj_set_style_line_rounded(min_hand, true, 0);

    hour_stripe = lv_line_create(face_classic_410);
    lv_obj_set_style_line_color(hour_stripe, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_width(hour_stripe, 2, 0);
    lv_obj_set_style_line_rounded(hour_stripe, true, 0);

    min_stripe = lv_line_create(face_classic_410);
    lv_obj_set_style_line_color(min_stripe, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_width(min_stripe, 2, 0);
    lv_obj_set_style_line_rounded(min_stripe, true, 0);

    sec_hand = lv_line_create(face_classic_410);
    lv_obj_set_style_line_color(sec_hand, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_width(sec_hand, 2, 0);
    lv_obj_set_style_line_rounded(sec_hand, true, 0);

    lv_obj_t *cap = lv_obj_create(face_classic_410);
    lv_obj_set_size(cap, 14, 14);
    lv_obj_set_pos(cap, SCREEN_CX - 7, SCREEN_CY - 7);
    lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cap, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cap, 0, 0);

    callback("Classic", &face_classic_410_dial_img_preview_0, &face_classic_410, &sec_hand);

#endif
}

static lv_point_precise_t hand_tip(int cx, int cy, float deg, float len)
{
#ifdef ENABLE_FACE_CLASSIC_410
    float rad = deg * PI_F / 180.0f;
    lv_point_precise_t p = {
        (lv_value_precise_t)(cx + (int32_t)(sinf(rad) * len)),
        (lv_value_precise_t)(cy - (int32_t)(cosf(rad) * len)),
    };
    return p;
#else
    lv_point_precise_t p = {0, 0};
    return p;
#endif
}

void update_time_classic_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday)
{
#ifdef ENABLE_FACE_CLASSIC_410
    if (!face_classic_410 || !hour_hand)
    {
        return;
    }

    lv_label_set_text_fmt(date_label, "%s %02d", DAY_NAMES[weekday % 7], day);

    float hour_angle = (hour % 12) * 30.0f + minute * 0.5f;
    float min_angle  = minute * 6.0f;
    float sec_angle  = second * 6.0f;

    hour_pts[0] = hand_tip(SCREEN_CX, SCREEN_CY, hour_angle + 180.0f, 15.0f);
    hour_pts[1] = hand_tip(SCREEN_CX, SCREEN_CY, hour_angle, 85.0f);
    lv_line_set_points(hour_hand, hour_pts, 2);
    lv_line_set_points(hour_stripe, hour_pts, 2);

    min_pts[0] = hand_tip(SCREEN_CX, SCREEN_CY, min_angle + 180.0f, 20.0f);
    min_pts[1] = hand_tip(SCREEN_CX, SCREEN_CY, min_angle, 130.0f);
    lv_line_set_points(min_hand, min_pts, 2);
    lv_line_set_points(min_stripe, min_pts, 2);

    sec_pts[0] = (lv_point_precise_t){ (lv_value_precise_t)SCREEN_CX, (lv_value_precise_t)SCREEN_CY };
    sec_pts[1] = hand_tip(SCREEN_CX, SCREEN_CY, sec_angle, 150.0f);
    lv_line_set_points(sec_hand, sec_pts, 2);

#endif
}

void update_weather_classic_410(int temp, int icon)
{
#ifdef ENABLE_FACE_CLASSIC_410
    if (!face_classic_410)
    {
        return;
    }

#endif
}

void update_status_classic_410(int battery, bool connection){
#ifdef ENABLE_FACE_CLASSIC_410
    if (!face_classic_410)
    {
        return;
    }

#endif
}

void update_activity_classic_410(int steps, int distance, int kcal)
{
#ifdef ENABLE_FACE_CLASSIC_410
    if (!face_classic_410)
    {
        return;
    }

#endif
}

void update_health_classic_410(int bpm, int oxygen)
{
#ifdef ENABLE_FACE_CLASSIC_410
    if (!face_classic_410)
    {
        return;
    }

#endif
}

void update_all_classic_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
    int temp, int icon, int battery, bool connection, int steps, int distance, int kcal, int bpm, int oxygen)
{
#ifdef ENABLE_FACE_CLASSIC_410
    update_time_classic_410(second, minute, hour, mode, am, day, month, year, weekday);
    update_weather_classic_410(temp, icon);
    update_status_classic_410(battery, connection);
    update_activity_classic_410(steps, distance, kcal);
    update_health_classic_410(bpm, oxygen);
#endif
}

void update_check_classic_410(lv_obj_t *root, int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
    int temp, int icon, int battery, bool connection, int steps, int distance, int kcal, int bpm, int oxygen)
{
#ifdef ENABLE_FACE_CLASSIC_410
    if (root != face_classic_410)
    {
        return;
    }
    update_time_classic_410(second, minute, hour, mode, am, day, month, year, weekday);
    update_weather_classic_410(temp, icon);
    update_status_classic_410(battery, connection);
    update_activity_classic_410(steps, distance, kcal);
    update_health_classic_410(bpm, oxygen);
#endif
}
