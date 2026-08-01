import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from lvgl_img import encode_from_rgba, format_c_array
from PIL import Image

# One-off: converts the hand-drawn Batman chronograph face (delivered as a zip
# of plain PNGs + a generic SPIFFS-loading .c/.h that doesn't match this
# project's compiled-in lv_img_dsc_t convention - see src/faces/batman_410/
# batman_watchface_410x494.zip's own README) into the project's normal asset
# shape, same as merge_citizen_410_bg.py / gen_red_magic_410.py did for their
# faces. Background is already authored at 410x494 (this project's standard
# "content canvas" size, see citizen_410.c/classic_410.c SCREEN_H), so no
# scaling/letterboxing math is needed here, unlike the _410-suffixed
# conversions of other-resolution source faces.

SRC_DIR = "/home/greg/chronos-watch/src/faces/batman_410/batman_v4_410x494"
OUT_DIR = "/home/greg/chronos-watch/src/faces/batman_410/assets"
os.makedirs(OUT_DIR, exist_ok=True)


def write_asset(name, img, has_alpha, header_extra=""):
    data = encode_from_rgba(img, has_alpha)
    w, h = img.size
    cf = "LV_COLOR_FORMAT_NATIVE_WITH_ALPHA" if has_alpha else "LV_COLOR_FORMAT_RGB565"
    comment = "//RGB565 data with alpha" if has_alpha else "//RGB565 data"
    c_src = f'''{header_extra}#include "../batman_410.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

const LV_ATTRIBUTE_MEM_ALIGN uint8_t {name}_data[] = {{
\t{comment}
{format_c_array(data)}
}};

const lv_img_dsc_t {name} = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w = {w},
    .header.h = {h},
    .data_size = sizeof({name}_data),
    .header.cf = {cf},
    .data = {name}_data}};
'''
    out_path = os.path.join(OUT_DIR, f"{name}.c")
    with open(out_path, "w") as f:
        f.write(c_src)
    print(f"wrote {out_path} ({w}x{h}, {'alpha' if has_alpha else 'opaque'}, {len(data)} bytes)")


# ---- Background: already opaque 410x494, straight RGB565 encode ----
bg = Image.open(os.path.join(SRC_DIR, "batman_bg_410x494.png")).convert("RGBA")
assert bg.size == (410, 494), bg.size
write_asset("face_batman_410_face_bg", bg, has_alpha=False)

# ---- Preview: 160x160 face-picker thumbnail. Per gen_red_magic_410.py's own
# note, the picker card (addWatchface() in ui.c) is a fixed 160x160 slot -
# every other _410 face keeps its preview at exactly that size. Square crop
# centered on the main dial (canvas center (205,247), see batman_410.c),
# vertical span 42..452 keeps it centered and inside the 0..494 canvas,
# then downscaled to 160x160. ----
preview_src = bg.crop((0, 42, 410, 452))
preview = preview_src.resize((160, 160), Image.LANCZOS).convert("RGB").convert("RGBA")
write_asset("face_batman_410_dial_img_preview", preview, has_alpha=False)

# ---- Hands: RGB565 + alpha, straight passthrough (already correctly sized/
# transparent PNGs, no scaling needed since bg is already native 410x494). ----
for fname, outname in [
    ("hand_hour.png", "face_batman_410_hand_hour"),
    ("hand_min.png", "face_batman_410_hand_minute"),
    ("hand_sec.png", "face_batman_410_hand_second"),
    ("hand_sub.png", "face_batman_410_hand_sub"),
]:
    img = Image.open(os.path.join(SRC_DIR, fname)).convert("RGBA")
    write_asset(outname, img, has_alpha=True)

print("done")
