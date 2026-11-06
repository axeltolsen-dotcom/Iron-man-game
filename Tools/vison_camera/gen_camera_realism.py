#!/usr/bin/env python3
"""
gen_camera_realism.py  --  Vison camera / post-process realism layer generator.

Reads the existing Vison lighting layer (../lighting_presets.json) and
realism layer (../postprocess_realism.json) and writes, into this folder only:

    camera_realism.json   bodies, lenses, shot types, exposure, lens effects,
                          DOF, motion blur, per-tier summary
    color_grade.json      tonemapper, white balance, grades, LUT spec
    camera_shake.json     CameraShakeBase Perlin / WaveOscillator sets
    textures/*.png        optional neutral + teal-orange 16^3 LUT strips,
                          anamorphic streak and 9-blade flare bokeh shapes

Everything that also exists in the lighting layer (scene EV100, WhiteTemp,
histogram bounds, cine bodies, f-stops, ISO, shutter) is COPIED from it, never
re-typed, so the two layers cannot drift. Re-run after editing either source:

    python3 gen_camera_realism.py            # writes json + textures
    python3 gen_camera_realism.py --no-textures
    python3 validate_camera.py               # consistency checks

Pure Python 3.8+. Pillow/numpy are only needed for the textures (skipped with
a warning when missing).
"""
import argparse
import json
import math
import os
import sys
from copy import deepcopy

HERE = os.path.dirname(os.path.abspath(__file__))
VISON = os.path.dirname(HERE)
LIGHTING = os.path.join(VISON, "lighting_presets.json")
REALISM = os.path.join(VISON, "postprocess_realism.json")
VERSION = "1.0.0"
DATE = "2026-10-30"

# ----------------------------------------------------------------------------
# physical helpers
# ----------------------------------------------------------------------------
S35_WIDTH = 24.89      # mm, UE "Super 35mm" filmback width (StoryDirector framing reference)
FF_WIDTH = 36.0        # mm, full-frame stills width (gameplay FOV reference)
SPEED_OF_SOUND = 343.0 # m/s at 20 C
PX_PER_DEG_1080_90 = 1920.0 / 90.0  # ~21.3 px per degree at 1080p, 90 deg hFOV


def r(x, n=3):
    return float(round(x, n))


def ev100_camera(n, t, iso):
    """EV100 a camera triangle exposes for: log2(N^2/t) - log2(ISO/100)."""
    return math.log2(n * n / t) - math.log2(iso / 100.0)


def shutter_seconds(angle_deg, fps):
    return (angle_deg / 360.0) / fps


def hfov_to_focal(hfov_deg, sensor_width_mm):
    return (sensor_width_mm / 2.0) / math.tan(math.radians(hfov_deg) / 2.0)


def focal_to_hfov(f_mm, sensor_width_mm):
    return math.degrees(2.0 * math.atan((sensor_width_mm / 2.0) / f_mm))


def third_stop(iso):
    """Snap an ISO to the nearest 1/3-stop marked value (100, 125, 160 ...)."""
    marked = [50, 64, 80, 100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600, 2000,
              2500, 3200, 4000, 5000, 6400, 8000, 10000, 12800]
    if iso <= 0:
        return iso
    return float(min(marked, key=lambda m: abs(math.log2(m / iso))))


def hyperfocal_m(f_mm, n, coc_mm):
    return (f_mm * f_mm / (n * coc_mm) + f_mm) / 1000.0


def load(p):
    with open(p, "r", encoding="utf-8") as fh:
        return json.load(fh)


# ----------------------------------------------------------------------------
# camera bodies and lens sets
# ----------------------------------------------------------------------------
BODIES = {
    "super35_4perf": {
        "label": "Super 35 4-perf (UE 'Super 35mm')",
        "Filmback": {"SensorWidth": 24.89, "SensorHeight": 18.66},
        "ue_filmback_preset": "Super 35mm",
        "squeeze": 1.0, "native_iso": 800, "coc_mm": 0.025,
        "use": "cutscene default body; StoryDirector focal lengths are authored against this width",
    },
    "super35_16x9": {
        "label": "Super 35 16:9 (24.89 x 14.00 mm, digital S35 16:9 crop class)",
        "Filmback": {"SensorWidth": 24.89, "SensorHeight": 14.0},
        "ue_filmback_preset": None,
        "squeeze": 1.0, "native_iso": 800, "coc_mm": 0.025,
        "use": "handheld / documentary style inserts if a 16:9 native gate is wanted",
    },
    "full_frame": {
        "label": "Full frame 36 x 24 mm (16:9 active 36 x 20.25)",
        "Filmback": {"SensorWidth": 36.0, "SensorHeight": 24.0},
        "ue_filmback_preset": "Full Frame DSLR (verify name in 5.8 CineCamera settings)",
        "squeeze": 1.0, "native_iso": 800, "coc_mm": 0.03,
        "use": "reference body for gameplay FOV -> focal-length equivalents",
    },
    "alexa_lf_open_gate": {
        "label": "ARRI ALEXA LF Open Gate",
        "Filmback": {"SensorWidth": 36.7, "SensorHeight": 25.54},
        "ue_filmback_preset": "ARRI ALEXA LF (Open Gate)",
        "squeeze": 1.0, "native_iso": 800, "coc_mm": 0.03,
        "use": "workshop / overcast chapters (large-format, shallow DOF, clean shadows)",
    },
    "anamorphic_35_2x": {
        "label": "35mm 4-perf 2x anamorphic (2.39:1 after desqueeze)",
        "Filmback": {"SensorWidth": 21.95, "SensorHeight": 18.59},
        "ue_filmback_preset": "35mm 4-perf Anamorphic",
        "squeeze": 2.0, "native_iso": 800, "coc_mm": 0.025,
        "use": "rooftops chapters (oval bokeh, horizontal flares)",
    },
    "imax_70mm": {
        "label": "IMAX 15/70",
        "Filmback": {"SensorWidth": 70.41, "SensorHeight": 52.63},
        "ue_filmback_preset": "IMAX 70mm",
        "squeeze": 1.0, "native_iso": 800, "coc_mm": 0.06,
        "use": "city / titan / finale chapters (scale, very shallow DOF for a given FOV)",
    },
}

LENS_SETS = {
    "s35_spherical_primes": {
        "label": "Modern S35 spherical primes (Master-Prime class)",
        "focal_lengths_mm": [14, 18, 21, 25, 27, 32, 35, 40, 50, 65, 75, 100, 135],
        "LensSettings": {"MinFocalLength": 14.0, "MaxFocalLength": 135.0, "MinFStop": 1.3, "MaxFStop": 22.0,
                          "MinimumFocusDistance": 350.0, "SqueezeFactor": 1.0, "DiaphragmBladeCount": 9},
        "natural_vignette_stops_wide_open": 0.8, "lateral_ca_px_1080": 0.4,
        "distortion_brown_conrady": {"18": {"k1": -0.035, "k2": 0.006}, "35": {"k1": -0.012, "k2": 0.0},
                                     "50": {"k1": 0.004, "k2": 0.0}, "100": {"k1": 0.008, "k2": 0.0}},
        "flare_character": "low; small 9-blade ghosts, faint veiling glare",
    },
    "lf_spherical_primes": {
        "label": "Large-format spherical primes (Signature-Prime class)",
        "focal_lengths_mm": [21, 25, 29, 35, 40, 47, 58, 75, 105, 125],
        "LensSettings": {"MinFocalLength": 21.0, "MaxFocalLength": 125.0, "MinFStop": 1.8, "MaxFStop": 22.0,
                          "MinimumFocusDistance": 350.0, "SqueezeFactor": 1.0, "DiaphragmBladeCount": 11},
        "natural_vignette_stops_wide_open": 1.0, "lateral_ca_px_1080": 0.3,
        "distortion_brown_conrady": {"29": {"k1": -0.02, "k2": 0.004}, "58": {"k1": 0.003, "k2": 0.0}},
        "flare_character": "very low; soft veiling glare, round ghosts",
    },
    "anamorphic_2x_primes": {
        "label": "2x front-anamorphic primes",
        "focal_lengths_mm": [40, 50, 75, 100, 135],
        "LensSettings": {"MinFocalLength": 40.0, "MaxFocalLength": 135.0, "MinFStop": 2.0, "MaxFStop": 22.0,
                          "MinimumFocusDistance": 900.0, "SqueezeFactor": 2.0, "DiaphragmBladeCount": 9},
        "natural_vignette_stops_wide_open": 1.2, "lateral_ca_px_1080": 1.0,
        "distortion_brown_conrady": {"40": {"k1": -0.05, "k2": 0.02, "note": "mustache; horizontal-axis bend dominates"},
                                     "75": {"k1": -0.02, "k2": 0.008}},
        "petzval_and_barrel": {"DepthOfFieldPetzvalBokeh": 0.5, "DepthOfFieldPetzvalBokehFalloff": 1.0,
                               "DepthOfFieldBarrelRadius": 5.0, "DepthOfFieldBarrelLength": 6.0},
        "flare_character": "horizontal blue streaks + oval ghosts (anamorphic option below)",
    },
    "imax_primes": {
        "label": "IMAX 65mm primes (Hasselblad-derived class)",
        "focal_lengths_mm": [40, 50, 80, 110, 150],
        "LensSettings": {"MinFocalLength": 40.0, "MaxFocalLength": 150.0, "MinFStop": 2.8, "MaxFStop": 32.0,
                          "MinimumFocusDistance": 600.0, "SqueezeFactor": 1.0, "DiaphragmBladeCount": 7},
        "natural_vignette_stops_wide_open": 1.1, "lateral_ca_px_1080": 0.5,
        "distortion_brown_conrady": {"40": {"k1": -0.025, "k2": 0.005}, "80": {"k1": 0.002, "k2": 0.0}},
        "flare_character": "moderate; heptagonal ghosts",
    },
}

