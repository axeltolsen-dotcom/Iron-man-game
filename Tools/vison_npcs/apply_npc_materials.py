"""
apply_npc_materials.py  (Vison, Tools/vison_npcs)

Creates NPC look assets from npc_materials.json, ONLY under /Game/Vison/NPCs/:
  * SubsurfaceProfile assets   SP_Skin_T0..T7, SP_Skin_Child, SP_Synthezoid
                               (hero / close-up only: player, Harley, Morgan, cinematics)
  * Hero material instances    MI_NPC_Skin_<Age>_T<n>, MI_NPC_Hair_<color>, MI_NPC_Eye_<iris>
  * Crowd material instances   MI_NPC_CrowdSkin_T<n> (default-lit, no Subsurface Profile)
  * Shared cloth / props       MI_NPC_Cloth_<fabric>, MI_NPC_<leather/rubber>, MI_NPC_Prop_<prop>
  * Optional simple masters    M_NPC_Skin / M_NPC_SkinDefaultLit / M_NPC_Hair / M_NPC_Eye /
                               M_NPC_Cloth / M_NPC_Generic
                               (only with --build-masters, and only if no better parent exists)

CityKit street crowd uses default-lit MIs only (no Subsurface Profile, eye, hair or SSS shading).
SP_Skin profiles are only for hero / close-up characters.

Skin parent for hero MIs is resolved by a runtime scan of /Game/Kevin, /Game/CharacterCreator,
/Game/Reallusion and /Game/MetaHumans/<Name>/, plus any material path containing Kevin,
CharacterCreator, Reallusion or CC_Base. Do not hardcode /Game/Characters/Kevin/...

SAFE BY DEFAULT
  * Dry run is the default. Nothing is created, changed or saved unless --apply is given
    (--dry-run wins over --apply).
  * Every asset path is checked against /Game/Vison/NPCs/ before any write; anything else aborts.
  * Never deletes, renames, or edits assets outside /Game/Vison/NPCs (MetaHuman, Kevin, CityKit,
    /Game/NPC, /Game/Edith, /Game/Realism are read-only parents at most).
  * Only parameters that exist on the parent are written; the rest are logged 'skip: not on parent'.
  * Every change is logged old -> new.
  * Outside the editor (no 'unreal' module) it runs as an offline planner.

USAGE (UE 5.8 editor, Output Log, Python):
  py "<repo>/Tools/vison_npcs/apply_npc_materials.py"                      # dry run, everything
  py "<repo>/Tools/vison_npcs/apply_npc_materials.py" --usage crowd         # CityKit default-lit only
  py "<repo>/Tools/vison_npcs/apply_npc_materials.py" --only skin,hair --apply
  py "<repo>/Tools/vison_npcs/apply_npc_materials.py" --build-masters --apply
Offline:  python3 apply_npc_materials.py
"""
import argparse
import json
import os
import sys

try:
    import unreal  # noqa: F401
    IN_UE = True
except ImportError:
    unreal = None
    IN_UE = False

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
DEFAULT_JSON = os.path.join(HERE, "npc_materials.json")
OUT_DIR = "/Game/Vison/NPCs"
ALLOWED_PREFIX = OUT_DIR + "/"
AGES = [("Child", "child_6_12"), ("Teen", "teen_13_17"), ("Adult", "adult_18_64"), ("Older", "older_65_plus")]

# Parent candidates, first that exists wins. Others' assets are only used as parents (read-only).
# Hero skin parents are NOT hardcoded: resolve_parent("skin") runs a runtime scan (see scan_premium_skin_parents).
PARENTS = {
    "skin": [OUT_DIR + "/M_NPC_Skin"],  # fallback after the Kevin / CC / MetaHuman scan
    "skin_default_lit": [OUT_DIR + "/M_NPC_SkinDefaultLit"],  # CityKit street crowd only
    "hair": [OUT_DIR + "/M_NPC_Hair"],
    "eye": [OUT_DIR + "/M_NPC_Eye"],
    "cloth": [OUT_DIR + "/M_NPC_Cloth"],
    "generic": [OUT_DIR + "/M_NPC_Generic"],  # not M_SurfacePBR: it is world-aligned triplanar and swims on moving NPCs
}

