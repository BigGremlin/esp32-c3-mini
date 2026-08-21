
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
//
// Rebuilt again 2026-08-20 with a v5 source package (batman_v5_410x494/) -
// wider background (pillarboxed down to the 410x494 canvas) and hour/min/sec
// hands with a baked-in hub+tail drawn at an angle, rotated straight during
// pre-processing rather than as-authored. See that directory's README.md for
// the pixel math and the HAND_BASE_DEG / HAND_*_PIVOT_* comments below for
// how that changed the hand placement code. Sub-hand art (hand_sub) is still
// the unchanged v4 asset - none was redelivered this round.
//
// v5.1 (2026-08-21): minute hand lengthened (see HAND_MIN_PIVOT_* below) and
// two of the background's baked dummy readouts wired up to live data: the
// top-left "FRI" day window now shows the real weekday, and the number
// baked above the bat emblem (was a static "31") now shows live seconds as
// an experiment. Both reuse the citizen_410 trick of an opaque patch image
// (a crop of the same background with the old text erased via inpainting,
// see day_bg_patch.png/sec_bg_patch.png and gen_batman_410.py's v5.1 note)
// laid over the original art, with a plain (non-rotated) LVGL label on top -
// unlike citizen_410's LCD windows the FRI pill is drawn at a diagonal tilt
// in the art, which a straight label doesn't match, but matching that tilt
// would need real text rotation for arbitrary-width day names, not
// attempted here. Battery/date/weather windows elsewhere on this dial are
// still static baked art, untouched.
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
static lv_obj_t *day_bg      = NULL;  // patched-blank copy of the top-left "FRI" pill
static lv_obj_t *day_label   = NULL;  // live weekday text drawn on top of day_bg
static lv_obj_t *sec_bg      = NULL;  // patched-blank copy of the number above the bat
static lv_obj_t *sec_label   = NULL;  // live seconds text drawn on top of sec_bg

// Canvas is already 410x494, the project's standard "content" size - see
// header comment above.
#define CW 410
#define CH 494

// Main dial center: re-measured 2026-08-20 against the v5 background - the
// canvas's raw geometric center (CH/2 = 247) was ~15px too high. Found the
// true center by locating the gold index ticks at the 3 and 9 o'clock
// positions (thresholded gold pixels outside the numeral ring, at x~45-59
// and x~359-366) - both sit at y=262, not 247, confirming the drawn dial
// isn't vertically centered on the canvas. CX unaffected (tick x-midpoint is
// 206, a rounding error from CW/2's 205).
#define MAIN_CX (CW / 2)
#define MAIN_CY 262

// Subdial centers, measured from the tick-ring bounding box on each subdial
// (see header comment). Right mirrors left around the canvas's horizontal
// center since the art is left/right symmetric at a glance.
#define SUB_L_CX 86
#define SUB_L_CY 257
#define SUB_R_CX (CW - SUB_L_CX)
#define SUB_R_CY 257

// hand_sub still follows the original v4 authoring convention: uniform-width
// shaft, pivot at the sprite's top row (blunt end), tip at the bottom row -
// i.e. "as drawn" pointing straight down, clock-angle 180 (0 = 12 o'clock/
// up, clockwise) before rotation. See its own lv_image_set_pivot(..., 6, 0)
// call below.
//
// hand_hour/min/sec are v5 sprites (2026-08-20 delivery): each was rotated
// during pre-processing (see batman_v5_410x494/README.md) so its tip points
// straight up, i.e. clock-angle 0, before rotation - the opposite of
// hand_sub's convention - and each keeps its own baked-in gold pivot hub +
// counterweight tail, with the pivot point sitting inside the image rather
// than at row 0 (see HAND_HOUR_PIVOT_* etc below).
#define HAND_BASE_DEG 0.0f
#define HAND_SUB_BASE_DEG 180.0f