BODY_FOR_FILMBACK = {  # lighting_presets FilmbackPreset -> (body, lens set)
    "Super 35mm": ("super35_4perf", "s35_spherical_primes"),
    "ARRI ALEXA LF (Open Gate)": ("alexa_lf_open_gate", "lf_spherical_primes"),
    "IMAX 70mm": ("imax_70mm", "imax_primes"),
}


def body_for(cine):
    fb = cine.get("FilmbackPreset", "")
    if fb in BODY_FOR_FILMBACK:
        return BODY_FOR_FILMBACK[fb]
    if "Anamorphic" in fb:
        return ("anamorphic_35_2x", "anamorphic_2x_primes")
    return ("super35_4perf", "s35_spherical_primes")


# ----------------------------------------------------------------------------
# tiers (must match DefaultScalability_realism.ini / live DefaultScalability.ini)
# ----------------------------------------------------------------------------
TIERS = ["Epic", "High", "Medium"]
MOTION_BLUR_TIERS = {
    "Epic":   {"cvars": {"r.MotionBlurQuality": 4, "r.MotionBlur.HalfResGather": 0},
               "MotionBlurAmount": 0.5, "MotionBlurMax": 5.0, "MotionBlurPerObjectSize": 0.0,
               "MotionBlurTargetFPS": 60, "cutscene_TargetFPS": 24},
    "High":   {"cvars": {"r.MotionBlurQuality": 3, "r.MotionBlur.HalfResGather": 1},
               "MotionBlurAmount": 0.5, "MotionBlurMax": 4.0, "MotionBlurPerObjectSize": 0.0,
               "MotionBlurTargetFPS": 60, "cutscene_TargetFPS": 24},
    "Medium": {"cvars": {"r.MotionBlurQuality": 3, "r.MotionBlur.HalfResGather": 1},
               "MotionBlurAmount": 0.4, "MotionBlurMax": 3.0, "MotionBlurPerObjectSize": 0.5,
               "MotionBlurTargetFPS": 60, "cutscene_TargetFPS": 24},
}

# photoreal runtime clamps enforced by ContribLighting::ApplyPhotorealGrade (read from source)
PHOTOREAL_CAPS = {"FilmGrainIntensity_gameplay": 0.08, "FilmGrainIntensity_cinematic": 0.15,
                  "BloomIntensity": 0.6, "LensFlareIntensity": 0.4, "ColorSaturation": 1.0,
                  "SceneFringeIntensity": 0.0}

# StoryDirector shot sizes (focal lengths authored for S35 framing)
STORY_SHOTS = {"wide": 20, "aerial": 22, "medium": 35, "close": 50, "rack": 60, "ecu": 70}

# cutscene grain by native/effective ISO (digital-cinema noise, not film)
GRAIN_BY_ISO = [(100, 0.02), (200, 0.03), (400, 0.04), (800, 0.05), (1600, 0.08), (3200, 0.11), (6400, 0.15)]


def grain_for_iso(iso):
    iso = max(GRAIN_BY_ISO[0][0], min(GRAIN_BY_ISO[-1][0], iso))
    for (i0, g0), (i1, g1) in zip(GRAIN_BY_ISO, GRAIN_BY_ISO[1:]):
        if i0 <= iso <= i1:
            t = (math.log2(iso) - math.log2(i0)) / (math.log2(i1) - math.log2(i0))
            return r(g0 + (g1 - g0) * t, 3)
    return GRAIN_BY_ISO[-1][1]


def gameplay_aperture(ev):
    if ev >= 12.0:
        return 8.0
    if ev >= 7.0:
        return 4.0
    return 2.0