# Same roots / path tokens as PilotAvatar.cpp AvatarCatalog and Docs/avatar_characters.md.
PREMIUM_SCAN_ROOTS = ("/Game/Kevin", "/Game/CharacterCreator", "/Game/Reallusion", "/Game/MetaHumans")
PREMIUM_PATH_NEEDLES = ("kevin", "charactercreator", "reallusion", "cc_base", "/cc4")
_SKIN_PARENT_CACHE = {}

LOG = []


def log(msg):
    LOG.append(msg)
    if IN_UE:
        unreal.log("[VisonNPC] " + msg)
    else:
        print(msg)


def guard(path):
    if not path.startswith(ALLOWED_PREFIX):
        raise RuntimeError("refusing to write outside %s: %s" % (OUT_DIR, path))
    return path


def lin(c):
    c = list(c) + [1.0] * (4 - len(c))
    return c[:4]


# --------------------------------------------------------------------------------------- plan (pure python)
def build_plan(d, only):
    plan = []  # dicts: kind, path, parent_key, scalars, vectors, profile
    skin = d["skin"]
    sss = skin["subsurface_profile_base"]
    if "skin" in only:
        for t in skin["tones"]:
            plan.append({"kind": "profile", "path": "%s/SP_Skin_T%d" % (OUT_DIR, t["index"]), "usage": "hero",
                         "albedo": t["albedo_linear"], "mfp_color": sss["mean_free_path_color_linear"],
                         "mfp_mm": round(sss["mean_free_path_distance_mm"] * t["sss_mfp_scale"], 3),
                         "transmission": t["sss_transmission_tint_linear"], "sss": sss})
        ch = d["age_groups"]["child_6_12"]
        plan.append({"kind": "profile", "path": OUT_DIR + "/SP_Skin_Child", "usage": "hero", "albedo": skin["tones"][2]["albedo_linear"],
                     "mfp_color": sss["mean_free_path_color_linear"],
                     "mfp_mm": round(sss["mean_free_path_distance_mm"] * ch["sss_mfp_scale"], 3),
                     "transmission": skin["tones"][2]["sss_transmission_tint_linear"], "sss": sss})
        syn = d["synthezoid_and_hologram"]["synthezoid_skin"]
        plan.append({"kind": "profile", "path": OUT_DIR + "/SP_Synthezoid", "usage": "hero", "albedo": syn["vison_red_linear"],
                     "mfp_color": [1.0, 0.6, 0.5], "mfp_mm": syn["mean_free_path_distance_mm"],
                     "transmission": [0.4, 0.1, 0.1], "sss": sss})
        for age_name, age_key in AGES:
            a = d["age_groups"][age_key]
            for t in skin["tones"]:
                plan.append({"kind": "mi", "path": "%s/MI_NPC_Skin_%s_T%d" % (OUT_DIR, age_name, t["index"]),
                             "parent_key": "skin", "usage": "hero",
                             "profile": "%s/SP_Skin_T%d" % (OUT_DIR, t["index"]),
                             "vectors": {"Tint": t["albedo_linear"]},
                             "scalars": {"TintMultiplier": 1.0, "Roughness": a["skin_roughness"], "Specular": skin["specular"],
                                         "PoreNormalStrength": a["pore_normal_strength"],
                                         "WrinkleNormalStrength": a["wrinkle_normal_strength"], "CavityAO": a["cavity_ao"],
                                         "FlushStrength": round(t["redness_flush_strength"] * a["cheek_flush"], 3),
                                         "DetailFadeStart": skin["detail_normal_fade_m"][0] * 100.0,
                                         "DetailFadeEnd": skin["detail_normal_fade_m"][1] * 100.0}})
        # CityKit street crowd: default-lit only — no Subsurface Profile, no SSS parent.
        adult = d["age_groups"]["adult_18_64"]
        for t in skin["tones"]:
            plan.append({"kind": "mi", "path": "%s/MI_NPC_CrowdSkin_T%d" % (OUT_DIR, t["index"]),
                         "parent_key": "skin_default_lit", "usage": "crowd",
                         "vectors": {"Tint": t["albedo_linear"]},
                         "scalars": {"TintMultiplier": 1.0, "Roughness": adult["skin_roughness"], "Specular": skin["specular"]}})
    if "hair" in only:
        hs = d["hair"]["shading"]
        for h in d["hair"]["colors"]:
            plan.append({"kind": "mi", "path": "%s/MI_NPC_Hair_%s" % (OUT_DIR, h["name"]), "parent_key": "hair", "usage": "hero",
                         "vectors": {"Tint": h["card_base_color_linear"], "DyeColor": h["groom_dye_color_linear"]},
                         "scalars": {"Melanin": h["groom_melanin"], "Redness": h["groom_redness"], "Specular": hs["specular"],
                                     "Roughness": hs["roughness_by_type"]["wavy"], "Backlit": hs["backlit"],
                                     "OpacityMaskClip": hs["cards_opacity_mask_clip"], "RootDarkening": hs["root_darkening"]}})
    if "eyes" in only:
        e = d["eyes"]
        for name, col in e["iris_colors_linear"].items():
            plan.append({"kind": "mi", "path": "%s/MI_NPC_Eye_%s" % (OUT_DIR, name), "parent_key": "eye", "usage": "hero",
                         "vectors": {"IrisColor": col, "ScleraColor": e["sclera_default_linear"]},
                         "scalars": {"CorneaRoughness": e["cornea"]["roughness"], "CorneaSpecular": e["cornea"]["specular"],
                                     "ScleraRoughness": e["sclera_roughness"], "IrisRadius": e["iris_radius_fraction"],
                                     "IrisDepth": e["cornea"]["iris_depth_cm"], "LimbalDarkening": e["limbal_ring_darkening"],
                                     "PupilDiameterMM": 3.0}})
    if "cloth" in only:
        for k, c in d["cloth"].items():
            if k.startswith("_"):
                continue
            base = c.get("base_color_default_linear", [0.18, 0.18, 0.2])
            plan.append({"kind": "mi", "path": "%s/MI_NPC_Cloth_%s" % (OUT_DIR, k), "parent_key": "cloth", "usage": "both",
                         "vectors": {"Tint": base},
                         "scalars": {"Roughness": c["roughness"], "Specular": c["specular"], "FuzzAmount": c["fuzz_amount"],
                                     "WeaveNormalStrength": c.get("weave_normal_strength", 0.2),
                                     "WeaveTilingCM": c.get("weave_tiling_cm", 0.4), "Anisotropy": c.get("anisotropy", 0.0),
                                     "Metallic": 0.0}})
        for k, c in d["leather_rubber"].items():
            plan.append({"kind": "mi", "path": "%s/MI_NPC_%s" % (OUT_DIR, k), "parent_key": "cloth", "usage": "both",
                         "vectors": {"Tint": c.get("base_color_default_linear", [0.05, 0.04, 0.035])},
                         "scalars": {"Roughness": c["roughness"], "Specular": c["specular"],
                                     "FuzzAmount": c.get("fuzz_amount", 0.0), "Metallic": 0.0}})
    if "props" in only:
        for k, p in d["props"].items():
            col = p.get("base_color_linear") or (p.get("palette_linear") or [[0.5, 0.5, 0.5]])[0]
            sc = {"Roughness": p.get("roughness", 0.5), "Metallic": p.get("metallic", 0.0)}
            if "specular" in p:
                sc["Specular"] = p["specular"]
            plan.append({"kind": "mi", "path": "%s/MI_NPC_Prop_%s" % (OUT_DIR, k), "parent_key": "generic", "usage": "both",
                         "vectors": {"Color": col}, "scalars": sc})
    for p in plan:
        guard(p["path"])
    return plan