// Pivot point of each v5 hand sprite (hub center). Re-measured 2026-08-20
// after a second downscale pass off the original (pre-v5-final-scale)
// normalized hand crops - the first v5 delivery's hands were too long
// (second hand overshot the bezel entirely). New target: second hand's
// pivot-to-tip length just clears the dial's inner edge at 3 o'clock
// (measured bezel inner edge at x=373 from MAIN_CX=205, i.e. radius 168;
// tip length set to 165 for a few px of clearance) - hour/minute rescaled
// by the same factor to keep their original relative proportions.
#define HAND_HOUR_PIVOT_X 20
#define HAND_HOUR_PIVOT_Y 104
// Minute hand rescaled 2026-08-21 (24x138 -> 31x179 in the source asset) so
// its pivot-to-tip length reaches the 3 o'clock minute-tick ring instead of
// stopping well short of it. Target radius measured directly off this
// background's pixels: the two minute ticks flanking the 3 o'clock position
// (the exact 3 o'clock tick itself is hidden under the right subdial) have
// their outer tips at (366,244) and (366,280), both radius 162.0px from
// (MAIN_CX,MAIN_CY) - old pivot-to-tip was only 125px, noticeably short.
// Scale factor 162/125 = 1.296 applied uniformly to the whole sprite so the
// hand doesn't distort, pivot coordinates scaled by the same factor.
#define HAND_MIN_PIVOT_X 16
#define HAND_MIN_PIVOT_Y 162
#define HAND_SEC_PIVOT_X 12
#define HAND_SEC_PIVOT_Y 166

// Day-of-week window: opaque day_bg patch is a straight crop of the
// background at (DAY_BG_X,DAY_BG_Y), see gen_batman_410.py's v5.1 note for
// how the baked "FRI" text was removed from it. day_label is a plain
// (non-rotated) label centered over where that text used to sit - the pill
// itself is drawn at a diagonal tilt in the art which the label doesn't
// match, see the file header comment.
#define DAY_BG_X 58
#define DAY_BG_Y 46
#define DAY_LABEL_X 73
#define DAY_LABEL_Y 58
#define DAY_LABEL_W 60
#define DAY_LABEL_H 26

// Seconds-experiment window: same technique, patched over the number that
// used to sit statically above the bat emblem (was baked as "31").
#define SEC_BG_X 180
#define SEC_BG_Y 294
#define SEC_LABEL_X 180
#define SEC_LABEL_Y 296
#define SEC_LABEL_W 55
#define SEC_LABEL_H 30
// Gold sampled directly off the original baked digit pixels (e.g. (215,310)
// = 251,227,96), used so the live seconds text keeps the same look.
#define SEC_TEXT_COLOR 0xFBE360

