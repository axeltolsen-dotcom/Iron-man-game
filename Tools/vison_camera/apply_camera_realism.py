#!/usr/bin/env python3
"""
apply_camera_realism.py  --  apply the Vison camera layer inside the UE 5.8 editor.

Layer order: apply_lighting_preset.py -> apply_realism.py -> THIS. It only writes
camera / lens / exposure / grade keys; never lights, sky, fog, VFX (Ultron) or
suit materials.

Run in the editor (Output Log, Python):
    py "<repo>/contrib/vison/camera/apply_camera_realism.py" --env city --tier High --dry-run
    py ".../apply_camera_realism.py" --env ch5_titan --apply-ppv --apply-cine
    py ".../apply_camera_realism.py" --create-shakes --import-textures
Outside the editor (plain python3) it always runs as a dry run and prints the plan.

Flags
  --env ID           environment preset id or chapter id (lighting_presets chapter_mapping)
  --tier T           Epic | High | Medium (default: detect from sg.PostProcessQuality, else High)
  --grade G          look from color_grade.json (default: environment default)
  --mode M           third_person | aim | flight | helmet_hud_pov (gameplay PPV deltas, default third_person)
  --apply-ppv        write gameplay camera keys to the unbound PostProcessVolume(s)
  --create-missing   spawn an unbound PPV if none exists (needs --apply-ppv)
  --apply-cine       write cutscene keys to CineCameraActors (all, or --cine-camera NAME substring)
  --shot S           wide|aerial|medium|close|rack|ecu: also set focal length/aperture for that shot
  --create-shakes    create/update CameraShakeBase Blueprints under --content-path/Shakes
  --import-textures  import textures/*.png (LUTs, flare shapes) to --content-path
  --content-path P   default /Game/Contrib/Vison/Camera
  --dry-run          log every change, write nothing
  --list             list environments, grades, shakes and exit
Missing assets, classes or properties are skipped with a warning; nothing raises.
"""
import argparse
import json
import math
import os
import re
import sys

try:
    import unreal  # noqa
    HAS_UE = True
except Exception:
    unreal = None
    HAS_UE = False

HERE = os.path.dirname(os.path.abspath(__file__))
VISON = os.path.dirname(HERE)
CONTENT_DEFAULT = "/Game/Contrib/Vison/Camera"
TIERS = ("Epic", "High", "Medium")
SHAKE_AXES = ("X", "Y", "Z", "Pitch", "Yaw", "Roll", "FOV")
STATS = {"set": 0, "skipped": 0, "warn": 0}


# ----------------------------------------------------------------------------
# logging / io
# ----------------------------------------------------------------------------
def log(msg):
    if HAS_UE:
        unreal.log("[vison.camera] " + msg)
    else:
        print("[vison.camera] " + msg)


def warn(msg):
    STATS["warn"] += 1
    if HAS_UE:
        unreal.log_warning("[vison.camera] " + msg)
    else:
        print("[vison.camera] WARN " + msg)


def load_json(name, base=HERE):
    p = os.path.join(base, name)
    try:
        with open(p, "r", encoding="utf-8") as fh:
            return json.load(fh)
    except Exception as e:
        warn("cannot read %s: %s" % (p, e))
        return None


def snake(name):
    s = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
    return s.lower()


# ----------------------------------------------------------------------------
# value conversion (handles Vector2f, Vector4, LinearColor, enums, assets)
# ----------------------------------------------------------------------------
ENUM_HINTS = {
    "BloomMethod": "BloomMethod", "AutoExposureMethod": "AutoExposureMethod",
    "TonemappingMethod": "TonemappingMethod", "TemperatureType": "TemperatureMethod",
    "VignetteType": "VignetteType", "LocalExposureMethod": "LocalExposureMethod",
    "FocusMethod": "CameraFocusMethod",
}
ENUM_ALIASES = {"StandardACES": ["STANDARD_ACES"], "Filmic": ["FILMIC"],
                "COSINE_FOURTH_LAW": ["COSINE_FOURTH_LAW", "OVAL"], "FUSION": ["FUSION", "BILATERAL"]}
