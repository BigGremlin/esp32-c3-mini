
// Hand-authored watchface (not bin2lvgl-generated): Batman chronograph dial.
// See batman_410.h for the delivery story. Background is a single opaque
// 410x494 raster (already the project's native "content canvas" size, see
// SCREEN_H in citizen_410.c/classic_410.c, so no scale/letterbox math is
// needed here unlike faces ported from other resolutions).
//
// Rebuilt again 2026-08-01 against a redelivered, cleaner source package
// (batman_410/batman_v4_410x494/ - "Fully Clean" background per its own
// README) that fixed the first delivery's messy geometry: numerals now sit
// evenly around a real circle, both subdials are blank (no baked-in "10"/"20"
// labels or numbers), and the hand sprites are proper tapered/arrowhead art
// instead of thin placeholder lines. Two subdials only, same as before - the
// 6 o'clock position is still fully occupied by the bat emblem and "6"
// numeral, so there's no room for a third.
//
// Centers below were measured programmatically off the source PNG (thresholding
// the tick-mark pixels and taking bounding-box/centroid, not eyeballed) and
// confirmed by overlaying crosshairs on the art. Still not a mathematically
// perfect circle fit - this is AI-generated dial art - but noticeably tighter
// than the first delivery.
// Watchface: batman_410

#include <math.h>
#include "batman_410.h"

#ifdef ENABLE_FACE_BATMAN_410

lv_obj_t *face_batman_410;

static lv_obj_t *face_bg     = NULL;
static lv_obj_t *hour_hand   = NULL;
static lv_obj_t *min_hand    = NULL;
static lv_obj_t *sec_hand    = NULL;
static lv_obj_t *sub_hand_l  = NULL;  // left (blank) subdial, running-seconds sweep
static lv_obj_t *sub_hand_r  = NULL;  // right (blank) subdial, slow hour-style counter

// Canvas is already 410x494, the project's standard "content" size - see
// header comment above.
#define CW 410
#define CH 494

// Main dial center: the "12" numeral/tick and the overall art are
// horizontally centered on the 410-wide canvas, and no single measured
// landmark (top tick vs. bottom "6" numeral vs. side "9"/"3" numerals, which
// disagree with each other by tens of px on this non-geometric art) beat
// simply using the canvas's own geometric center, which fell within a few px
// of every measurement anyway.
#define MAIN_CX (CW / 2)
#define MAIN_CY (CH / 2)

// Subdial centers, measured from the tick-ring bounding box on each subdial
// (see header comment). Right mirrors left around the canvas's horizontal
// center since the art is left/right symmetric at a glance.
#define SUB_L_CX 86
#define SUB_L_CY 257
#define SUB_R_CX (CW - SUB_L_CX)
#define SUB_R_CY 257

// All 4 hand sprites (hand_hour/min/sec/sub) share the same authoring
// convention: a uniform-width shaft with a small arrowhead flare only in the
// last ~15% of the image, at the bottom edge - i.e. pivot end (blunt, near
// the hub) at the sprite's top row, tip (flare) at the bottom row. Confirmed
// by measuring opaque-pixel width per row in gen_batman_410.py's source
// (constant width until y~85% of height, then widening to the bottom edge).
// So each sprite's own "as drawn" pointing direction is straight down, i.e.
// clock-angle 180 deg (0 = 12 o'clock/up, clockwise) before any rotation.
#define HAND_BASE_DEG 180.0f

static int32_t norm_angle_deci(float deg)
{
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return (int32_t)(deg * 10.0f);
}

#endif