# ----------------------------------------------------------------------------
# camera_realism.json
# ----------------------------------------------------------------------------
def build_camera(L, R):
    LP = L["presets"]
    ids = list(LP.keys())
    chap = L.get("chapter_mapping", {})

    # ---- gameplay shot types --------------------------------------------
    tp_hfov = 90.0
    shot_types = {
        "_convention": ("Gameplay cameras are UCameraComponent with a HORIZONTAL FOV; the user's FieldOfView setting "
                        "(ArenaGameUserSettings, default 95, slider 70-120) stays authoritative. Shot types are expressed "
                        "as multipliers/offsets on it plus the full-frame (36 mm wide) equivalent focal length so "
                        "cinematography and gameplay talk the same units: f_ff = 18 / tan(hFOV/2)."),
        "gameplay_third_person": {
            "camera": "IronManCharacter.FollowCamera on SpringArm (TargetArmLength 420 cm, Hulkbuster 650)",
            "hfov_rule": "user_fov - 5 (current code: SetCameraFOV TP = FP - 5)",
            "reference_hfov_deg": tp_hfov,
            "ff_equivalent_focal_mm": r(hfov_to_focal(tp_hfov, FF_WIDTH), 1),
            "recommended_hfov_rule": "user_fov - 10 (85 at default 95 = 19.6 mm FF); 90 deg at 4.2 m stretches the suit at frame edges",
            "spring_arm": {"TargetArmLength": 420.0, "SocketOffset": [0.0, 70.0, 70.0], "CameraLagSpeed": 14.0,
                            "recommended_CameraRotationLagSpeed": 0.0},
            "depth_of_field": "off (DepthOfFieldFocalDistance overridden to 0). At 18-20 mm FF and f/4+ the hyperfocal "
                              "distance is ~3 m, so a real lens would render everything past the suit sharp.",
            "motion_blur": "per tier; camera + object",
        },
        "aim": {
            "hfov_multiplier": 0.8,
            "reference_hfov_deg": r(tp_hfov * 0.8, 1),
            "ff_equivalent_focal_mm": r(hfov_to_focal(tp_hfov * 0.8, FF_WIDTH), 1),
            "blend_in_s": 0.15, "blend_out_s": 0.2, "ease": "cubic in/out",
            "spring_arm": {"TargetArmLength": 250.0, "SocketOffset": [0.0, 80.0, 60.0], "CameraLagSpeed": 20.0},
            "depth_of_field": {
                "default": "off",
                "epic_optional": {"DepthOfFieldFstop": 2.8, "DepthOfFieldSensorWidth": 36.0,
                                   "DepthOfFieldMinFstop": 1.2, "DepthOfFieldBladeCount": 9,
                                   "DepthOfFieldFocalDistance": "aim trace hit distance (cm), interp 6/s",
                                   "note": "only near geometry (<~3 m) visibly blurs; hyperfocal at 22.5 mm f/2.8 is ~6 m"},
            },
            "motion_blur_amount_scale": 0.7,
            "camera_rotation_blur": "recommend off while aiming (needs C++ bCameraMotionBlur=false, see notes)",
        },
        "flight": {
            "fov_vs_speed_deg_add": [{"speed_ms": 0.0, "add": 0.0}, {"speed_ms": 13.5, "add": 3.0},
                                      {"speed_ms": 22.3, "add": 6.0}, {"speed_ms": 29.0, "add": 8.0},
                                      {"speed_ms": 100.0, "add": 10.0}, {"speed_ms": SPEED_OF_SOUND, "add": 12.0}],
            "speed_source": "velocity cm/s / 100 (same as HelmetHUDWidget SpeedMs)",
            "fov_interp_speed": 3.0, "boost_onset_fov_kick_deg": 2.0, "boost_onset_kick_s": 0.25,
            "max_hfov_deg": 120.0,
            "note": "non-physical perceptual cue (a real camera does not widen with speed); kept modest. Reduced-motion: x0.5.",
            "spring_arm": {"CameraLagSpeed": 10.0, "CameraRotationLagSpeed": 12.0,
                            "note": "slight rotation lag reads as a chase drone instead of a rigid boom"},
        },
        "helmet_hud_pov": {
            "camera": "FirstPersonCamera, eye height 74 cm, FirstPersonFOV 95 (60-120)",
            "reference_hfov_deg": 95.0,
            "ff_equivalent_focal_mm": r(hfov_to_focal(95.0, FF_WIDTH), 1),
            "post_overrides": {"VignetteIntensity": 0.0, "SceneFringeIntensity": 0.0,
                                "FilmGrainIntensity_scale": 0.5, "LensFlareIntensity_cap": 0.1,
                                "DepthOfFieldFocalDistance": 0.0, "MotionBlurAmount_scale": 0.7},
            "why": ("EDITH's visor widget already adds barrel warp (K1 .06/K2 .015), chromatic edge split 1.5 px, "
                    "0.02 noise and a warning vignette (contrib/edith/hud_spec.md). Adding scene-level vignette, CA, "
                    "grain or lens distortion would double them. The visor is a display, not a lens, so the scene "
                    "behind it should look clean."),
            "exposure": "same histogram as gameplay; consider AutoExposureMeterMask (center-weighted) so HUD-lit visor frame does not meter",
        },
        "cutscene": {
            "camera": "ACineCameraActor spawned by StoryDirector / SuitUpCinematic",
            "story_director_focal_mm_s35": STORY_SHOTS,
            "focal_scale_rule": "f_body = f_s35 * (SensorWidth * SqueezeFactor / 24.89) keeps StoryDirector horizontal framing on non-S35 filmbacks",
            "aperture_rule_by_shot": {"wide": "max(preset, f/4 S35-equiv)", "aerial": "max(preset, f/5.6 S35-equiv)",
                                       "medium": "preset", "close": "preset", "rack": "preset (wide open)",
                                       "ecu": "max(preset, f/2.8 S35-equiv) - wider than that leaves eyelashes soft"},
            "dof_equivalence": "N_body = N_s35 * SensorWidth/24.89 gives the same DOF for the same framing",
            "focus": {"FocusMethod": "Tracking (face/eye socket) or Manual from look-at distance",
                       "bSmoothFocusChanges": True, "FocusSmoothingInterpSpeed": 4.0,
                       "rack_focus_s": 0.8, "eye_offset_cm": [0.0, 0.0, 8.0]},
            "shutter": "180 deg at 24 fps (1/48); ruins + titan_battlefield 90 deg (1/96, combat staccato) - from lighting_presets",
        },
    }

    # ---- cutscene packages per environment ------------------------------
    cut = {}
    for k, p in LP.items():
        c = p["CineCamera"]
        o = c["PostProcessOverrides"]
        body, lens = body_for(c)
        sw = c["Filmback"]["SensorWidth"]
        squeeze = c["LensSettings"].get("SqueezeFactor", 1.0)
        scale = sw * squeeze / S35_WIDTH  # desqueezed horizontal width vs S35
        fset = LENS_SETS[lens]["focal_lengths_mm"]
        shots = {}
        for s, f in STORY_SHOTS.items():
            exact = f * scale
            snapped = min(fset, key=lambda x: abs(x - exact))
            shots[s] = {"focal_exact_mm": r(exact, 1), "focal_prime_mm": snapped,
                        "hfov_deg": r(focal_to_hfov(snapped, sw * squeeze), 1),
                        "out_of_lens_set": bool(abs(snapped - exact) / exact > 0.2)}
        n = c["CurrentAperture"]
        t = 1.0 / o["CameraShutterSpeed"]
        iso = o["CameraISO"]
        angle = 360.0 * t * o.get("MotionBlurTargetFPS", 24)
        cut[k] = {
            "body": body, "lens_set": lens,
            "FilmbackPreset": c.get("FilmbackPreset"), "Filmback": c["Filmback"],
            "SqueezeFactor": c["LensSettings"].get("SqueezeFactor", 1.0),
            "CurrentFocalLength": c["CurrentFocalLength"], "CurrentAperture": n,
            "CameraShutterSpeed": o["CameraShutterSpeed"], "shutter_angle_deg": r(angle, 1),
            "CameraISO_effective": iso, "native_iso": c.get("_iso_native"),
            "nd_filter": c.get("_nd_filter"),
            "camera_ev100": r(ev100_camera(n, t, iso), 2),
            "scene_ev100": p["exposure_reference"]["scene_ev100"],
            "focal_scale_vs_s35": r(scale, 3),
            "story_shots": shots,
            "DepthOfFieldBladeCount": LENS_SETS[lens]["LensSettings"]["DiaphragmBladeCount"],
            "film_grain_cinematic": min(grain_for_iso(c.get("_iso_native", 800)), PHOTOREAL_CAPS["FilmGrainIntensity_cinematic"]),
            "hyperfocal_m_at_base": r(hyperfocal_m(c["CurrentFocalLength"], n, BODIES[body]["coc_mm"]), 1),
        }
        if lens == "anamorphic_2x_primes":
            cut[k].update(LENS_SETS[lens]["petzval_and_barrel"])

    # ---- exposure --------------------------------------------------------
    exp_env = {}
    for k, p in LP.items():
        pp = p["PostProcessVolume"]
        ev = p["exposure_reference"]["scene_ev100"]
        n = gameplay_aperture(ev)
        t = shutter_seconds(180.0, 60.0)
        iso_eff = 100.0 * (n * n / t) / (2.0 ** ev)
        native = 800.0 if iso_eff < 800.0 else third_stop(iso_eff)
        nd = max(0.0, math.log2(native / iso_eff)) if iso_eff < 800.0 else 0.0
        exp_env[k] = {
            "scene_ev100": ev,
            "manual_ev100": ev,
            "histogram": {"AutoExposureMethod": "AEM_Histogram",
                           "AutoExposureMinBrightness": pp["AutoExposureMinBrightness"],
                           "AutoExposureMaxBrightness": pp["AutoExposureMaxBrightness"],
                           "AutoExposureLowPercent": pp.get("AutoExposureLowPercent", 80.0),
                           "AutoExposureHighPercent": pp.get("AutoExposureHighPercent", 98.3),
                           "AutoExposureSpeedUp": pp.get("AutoExposureSpeedUp", 3.0),
                           "AutoExposureSpeedDown": pp.get("AutoExposureSpeedDown", 1.0),
                           "AutoExposureBias": pp.get("AutoExposureBias", 0.0)},
            "gameplay_manual_triangle": {
                "use": "photo mode / locked exposure only (AEM_Manual + AutoExposureApplyPhysicalCameraExposure)",
                "DepthOfFieldFstop": n, "CameraShutterSpeed": 120.0, "shutter_angle_deg_at_60fps": 180.0,
                "CameraISO": r(iso_eff, 2), "native_iso": native, "nd_stops": r(nd, 1),
                "check_ev100": r(ev100_camera(n, t, iso_eff), 2)},
            "local_exposure": {
                "LocalExposureHighlightContrastScale": pp.get("LocalExposureHighlightContrastScale", 1.0),
                "LocalExposureShadowContrastScale": pp.get("LocalExposureShadowContrastScale", 1.0),
                "LocalExposureDetailStrength": pp.get("LocalExposureDetailStrength", 1.0),
                "LocalExposureBlurredLuminanceBlend": R["presets"][k]["PostProcessVolume"].get("LocalExposureBlurredLuminanceBlend", 0.6),
                "LocalExposureMiddleGreyBias": 0.0,
            },
        }
    w, c = LP["workshop"], LP["city"]
    exposure = {
        "model": {
            "units": "EV100 (r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True, so Min/MaxBrightness and HistogramLogMin/Max are EV100)",
            "scene_ev100": "log2(illuminance_lux / 2.5) from lighting_presets.exposure_reference",
            "camera_ev100": "log2(N^2 / t) - log2(ISO / 100); ND folded into effective ISO",
            "shutter": "t = (shutter_angle / 360) / fps",
        },
        "histogram_range": {"HistogramLogMin": -2.0, "HistogramLogMax": 18.0,
                             "why": "covers city_night (3.9) to sunlit city (15.6) with >2 EV headroom both ends"},
        "per_environment": exp_env,
        "transitions": {
            "workshop_to_outdoors": {
                "volume": "bounded PPV at the hangar door / exit tunnel, Priority 1, BlendRadius 300 cm",
                "AutoExposureMinBrightness": w["PostProcessVolume"]["AutoExposureMinBrightness"],
                "AutoExposureMaxBrightness": c["PostProcessVolume"]["AutoExposureMaxBrightness"],
                "AutoExposureSpeedUp": 2.5, "AutoExposureSpeedDown": 1.0,
                "stops": r(c["exposure_reference"]["scene_ev100"] - w["exposure_reference"]["scene_ev100"], 2),
                "approx_adapt_s": {"to_bright": r((c["exposure_reference"]["scene_ev100"] - w["exposure_reference"]["scene_ev100"]) / 2.5, 1),
                                   "to_dark": r((c["exposure_reference"]["scene_ev100"] - w["exposure_reference"]["scene_ev100"]) / 1.0, 1)},
                "why": "eyes/cameras adapt to light in seconds and to dark much slower; 2.5 EV/s reads as a real exposure pull without a white-out longer than ~1 s",
            },
            "flash_events": {
                "explosions_lightning": "keep AutoExposureHighPercent 98.3 so small bright VFX are ignored; combat overlay drops SpeedUp to 2.0 to stop pumping on sub-0.3 s flashes",
                "unibeam_pov": "flash lasts 0.9 s; at SpeedUp 2 the camera pulls ~0.5 EV - intended",
            },
            "eye_adaptation_cvars": {"r.EyeAdaptation.ExponentialTransitionDistance": 1.5,
                                      "note": "verify name/default in 5.8; controls where the speed switches from linear to exponential"},
        },
        "bias_curve_optional": {
            "property": "AutoExposureBiasCurve (CurveFloat, x = average scene EV100, y = bias EV)",
            "keys": [{"ev100": 2.0, "bias": -0.7}, {"ev100": 5.0, "bias": -0.4}, {"ev100": 9.0, "bias": 0.0},
                     {"ev100": 15.0, "bias": 0.0}, {"ev100": 17.0, "bias": 0.3}],
            "why": "night stays night (real cameras/eyes do not lift a moonlit street to mid-grey); midday snow/sky gets +0.3",
            "asset": "/Game/Contrib/Vison/Camera/C_AutoExposureBias (create in editor; not created by script)",
        },
        "meter_mask_optional": "AutoExposureMeterMask: centre-weighted radial texture (centre 1.0, edge 0.35) for helmet POV",
        "local_exposure_method_by_tier": {"Epic": {"gameplay": "BILATERAL", "cutscene": "FUSION (verify in 5.8 - fall back to BILATERAL)"},
                                           "High": "BILATERAL", "Medium": "BILATERAL"},
        "cutscene": "AEM_Manual + AutoExposureApplyPhysicalCameraExposure from the cutscene camera triangle (cutscene_packages)",
    }

    # ---- lens effects ----------------------------------------------------
    gp_lens = {}
    for k, p in LP.items():
        pp = p["PostProcessVolume"]
        night = p["exposure_reference"]["scene_ev100"] < 7.0
        gp_lens[k] = {
            "VignetteIntensity": r(min(pp.get("VignetteIntensity", 0.4) * 0.6, 0.3), 3),
            "FilmGrainIntensity": r(min(pp.get("FilmGrainIntensity", 0.0), 0.05 if night else 0.03,
                                         PHOTOREAL_CAPS["FilmGrainIntensity_gameplay"]), 3),
            "LensFlareIntensity": r(min(pp.get("LensFlareIntensity", 0.4), PHOTOREAL_CAPS["LensFlareIntensity"]) * 0.5, 3),
            "SceneFringeIntensity": 0.0,
            "cutscene_VignetteIntensity": pp.get("VignetteIntensity", 0.4),
            "cutscene_LensFlareIntensity": min(pp.get("LensFlareIntensity", 0.4), PHOTOREAL_CAPS["LensFlareIntensity"]),
            "cutscene_SceneFringeIntensity_spec": r(min(max(pp.get("SceneFringeIntensity", 0.1), 0.1), 0.25), 2),
        }
    lens_effects = {
        "chromatic_aberration": {
            "gameplay": {"SceneFringeIntensity": 0.0, "why": "matches the live photoreal pass (r.SceneColorFringeQuality=0)"},
            "cutscene_spec": {"SceneFringeIntensity": "0.10-0.25 (per environment)", "ChromaticAberrationStartOffset": 0.5,
                               "real_reference": "0.3-1.0 px lateral CA at frame edge at 1080p for cine primes",
                               "status": "INERT today: DefaultScalability sets r.SceneColorFringeQuality=0 on every tier and ApplyPhotorealGrade forces 0 on cine cameras too"},
        },
        "vignette": {
            "VignetteType": "COSINE_FOURTH_LAW (5.8: physically based cos^4 falloff; fall back to OVAL if unavailable)",
            "rule_gameplay": "min(environment * 0.6, 0.3)",
            "rule_cutscene": "environment value (lighting_presets), which matches ~0.8-1.2 stops natural falloff wide open",
            "helmet_pov": 0.0,
        },
        "bloom": {
            "gameplay_all_tiers": {"BloomMethod": "BM_SOG", "BloomThreshold": -1.0, "BloomIntensity": "realism preset, cap 0.6",
                                    "why": "threshold -1 = energy-conserving (every pixel scatters a little), like real veiling glare"},
            "cutscene_Epic": {"BloomMethod": "BM_FFT", "BloomIntensity": 1.0, "BloomGaussianIntensity": 0.0,
                               "BloomConvolutionSize": 1.0, "BloomConvolutionBufferScale": 0.133,
                               "BloomConvolutionPreFilterMin": 7.0, "BloomConvolutionPreFilterMax": 15000.0,
                               "BloomConvolutionPreFilterMult": 15.0, "BloomConvolutionScatterDispersion": 1.0,
                               "kernel": "engine default; upgrade = measured 1024^2 EXR kernel per lens set (9/11/7-blade starbursts)",
                               "source": "postprocess_realism _tier_overrides.Epic (copied)"},
            "cutscene_High_Medium": "BM_SOG, same as gameplay",
        },
        "lens_flare": {
            "minimal_default": {"rule_gameplay": "min(env, 0.4) * 0.5", "LensFlareThreshold": 8.0, "LensFlareBokehSize": 3.0,
                                 "LensFlareBokehShape": "/Game/Contrib/Vison/Camera/T_Flare_Bokeh_9Blade (optional import)"},
            "anamorphic_option": {"applies_to": ["rooftops", "rooftops_storm"],
                                  "LensFlareBokehShape": "/Game/Contrib/Vison/Camera/T_Flare_Anamorphic_Streak",
                                  "LensFlareTint": {"R": 0.62, "G": 0.78, "B": 1.0, "A": 1.0},
                                  "LensFlareBokehSize": 6.0, "LensFlareIntensity": 0.4,
                                  "note": "image-based flare only; a true anamorphic streak needs a convolution kernel (Epic FFT) or a VFX sprite (Ultron's domain)"},
        },
        "film_grain": {
            "model": "digital sensor noise: strongest in shadows",
            "FilmGrainIntensityShadows": 1.0, "FilmGrainIntensityMidtones": 0.7, "FilmGrainIntensityHighlights": 0.25,
            "FilmGrainShadowsMax": 0.09, "FilmGrainHighlightsMin": 0.5, "FilmGrainHighlightsMax": 1.0,
            "cutscene_by_native_iso": [{"iso": i, "FilmGrainIntensity": g} for i, g in GRAIN_BY_ISO],
            "caps": {"gameplay": PHOTOREAL_CAPS["FilmGrainIntensity_gameplay"], "cinematic": PHOTOREAL_CAPS["FilmGrainIntensity_cinematic"]},
        },
        "lens_distortion": {
            "default": "off",
            "option_A": "Lens Distortion / CameraCalibrationCore plugin (Brown-Conrady k1,k2,p1,p2,k3 per lens set below) - NOT enabled in IronManArena.uproject",
            "option_B": "ST-map displacement material as a post-process blendable (BL_SceneColorAfterTonemapping is wrong for distortion; use BeforeTranslucency/ReplacingTonemapper-safe pass)",
            "overscan": 0.05,
            "cost_estimate_ms_High": "1.0-1.4 (overscan + extra full-screen pass) - estimate",
            "helmet_pov": "never (EDITH visor warp already does this)",
            "coefficients": {k: v["distortion_brown_conrady"] for k, v in LENS_SETS.items()},
        },
        "per_environment": gp_lens,
    }

    # ---- DOF ---------------------------------------------------------------
    dof = {
        "gameplay": "off in third person, flight and helmet POV; optional subtle aim DOF on Epic (see shot_types.aim)",
        "cutscene": "cinematic: per-shot aperture rules, focus tracking with smoothing, blade count from lens set",
        "tiers": {"Epic": {"r.DepthOfFieldQuality": 2, "r.DOF.Gather.RingCount": 5},
                  "High": {"r.DepthOfFieldQuality": 2, "r.DOF.Gather.RingCount": 4},
                  "Medium": {"r.DepthOfFieldQuality": 1, "r.DOF.Gather.RingCount": 3}},
        "DepthOfFieldSkyFocusDistance": 0.0,
        "DepthOfFieldMatteBoxFlags": "unset (no matte box occlusion of bokeh)",
    }

    # ---- motion blur -------------------------------------------------------
    motion_blur = {
        "shutter_model": "MotionBlurAmount = shutter_angle / 360; 180 deg -> 0.5",
        "per_object_vs_camera": {
            "per_object": "always on (velocity buffer); MotionBlurPerObjectSize 0 = no minimum size (all objects)",
            "camera": "on in third person and flight; recommend OFF in aim and helmet POV via FSceneView.bCameraMotionBlur=false "
                      "from a custom UGameViewportClient (C++; code owner). Without it, use MotionBlurAmount x0.7 there.",
        },
        "tiers": MOTION_BLUR_TIERS,
        "cutscene": {"MotionBlurAmount": "from lighting_presets (0.5 = 180 deg; 0.25 = 90 deg ruins/titan)", "MotionBlurTargetFPS": 24},
        "why_target_fps_60": "TargetFPS 60 locks blur length to a 1/120 s shutter regardless of the real frame rate (dynamic res keeps ~60 on High); at 30-45 fps blur would otherwise stretch",
        "reduced_motion": "player toggle bMotionBlur already exists; reduced-motion users also get camera blur off",
    }

    # ---- per-tier summary + estimates -------------------------------------
    tiers = {
        "_label": "GPU ms on RTX 2070 SUPER @1080p (High 75% screen percentage) - ESTIMATES, not measured",
        "Epic":   {"gameplay_ms": {"motion_blur": 0.4, "bloom_sog": 0.3, "lens_flare": 0.15, "local_exposure_bilateral": 0.15,
                                    "eye_adaptation": 0.05, "tonemap_grain_vignette_lut": 0.1, "aim_dof_optional": 0.5},
                   "cutscene_extra_ms": {"fft_bloom": 2.0, "local_exposure_fusion": 0.4, "dof": 0.7}},
        "High":   {"gameplay_ms": {"motion_blur": 0.3, "bloom_sog": 0.25, "lens_flare": 0.12, "local_exposure_bilateral": 0.12,
                                    "eye_adaptation": 0.04, "tonemap_grain_vignette_lut": 0.08},
                   "cutscene_extra_ms": {"dof": 0.4}},
        "Medium": {"gameplay_ms": {"motion_blur": 0.25, "bloom_sog": 0.2, "lens_flare": 0.1, "local_exposure_bilateral": 0.1,
                                    "eye_adaptation": 0.03, "tonemap_grain_vignette_lut": 0.07},
                   "cutscene_extra_ms": {"dof": 0.25}},
    }
    for t in TIERS:
        g = tiers[t]["gameplay_ms"]
        tiers[t]["gameplay_total_ms"] = r(sum(v for kk, v in g.items() if not kk.endswith("optional")), 2)
        tiers[t]["cutscene_total_ms"] = r(tiers[t]["gameplay_total_ms"] + sum(tiers[t]["cutscene_extra_ms"].values()), 2)

    return {
        "metadata": {
            "name": "Vison camera realism layer", "version": VERSION, "date": DATE, "engine": "Unreal Engine 5.8",
            "generated_by": "contrib/vison/camera/gen_camera_realism.py (edit the generator, not this file)",
            "sources": ["../lighting_presets.json", "../postprocess_realism.json", "IronManArena/Config/*.ini (read only)",
                        "Source: IronManCharacter, ArenaFeel, StoryDirector, SuitUpCinematic, ContribLighting, ArenaGameUserSettings (read only)"],
            "layer_order": ["lighting_presets (apply_lighting_preset.py)", "postprocess_realism (apply_realism.py)",
                             "camera layer (apply_camera_realism.py) - camera/lens/exposure/grade keys only"],
            "not_authored_here": "sun/sky/fog/clouds/lights, VFX, destruction (Ultron); suit materials (contrib/vison/suits)",
            "target": "RTX 2070 SUPER, 1080p, ~60 fps on High",
            "environments": ids, "chapter_mapping": chap,
            "photoreal_runtime_caps": PHOTOREAL_CAPS,
        },
        "bodies": BODIES,
        "lens_sets": LENS_SETS,
        "shot_types": shot_types,
        "cutscene_packages": cut,
        "exposure": exposure,
        "lens_effects": lens_effects,
        "depth_of_field": dof,
        "motion_blur": motion_blur,
        "tiers": tiers,
    }


