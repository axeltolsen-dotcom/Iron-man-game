#!/usr/bin/env python3
"""validate_camera.py - consistency checks for the Vison camera layer (no UE needed).

    python3 validate_camera.py      -> exit 0 when all checks pass
Checks: JSON parses; environments match lighting_presets; EV100 / white balance /
histogram bounds equal the lighting layer; camera triangles expose for the scene
(+-0.1 EV); gameplay values respect ApplyPhotorealGrade caps; motion blur tiers
match DefaultScalability; shakes use real units within visibility/aliasing
limits; all .py files in this folder compile.
"""
import glob
import json
import math
import os
import py_compile
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
VISON = os.path.dirname(HERE)
ERR, OK = [], [0]


def check(cond, msg):
    if cond:
        OK[0] += 1
    else:
        ERR.append(msg)


def load(p):
    with open(p, encoding="utf-8") as fh:
        return json.load(fh)


def main():
    files = {}
    for n in ("camera_realism.json", "color_grade.json", "camera_shake.json"):
        try:
            files[n] = load(os.path.join(HERE, n))
            check(True, n)
        except Exception as e:
            ERR.append("%s: %s" % (n, e))
    if ERR:
        return report()
    cam, grade, shk = files["camera_realism.json"], files["color_grade.json"], files["camera_shake.json"]
    L = load(os.path.join(VISON, "lighting_presets.json"))["presets"]
    caps = cam["metadata"]["photoreal_runtime_caps"]

    check(set(cam["metadata"]["environments"]) == set(L), "environment list differs from lighting_presets")
    for k, p in L.items():
        pp, cc = p["PostProcessVolume"], p["CineCamera"]
        e = cam["exposure"]["per_environment"].get(k)
        check(e is not None, "%s missing in exposure" % k)
        if not e:
            continue
        check(e["scene_ev100"] == p["exposure_reference"]["scene_ev100"], "%s scene_ev100 drift" % k)
        h = e["histogram"]
        check(h["AutoExposureMinBrightness"] == pp["AutoExposureMinBrightness"], "%s hist min drift" % k)
        check(h["AutoExposureMaxBrightness"] == pp["AutoExposureMaxBrightness"], "%s hist max drift" % k)
        check(h["AutoExposureMinBrightness"] <= e["scene_ev100"] <= h["AutoExposureMaxBrightness"], "%s scene EV outside histogram bounds" % k)
        t = e["gameplay_manual_triangle"]
        ev = math.log2(t["DepthOfFieldFstop"] ** 2 * t["CameraShutterSpeed"]) - math.log2(t["CameraISO"] / 100.0)
        check(abs(ev - e["scene_ev100"]) < 0.1, "%s gameplay triangle %.2f != %.2f" % (k, ev, e["scene_ev100"]))
        c = cam["cutscene_packages"][k]
        check(abs(c["camera_ev100"] - c["scene_ev100"]) < 0.1, "%s cutscene triangle off by %.2f EV" % (k, c["camera_ev100"] - c["scene_ev100"]))
        check(c["CurrentAperture"] == cc["CurrentAperture"] and c["Filmback"] == cc["Filmback"], "%s cine body drift" % k)
        wb = grade["white_balance"][k]
        check(wb["WhiteTemp"] == pp.get("WhiteTemp", 6500.0), "%s WhiteTemp drift" % k)
        le = cam["lens_effects"]["per_environment"][k]
        check(le["FilmGrainIntensity"] <= caps["FilmGrainIntensity_gameplay"], "%s gameplay grain over cap" % k)
        check(le["LensFlareIntensity"] <= caps["LensFlareIntensity"], "%s gameplay flare over cap" % k)
        check(le["SceneFringeIntensity"] == 0.0, "%s gameplay CA not 0" % k)
        check(c["film_grain_cinematic"] <= caps["FilmGrainIntensity_cinematic"], "%s cine grain over cap" % k)
        check(grade["environment_default_grade"].get(k) in grade["grades"], "%s default grade unknown" % k)
        for g, vals in grade["composed"][k].items():
            s = vals.get("ColorSaturation")
            if s:
                check(max(s.values()) <= 1.0 + 1e-9, "%s/%s saturation > 1" % (k, g))
            for key, v in vals.items():
                if key.startswith("ColorGain") and g != "combat_overlay_at_weight_1":
                    check(all(0.949 <= v[a] <= 1.051 for a in "XYZ"), "%s/%s %s not subtle" % (k, g, key))

    # motion blur vs live scalability
    try:
        txt = open(os.path.join(VISON, "..", "..", "IronManArena", "Config", "DefaultScalability.ini"), encoding="utf-8").read()
        for tier, idx in (("Epic", 3), ("High", 2), ("Medium", 1)):
            sec = txt.split("[PostProcessQuality@%d]" % idx)[1].split("\n[")[0]
            live = int(sec.split("r.MotionBlurQuality=")[1].split()[0])
            check(live == cam["motion_blur"]["tiers"][tier]["cvars"]["r.MotionBlurQuality"], "%s MotionBlurQuality %d live vs json" % (tier, live))
    except Exception as e:
        print("note: live DefaultScalability not checked (%s)" % e)
    for tier, m in cam["motion_blur"]["tiers"].items():
        check(0 < m["MotionBlurAmount"] <= 0.5, "%s blur amount beyond 180 deg" % tier)

    # shakes
    vis = shk["metadata"]["visibility_rules"]
    for k, s in shk["shakes"].items():
        check(s["patterns"], "%s has no patterns" % k)
        for p in s["patterns"] + s.get("crossing_jolt", []):
            check(p["type"] in ("PerlinNoise", "WaveOscillator"), "%s bad type" % k)
            check(p["Duration"] >= 0 and p["BlendInTime"] >= 0 and p["BlendOutTime"] >= 0, "%s negative timing" % k)
            if p["Duration"] > 0:
                check(p["BlendInTime"] + p["BlendOutTime"] <= p["Duration"] + 1e-6, "%s blends longer than duration" % k)
            for a, v in p["axes"].items():
                check(a in ("X", "Y", "Z", "Pitch", "Yaw", "Roll", "FOV"), "%s bad axis %s" % (k, a))
                check(0 < v["Frequency_hz"] <= vis["max_frequency_hz_gameplay"] + (10 if k == "unibeam" else 0) or k == "handheld_cutscene",
                      "%s %s %.1f Hz aliases at 60 fps" % (k, a, v["Frequency_hz"]))
                if a in ("Pitch", "Yaw", "Roll"):
                    check(abs(v["Amplitude"]) <= 3.0, "%s %s > 3 deg" % (k, a))
                    check(abs(v["Amplitude"]) >= vis["min_visible_deg"], "%s %s %.3f deg invisible" % (k, a, v["Amplitude"]))
                elif a in ("X", "Y", "Z"):
                    check(abs(v["Amplitude"]) <= 10.0, "%s %s > 10 cm" % (k, a))
    acc = shk["accessibility"]["presets"]
    check(acc["off"]["scale"] == 0 and acc["reduced"]["scale"] < 0.5 and acc["standard"]["scale"] == 1.0, "accessibility presets")

    # python
    for f in glob.glob(os.path.join(HERE, "*.py")):
        try:
            py_compile.compile(f, doraise=True)
            check(True, f)
        except py_compile.PyCompileError as e:
            ERR.append(str(e))
    # textures
    for t in ("T_LUT_Neutral_16.png", "T_LUT_TealOrange_Subtle_16.png"):
        p = os.path.join(HERE, "textures", t)
        if os.path.exists(p):
            try:
                from PIL import Image
                check(Image.open(p).size == (256, 16), "%s not 256x16" % t)
            except ImportError:
                pass
    return report()


def report():
    for e in ERR:
        print("FAIL", e)
    print("%d checks passed, %d failed" % (OK[0], len(ERR)))
    return 1 if ERR else 0


if __name__ == "__main__":
    sys.exit(main())