VEC2_KEYS = ("FilmGrainScale", "VignetteCenter", "VignetteSize", "BloomConvolutionCenterUV",
             "BloomConvolutionSize2D")


def enum_value(key, text):
    if not HAS_UE:
        return text
    cls = getattr(unreal, ENUM_HINTS.get(key, ""), None)
    if cls is None:
        return None
    tries = ENUM_ALIASES.get(text, []) + [text, text.upper(), snake(text).upper(),
                                            re.sub(r"^[A-Z]+_", "", text).upper()]
    for t in tries:
        if hasattr(cls, t):
            if t != text and text in ENUM_ALIASES and t != ENUM_ALIASES[text][0]:
                warn("%s=%s not in this engine, fell back to %s" % (key, text, t))
            return getattr(cls, t)
    return None


def to_ue(key, val):
    """Convert JSON value -> unreal value. Returns (ok, value)."""
    if not HAS_UE:
        return True, val
    try:
        if isinstance(val, bool) or isinstance(val, (int, float)):
            return True, val
        if isinstance(val, str):
            if val.startswith("/Game/") or val.startswith("/Engine/"):
                if not unreal.EditorAssetLibrary.does_asset_exist(val):
                    warn("asset missing, skipped: %s = %s" % (key, val))
                    return False, None
                return True, unreal.EditorAssetLibrary.load_asset(val)
            if key in ENUM_HINTS:
                e = enum_value(key, val)
                return (e is not None), e
            return False, None
        if isinstance(val, dict):
            ks = set(val.keys())
            if ks == {"X", "Y"} or key in VEC2_KEYS:
                cls = getattr(unreal, "Vector2f", None) or unreal.Vector2D
                return True, cls(float(val["X"]), float(val["Y"]))
            if ks == {"X", "Y", "Z", "W"}:
                return True, unreal.Vector4(val["X"], val["Y"], val["Z"], val["W"])
            if ks >= {"R", "G", "B"}:
                return True, unreal.LinearColor(val["R"], val["G"], val["B"], val.get("A", 1.0))
    except Exception as e:
        warn("convert %s failed: %s" % (key, e))
    return False, None


def set_pp(settings, key, val, dry):
    """Set one FPostProcessSettings key + its override flag. Works on a copy/struct."""
    if key.startswith("_") or val is None:
        return settings
    prop = snake(key)
    if dry or not HAS_UE:
        log("  pp %-42s = %s" % (prop, json.dumps(val) if not isinstance(val, str) else val))
        STATS["set"] += 1
        return settings
    ok, v = to_ue(key, val)
    if not ok:
        STATS["skipped"] += 1
        warn("skip %s (no conversion)" % key)
        return settings
    try:
        settings.set_editor_property(prop, v)
        try:
            settings.set_editor_property("override_" + prop, True)
        except Exception:
            pass  # some keys have no override flag
        STATS["set"] += 1
    except Exception as e:
        STATS["skipped"] += 1
        warn("property %s not settable in this engine: %s" % (prop, e))
    return settings


def apply_block(owner, prop_name, block, dry, label):
    """owner.<prop_name> is a PostProcessSettings struct: get, mutate, set back."""
    log("%s -> %d keys" % (label, len([k for k in block if not k.startswith("_")])))
    if dry or not HAS_UE:
        for k, v in block.items():
            set_pp(None, k, v, True)
        return
    try:
        s = owner.get_editor_property(prop_name)
        for k, v in block.items():
            s = set_pp(s, k, v, False)
        owner.set_editor_property(prop_name, s)
    except Exception as e:
        warn("%s: cannot write %s: %s" % (label, prop_name, e))


# ----------------------------------------------------------------------------
# data resolution
# ----------------------------------------------------------------------------
def resolve_env(env, cam):
    envs = cam["metadata"]["environments"]
    if env in envs:
        return env
    chap = cam["metadata"].get("chapter_mapping", {})
    for table in chap.values():
        if isinstance(table, dict) and env in table and table[env] in envs:
            return table[env]
    warn("unknown environment '%s' (known: %s)" % (env, ", ".join(envs)))
    return None