# --------------------------------------------------------------------------------------- unreal helpers
def asset_exists(path):
    return IN_UE and unreal.EditorAssetLibrary.does_asset_exist(path)


def split(path):
    return path.rsplit("/", 1)


def _asset_name(path):
    tail = path.rsplit("/", 1)[-1]
    return (tail.split(".", 1)[0] if "." in tail else tail).lower()


def _looks_like_skin_material(path):
    n = _asset_name(path)
    pl = path.lower()
    if any(x in n for x in ("hair", "eye", "teeth", "tongue", "brow", "lash", "beard", "mustache", "cloth", "shirt", "pant", "shoe")):
        return False
    return "skin" in n or "body" in n or "/skin" in pl


def _path_is_kevin_or_cc(path):
    pl = path.lower()
    if any(n in pl for n in PREMIUM_PATH_NEEDLES):
        return True
    return any(pl == r.lower() or pl.startswith(r.lower() + "/") for r in ("/Game/Kevin", "/Game/CharacterCreator", "/Game/Reallusion"))


def _path_is_metahuman(path):
    return path.lower().startswith("/game/metahumans/")


def _list_material_paths():
    """Material / MaterialInstance paths from the asset registry, or list_assets on the scan roots."""
    if not IN_UE:
        return []
    paths = []
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        try:
            registry.scan_paths_synchronous(list(PREMIUM_SCAN_ROOTS), False)
        except Exception as ex:
            log("skin parent scan: ScanPathsSynchronous failed (%s); continuing with registered assets" % ex)
        for cls_name in ("Material", "MaterialInstanceConstant", "MaterialInstance"):
            cls = getattr(unreal, cls_name, None)
            if cls is None:
                continue
            try:
                assets = registry.get_assets_by_class(cls.static_class().get_class_path_name(), True)
            except Exception:
                continue
            for a in assets or []:
                if hasattr(a, "get_object_path_string"):
                    p = a.get_object_path_string()
                else:
                    p = str(getattr(a, "object_path", "") or "")
                if p:
                    paths.append(p)
    except Exception as ex:
        log("skin parent scan: AssetRegistry unavailable (%s); falling back to list_assets" % ex)
        for root in PREMIUM_SCAN_ROOTS:
            try:
                for p in unreal.EditorAssetLibrary.list_assets(root, True, False) or []:
                    paths.append(str(p))
            except Exception:
                pass
    return paths


