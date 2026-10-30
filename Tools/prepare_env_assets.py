#!/usr/bin/env python3
"""Pick the subset of Dum-E's CC0 pack (contrib/dume/assets) the game uses and shrink it to 1K.

  python3 Tools/prepare_env_assets.py [--res 1024] [--model-res 512]

Writes SourceAssets/Env/ (not shipped; imported by Content/Python/import_env_assets.py into /Game/Env):
  Textures/<id>/<id>_diff.jpg, _nor_dx.jpg (DirectX normals), _arm.jpg (AO, Roughness, Metal packed)
  Models/<id>/...gltf + .bin + textures resized (same file names, so the glTF still resolves)
  HDRI/<id>.hdr (copied as is)
Why 1K: the full pack is 605 MB; the repo budget is ~400 MB in total. All assets are CC0.
"""
import argparse, json, os, shutil, sys
from PIL import Image

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACK = os.path.join(os.path.dirname(HERE), "contrib", "dume", "assets")
OUT = os.path.join(HERE, "SourceAssets", "Env")

TEXTURES = ["Road007", "Road008A", "concrete_pavement", "TactilePaving001", "Facade018A", "concrete_tile_facade",
            "concrete_wall_008", "concrete_debris", "Ground107", "Metal012"]
MODELS = ["modular_urban_apartments_facade", "modular_factory_facade", "rollershutter_door", "rollershutter_window_01",
          "modular_metal_gutter", "street_lamp_01", "street_lamp_02", "concrete_road_barrier", "concrete_road_barrier_02",
          "wooden_crate_01", "plastic_crate_01", "wooden_military_crate", "fire_hydrant", "metal_trash_can", "covered_car"]
HDRIS = ["kloofendal_43d_clear_puresky", "qwantani_dusk_2_puresky"]


def find(folder, *keys):
    for fn in sorted(os.listdir(folder)):
        low = fn.lower()
        if low.endswith(".jpg") and any(k in low for k in keys):
            return os.path.join(folder, fn)
    return None


def load(path, res, mode):
    im = Image.open(path).convert(mode)
    return im.resize((res, res), Image.LANCZOS) if im.size[0] > res else im


def texture_set(tid, res):
    src = None
    for cat in os.listdir(PACK):
        p = os.path.join(PACK, cat, tid)
        if os.path.isdir(p):
            src = p
    if not src:
        return "FAIL %s: not in pack" % tid
    dst = os.path.join(OUT, "Textures", tid)
    os.makedirs(dst, exist_ok=True)
    diff = find(src, "_diff", "_color")
    nor = find(src, "_nor_dx", "normaldx")
    if not diff or not nor:
        return "FAIL %s: no diffuse/normal" % tid
    load(diff, res, "RGB").save(os.path.join(dst, tid + "_diff.jpg"), quality=92)
    load(nor, res, "RGB").save(os.path.join(dst, tid + "_nor_dx.jpg"), quality=95)
    arm = find(src, "_arm")
    if arm:
        load(arm, res, "RGB").save(os.path.join(dst, tid + "_arm.jpg"), quality=92)
    else:
        rough = find(src, "_rough", "roughness")
        ao = find(src, "_ao", "ambientocclusion")
        metal = find(src, "metalness", "_metal")
        size = load(diff, res, "RGB").size
        r = load(ao, res, "L") if ao else Image.new("L", size, 255)
        g = load(rough, res, "L") if rough else Image.new("L", size, 200)
        b = load(metal, res, "L") if metal else Image.new("L", size, 0)
        Image.merge("RGB", (r, g, b)).save(os.path.join(dst, tid + "_arm.jpg"), quality=92)
    return "OK   texture " + tid


def model(mid, res):
    src = None
    for cat in os.listdir(PACK):
        p = os.path.join(PACK, cat, mid)
        if os.path.isdir(p) and any(f.endswith(".gltf") for f in os.listdir(p)):
            src = p
    if not src:
        return "FAIL %s: not in pack" % mid
    dst = os.path.join(OUT, "Models", mid)
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    for root, _, files in os.walk(src):
        for fn in files:
            rel = os.path.relpath(os.path.join(root, fn), src)
            out = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            if fn.lower().endswith(".jpg"):
                im = Image.open(os.path.join(root, fn))
                if im.size[0] > res:
                    im = im.resize((res, res), Image.LANCZOS)
                im.convert("RGB").save(out, quality=92)
            else:
                shutil.copy2(os.path.join(root, fn), out)
    return "OK   model " + mid


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--res", type=int, default=1024)
    # Interchange stores glTF texture sources losslessly (about 1.3-2.7 MB per 1K map), so model
    # textures go in at 512 to keep the repo inside the ~400 MB budget.
    ap.add_argument("--model-res", type=int, default=512)
    a = ap.parse_args()
    if not os.path.isdir(PACK):
        sys.exit("no pack at " + PACK)
    log = [texture_set(t, a.res) for t in TEXTURES] + [model(m, a.model_res) for m in MODELS]
    os.makedirs(os.path.join(OUT, "HDRI"), exist_ok=True)
    for h in HDRIS:
        p = os.path.join(PACK, "hdris", h)
        hdr = [f for f in os.listdir(p) if f.endswith(".hdr")] if os.path.isdir(p) else []
        if hdr:
            shutil.copy2(os.path.join(p, hdr[0]), os.path.join(OUT, "HDRI", h + ".hdr"))
            log.append("OK   hdri " + h)
        else:
            log.append("FAIL hdri " + h)
    print("\n".join(log))


if __name__ == "__main__":
    main()