def detect_tier():
    if not HAS_UE:
        return "High"
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        q = unreal.SystemLibrary.get_console_variable_int_value("sg.PostProcessQuality")
        return {3: "Epic", 4: "Epic", 2: "High"}.get(q, "Medium" if q <= 1 else "High")
    except Exception:
        return "High"


def gameplay_block(cam, grade, env, tier, look, mode):
    exp = cam["exposure"]["per_environment"][env]
    le = cam["lens_effects"]["per_environment"][env]
    mb = cam["motion_blur"]["tiers"][tier]
    fg = cam["lens_effects"]["film_grain"]
    b = {}
    b.update(exp["histogram"])
    b["HistogramLogMin"] = cam["exposure"]["histogram_range"]["HistogramLogMin"]
    b["HistogramLogMax"] = cam["exposure"]["histogram_range"]["HistogramLogMax"]
    b.update(exp["local_exposure"])
    b["LocalExposureMethod"] = "BILATERAL"
    b.update({"VignetteType": "COSINE_FOURTH_LAW", "VignetteIntensity": le["VignetteIntensity"],
              "FilmGrainIntensity": le["FilmGrainIntensity"], "LensFlareIntensity": le["LensFlareIntensity"],
              "SceneFringeIntensity": 0.0, "DepthOfFieldFocalDistance": 0.0,
              "FilmGrainIntensityShadows": fg["FilmGrainIntensityShadows"],
              "FilmGrainIntensityMidtones": fg["FilmGrainIntensityMidtones"],
              "FilmGrainIntensityHighlights": fg["FilmGrainIntensityHighlights"]})
    for k in ("MotionBlurAmount", "MotionBlurMax", "MotionBlurPerObjectSize", "MotionBlurTargetFPS"):
        b[k] = mb[k]
    wb = grade["white_balance"][env]
    b.update({"TemperatureType": wb["TemperatureType"], "WhiteTemp": wb["WhiteTemp"], "WhiteTint": wb["WhiteTint"]})
    b["TonemappingMethod"] = grade["tonemapper"]["sdr_default"]["TonemappingMethod"]
    b.update(grade["composed"][env].get(look, {}))
    # camera-mode deltas
    st = cam["shot_types"]
    if mode == "helmet_hud_pov":
        po = st["helmet_hud_pov"]["post_overrides"]
        b["VignetteIntensity"] = po["VignetteIntensity"]
        b["FilmGrainIntensity"] = round(b["FilmGrainIntensity"] * po["FilmGrainIntensity_scale"], 3)
        b["LensFlareIntensity"] = min(b["LensFlareIntensity"], po["LensFlareIntensity_cap"])
        b["MotionBlurAmount"] = round(b["MotionBlurAmount"] * po["MotionBlurAmount_scale"], 3)
    elif mode == "aim":
        b["VignetteIntensity"] = round(min(b["VignetteIntensity"] + 0.05, 0.35), 3)
        b["MotionBlurAmount"] = round(b["MotionBlurAmount"] * st["aim"]["motion_blur_amount_scale"], 3)
    return b