def scan_premium_skin_parents():
    """Runtime scan for Kevin / Character Creator / MetaHuman skin materials.

    Roots: /Game/Kevin, /Game/CharacterCreator, /Game/Reallusion, /Game/MetaHumans/<Name>/.
    Also any material whose path contains Kevin, CharacterCreator, Reallusion or CC_Base.
    Never hardcode /Game/Characters/Kevin/...
    """
    if not IN_UE:
        return []
    if "list" in _SKIN_PARENT_CACHE:
        return _SKIN_PARENT_CACHE["list"]
    kevin_cc, metahuman = [], []
    for p in _list_material_paths():
        if not _looks_like_skin_material(p):
            continue
        if _path_is_kevin_or_cc(p):
            kevin_cc.append(p)
        elif _path_is_metahuman(p):
            metahuman.append(p)
    out = []
    for p in kevin_cc + metahuman:
        if p not in out:
            out.append(p)
    if out:
        preview = ", ".join(out[:8]) + ("..." if len(out) > 8 else "")
        log("skin parent scan found %d candidate(s): %s" % (len(out), preview))
    else:
        log("skin parent scan: no Kevin/CharacterCreator/Reallusion/MetaHuman skin material yet; will use M_NPC_Skin if built")
    _SKIN_PARENT_CACHE["list"] = out
    return out


def resolve_parent(key):
    if key == "skin":
        for p in scan_premium_skin_parents():
            if asset_exists(p):
                return p
    for p in PARENTS.get(key, []):
        if asset_exists(p):
            return p
    return None


def lc(c):
    c = lin(c)
    return unreal.LinearColor(c[0], c[1], c[2], c[3])


def ensure_asset(path, cls, factory, apply):
    guard(path)
    if asset_exists(path):
        return unreal.load_asset(path), False
    if not apply:
        log("would create %s (%s)" % (path, cls.__name__))
        return None, True
    folder, name = split(path)
    a = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)
    log("created %s" % path)
    return a, True