static const char *const DAY_NAMES[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

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

    /* ---- Clock hands: each v5 sprite's pivot is its own baked-in hub
       center (see HAND_*_PIVOT_* above), positioned so that pivot lands
       exactly on the dial center, then rotated. ---- */
    hour_hand = lv_image_create(face_batman_410);
    lv_image_set_src(hour_hand, &face_batman_410_hand_hour);
    lv_obj_set_pos(hour_hand, MAIN_CX - HAND_HOUR_PIVOT_X, MAIN_CY - HAND_HOUR_PIVOT_Y);
    lv_image_set_pivot(hour_hand, HAND_HOUR_PIVOT_X, HAND_HOUR_PIVOT_Y);
    lv_obj_remove_flag(hour_hand, LV_OBJ_FLAG_SCROLLABLE);

    min_hand = lv_image_create(face_batman_410);
    lv_image_set_src(min_hand, &face_batman_410_hand_minute);
    lv_obj_set_pos(min_hand, MAIN_CX - HAND_MIN_PIVOT_X, MAIN_CY - HAND_MIN_PIVOT_Y);
    lv_image_set_pivot(min_hand, HAND_MIN_PIVOT_X, HAND_MIN_PIVOT_Y);
    lv_obj_remove_flag(min_hand, LV_OBJ_FLAG_SCROLLABLE);

    sec_hand = lv_image_create(face_batman_410);
    lv_image_set_src(sec_hand, &face_batman_410_hand_second);
    lv_obj_set_pos(sec_hand, MAIN_CX - HAND_SEC_PIVOT_X, MAIN_CY - HAND_SEC_PIVOT_Y);
    lv_image_set_pivot(sec_hand, HAND_SEC_PIVOT_X, HAND_SEC_PIVOT_Y);
    lv_obj_remove_flag(sec_hand, LV_OBJ_FLAG_SCROLLABLE);

    // v5 hands carry their own baked-in gold hub, so the separately-drawn
    // cap circle below is redundant - commented out rather than deleted in
    // case we want the extra cap back on top (e.g. if the baked-in hub
    // reads too small once on real hardware).
    // lv_obj_t *cap = lv_obj_create(face_batman_410);
    // lv_obj_set_size(cap, 8, 8);
    // lv_obj_set_pos(cap, MAIN_CX - 4, MAIN_CY - 4);
    // lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
    // lv_obj_set_style_bg_color(cap, lv_color_hex(0xFFD700), 0);
    // lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
    // lv_obj_set_style_border_width(cap, 0, 0);

    /* ---- Subdial hands: reuse the single hand_sub sprite on both dials. ----
       Removed 2026-08-20 at user request ("remove vector drawing of smaller
       chronos dials, leave those alone for now") - the two subdial *faces*
       (raster, baked into face_bg) stay as-is, only the little animated
       needle sprites are pulled. Commented out, not deleted, in case they
       come back once the subdials get real chronograph data behind them. */
    // sub_hand_l = lv_image_create(face_batman_410);
    // lv_image_set_src(sub_hand_l, &face_batman_410_hand_sub);
    // lv_obj_set_pos(sub_hand_l, SUB_L_CX - 6, SUB_L_CY);
    // lv_image_set_pivot(sub_hand_l, 6, 0);
    // lv_obj_remove_flag(sub_hand_l, LV_OBJ_FLAG_SCROLLABLE);

    // sub_hand_r = lv_image_create(face_batman_410);
    // lv_image_set_src(sub_hand_r, &face_batman_410_hand_sub);
    // lv_obj_set_pos(sub_hand_r, SUB_R_CX - 6, SUB_R_CY);
    // lv_image_set_pivot(sub_hand_r, 6, 0);
    // lv_obj_remove_flag(sub_hand_r, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Day-of-week window: blanked pill patch + live weekday label. ---- */
    day_bg = lv_image_create(face_batman_410);
    lv_image_set_src(day_bg, &face_batman_410_day_bg);
    lv_obj_set_pos(day_bg, DAY_BG_X, DAY_BG_Y);
    lv_obj_remove_flag(day_bg, LV_OBJ_FLAG_SCROLLABLE);

    day_label = lv_label_create(face_batman_410);
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(day_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(day_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(day_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(day_label, 0, 0);
    lv_label_set_text(day_label, "FRI");
    lv_obj_set_pos(day_label, DAY_LABEL_X, DAY_LABEL_Y);
    lv_obj_set_size(day_label, DAY_LABEL_W, DAY_LABEL_H);

    /* ---- Seconds-experiment window: blanked patch + live seconds label. ---- */
    sec_bg = lv_image_create(face_batman_410);
    lv_image_set_src(sec_bg, &face_batman_410_sec_bg);
    lv_obj_set_pos(sec_bg, SEC_BG_X, SEC_BG_Y);
    lv_obj_remove_flag(sec_bg, LV_OBJ_FLAG_SCROLLABLE);

    sec_label = lv_label_create(face_batman_410);
    lv_obj_set_style_text_font(sec_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(sec_label, lv_color_hex(SEC_TEXT_COLOR), 0);
    lv_obj_set_style_text_align(sec_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(sec_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sec_label, 0, 0);
    lv_label_set_text(sec_label, "00");
    lv_obj_set_pos(sec_label, SEC_LABEL_X, SEC_LABEL_Y);
    lv_obj_set_size(sec_label, SEC_LABEL_W, SEC_LABEL_H);

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

    lv_label_set_text(day_label, DAY_NAMES[weekday % 7]);
    lv_label_set_text_fmt(sec_label, "%02d", second);

    float hour_angle = (hour % 12) * 30.0f + minute * 0.5f;
    float min_angle  = minute * 6.0f;
    float sec_angle  = second * 6.0f;

    lv_image_set_rotation(hour_hand, norm_angle_deci(hour_angle - HAND_BASE_DEG));
    lv_image_set_rotation(min_hand, norm_angle_deci(min_angle - HAND_BASE_DEG));
    lv_image_set_rotation(sec_hand, norm_angle_deci(sec_angle - HAND_BASE_DEG));

    // Subdial hand sweep removed 2026-08-20 along with their creation above -
    // see that comment. Left commented rather than deleted for the same
    // reason.
    // float sub_l_angle = second * 6.0f;
    // float sub_r_angle = (hour % 12) * 30.0f + minute * 0.5f;
    // lv_image_set_rotation(sub_hand_l, norm_angle_deci(sub_l_angle - HAND_SUB_BASE_DEG));
    // lv_image_set_rotation(sub_hand_r, norm_angle_deci(sub_r_angle - HAND_SUB_BASE_DEG));

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
