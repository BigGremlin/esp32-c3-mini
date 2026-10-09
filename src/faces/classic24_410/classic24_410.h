
// Hand-authored watchface: 24-hour variant of classic_410 (hour hand makes one
// turn per day, 24 at the top). Reuses classic_410's baked cog images, so
// ENABLE_FACE_CLASSIC_410 must also be defined.
// Watchface: classic24_410

#ifndef _FACE_CLASSIC24_410_H
#define _FACE_CLASSIC24_410_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "app_hal.h"

//#define ENABLE_FACE_CLASSIC24_410 // (Classic 24) uncomment to enable or define it elsewhere

#if defined(ENABLE_FACE_CLASSIC24_410) && !defined(ENABLE_FACE_CLASSIC_410)
#error "classic24_410 reuses classic_410's cog images - define ENABLE_FACE_CLASSIC_410 too"
#endif

#ifdef ENABLE_FACE_CLASSIC24_410
    extern lv_obj_t *face_classic24_410;

    LV_IMG_DECLARE(face_classic24_410_dial_img_preview_0);
    LV_IMG_DECLARE(classic_410_gear_a_img);
    LV_IMG_DECLARE(classic_410_gear_b_img);
    LV_IMG_DECLARE(classic_410_gear_c_img);
#endif

    void onFaceEvent(lv_event_t * e);

    void init_face_classic24_410(void (*callback)(const char*, const lv_img_dsc_t *, lv_obj_t **, lv_obj_t **));
    void update_time_classic24_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday);
    void update_weather_classic24_410(int temp, int icon);
    void update_status_classic24_410(int battery, bool connection, bool plugged, bool charging);
    void update_activity_classic24_410(int steps, int distance, int kcal);
    void update_health_classic24_410(int bpm, int oxygen);
    void update_all_classic24_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
                int temp, int icon, int battery, bool connection, bool plugged, bool charging, int steps, int distance, int kcal, int bpm, int oxygen);
    void update_check_classic24_410(lv_obj_t *root, int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
                int temp, int icon, int battery, bool connection, bool plugged, bool charging, int steps, int distance, int kcal, int bpm, int oxygen);


#ifdef __cplusplus
}
#endif

#endif