def cine_block(cam, grade, env, tier, look, shot=None):
    pk = cam["cutscene_packages"][env]
    le = cam["lens_effects"]["per_environment"][env]
    exp = cam["exposure"]
    b = {"AutoExposureMethod": "AEM_Manual", "AutoExposureApplyPhysicalCameraExposure": True,
         "CameraShutterSpeed": pk["CameraShutterSpeed"], "CameraISO": pk["CameraISO_effective"],
         "AutoExposureBias": 0.0,
         "MotionBlurAmount": round(pk["shutter_angle_deg"] / 360.0, 3), "MotionBlurTargetFPS": 24,
         "VignetteType": "COSINE_FOURTH_LAW", "VignetteIntensity": le["cutscene_VignetteIntensity"],
         "LensFlareIntensity": le["cutscene_LensFlareIntensity"],
         "SceneFringeIntensity": le["cutscene_SceneFringeIntensity_spec"],
         "ChromaticAberrationStartOffset": 0.5,
         "FilmGrainIntensity": pk["film_grain_cinematic"],
         "DepthOfFieldBladeCount": pk["DepthOfFieldBladeCount"],
         "LocalExposureMethod": "FUSION" if tier == "Epic" else "BILATERAL"}
    for k in ("DepthOfFieldPetzvalBokeh", "DepthOfFieldPetzvalBokehFalloff", "DepthOfFieldBarrelRadius", "DepthOfFieldBarrelLength"):
        if k in pk:
            b[k] = pk[k]
    if env in cam["lens_effects"]["lens_flare"]["anamorphic_option"]["applies_to"]:
        ao = cam["lens_effects"]["lens_flare"]["anamorphic_option"]
        b.update({"LensFlareBokehShape": ao["LensFlareBokehShape"], "LensFlareTint": ao["LensFlareTint"],
                  "LensFlareBokehSize": ao["LensFlareBokehSize"]})
    if tier == "Epic":
        fft = dict(cam["lens_effects"]["bloom"]["cutscene_Epic"])
        for k in list(fft):
            if not k.startswith("Bloom"):
                fft.pop(k)
        b.update(fft)
    wb = grade["white_balance"][env]
    b.update({"TemperatureType": wb["TemperatureType"], "WhiteTemp": wb["WhiteTemp"], "WhiteTint": wb["WhiteTint"]})
    b.update(grade["composed"][env].get(look, {}))
    return b


# ----------------------------------------------------------------------------
# editor actions
# ----------------------------------------------------------------------------
def all_actors(cls_name):
    if not HAS_UE:
        return []
    cls = getattr(unreal, cls_name, None)
    if cls is None:
        return []
    try:
        sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        return [a for a in sub.get_all_level_actors() if isinstance(a, cls)]
    except Exception:
        return list(unreal.EditorLevelLibrary.get_all_level_actors() or [])


def do_ppv(block, dry, create_missing):
    if not HAS_UE:
        apply_block(None, "settings", block, True, "PostProcessVolume (unbound) [dry, no editor]")
        return
    vols = [v for v in all_actors("PostProcessVolume") if v.get_editor_property("unbound")]
    if not vols and create_missing and not dry:
        try:
            v = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
            v.set_editor_property("unbound", True)
            v.set_actor_label("PPV_Vison_Camera")
            vols = [v]
            log("spawned unbound PPV_Vison_Camera")
        except Exception as e:
            warn("could not spawn PPV: %s" % e)
    if not vols:
        warn("no unbound PostProcessVolume (use --create-missing)")
        return
    for v in vols:
        apply_block(v, "settings", block, dry, "PPV %s" % v.get_actor_label())


def do_cine(cam, block, env, shot, name_filter, dry):
    pk = cam["cutscene_packages"][env]
    actors = all_actors("CineCameraActor")
    if name_filter:
        actors = [a for a in actors if name_filter.lower() in a.get_actor_label().lower()]
    if not HAS_UE:
        apply_block(None, "post_process_settings", block, True, "CineCamera [dry, no editor]")
        if shot:
            log("  focal %s mm for shot %s" % (pk["story_shots"][shot]["focal_prime_mm"], shot))
        return
    if not actors:
        warn("no CineCameraActor matched; StoryDirector spawns its cameras at runtime - use ContribLighting::ApplyCamera there")
        return
    for a in actors:
        try:
            comp = a.get_cine_camera_component()
        except Exception:
            comp = a.get_component_by_class(unreal.CineCameraComponent)
        if comp is None:
            continue
        apply_block(comp, "post_process_settings", block, dry, "Cine %s" % a.get_actor_label())
        if dry:
            continue
        try:
            comp.set_editor_property("post_process_blend_weight", 1.0)
            fs = comp.get_editor_property("focus_settings")
            fs.set_editor_property("smooth_focus_changes", True)
            fs.set_editor_property("focus_smoothing_interp_speed", cam["shot_types"]["cutscene"]["focus"]["FocusSmoothingInterpSpeed"])
            comp.set_editor_property("focus_settings", fs)
            if shot:
                comp.set_editor_property("current_focal_length", float(pk["story_shots"][shot]["focal_prime_mm"]))
                comp.set_editor_property("current_aperture", float(pk["CurrentAperture"]))
        except Exception as e:
            warn("cine extras on %s: %s" % (a.get_actor_label(), e))


