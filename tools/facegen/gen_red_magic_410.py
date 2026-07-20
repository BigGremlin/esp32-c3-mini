import re, os, sys, glob
sys.path.insert(0, os.path.dirname(__file__))
from lvgl_img import decode_to_rgba, encode_from_rgba, format_c_array
from PIL import Image

SRC_DIR = "/home/greg/chronos-watch/src/faces/red_magic"
OUT_DIR = "/home/greg/chronos-watch/src/faces/red_magic_410"
OLD = "red_magic"
NEW = "red_magic_410"
OLD_UP = "RED_MAGIC"
NEW_UP = "RED_MAGIC_410"
SRC_W = 360
FACTOR = 410.0 / SRC_W
Y_OFFSET = round((494 - round(SRC_W * FACTOR)) / 2)  # vertical centering on the taller canvas

os.makedirs(os.path.join(OUT_DIR, "assets"), exist_ok=True)


# ---- Asset images: decode every frame in every asset file, scale, re-encode ----
def parse_all_frames(path):
    src = open(path).read()
    frames = []
    for m in re.finditer(
        r'const LV_ATTRIBUTE_MEM_ALIGN uint8_t (\w+)_data_(\d+)\[\] = \{(.*?)\};\s*'
        r'.*?const lv_img_dsc_t (\w+) = \{\s*'
        r'(?:\.header\.(?:always_zero = 0|magic = \w+),\s*)?'
        r'\.header\.w = (\d+),\s*'
        r'\.header\.h = (\d+),\s*'
        r'\.data_size = sizeof\([^)]*\),\s*'
        r'\.header\.cf = (LV_COLOR_FORMAT_\w+),',
        src, re.S):
        base, idx, body, dscname, w, h, cf = m.groups()
        body = re.sub(r'//[^\n]*', '', body)
        vals = bytes(int(x.strip(), 16) for x in body.split(',') if x.strip())
        frames.append({
            "base": base, "idx": idx, "dscname": dscname,
            "w": int(w), "h": int(h), "cf": cf, "vals": vals,
            "has_alpha": "WITH_ALPHA" in cf,
        })
    return frames


