"""
Imports the Poly Haven CC0 textures + HDRI fetched by Tools/fetch_cc0_assets.py
(SourceAssets/PolyHaven/...) into /Game/Realism/Textures/<asset>/ with the right settings:
  *_diff  -> sRGB colour (TC_Default), max 2048
  *_nor_dx-> TC_Normalmap (DirectX green, no flip), max 2048
  *_arm   -> TC_Masks, linear (R = AO, G = Roughness, B = Metallic), max 2048
  *.hdr   -> TextureCube (sky light cubemap for the indoor workshop)

Idempotent (re-imports over existing assets). Called by build_realism_materials.py, or alone:
  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<proj>/Content/Python/import_cc0_textures.py" -unattended -nullrhi
Writes Saved/Realism/import_report.txt (Python log lines do not reach the commandlet log).
"""
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceAssets", "PolyHaven")
DEST = "/Game/Realism/Textures"
REPORT = os.path.join(PROJECT, "Saved", "Realism", "import_report.txt")


def _report(lines):
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as fh:
        fh.write("\n".join(lines) + "\n")


def _configure(tex, filename):
    low = filename.lower()
    if isinstance(tex, unreal.TextureCube):
        return
    tex.set_editor_property("max_texture_size", 2048)
    if "_nor_" in low:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif "_arm_" in low:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)


def asset_name_for(filename):
    base = os.path.splitext(os.path.basename(filename))[0]
    return "T_" + base.replace("_2k", "")


def run():
    lines = []
    if not os.path.isdir(SRC):
        lines.append("NO SOURCE: %s (run Tools/fetch_cc0_assets.py first)" % SRC)
        _report(lines)
        return lines
    tasks = []
    for folder in sorted(os.listdir(SRC)):
        full = os.path.join(SRC, folder)
        if not os.path.isdir(full):
            continue
        for fn in sorted(os.listdir(full)):
            if not fn.lower().endswith((".jpg", ".png", ".hdr", ".exr")):
                continue
            t = unreal.AssetImportTask()
            t.set_editor_property("filename", os.path.join(full, fn))
            t.set_editor_property("destination_path", DEST + "/" + folder)
            t.set_editor_property("destination_name", asset_name_for(fn))
            t.set_editor_property("automated", True)
            t.set_editor_property("replace_existing", True)
            t.set_editor_property("save", False)
            tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    ok = 0
    for t in tasks:
        paths = list(t.get_editor_property("imported_object_paths") or [])
        fn = t.get_editor_property("filename")
        if not paths:
            lines.append("FAIL " + fn)
            continue
        tex = unreal.load_asset(paths[0])
        if tex is None:
            lines.append("FAIL(load) " + paths[0])
            continue
        _configure(tex, os.path.basename(fn))
        unreal.EditorAssetLibrary.save_asset(paths[0].split(".")[0], only_if_is_dirty=False)
        lines.append("OK   %s  (%s)" % (paths[0], tex.get_class().get_name()))
        ok += 1
    lines.append("imported %d / %d" % (ok, len(tasks)))
    _report(lines)
    return lines


if __name__ != "import_cc0_textures":  # run when executed as a script, not when imported
    run()