# ----------------------------------------------------------------------------
# color_grade.json
# ----------------------------------------------------------------------------
V4 = lambda x, y, z, w: {"X": x, "Y": y, "Z": z, "W": w}
GRADE_OPS = {"mul": ["ColorSaturation", "ColorContrast", "ColorGamma", "ColorGain"],
             "add": ["ColorOffset", "VignetteIntensity", "FilmGrainIntensity"]}

GRADES = {
    "neutral_realistic": {
        "description": "Reference camera look: the environment's own grade only (lighting_presets), no extra stylisation.",
        "mode": "look", "values": {},
    },
    "cinematic_teal_orange": {
        "description": "Subtle complementary split: cool shadows, warm highlights, <=4% per channel so skin and gold titanium stay true.",
        "mode": "look",
        "values": {"ColorOffsetShadows": V4(-0.0025, 0.0005, 0.004, 0.0),
                   "ColorGainHighlights": V4(1.035, 1.0, 0.955, 1.0),
                   "ColorSaturationHighlights": V4(1.0, 1.0, 1.0, 0.96),
                   "ColorContrast": V4(1.0, 1.0, 1.0, 1.03)},
    },
    "night": {
        "description": "Mesopic vision: lower saturation (shadows most), slight blue shift in the darks (Purkinje), mild contrast.",
        "mode": "look",
        "values": {"ColorSaturation": V4(1.0, 1.0, 1.0, 0.85),
                   "ColorSaturationShadows": V4(1.0, 1.0, 1.0, 0.75),
                   "ColorGainShadows": V4(0.97, 1.0, 1.05, 1.0),
                   "ColorOffsetShadows": V4(-0.001, 0.0, 0.002, 0.0),
                   "ColorContrast": V4(1.0, 1.0, 1.0, 1.04)},
    },
    "overcast": {
        "description": "Soft diffuse light: slightly lower saturation, a touch more contrast to restore shape, faint cool highlights.",
        "mode": "look",
        "values": {"ColorSaturation": V4(1.0, 1.0, 1.0, 0.94),
                   "ColorContrast": V4(1.0, 1.0, 1.0, 1.04),
                   "ColorGainHighlights": V4(0.99, 1.0, 1.02, 1.0)},
    },
    "combat": {
        "description": "Overlay, not a look: weighted PPV (BlendWeight 0..1 from combat intensity) on top of the environment look.",
        "mode": "overlay",
        "blend": {"weight_source": "combat intensity 0..1 (enemies engaged / damage taken in last 3 s)",
                  "fade_in_s": 0.6, "fade_out_s": 2.5, "priority": 2},
        "values": {"ColorContrast": V4(1.0, 1.0, 1.0, 1.08),
                   "ColorSaturation": V4(1.0, 1.0, 1.0, 0.92),
                   "ColorSaturationHighlights": V4(1.0, 1.0, 1.0, 0.95),
                   "ColorGainHighlights": V4(1.03, 1.0, 0.97, 1.0),
                   "ColorOffsetShadows": V4(0.0, 0.0, 0.0, -0.002),
                   "VignetteIntensity": 0.08,
                   "FilmGrainIntensity": 0.015,
                   "AutoExposureSpeedUp": 2.0},
    },
}

