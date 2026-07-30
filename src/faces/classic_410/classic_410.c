
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

// Mechanical "cogs" window at 12 o'clock, ported from the original RetroPie
// LVGL-sim prototype (/home/pi/lv_port_linux/src/watch_face.c on the
// 192.168.0.37 dev Pi, create_gear()/create_mech_window()/gear_timer_cb() -
// pulled 2026-07-30 via SSH, same source this project's own dial/hands/ticks
// were originally derived from). Three interlocking gears, each a real LVGL
// widget (not canvas-drawn like the ticks/numerals above) so they can be
// rotated independently via lv_obj_set_style_transform_angle.
static lv_obj_t *mech_win    = NULL;
static lv_obj_t *gear_layer  = NULL;
static lv_obj_t *gear_a      = NULL;
static lv_obj_t *gear_b      = NULL;
static lv_obj_t *gear_c      = NULL;
static int gear_a_ang = 0;
static int gear_b_ang = 0;
static int gear_c_ang = 0;
static lv_timer_t *gear_timer = NULL;

// Cogs window frame: an annulus sector centred on the dial, not a rectangle - top/bottom
// edges are arcs concentric with the dial, left/right edges are straight radial lines (so
// unlike a rectangle's corners, no part of the frame sticks out further from the dial centre
// than its own outer edge - see the 2026-07-30 canvas pass below for the actual draw calls).
// Radii chosen so the outer edge sits just inside hour_nums's "12" radius band (~121-165, see
// Pass 3) so "12" can be drawn again; thickness (outer-inner) deliberately kept identical to
// the gears' own original 83px window height - the gear positions/sizes below are untouched,
// only this frame's shape and the window's overall on-screen position move to match it.
#define COG_OUTER_R 115
#define COG_INNER_R 32
#define COG_HALF_ANGLE_DEG 40.0f

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