def process_asset_file(path, scale=True):
    # The face-picker's thumbnail card (addWatchface() in ui.c) is a fixed 160x160
    # slot regardless of the main canvas resolution - every correctly-sized _410 face
    # (174_410, 1167_410, 2151_410, ...) keeps its preview asset at the original 160x160
    # and only scales the main-canvas assets. First attempt at this script scaled
    # everything uniformly including the preview (160 -> 182), which overflowed that
    # fixed picker slot and looked like the swipe only moving the image within its own
    # frame rather than the whole card - hence the `scale` param to exempt it.
    fname = os.path.basename(path)
    frames = parse_all_frames(path)
    assert frames, f"no frames parsed from {fname}"
    out_lines = []
    header = open(path).read().split("const LV_ATTRIBUTE_MEM_ALIGN", 1)[0]
    header = header.replace(OLD_UP, NEW_UP).replace(OLD, NEW)
    out_lines.append(header.rstrip())
    out_lines.append("")
    dims = None
    for fr in frames:
        img = decode_to_rgba(fr["w"], fr["h"], fr["vals"], fr["has_alpha"])
        new_w = max(1, round(fr["w"] * FACTOR)) if scale else fr["w"]
        new_h = max(1, round(fr["h"] * FACTOR)) if scale else fr["h"]
        dims = (new_w, new_h)
        img2 = img.resize((new_w, new_h), Image.BICUBIC) if scale else img
        data = encode_from_rgba(img2, fr["has_alpha"])
        new_base = fr["base"].replace(OLD, NEW)
        new_dscname = fr["dscname"].replace(OLD, NEW)
        arr_name = f"{new_base}_data_{fr['idx']}"
        out_lines.append(f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {arr_name}[] = {{")
        out_lines.append("\t//RGB565 data" if not fr["has_alpha"] else "\t//RGB565 data with alpha")
        out_lines.append(format_c_array(data))
        out_lines.append("};")
        out_lines.append("")
        out_lines.append(f"const lv_img_dsc_t {new_dscname} = {{")
        out_lines.append("    .header.magic = LV_IMAGE_HEADER_MAGIC,")
        out_lines.append(f"    .header.w = {new_w},")
        out_lines.append(f"    .header.h = {new_h},")
        out_lines.append(f"    .data_size = sizeof({arr_name}),")
        out_lines.append(f"    .header.cf = {fr['cf']},")
        out_lines.append(f"    .data = {arr_name}}};")
        out_lines.append("")
    new_fname = fname.replace(OLD, NEW)
    with open(os.path.join(OUT_DIR, "assets", new_fname), "w") as f:
        f.write("\n".join(out_lines))
    print(f"{fname} -> {new_fname}  ({frames[0]['w']}x{frames[0]['h']} -> {dims[0]}x{dims[1]}, {len(frames)} frame(s))")


asset_paths = sorted(glob.glob(os.path.join(SRC_DIR, "assets", "*.c")))
assert asset_paths, "no asset files found"
for p in asset_paths:
    process_asset_file(p, scale="preview" not in os.path.basename(p))

print("Y_OFFSET:", Y_OFFSET, "FACTOR:", FACTOR)


# ---- Logic .c/.h: regex-rewrite the bin2lvgl output rather than hand-transcribing ----
# (75_2_410 was hand-transcribed into a template because it only had 5 elements;
# red_magic has ~50, so a mechanical position-scaling pass over the original
# generated source is far less error-prone than retyping it by hand.)
def scale_positions(src):
    def repl_x(m):
        return f"{m.group(1)}({m.group(2)}, {round(int(m.group(3)) * FACTOR)});"

    def repl_y(m):
        return f"{m.group(1)}({m.group(2)}, {round(int(m.group(3)) * FACTOR) + Y_OFFSET});"

    def repl_pivot(m):
        fn, ident, x, y = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
        return f"{fn}({ident}, {round(x * FACTOR)}, {round(y * FACTOR)});"

    src = re.sub(r'(lv_obj_set_x)\((\w+), (-?\d+)\);', repl_x, src)
    src = re.sub(r'(lv_obj_set_y)\((\w+), (-?\d+)\);', repl_y, src)
    src = re.sub(r'(lv_img_set_pivot|lv_image_set_pivot)\((\w+), (-?\d+), (-?\d+)\);', repl_pivot, src)
    return src


def rename(src):
    # Longest/most-specific identifiers first so e.g. "RED_MAGIC" inside
    # "_FACE_RED_MAGIC_H" isn't left half-renamed. Plain substring replace,
    # not \b-bounded regex: every occurrence is "..._red_magic_..." with
    # underscores on both sides, and \w includes "_" so \b never matches there.
    src = src.replace(f"_FACE_{OLD_UP}_H", f"_FACE_{NEW_UP}_H")
    src = src.replace(f"ENABLE_FACE_{OLD_UP}", f"ENABLE_FACE_{NEW_UP}")
    src = src.replace(OLD, NEW)
    return src


c_src = open(os.path.join(SRC_DIR, f"{OLD}.c")).read()
h_src = open(os.path.join(SRC_DIR, f"{OLD}.h")).read()

c_src = rename(scale_positions(c_src))
h_src = rename(h_src)

banner = (f"// File generated by bin2lvgl, then scaled {SRC_W}x{SRC_W} -> "
          f"{round(SRC_W*FACTOR)}x{round(SRC_W*FACTOR)} (centered on a 410x494 canvas)\n"
          f"// developed by fbiego.\n// https://github.com/fbiego\n"
          f"// Watchface: {NEW} (derived from {OLD} \"Red Magic\" by sernason, via chronos.ke)\n")
c_src = re.sub(r'// File generated by bin2lvgl\n.*?\n.*?\n.*?\n', banner, c_src, count=1)
h_src = re.sub(r'// File generated by bin2lvgl\n.*?\n.*?\n.*?\n', banner, h_src, count=1)

with open(os.path.join(OUT_DIR, f"{NEW}.c"), "w") as f:
    f.write(c_src)
with open(os.path.join(OUT_DIR, f"{NEW}.h"), "w") as f:
    f.write(h_src)

print(f"wrote {NEW}.c and {NEW}.h")