ENV_DEFAULT_GRADE = {
    "prologue": "neutral_realistic", "workshop": "neutral_realistic", "rooftops": "cinematic_teal_orange",
    "city": "neutral_realistic", "ruins": "neutral_realistic", "titan_battlefield": "cinematic_teal_orange",
    "finale": "cinematic_teal_orange", "workshop_night": "night", "rooftops_storm": "night",
    "city_night": "night", "city_overcast": "overcast",
}


# "subtle" guard: composed per-channel gain stays within +-5 %, offsets within +-0.006,
# so a look stacked on a preset that already split-tones (rooftops) cannot overshoot.
LOOK_LIMITS = {"gain": (0.95, 1.05), "add": (-0.006, 0.006)}


def op_for(key):
    for op, prefixes in GRADE_OPS.items():
        if any(key.startswith(p) for p in prefixes):
            return op
    return "set"


def compose(base_pp, grade_values):
    out = {}
    for key, val in grade_values.items():
        op = op_for(key)
        if isinstance(val, dict):
            ident = 0.0 if op == "add" else 1.0
            b = base_pp.get(key, {a: ident for a in "XYZW"})
            out[key] = {a: r((b[a] * val[a]) if op == "mul" else (b[a] + val[a]) if op == "add" else val[a], 4) for a in "XYZW"}
            lim = LOOK_LIMITS.get(op_for(key) if not key.startswith("ColorGain") else "gain")
            if lim and (key.startswith("ColorGain") or key.startswith("ColorOffset")):
                lo, hi = lim
                out[key] = {a: (min(max(v, lo), hi) if a != "W" else v) for a, v in out[key].items()}
            if key == "ColorSaturation":  # ApplyPhotorealGrade clamps saturation to <= 1
                out[key] = {a: min(v, PHOTOREAL_CAPS["ColorSaturation"]) for a, v in out[key].items()}
        else:
            b = base_pp.get(key, 0.0)
            out[key] = r(b + val, 4) if op == "add" else val
    return out