// Ported near-verbatim from the RetroPie source's create_gear(): a gear as a
// plain lv_obj (arc rim + radial teeth + a black hub with a coloured ring),
// sized/positioned in the parent's own coordinate space and rotatable as one
// unit via the object's own transform_angle (pivot set to its centre) - LVGL
// applies a widget's transform to itself and its children together, so the
// rim/teeth/hub all turn as a single gear despite being separate child objects.
static lv_obj_t *create_gear(lv_obj_t *parent, int32_t x, int32_t y, int32_t r, int teeth, lv_color_t color)
{
    lv_obj_t *g = lv_obj_create(parent);
    lv_obj_set_size(g, 2 * r, 2 * r);
    lv_obj_set_pos(g, x, y);
    lv_obj_remove_flag(g, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(g, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g, 0, 0);
    lv_obj_set_style_pad_all(g, 0, 0);
    lv_obj_set_style_transform_pivot_x(g, r, 0);
    lv_obj_set_style_transform_pivot_y(g, r, 0);

    lv_obj_t *arc = lv_arc_create(g);
    lv_obj_set_size(arc, 2 * r, 2 * r);
    lv_obj_set_pos(arc, 0, 0);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_set_style_arc_width(arc, (r >= 30) ? 6 : 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc, 0, 0);
    // LVGL's default theme puts an accent-coloured circular knob on every arc widget
    // (theme->styles.knob, LV_PART_KNOB) unless explicitly overridden - this is the stray
    // "disc" that shows up riding the rim of each gear. Not part of the intended gear design
    // at all, just an un-styled theme default; recolour it black to match the hub.
    lv_obj_set_style_bg_color(arc, lv_color_black(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(arc, 0, LV_PART_KNOB);

    int32_t tooth_w = (r >= 30) ? 10 : 8;
    int32_t tooth_h = (r >= 30) ? 10 : 8;

    for (int i = 0; i < teeth; i++) {
        float a = (i * 360.0f / teeth) * PI_F / 180.0f;
        int32_t cx = r + (int32_t)lroundf((r - (tooth_h / 2)) * sinf(a));
        int32_t cy = r - (int32_t)lroundf((r - (tooth_h / 2)) * cosf(a));

        lv_obj_t *tooth = lv_obj_create(g);
        lv_obj_set_size(tooth, tooth_w, tooth_h);
        lv_obj_set_pos(tooth, cx - tooth_w / 2, cy - tooth_h / 2);
        lv_obj_set_style_bg_color(tooth, color, 0);
        lv_obj_set_style_bg_opa(tooth, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tooth, 0, 0);
        lv_obj_set_style_radius(tooth, 2, 0);
        lv_obj_set_style_transform_pivot_x(tooth, tooth_w / 2, 0);
        lv_obj_set_style_transform_pivot_y(tooth, tooth_h / 2, 0);
        lv_obj_set_style_transform_angle(tooth, (int32_t)lroundf((i * 360.0f / teeth) * 10), 0);
    }

    lv_obj_t *hub = lv_obj_create(g);
    int32_t hub_r = r / 3;
    lv_obj_set_size(hub, 2 * hub_r, 2 * hub_r);
    lv_obj_set_pos(hub, r - hub_r, r - hub_r);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hub, 2, 0);
    lv_obj_set_style_border_color(hub, color, 0);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_CLICKABLE);

    return g;
}

// Same per-tick deltas/direction as the RetroPie source (a:+3, b:-4, c:+6
// degrees per 50ms tick - the three gears turn at different speeds/directions
// on purpose, it's decorative rather than a real meshed gear ratio). Only
// does work while the classic face is actually the visible screen, so it
// doesn't spend cycles animating an off-screen watchface.
static void gear_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (lv_screen_active() != face_classic_410) return;

    gear_a_ang = (gear_a_ang + 3) % 360;
    gear_b_ang = (gear_b_ang - 4 + 360) % 360;
    gear_c_ang = (gear_c_ang + 6) % 360;

    lv_obj_set_style_transform_angle(gear_a, gear_a_ang * 10, 0);
    lv_obj_set_style_transform_angle(gear_b, gear_b_ang * 10, 0);
    lv_obj_set_style_transform_angle(gear_c, gear_c_ang * 10, 0);
}

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

    /* ---- Pass 4: cogs window frame - annulus sector (arc top/bottom, radial sides) ----
       LVGL's own arc-angle convention (0deg = 3 o'clock, clockwise) differs from the
       clock-angle convention (0deg = 12 o'clock, clockwise) used everywhere else in this
       file for ticks/numerals/hands, so angles are converted at the point of use below
       rather than changing the shared convention. */
    lv_canvas_init_layer(dial_canvas, &layer);
    {
        float half_a = COG_HALF_ANGLE_DEG * PI_F / 180.0f;

        // Filled band: a wide arc stroke, radius = band midline, width = band thickness.
        lv_draw_arc_dsc_t band; lv_draw_arc_dsc_init(&band);
        band.color = lv_color_hex(0x141414); band.opa = LV_OPA_COVER;
        band.center.x = CANVAS_CX; band.center.y = CANVAS_CY;
        band.radius = (COG_OUTER_R + COG_INNER_R) / 2;
        band.width = COG_OUTER_R - COG_INNER_R;
        band.rounded = 0;
        band.start_angle = 270.0f - COG_HALF_ANGLE_DEG;
        band.end_angle   = 270.0f + COG_HALF_ANGLE_DEG;
        lv_draw_arc(&layer, &band);

        // Border: outer arc, inner arc, then the two radial sides as straight lines.
        lv_draw_arc_dsc_t border; lv_draw_arc_dsc_init(&border);
        border.color = lv_color_hex(0xFF0000); border.opa = LV_OPA_COVER;
        border.center.x = CANVAS_CX; border.center.y = CANVAS_CY;
        border.width = 2; border.rounded = 0;
        border.start_angle = band.start_angle; border.end_angle = band.end_angle;

        border.radius = COG_OUTER_R;
        lv_draw_arc(&layer, &border);
        border.radius = COG_INNER_R;
        lv_draw_arc(&layer, &border);

        lv_draw_line_dsc_t side; lv_draw_line_dsc_init(&side);
        side.color = lv_color_hex(0xFF0000); side.width = 2; side.opa = LV_OPA_COVER;
        side.round_start = side.round_end = 0;

        side.p1.x = (int32_t)(CANVAS_CX + sinf(-half_a) * COG_OUTER_R);
        side.p1.y = (int32_t)(CANVAS_CY - cosf(-half_a) * COG_OUTER_R);
        side.p2.x = (int32_t)(CANVAS_CX + sinf(-half_a) * COG_INNER_R);
        side.p2.y = (int32_t)(CANVAS_CY - cosf(-half_a) * COG_INNER_R);
        lv_draw_line(&layer, &side);

        side.p1.x = (int32_t)(CANVAS_CX + sinf(half_a) * COG_OUTER_R);
        side.p1.y = (int32_t)(CANVAS_CY - cosf(half_a) * COG_OUTER_R);
        side.p2.x = (int32_t)(CANVAS_CX + sinf(half_a) * COG_INNER_R);
        side.p2.y = (int32_t)(CANVAS_CY - cosf(half_a) * COG_INNER_R);
        lv_draw_line(&layer, &side);
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

    /* ---- Cogs window - 12 o'clock. The visible frame (dark fill, red arc/radial border) is
       now drawn on the canvas in Pass 4 above, at radius COG_OUTER_R..COG_INNER_R; mech_win
       here is just a plain (invisible) clipping rect for the gears, sized/positioned to bound
       that same annulus sector so the two line up. Gears themselves (below) are untouched from
       the original rectangular-window version - same sizes, same layout relative to this rect. ---- */
    {
        const int32_t MECH_W = 128;
        const int32_t MECH_H = COG_OUTER_R - COG_INNER_R;
        int32_t mech_x = SCREEN_CX - MECH_W / 2;
        int32_t mech_y = SCREEN_CY - COG_OUTER_R;

        mech_win = lv_obj_create(face_classic_410);
        lv_obj_set_size(mech_win, MECH_W, MECH_H);
        lv_obj_set_pos(mech_win, mech_x, mech_y);
        lv_obj_set_style_bg_opa(mech_win, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(mech_win, 0, 0);
        lv_obj_set_style_pad_all(mech_win, 0, 0);
        lv_obj_remove_flag(mech_win, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(mech_win, LV_OBJ_FLAG_CLICKABLE);

        // Oversized inner layer (window clips it) so gear coordinates stay simple positive
        // numbers, same trick as the source's gear_layer.
        gear_layer = lv_obj_create(mech_win);
        lv_obj_set_pos(gear_layer, 0, 0);
        lv_obj_set_size(gear_layer, 260, 200);
        lv_obj_set_style_bg_opa(gear_layer, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(gear_layer, 0, 0);
        lv_obj_set_style_pad_all(gear_layer, 0, 0);
        lv_obj_remove_flag(gear_layer, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(gear_layer, LV_OBJ_FLAG_CLICKABLE);

        lv_color_t metal = lv_color_make(180, 180, 180);
        const int32_t ga_x = 18,  ga_y = 20,  ga_r = 34;
        const int32_t gb_x = 70,  gb_y = 44,  gb_r = 28;
        const int32_t gc_x = 112, gc_y = 18,  gc_r = 22;

        gear_a = create_gear(gear_layer, ga_x, ga_y, ga_r, 10, metal);
        gear_b = create_gear(gear_layer, gb_x, gb_y, gb_r, 10, metal);
        gear_c = create_gear(gear_layer, gc_x, gc_y, gc_r, 10, metal);

        // Recentre the group of three gear centres inside the MECH_W x MECH_H window by
        // shifting gear_layer as a whole (same approach as the source).
        float cax = (float)ga_x + ga_r, cay = (float)ga_y + ga_r;
        float cbx = (float)gb_x + gb_r, cby = (float)gb_y + gb_r;
        float ccx = (float)gc_x + gc_r, ccy = (float)gc_y + gc_r;
        float minx = cax, maxx = cax, miny = cay, maxy = cay;
        if (cbx < minx) minx = cbx; if (cbx > maxx) maxx = cbx;
        if (ccx < minx) minx = ccx; if (ccx > maxx) maxx = ccx;
        if (cby < miny) miny = cby; if (cby > maxy) maxy = cby;
        if (ccy < miny) miny = ccy; if (ccy > maxy) maxy = ccy;
        float group_cx = (minx + maxx) * 0.5f, group_cy = (miny + maxy) * 0.5f;
        float target_cx = (float)MECH_W * 0.5f, target_cy = (float)MECH_H * 0.5f;
        lv_obj_set_pos(gear_layer, (int32_t)lroundf(target_cx - group_cx), (int32_t)lroundf(target_cy - group_cy));

        // Erase mask: "a window is a window" - anything the gears draw outside the actual
        // keystone shape (not just the rectangular gear_layer clip) needs to disappear, same
        // hard edge the rectangle already gives on its own sides. Since LVGL has no built-in
        // way to clip a widget tree to an arbitrary sector, this is done as a same-size ARGB
        // overlay canvas sitting on top of the gears (created after them, so it paints over
        // them), opaque black everywhere in the MECH_W x MECH_H rect that's NOT inside the
        // sector (radius/angle test per pixel, done once at init - the sector itself is
        // static, only the gears under it rotate), fully transparent everywhere inside it so
        // the gears show through untouched.
        void *mask_buf = lv_malloc(MECH_W * MECH_H * 4);
        if (mask_buf) {
            lv_obj_t *mask_canvas = lv_canvas_create(face_classic_410);
            lv_canvas_set_buffer(mask_canvas, mask_buf, MECH_W, MECH_H, LV_COLOR_FORMAT_ARGB8888);
            lv_obj_set_pos(mask_canvas, mech_x, mech_y);
            lv_obj_remove_flag(mask_canvas, LV_OBJ_FLAG_CLICKABLE);
            lv_canvas_fill_bg(mask_canvas, lv_color_black(), LV_OPA_TRANSP);

            for (int32_t py = 0; py < MECH_H; py++) {
                for (int32_t px = 0; px < MECH_W; px++) {
                    float dx = (float)(mech_x + px) - CANVAS_CX;
                    float dy = (float)(mech_y - Y_OFFSET + py) - CANVAS_CY;
                    float rad = sqrtf(dx * dx + dy * dy);
                    float clock_deg = atan2f(dx, -dy) * 180.0f / PI_F;
                    bool inside = (rad >= COG_INNER_R && rad <= COG_OUTER_R &&
                                   fabsf(clock_deg) <= COG_HALF_ANGLE_DEG);
                    if (!inside) {
                        lv_canvas_set_px(mask_canvas, px, py, lv_color_black(), LV_OPA_COVER);
                    }
                }
            }
        }
    }

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

    /* Cogs animation timer - same 50ms cadence as the RetroPie source, gated to only spend
       cycles while this face is the visible screen (see gear_timer_cb above). */
    gear_timer = lv_timer_create(gear_timer_cb, 50, NULL);
    gear_timer_cb(gear_timer);

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