void init_face_batman_410(void (*callback)(const char*, const lv_img_dsc_t *, lv_obj_t **, lv_obj_t **)){
#ifdef ENABLE_FACE_BATMAN_410
    face_batman_410 = lv_obj_create(NULL);
    lv_obj_remove_flag(face_batman_410, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(face_batman_410, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(face_batman_410, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(face_batman_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(face_batman_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(face_batman_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(face_batman_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(face_batman_410, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(face_batman_410, onFaceEvent, LV_EVENT_ALL, NULL);

    face_bg = lv_image_create(face_batman_410);
    lv_image_set_src(face_bg, &face_batman_410_face_bg);
    lv_obj_set_pos(face_bg, 0, 0);
    lv_obj_remove_flag(face_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Clock hands: each sprite's pivot is its own top-center pixel (see
       HAND_BASE_DEG note above), positioned so that pivot lands exactly on
       its dial's center, then rotated. ---- */
    hour_hand = lv_image_create(face_batman_410);
    lv_image_set_src(hour_hand, &face_batman_410_hand_hour);
    lv_obj_set_pos(hour_hand, MAIN_CX - 12, MAIN_CY);
    lv_image_set_pivot(hour_hand, 12, 0);
    lv_obj_remove_flag(hour_hand, LV_OBJ_FLAG_SCROLLABLE);

    min_hand = lv_image_create(face_batman_410);
    lv_image_set_src(min_hand, &face_batman_410_hand_minute);
    lv_obj_set_pos(min_hand, MAIN_CX - 9, MAIN_CY);
    lv_image_set_pivot(min_hand, 9, 0);
    lv_obj_remove_flag(min_hand, LV_OBJ_FLAG_SCROLLABLE);

    sec_hand = lv_image_create(face_batman_410);
    lv_image_set_src(sec_hand, &face_batman_410_hand_second);
    lv_obj_set_pos(sec_hand, MAIN_CX - 5, MAIN_CY);
    lv_image_set_pivot(sec_hand, 5, 0);
    lv_obj_remove_flag(sec_hand, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cap = lv_obj_create(face_batman_410);
    lv_obj_set_size(cap, 8, 8);
    lv_obj_set_pos(cap, MAIN_CX - 4, MAIN_CY - 4);
    lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cap, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cap, 0, 0);

    /* ---- Subdial hands: reuse the single hand_sub sprite on both dials. ---- */
    sub_hand_l = lv_image_create(face_batman_410);
    lv_image_set_src(sub_hand_l, &face_batman_410_hand_sub);
    lv_obj_set_pos(sub_hand_l, SUB_L_CX - 6, SUB_L_CY);
    lv_image_set_pivot(sub_hand_l, 6, 0);
    lv_obj_remove_flag(sub_hand_l, LV_OBJ_FLAG_SCROLLABLE);

    sub_hand_r = lv_image_create(face_batman_410);
    lv_image_set_src(sub_hand_r, &face_batman_410_hand_sub);
    lv_obj_set_pos(sub_hand_r, SUB_R_CX - 6, SUB_R_CY);
    lv_image_set_pivot(sub_hand_r, 6, 0);
    lv_obj_remove_flag(sub_hand_r, LV_OBJ_FLAG_SCROLLABLE);

    callback("Batman", &face_batman_410_dial_img_preview, &face_batman_410, &sec_hand);

#endif
}

void update_time_batman_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday)
{
#ifdef ENABLE_FACE_BATMAN_410
    if (!face_batman_410 || !hour_hand)
    {
        return;
    }

    float hour_angle = (hour % 12) * 30.0f + minute * 0.5f;
    float min_angle  = minute * 6.0f;
    float sec_angle  = second * 6.0f;

    lv_image_set_rotation(hour_hand, norm_angle_deci(hour_angle - HAND_BASE_DEG));
    lv_image_set_rotation(min_hand, norm_angle_deci(min_angle - HAND_BASE_DEG));
    lv_image_set_rotation(sec_hand, norm_angle_deci(sec_angle - HAND_BASE_DEG));

    // Decorative, not backed by real chronograph data (this face has no
    // stopwatch feature wired in) - left subdial sweeps once per minute like
    // a running-seconds register, right subdial once per 12h like a slow
    // hour-style counter, matching the "animated sub-hands" behaviour the
    // delivered design asked for.
    float sub_l_angle = second * 6.0f;
    float sub_r_angle = (hour % 12) * 30.0f + minute * 0.5f;
    lv_image_set_rotation(sub_hand_l, norm_angle_deci(sub_l_angle - HAND_BASE_DEG));
    lv_image_set_rotation(sub_hand_r, norm_angle_deci(sub_r_angle - HAND_BASE_DEG));

#endif
}

void update_weather_batman_410(int temp, int icon)
{
#ifdef ENABLE_FACE_BATMAN_410
    if (!face_batman_410)
    {
        return;
    }

#endif
}

void update_status_batman_410(int battery, bool connection){
#ifdef ENABLE_FACE_BATMAN_410
    if (!face_batman_410)
    {
        return;
    }

#endif
}

void update_activity_batman_410(int steps, int distance, int kcal)
{
#ifdef ENABLE_FACE_BATMAN_410
    if (!face_batman_410)
    {
        return;
    }

#endif
}

void update_health_batman_410(int bpm, int oxygen)
{
#ifdef ENABLE_FACE_BATMAN_410
    if (!face_batman_410)
    {
        return;
    }

#endif
}

void update_all_batman_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
    int temp, int icon, int battery, bool connection, int steps, int distance, int kcal, int bpm, int oxygen)
{
#ifdef ENABLE_FACE_BATMAN_410
    update_time_batman_410(second, minute, hour, mode, am, day, month, year, weekday);
    update_weather_batman_410(temp, icon);
    update_status_batman_410(battery, connection);
    update_activity_batman_410(steps, distance, kcal);
    update_health_batman_410(bpm, oxygen);
#endif
}

void update_check_batman_410(lv_obj_t *root, int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
    int temp, int icon, int battery, bool connection, int steps, int distance, int kcal, int bpm, int oxygen)
{
#ifdef ENABLE_FACE_BATMAN_410
    if (root != face_batman_410)
    {
        return;
    }
    update_time_batman_410(second, minute, hour, mode, am, day, month, year, weekday);
    update_weather_batman_410(temp, icon);
    update_status_batman_410(battery, connection);
    update_activity_batman_410(steps, distance, kcal);
    update_health_batman_410(bpm, oxygen);
#endif
}