def build_grade(L):
    LP = L["presets"]
    wb, composed = {}, {}
    for k, p in LP.items():
        pp = p["PostProcessVolume"]
        wb[k] = {"TemperatureType": "TEMP_WHITE_BALANCE", "WhiteTemp": pp.get("WhiteTemp", 6500.0),
                 "WhiteTint": pp.get("WhiteTint", 0.0),
                 "meaning": "the illuminant being neutralised (camera white-balanced to the dominant light); copied from lighting_presets"}
        composed[k] = {}
        for g, spec in GRADES.items():
            if spec["mode"] != "look":
                continue
            composed[k][g] = compose(pp, spec["values"])
            cc = composed[k][g].get("ColorContrast")
            if cc:  # a look may add contrast, but never past max(base, 1.12)
                cc["W"] = r(min(cc["W"], max(pp.get("ColorContrast", {}).get("W", 1.0), 1.12)), 4)
            # report the base keys the look leaves untouched so the result is self-contained
            for base_key in ("ColorSaturation", "ColorContrast"):
                if base_key in pp and base_key not in composed[k][g]:
                    composed[k][g][base_key] = {a: r(min(pp[base_key][a], PHOTOREAL_CAPS["ColorSaturation"]) if base_key == "ColorSaturation" else pp[base_key][a], 4) for a in "XYZW"}
        combat_base = dict(composed[k][ENV_DEFAULT_GRADE[k]])
        combat_base.update({kk: pp[kk] for kk in ("VignetteIntensity", "FilmGrainIntensity") if kk in pp})
        composed[k]["combat_overlay_at_weight_1"] = compose(combat_base, GRADES["combat"]["values"])
        cg = composed[k]["combat_overlay_at_weight_1"]
        cg["FilmGrainIntensity"] = r(min(cg.get("FilmGrainIntensity", 0.0), PHOTOREAL_CAPS["FilmGrainIntensity_gameplay"]), 3)
    return {
        "metadata": {"name": "Vison camera color grade", "version": VERSION, "date": DATE,
                     "generated_by": "gen_camera_realism.py",
                     "composition": "look values are deltas on top of each environment's lighting_presets grade: "
                                    "Saturation/Contrast/Gamma/Gain multiply per channel, Offset/Vignette/Grain add, everything else sets. "
                                    "ColorSaturation result clamped to <= 1.0 (ApplyPhotorealGrade); composed ColorGain* R/G/B clamped to 0.95-1.05 and ColorOffset* R/G/B to +-0.006 (keeps looks subtle).",
                     "keys": "C++ FPostProcessSettings names (apply script converts to snake_case)"},
        "tonemapper": {
            "sdr_default": {"TonemappingMethod": "Filmic",
                            "why": "keeps the per-environment Film Slope/Toe/Shoulder/BlackClip/WhiteClip authored in lighting_presets (ACES-fitted filmic curve)",
                            "film_reference": {"FilmSlope": 0.88, "FilmToe": 0.55, "FilmShoulder": 0.26, "FilmBlackClip": 0.0, "FilmWhiteClip": 0.04},
                            "ExpandGamut": 1.0, "BlueCorrection": 0.6, "ToneCurveAmount": 1.0,
                            "cvars": {"r.Tonemapper.Quality": 5, "r.Tonemapper.Sharpen": "0.5 (live DefaultEngine)", "r.LUT.TonemappingMethod": -1}},
            "hdr_output": {"TonemappingMethod": "StandardACES",
                           "cvars": {"r.AllowHDR": "1 (ini only)", "r.HDR.EnableHDROutput": 1, "r.HDR.Aces.Version": 2,
                                     "r.HDR.PaperWhite": 203, "r.HDR.Display.MaxLuminance": "from OS / user calibration",
                                     "r.HDR.Display.OverrideOSMaxLuminance": "only if the user calibrates"},
                           "note": "Film* parameters do not apply to Standard ACES; grades still apply (pre-tonemap). The LUT is SDR-only."},
        },
        "white_balance": wb,
        "grades": GRADES,
        "environment_default_grade": ENV_DEFAULT_GRADE,
        "composed": composed,
        "lut": {
            "optional": True,
            "format": "256 x 16 px strip = 16^3 cube, sRGB PNG, red along x within each 16 px tile, green down y, blue across tiles",
            "import": {"TextureGroup": "TEXTUREGROUP_ColorLookupTable", "MipGenSettings": "NoMipmaps", "sRGB": True,
                        "CompressionSettings": "TC_Default (ColorLookupTable group disables compression)"},
            "applied": "display-referred, after the tonemapper; ColorGradingIntensity 0..1. SDR only - ignored with HDR output.",
            "files": {"neutral": "textures/T_LUT_Neutral_16.png (identity; start point for a colourist)",
                      "teal_orange_subtle": "textures/T_LUT_TealOrange_Subtle_16.png (display-space twin of cinematic_teal_orange, use at 0.5)"},
            "recommendation": "prefer the post-process grade values (work in HDR and SDR); use the LUT only for a final colourist pass",
        },
    }


# ----------------------------------------------------------------------------
# camera_shake.json
# ----------------------------------------------------------------------------
def ax(amp, hz):
    return {"Amplitude": amp, "Frequency_hz": hz}


def perlin(dur, bi, bo, **axes):
    return {"type": "PerlinNoise", "Duration": dur, "BlendInTime": bi, "BlendOutTime": bo, "axes": axes}


def wave(dur, bi, bo, offset="Zero", **axes):
    return {"type": "WaveOscillator", "Duration": dur, "BlendInTime": bi, "BlendOutTime": bo,
            "InitialOffsetType": offset, "axes": axes}


def trauma_eq(pitch_deg):
    return r(min(1.0, math.sqrt(max(pitch_deg, 0.0) / 2.4)), 2)


