"""
Imports EDITH's helmet-HUD fonts (SIL OFL 1.1; fetched by Tools/fetch_ui_fonts.py into
SourceAssets/Fonts/<family>/) as Font Faces under /Game/UI/Fonts/Faces/ and assembles one
runtime composite Font per family at /Game/UI/Fonts/F_<family> (the paths hud_layout.json names).
Typeface names come from hud_layout.json "fonts"; a typeface without its own file (Share Tech Mono
Bold, the variable Exo 2 / Orbitron weights) maps to the family's one face - variable-font weight
axes are not selected by UE's FFontData, so those render at the file's default instance.

  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<proj>/Content/Python/import_ui_fonts.py" -unattended -nullrhi
Report: Saved/UI/import_fonts_report.txt
"""
import json
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceAssets", "Fonts")
LAYOUT = os.path.join(PROJECT, "Content", "UI", "hud_layout.json")
FACE_DIR = "/Game/UI/Fonts/Faces"
REPORT = os.path.join(PROJECT, "Saved", "UI", "import_fonts_report.txt")
LOG = []


def log(s):
    LOG.append(s)
    unreal.log(s)


def import_face(path, name):
    """Font Face asset from a .ttf without the import factory (its options dialog needs Slate)."""
    pkg = FACE_DIR + "/" + name
    face = unreal.ArenaFontLibrary.create_font_face_from_file(path, pkg)
    if face is None:
        return None
    unreal.EditorAssetLibrary.save_loaded_asset(face, only_if_is_dirty=False)
    return face


def pick_file(files, typeface):
    """Best file in a family folder for a typeface name ('Bold' -> *-Bold.ttf, else the Regular/variable one)."""
    low = {f.lower(): f for f in files}
    for f in files:
        stem = os.path.splitext(f)[0].lower()
        if stem.endswith("-" + typeface.lower()):
            return f
    for key in ("-regular.ttf", "-variable.ttf"):
        for l, f in low.items():
            if l.endswith(key):
                return f
    return files[0] if files else None


def main():
    layout = json.load(open(LAYOUT))
    fonts = {k: v for k, v in layout.get("fonts", {}).items() if not k.startswith("_")}
    ok = 0
    for key, spec in fonts.items():
        asset = spec.get("asset", "")
        family_dir = os.path.join(SRC, asset.rsplit("/F_", 1)[-1]) if "/F_" in asset else None
        if not family_dir or not os.path.isdir(family_dir):
            log("SKIP %s: no folder %s (run Tools/fetch_ui_fonts.py)" % (key, family_dir))
            continue
        files = sorted(f for f in os.listdir(family_dir) if f.lower().endswith((".ttf", ".otf")))
        faces = {}
        names, objs = [], []
        for tf in spec.get("typefaces", ["Regular"]):
            f = pick_file(files, tf)
            if not f:
                continue
            if f not in faces:
                faces[f] = import_face(os.path.join(family_dir, f), "FF_" + os.path.splitext(f)[0].replace("-", "_"))
            if faces[f] is None:
                log("FAIL %s: could not import %s" % (key, f))
                continue
            names.append(tf)
            objs.append(faces[f])
        if not objs:
            continue
        pkg, short = asset.rsplit("/", 1)
        font = unreal.load_asset(asset) if unreal.EditorAssetLibrary.does_asset_exist(asset) else None
        if font is None:
            font = unreal.AssetToolsHelpers.get_asset_tools().create_asset(short, pkg, unreal.Font, unreal.FontFactory())
        if font is None:
            log("FAIL %s: could not create %s" % (key, asset))
            continue
        n = unreal.ArenaFontLibrary.build_composite_font(font, [unreal.Name(x) for x in names], objs)
        unreal.EditorAssetLibrary.save_loaded_asset(font, only_if_is_dirty=False)
        got = [str(x) for x in unreal.ArenaFontLibrary.get_typeface_names(font)]
        log("OK   %s -> %s typefaces %s (%d entries, faces: %s)" % (key, asset, got, n, ", ".join(sorted(faces))))
        ok += 1
    log("fonts built %d / %d" % (ok, len(fonts)))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    open(REPORT, "w").write("\n".join(LOG) + "\n")


main()