def ensure_dir(path, dry):
    if HAS_UE and not dry and not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def build_pattern(spec, outer, conv):
    kind = spec["type"]
    cls = getattr(unreal, "PerlinNoiseCameraShakePattern" if kind == "PerlinNoise" else "WaveOscillatorCameraShakePattern", None)
    if cls is None:
        raise RuntimeError("EngineCameras pattern class missing (enable EngineCameras plugin)")
    pat = unreal.new_object(cls, outer)
    pat.set_editor_property("duration", float(spec["Duration"]))
    pat.set_editor_property("blend_in_time", float(spec["BlendInTime"]))
    pat.set_editor_property("blend_out_time", float(spec["BlendOutTime"]))
    factor = conv[kind]["factor"]
    for axis, v in spec["axes"].items():
        if kind == "PerlinNoise":
            sh = unreal.PerlinNoiseShaker()
        else:
            sh = unreal.WaveOscillator()
            try:
                sh.set_editor_property("initial_offset_type", getattr(unreal.InitialWaveOscillatorOffsetType,
                                                                      spec.get("InitialOffsetType", "Zero").upper()))
            except Exception:
                pass
        sh.set_editor_property("amplitude", float(v["Amplitude"]))
        sh.set_editor_property("frequency", float(v["Frequency_hz"]) * factor)
        pat.set_editor_property(axis.lower(), sh)
    return pat


def create_shake(name, shake, conv, path, dry):
    pats = shake["patterns"]
    log("shake %s: %d pattern(s) -> %s/%s" % (name, len(pats), path, name))
    for p in pats:
        log("   %s dur %.2fs  %s" % (p["type"], p["Duration"],
                                     ", ".join("%s %.2f@%.1fHz" % (a, v["Amplitude"], v["Frequency_hz"]) for a, v in p["axes"].items())))
    if dry or not HAS_UE:
        return
    try:
        if getattr(unreal, "CameraShakeBase", None) is None:
            raise RuntimeError("CameraShakeBase not exposed")
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        full = "%s/%s" % (path, name)
        if unreal.EditorAssetLibrary.does_asset_exist(full):
            bp = unreal.EditorAssetLibrary.load_asset(full)
        else:
            fac = unreal.BlueprintFactory()
            fac.set_editor_property("parent_class", unreal.CameraShakeBase)
            bp = tools.create_asset(name, path, unreal.Blueprint, fac)
        gen = unreal.EditorAssetLibrary.load_blueprint_class(full)
        cdo = unreal.get_default_object(gen)
        cdo.set_editor_property("single_instance", bool(shake.get("single_instance", False)))
        if len(pats) == 1:
            root = build_pattern(pats[0], cdo, conv)
        else:
            comp_cls = getattr(unreal, "CompositeCameraShakePattern", None)
            if comp_cls is None:
                raise RuntimeError("CompositeCameraShakePattern missing; author extra patterns by hand")
            root = unreal.new_object(comp_cls, cdo)
            root.set_editor_property("child_patterns", [build_pattern(p, root, conv) for p in pats])
        cdo.set_editor_property("root_shake_pattern", root)
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception:
            pass
        unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
    except Exception as e:
        warn("shake %s not created (%s) - author it by hand from camera_shake.json" % (name, e))