def build_shakes():
    S = {
        "idle_hover": {
            "description": "station-keeping hover: slow bob/sway from thrust vectoring (helicopter-hover class)",
            "trigger": "loop while flying and |velocity| < 2 m/s; stop on landing",
            "play_space": "CameraLocal", "single_instance": True, "first_person_rotation_scale": 0.5,
            "patterns": [perlin(0.0, 0.6, 0.6, X=ax(1.0, 0.35), Y=ax(1.4, 0.3), Z=ax(2.2, 0.45),
                                Pitch=ax(0.15, 0.4), Yaw=ax(0.1, 0.3), Roll=ax(0.25, 0.35))],
        },
        "thruster_boost": {
            "description": "boost onset kick (body pushed back into the camera) + sustained thrust rumble",
            "trigger": "kick on boost press; rumble loops while boosting, scale = throttle 0..1",
            "play_space": "CameraLocal", "single_instance": True,
            "patterns": [wave(0.35, 0.02, 0.25, X=ax(-6.0, 3.0), Pitch=ax(-0.7, 3.5)),
                         perlin(0.0, 0.15, 0.3, Z=ax(0.4, 14.0), Pitch=ax(0.12, 12.0), Yaw=ax(0.08, 10.0), Roll=ax(0.15, 8.0))],
            "fov_event": "flight.boost_onset_fov_kick_deg (+2 for 0.25 s) - FOV handled by the flight FOV curve, not the shake",
        },
        "supersonic_flight": {
            "description": "transonic buffet peaking at Mach 0.95-1.05, smoother once supersonic; one-shot jolt at the crossing",
            "trigger": "loop with scale = mach_scale(speed_ms / 343); jolt when crossing Mach 1",
            "mach_scale_curve": [{"mach": 0.0, "scale": 0.0}, {"mach": 0.7, "scale": 0.0}, {"mach": 0.9, "scale": 0.7},
                                 {"mach": 1.0, "scale": 1.3}, {"mach": 1.05, "scale": 1.0}, {"mach": 1.3, "scale": 0.5},
                                 {"mach": 3.0, "scale": 0.4}],
            "dynamic_pressure_note": "buffet scales ~ q = 0.5 rho v^2; the curve above is normalised for sea level - multiply by air density ratio for altitude",
            "play_space": "CameraLocal", "single_instance": True,
            "patterns": [perlin(0.0, 0.4, 0.8, Y=ax(1.2, 8.0), Z=ax(1.6, 10.0), Pitch=ax(0.35, 9.0), Yaw=ax(0.22, 7.0), Roll=ax(0.45, 5.0))],
            "crossing_jolt": [wave(0.7, 0.0, 0.5, Z=ax(4.0, 7.0), Pitch=ax(1.1, 6.0), Roll=ax(0.6, 4.5))],
            "reachability": "FlySpeed 13.5 m/s x boost 1.65 x overdrive 1.3 tops out ~29 m/s (Mach 0.085): never fires in gameplay today; use in cutscenes or a future high-speed mode",
        },
        "superhero_landing": {
            "description": "three-point landing: immediate downward hit, pitch nod, short ground rumble",
            "trigger": "on Landed() with vertical speed > 6 m/s (no landing shake hook exists today)",
            "scale_rule": "clamp(vertical_impact_ms / 12, 0.35, 1.5); Hulkbuster x1.4",
            "play_space": "CameraLocal", "single_instance": True,
            "patterns": [wave(0.55, 0.0, 0.35, Z=ax(-7.0, 6.0), Pitch=ax(1.6, 5.5), Roll=ax(0.5, 4.0)),
                         perlin(0.5, 0.0, 0.4, Z=ax(0.8, 18.0), Pitch=ax(0.3, 16.0), Yaw=ax(0.2, 14.0))],
            "sync_with": "Ultron ground-impact VFX/decal on the same frame",
        },
        "repulsor_fire": {
            "description": "recoil: short pitch kick and a push back; it is the arm that recoils so it is small",
            "trigger": "FireRepulsor() each shot",
            "weapon_scale": {"Repulsor": 1.0, "Laser": 0.6, "Sonic": 1.3, "Flamethrower": 0.25, "Frost": 0.7,
                             "Minigun": 0.3, "RedBlast": 1.1, "Hulkbuster": 1.6},
            "rapid_fire_rule": "fire interval < 0.1 s: single_instance restarts, so it reads as a continuous buzz instead of stacking",
            "play_space": "CameraLocal", "single_instance": True, "first_person_rotation_scale": 0.7,
            "patterns": [wave(0.1, 0.0, 0.08, X=ax(-0.6, 14.0), Pitch=ax(0.22, 14.0))],
        },
        "unibeam": {
            "description": "chest beam: push back on ignition, sustained high-frequency vibration for the beam's 0.9 s",
            "trigger": "unibeam fire (duration 0.9 s in IronManCharacter)",
            "play_space": "CameraLocal", "single_instance": True,
            "patterns": [wave(0.25, 0.0, 0.15, X=ax(-2.5, 4.0), Pitch=ax(0.35, 4.0)),
                         perlin(0.9, 0.08, 0.25, X=ax(0.6, 20.0), Pitch=ax(0.25, 18.0), Yaw=ax(0.2, 16.0), Roll=ax(0.2, 12.0))],
            "fov_event": "-2 deg tunnel for 0.9 s (camera FOV, not shake)",
        },
        "hit_light": {
            "description": "small-arms / drone hit",
            "trigger": "damage < 25 (ArenaFeel damage trauma uses clamp(dmg/25, .12, .55))",
            "orient": "flip Yaw/Roll sign toward the hit direction",
            "play_space": "CameraLocal", "single_instance": False,
            "patterns": [wave(0.22, 0.0, 0.15, offset="Zero", Y=ax(1.2, 10.0), Pitch=ax(0.55, 11.0), Yaw=ax(0.35, 9.0), Roll=ax(0.6, 8.0))],
        },
        "hit_heavy": {
            "description": "titan punch / missile hit; pairs with the existing HitStop",
            "trigger": "damage >= 25 or boss melee",
            "orient": "flip Yaw/Roll sign toward the hit direction",
            "play_space": "CameraLocal", "single_instance": False,
            "patterns": [wave(0.55, 0.0, 0.4, Y=ax(3.5, 6.0), Z=ax(2.5, 8.0), Pitch=ax(1.8, 7.0), Yaw=ax(1.0, 6.0), Roll=ax(2.2, 5.0)),
                         perlin(0.6, 0.0, 0.45, Pitch=ax(0.4, 18.0), Yaw=ax(0.3, 16.0))],
        },
        "explosion_nearby": {
            "description": "pressure-wave jolt + low rumble, attenuated with distance and delayed by the speed of sound",
            "trigger": "ArenaBlastFX (Ultron) -> UGameplayStatics::PlayWorldCameraShake(Epicenter, Inner, Outer, Falloff, bOrientShakeTowardsEpicenter=true)",
            "falloff": {"InnerRadius": "blast radius R (cm) - full strength", "OuterRadius": "6 * R + 400 cm (same reach as ArenaFeel::AddBlastTrauma)",
                        "Falloff": 1.6, "size_scale": "clamp(R / 400, 0.1, 1.0) (same as AddBlastTrauma)",
                        "shock_delay_s": "distance_cm / 34300, capped 0.6", "bOrientShakeTowardsEpicenter": True},
            "play_space": "World", "single_instance": False,
            "patterns": [wave(0.5, 0.0, 0.35, Z=ax(4.0, 9.0), Pitch=ax(1.4, 8.0), Roll=ax(1.2, 6.0)),
                         perlin(1.2, 0.05, 0.8, Z=ax(2.0, 3.5), Pitch=ax(0.5, 3.0), Yaw=ax(0.4, 2.5), Roll=ax(0.6, 2.0))],
        },
        "handheld_cutscene": {
            "description": "operator handheld: breathing, slow sway, tiny muscle jitter",
            "trigger": "cutscene shots flagged handheld (replaces StoryDirector's sine handheld)",
            "focal_rule": "rotation scale = sqrt(35 / f_s35equiv): operators steady longer lenses, image shake still grows ~sqrt(f)",
            "play_space": "CameraLocal", "single_instance": True,
            "patterns": [perlin(0.0, 0.5, 0.5, X=ax(0.8, 0.5), Y=ax(0.7, 0.4), Z=ax(0.6, 0.25),
                                Pitch=ax(0.3, 0.45), Yaw=ax(0.4, 0.35), Roll=ax(0.22, 0.3)),
                         perlin(0.0, 0.5, 0.5, Pitch=ax(0.04, 5.0), Yaw=ax(0.04, 4.5))],
        },
    }
    for k, s in S.items():
        pitch = max([abs(p["axes"].get("Pitch", {}).get("Amplitude", 0.0)) for p in s["patterns"]] + [0.0])
        s["arenafeel_trauma_equivalent"] = trauma_eq(pitch)
        s["asset"] = "/Game/Contrib/Vison/Camera/Shakes/CS_" + "".join(w.capitalize() for w in k.split("_"))
        s["peak_px_at_1080p_90deg"] = r(pitch * PX_PER_DEG_1080_90, 1)
    return {
        "metadata": {
            "name": "Vison camera shakes", "version": VERSION, "date": DATE, "generated_by": "gen_camera_realism.py",
            "classes": "CameraShakeBase (EngineCameras plugin) with RootShakePattern = PerlinNoiseCameraShakePattern / "
                       "WaveOscillatorCameraShakePattern; several patterns -> CompositeCameraShakePattern (or one asset per pattern)",
            "units": {"location": "cm, camera-local X fwd / Y right / Z up", "rotation": "degrees (Pitch, Yaw, Roll)",
                      "fov": "degrees", "Frequency_hz": "cycles per second (real units)",
                      "Amplitude": "peak value of the oscillation (Perlin: bound - typical motion ~0.4-0.6 x)"},
            "ue_frequency_conversion": {
                "WaveOscillator": {"factor": 1.0, "rule": "ue_frequency = Frequency_hz (verified 2026-10-30 in UE 5.8 source: FWaveOscillator::Update does offset += dt * Frequency * 2*pi, so Frequency is already Hz)"},
                "PerlinNoise": {"factor": r(1.0 / 0.6, 5), "rule": "ue_frequency = Frequency_hz / 0.6 (1D Perlin with unit lattice peaks near 0.6 cycles per unit)"},
                "verify": "Calibration: play CS_Calibration_1Hz (Wave pitch 1 deg @ 1 Hz) and count 10 peaks in 10 s; if you count ~63, the engine uses Hz - set factor 1.0 in this block and re-run apply.",
            },
            "visibility_rules": {"min_visible_deg": r(0.5 / PX_PER_DEG_1080_90, 3), "max_frequency_hz_gameplay": 20.0,
                                 "why": "below ~0.5 px motion is lost under TSR; above fps/3 (20 Hz at 60 fps) oscillation aliases into a slower wobble"},
            "arenafeel": {"current": "trauma t in 0..1 decaying 1.6/s; offsets = t^2 * ima.ShakeScale * (pitch/yaw 2.4 deg, roll 3 deg, Y/Z 6 cm), Perlin at clock*22",
                          "equivalence": "trauma_equivalent = sqrt(peak_pitch_deg / 2.4)",
                          "integration": "pick ONE driver: either route ArenaFeel AddTrauma -> StartCameraShake(asset, scale) or keep trauma and use these values to retune it - never both at full strength"},
        },
        "accessibility": {
            "intensity_multiplier": {"range": [0.0, 1.5], "default": 1.0,
                                     "applies_to": "PlayerCameraManager::StartCameraShake Scale, PlayWorldCameraShake scale, ima.ShakeScale"},
            "presets": {
                "off":      {"scale": 0.0},
                "reduced":  {"scale": 0.35, "roll_scale": 0.3, "fov_scale": 0.0, "location_scale": 0.5,
                             "loops_disabled": ["idle_hover", "handheld_cutscene"], "high_freq_over_10hz_scale": 0.5,
                             "flight_fov_curve_scale": 0.5},
                "standard": {"scale": 1.0},
                "intense":  {"scale": 1.25},
            },
            "settings_mapping": {"bCameraShake=false": "off", "bHudReducedMotion=true": "reduced",
                                 "proposed": "float CameraShakeIntensity (0-150%) in ArenaGameUserSettings + slider in SettingsWidget"},
            "first_person": {"rotation_scale": 0.6, "location_scale": 1.0,
                             "why": "vestibulo-ocular reflex stabilises the head; full rotation in POV is the main nausea trigger"},
        },
        "calibration": {"CS_Calibration_1Hz": {"patterns": [wave(10.0, 0.0, 0.0, Pitch=ax(1.0, 1.0))]}},
        "shakes": S,
    }