def build_master(key, apply):
    """Minimal parameter-driven master in /Game/Vison/NPCs. Deliberately simple; see npc_look.md section 3."""
    path = OUT_DIR + "/M_NPC_" + {
        "skin": "Skin", "skin_default_lit": "SkinDefaultLit", "hair": "Hair",
        "eye": "Eye", "cloth": "Cloth", "generic": "Generic"}[key]
    guard(path)
    if asset_exists(path):
        log("master exists: %s (left as is)" % path)
        return
    if not apply:
        log("would build master %s" % path)
        return
    mel = unreal.MaterialEditingLibrary
    mat, _ = ensure_asset(path, unreal.Material, unreal.MaterialFactoryNew(), True)
    if mat is None:
        return
    MSM = unreal.MaterialShadingModel
    model = {"skin": MSM.MSM_SUBSURFACE_PROFILE, "skin_default_lit": MSM.MSM_DEFAULT_LIT, "hair": MSM.MSM_HAIR,
             "eye": MSM.MSM_DEFAULT_LIT, "cloth": MSM.MSM_CLOTH, "generic": MSM.MSM_DEFAULT_LIT}[key]
    try:
        mat.set_editor_property("shading_model", model)
    except Exception as ex:  # Substrate projects may convert legacy shading models on compile
        log("shading model set failed on %s: %s" % (path, ex))
    if key == "hair":
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("dithered_lod_transition", True)
    MP = unreal.MaterialProperty
    y = [0]

    def vparam(name, default):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, y[0])
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", lc(default))
        y[0] += 160
        return n

    def sparam(name, default):
        n = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, y[0])
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", float(default))
        y[0] += 110
        return n

    tint = vparam("Tint" if key != "generic" else "Color", [0.5, 0.5, 0.5])
    if key in ("skin", "skin_default_lit", "hair", "cloth"):
        mult = sparam("TintMultiplier", 1.0)
        m = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 0)
        mel.connect_material_expressions(tint, "", m, "A")
        mel.connect_material_expressions(mult, "", m, "B")
        mel.connect_material_property(m, "", MP.MP_BASE_COLOR)
    elif key == "eye":
        iris = vparam("IrisColor", [0.14, 0.07, 0.03])
        mel.connect_material_property(iris, "", MP.MP_BASE_COLOR)
        sparam("IrisRadius", 0.14)
        sparam("PupilDiameterMM", 3.0)
    else:
        mel.connect_material_property(tint, "", MP.MP_BASE_COLOR)
    rough = sparam("Roughness" if key != "eye" else "CorneaRoughness", 0.5)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    spec_ = sparam("Specular" if key != "eye" else "CorneaSpecular", 0.5)
    mel.connect_material_property(spec_, "", MP.MP_SPECULAR)
    if key in ("cloth", "generic"):
        met = sparam("Metallic", 0.0)
        mel.connect_material_property(met, "", MP.MP_METALLIC)
    if key == "cloth":
        fuzz = sparam("FuzzAmount", 0.4)
        mel.connect_material_property(fuzz, "", MP.MP_CUSTOM_DATA1)  # Cloth: CustomData1 = 'Cloth' amount
        mel.connect_material_property(tint, "", MP.MP_SUBSURFACE_COLOR)  # Cloth: SubsurfaceColor = FuzzColor
    if key == "skin":
        sparam("PoreNormalStrength", 0.45)
        sparam("FlushStrength", 0.2)
    if key == "hair":
        sparam("Melanin", 0.6)
        sparam("Redness", 0.2)
        sc = sparam("Scatter", 0.2)
        mel.connect_material_property(sc, "", MP.MP_SCATTER)
        bl = sparam("Backlit", 0.35)
        mel.connect_material_property(bl, "", MP.MP_BACKLIT)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    log("built master %s" % path)


def apply_profile(p, apply):
    if not IN_UE:
        log("plan profile %s albedo=%s mfp=%.3f mm" % (p["path"], p["albedo"], p["mfp_mm"]))
        return
    factory = getattr(unreal, "SubsurfaceProfileFactory", None)
    sp, created = ensure_asset(p["path"], unreal.SubsurfaceProfile, factory() if factory else None, apply)
    if sp is None:
        return
    try:
        s = sp.get_editor_property("settings")
        sss = p["sss"]
        vals = {"surface_albedo": lc(p["albedo"]), "mean_free_path_color": lc(p["mfp_color"]),
                "mean_free_path_distance": float(p["mfp_mm"]), "world_unit_scale": float(sss["world_unit_scale"]),
                "enable_burley": True, "enable_mean_free_path": True, "tint": lc(sss["tint_linear"]),
                "transmission_tint_color": lc(p["transmission"]), "boundary_color_bleed": lc(sss["boundary_color_bleed_linear"]),
                "roughness0": sss["dual_specular"]["roughness0"], "roughness1": sss["dual_specular"]["roughness1"],
                "lobe_mix": sss["dual_specular"]["lobe_mix"], "ior": float(sss["ior"])}
        for k, v in vals.items():
            try:
                old = s.get_editor_property(k)
                if apply:
                    s.set_editor_property(k, v)
                log("%s.%s: %s -> %s%s" % (p["path"], k, old, v, "" if apply else " (dry)"))
            except Exception:
                log("%s.%s: skip (not in this engine version)" % (p["path"], k))
        if apply:
            sp.set_editor_property("settings", s)
            unreal.EditorAssetLibrary.save_asset(guard(p["path"]), only_if_is_dirty=False)
    except Exception as ex:
        log("profile %s: settings not editable from Python (%s); set by hand from npc_materials.json" % (p["path"], ex))