def import_textures(path, dry):
    tex_dir = os.path.join(HERE, "textures")
    files = sorted(f for f in os.listdir(tex_dir) if f.endswith(".png")) if os.path.isdir(tex_dir) else []
    for f in files:
        log("texture %s -> %s" % (f, path))
    if dry or not HAS_UE or not files:
        return
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(tex_dir, f))
        t.set_editor_property("destination_path", path)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    except Exception as e:
        warn("texture import failed: %s" % e)
        return
    for f in files:
        full = "%s/%s" % (path, os.path.splitext(f)[0])
        try:
            tex = unreal.EditorAssetLibrary.load_asset(full)
            if f.startswith("T_LUT"):
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_COLOR_LOOKUP_TABLE)
                tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                tex.set_editor_property("srgb", True)
            else:
                tex.set_editor_property("srgb", False)
            unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
        except Exception as e:
            warn("texture settings %s: %s" % (full, e))


# ----------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(description="Apply Vison camera realism layer")
    ap.add_argument("--env")
    ap.add_argument("--tier", choices=TIERS)
    ap.add_argument("--grade")
    ap.add_argument("--mode", default="third_person", choices=["third_person", "aim", "flight", "helmet_hud_pov"])
    ap.add_argument("--apply-ppv", action="store_true")
    ap.add_argument("--create-missing", action="store_true")
    ap.add_argument("--apply-cine", action="store_true")
    ap.add_argument("--cine-camera")
    ap.add_argument("--shot", choices=["wide", "aerial", "medium", "close", "rack", "ecu"])
    ap.add_argument("--create-shakes", action="store_true")
    ap.add_argument("--import-textures", action="store_true")
    ap.add_argument("--content-path", default=CONTENT_DEFAULT)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args(argv)

    cam, grade, shakes = load_json("camera_realism.json"), load_json("color_grade.json"), load_json("camera_shake.json")
    if not (cam and grade and shakes):
        warn("run gen_camera_realism.py first")
        return 1
    dry = a.dry_run or not HAS_UE
    if not HAS_UE:
        log("unreal module not available: dry run only")
    if a.list:
        log("environments: " + ", ".join(cam["metadata"]["environments"]))
        log("grades: " + ", ".join(grade["grades"]))
        log("shakes: " + ", ".join(shakes["shakes"]))
        return 0

    tier = a.tier or detect_tier()
    tx = None
    if HAS_UE and not dry:
        tx = unreal.ScopedEditorTransaction("Vison camera realism")
    try:
        if a.env and (a.apply_ppv or a.apply_cine or a.dry_run):
            env = resolve_env(a.env, cam)
            if env:
                look = a.grade or grade["environment_default_grade"][env]
                if look not in grade["grades"] or grade["grades"][look]["mode"] != "look":
                    warn("grade '%s' is not a look; using environment default" % look)
                    look = grade["environment_default_grade"][env]
                log("env=%s tier=%s look=%s mode=%s dry=%s" % (env, tier, look, a.mode, dry))
                if a.apply_ppv or (a.dry_run and not a.apply_cine):
                    do_ppv(gameplay_block(cam, grade, env, tier, look, a.mode), dry, a.create_missing)
                if a.apply_cine:
                    do_cine(cam, cine_block(cam, grade, env, tier, look, a.shot), env, a.shot, a.cine_camera, dry)
        if a.create_shakes:
            path = a.content_path.rstrip("/") + "/Shakes"
            ensure_dir(path, dry)
            conv = shakes["metadata"]["ue_frequency_conversion"]
            items = dict(shakes["shakes"])
            items["calibration_1hz"] = shakes["calibration"]["CS_Calibration_1Hz"]
            for key, s in items.items():
                name = "CS_Calibration_1Hz" if key == "calibration_1hz" else "CS_" + "".join(w.capitalize() for w in key.split("_"))
                create_shake(name, s, conv, path, dry)
        if a.import_textures:
            ensure_dir(a.content_path, dry)
            import_textures(a.content_path, dry)
    finally:
        if tx is not None:
            del tx
    log("done: %(set)d set, %(skipped)d skipped, %(warn)d warnings" % STATS)
    return 0


if __name__ == "__main__":
    # In the editor `py file.py args` passes args via sys.argv as usual.
    rc = main()
    if not HAS_UE:
        sys.exit(rc)
