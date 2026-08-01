
// Hand-authored watchface (not bin2lvgl-generated): Batman chronograph dial,
// delivered 2026-07-31 as a zip of plain PNGs (batman_bg_410x494.png + 4 hand
// sprites) plus a generic batman_watchface.c/.h that assumed a totally
// different architecture (runtime SPIFFS PNG loading, standalone create()/
// update() functions) - see batman_410/batman_watchface_410x494.zip's own
// README. Rebuilt here to match this project's real convention (compiled-in
// lv_img_dsc_t assets, init_face_*/update_*_*/update_check_* functions), same
// technique as citizen_410. Re-sourced 2026-08-01 from a redelivered, cleaner
// package (batman_v4_410x494/) with the same architecture mismatch, same fix.
// Watchface: batman_410

#ifndef _FACE_BATMAN_410_H
#define _FACE_BATMAN_410_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "app_hal.h"

//#define ENABLE_FACE_BATMAN_410 // (Batman) uncomment to enable or define it elsewhere

#ifdef ENABLE_FACE_BATMAN_410
    extern lv_obj_t *face_batman_410;

    LV_IMG_DECLARE(face_batman_410_face_bg);
    LV_IMG_DECLARE(face_batman_410_dial_img_preview);
    LV_IMG_DECLARE(face_batman_410_hand_hour);
    LV_IMG_DECLARE(face_batman_410_hand_minute);
    LV_IMG_DECLARE(face_batman_410_hand_second);
    LV_IMG_DECLARE(face_batman_410_hand_sub);
#endif

    void onFaceEvent(lv_event_t * e);

    void init_face_batman_410(void (*callback)(const char*, const lv_img_dsc_t *, lv_obj_t **, lv_obj_t **));
    void update_time_batman_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday);
    void update_weather_batman_410(int temp, int icon);
    void update_status_batman_410(int battery, bool connection);
    void update_activity_batman_410(int steps, int distance, int kcal);
    void update_health_batman_410(int bpm, int oxygen);
    void update_all_batman_410(int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
                int temp, int icon, int battery, bool connection, int steps, int distance, int kcal, int bpm, int oxygen);
    void update_check_batman_410(lv_obj_t *root, int second, int minute, int hour, bool mode, bool am, int day, int month, int year, int weekday,
                int temp, int icon, int battery, bool connection, int steps, int distance, int kcal, int bpm, int oxygen);


#ifdef __cplusplus
}
#endif

#endif