def param_names(parent_path):
    mel = unreal.MaterialEditingLibrary
    parent = unreal.load_asset(parent_path)
    s = set(str(n) for n in mel.get_scalar_parameter_names(parent))
    v = set(str(n) for n in mel.get_vector_parameter_names(parent))
    return parent, s, v


def apply_mi(p, apply, cache):
    if not IN_UE:
        log("plan MI %s <- %s  scalars=%s vectors=%s" % (p["path"], p["parent_key"], p["scalars"], p["vectors"]))
        return
    parent_path = resolve_parent(p["parent_key"])
    if not parent_path:
        log("skip %s: no parent for '%s' (run with --build-masters, or import Kevin/MetaHuman)" % (p["path"], p["parent_key"]))
        return
    if parent_path not in cache:
        cache[parent_path] = param_names(parent_path)
    parent, snames, vnames = cache[parent_path]
    mi, created = ensure_asset(p["path"], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew(), apply)
    mel = unreal.MaterialEditingLibrary
    if mi is not None and apply and mi.get_editor_property("parent") != parent:
        mel.set_material_instance_parent(mi, parent)
    for k, v in p["scalars"].items():
        if k not in snames:
            log("%s.%s: skip: not on parent" % (p["path"], k))
            continue
        old = mel.get_material_instance_scalar_parameter_value(mi, k) if mi else None
        if apply and mi:
            mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
        log("%s.%s: %s -> %s%s" % (p["path"], k, old, v, "" if apply else " (dry)"))
    for k, v in p["vectors"].items():
        if k not in vnames:
            log("%s.%s: skip: not on parent" % (p["path"], k))
            continue
        old = mel.get_material_instance_vector_parameter_value(mi, k) if mi else None
        if apply and mi:
            mel.set_material_instance_vector_parameter_value(mi, k, lc(v))
        log("%s.%s: %s -> %s%s" % (p["path"], k, old, v, "" if apply else " (dry)"))
    if p.get("profile") and mi is not None and apply:
        sp = unreal.load_asset(p["profile"]) if asset_exists(p["profile"]) else None
        if sp:
            try:
                mi.set_editor_property("subsurface_profile", sp)
                ov = mi.get_editor_property("base_property_overrides")
                log("%s: subsurface profile -> %s (base overrides: %s)" % (p["path"], p["profile"], type(ov).__name__))
            except Exception as ex:
                log("%s: could not override subsurface profile (%s); assign by hand" % (p["path"], ex))
    if apply and mi:
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_asset(guard(p["path"]), only_if_is_dirty=False)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Vison NPC materials (writes only /Game/Vison/NPCs)")
    ap.add_argument("--json", default=DEFAULT_JSON)
    ap.add_argument("--only", default="skin,hair,eyes,cloth,props", help="comma list: skin,hair,eyes,cloth,props")
    ap.add_argument("--usage", default="all", choices=("all", "hero", "crowd"),
                    help="hero = close-up SSS/eye/hair (player, Harley, Morgan, cinematics); "
                         "crowd = CityKit default-lit MIs only (no SP, eye, hair, SSS); all = both")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--build-masters", action="store_true", help="build minimal M_NPC_* masters in /Game/Vison/NPCs if missing")
    a = ap.parse_args(argv)
    apply = a.apply and not a.dry_run and IN_UE
    only = set(x.strip() for x in a.only.split(",") if x.strip())
    with open(a.json) as f:
        d = json.load(f)
    if d.get("output_folder") != OUT_DIR:
        raise RuntimeError("json output_folder mismatch: %s" % d.get("output_folder"))
    plan = build_plan(d, only)
    if a.usage != "all":
        plan = [p for p in plan if p.get("usage", "both") in (a.usage, "both")]
    log("Vison NPC materials: %d items, usage=%s, mode=%s, in_editor=%s" % (
        len(plan), a.usage, "APPLY" if apply else "DRY RUN", IN_UE))
    if IN_UE and a.build_masters:
        need = set(p["parent_key"] for p in plan if p["kind"] == "mi")
        for key in sorted(need):
            if not resolve_parent(key) or resolve_parent(key).startswith(ALLOWED_PREFIX):
                build_master(key, apply)
    cache = {}
    for p in plan:
        if p["kind"] == "profile":
            apply_profile(p, apply)
        else:
            apply_mi(p, apply, cache)
    log("done: %d log lines%s" % (len(LOG), "" if apply else " (dry run: nothing written)"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] if not IN_UE else [x for x in sys.argv[1:]]))