# ----------------------------------------------------------------------------
# textures
# ----------------------------------------------------------------------------
def write_textures(outdir):
    try:
        import numpy as np
        from PIL import Image
    except Exception as e:  # pragma: no cover
        print("WARN textures skipped (needs numpy + Pillow): %s" % e)
        return []
    os.makedirs(outdir, exist_ok=True)
    n = 16
    v = np.arange(n) / (n - 1.0)
    b, g, rr = np.meshgrid(v, v, v, indexing="ij")  # b slow, g, r fast
    def strip(R_, G_, B_):
        img = np.zeros((n, n * n, 3))
        for bi in range(n):
            img[:, bi * n:(bi + 1) * n, 0] = R_[bi]
            img[:, bi * n:(bi + 1) * n, 1] = G_[bi]
            img[:, bi * n:(bi + 1) * n, 2] = B_[bi]
        return Image.fromarray((np.clip(img, 0, 1) * 255.0 + 0.5).astype("uint8"), "RGB")
    files = []
    p = os.path.join(outdir, "T_LUT_Neutral_16.png"); strip(rr, g, b).save(p); files.append(p)
    # display-space teal/orange: shadows toward teal, highlights toward warm, luma preserved
    luma = 0.2126 * rr + 0.7152 * g + 0.0722 * b
    sh = np.clip(1.0 - luma * 2.0, 0, 1) ** 1.5
    hi = np.clip(luma * 2.0 - 1.0, 0, 1) ** 1.5
    R2 = rr - 0.02 * sh + 0.025 * hi
    G2 = g + 0.004 * sh + 0.002 * hi
    B2 = b + 0.03 * sh - 0.035 * hi
    l2 = 0.2126 * R2 + 0.7152 * G2 + 0.0722 * B2
    R2, G2, B2 = R2 + (luma - l2), G2 + (luma - l2), B2 + (luma - l2)
    p = os.path.join(outdir, "T_LUT_TealOrange_Subtle_16.png"); strip(R2, G2, B2).save(p); files.append(p)
    # flare bokeh shapes
    s = 256
    yy, xx = np.mgrid[0:s, 0:s] / (s - 1.0) * 2.0 - 1.0
    streak = np.exp(-(xx / 0.9) ** 2 * 3.0) * np.exp(-(yy / 0.06) ** 2)
    p = os.path.join(outdir, "T_Flare_Anamorphic_Streak.png")
    Image.fromarray((np.clip(streak, 0, 1) * 255 + 0.5).astype("uint8"), "L").save(p); files.append(p)
    ang = np.arctan2(yy, xx); rad = np.hypot(xx, yy); blades = 9
    edge = np.cos(math.pi / blades) / np.cos((ang % (2 * math.pi / blades)) - math.pi / blades)
    edge = 0.85 * edge + 0.15  # slightly rounded blades
    poly = np.clip((edge * 0.92 - rad) / 0.03, 0, 1) * (0.85 + 0.15 * np.clip(rad / 0.92, 0, 1))  # brighter rim (onion-ring free)
    p = os.path.join(outdir, "T_Flare_Bokeh_9Blade.png")
    Image.fromarray((np.clip(poly, 0, 1) * 255 + 0.5).astype("uint8"), "L").save(p); files.append(p)
    return files


def dump(obj, name):
    p = os.path.join(HERE, name)
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(obj, fh, indent=2)
        fh.write("\n")
    return p


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--no-textures", action="store_true")
    a = ap.parse_args(argv)
    L, R = load(LIGHTING), load(REALISM)
    out = [dump(build_camera(L, R), "camera_realism.json"),
           dump(build_grade(L), "color_grade.json"),
           dump(build_shakes(), "camera_shake.json")]
    if not a.no_textures:
        out += write_textures(os.path.join(HERE, "textures"))
    for p in out:
        print("wrote", os.path.relpath(p, HERE))
    return 0


if __name__ == "__main__":
    sys.exit(main())
