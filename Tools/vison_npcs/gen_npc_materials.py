"""Generates npc_materials.json (Vison). Offline only; run: python3 gen_npc_materials.py
All colours are LINEAR sRGB/Rec.709 albedo (0..1). Roughness is UE perceptual roughness.
UE 'Specular' input = F0 / 0.08, F0 = ((n-1)/(n+1))^2."""
import json, os

def spec(ior):
    f0 = ((ior - 1.0) / (ior + 1.0)) ** 2
    return round(f0 / 0.08, 3), round(f0, 4)

def lum(c):
    return round(0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2], 4)

SKIN_SPEC, SKIN_F0 = spec(1.40)
HAIR_SPEC, HAIR_F0 = spec(1.55)
CORNEA_SPEC, CORNEA_F0 = spec(1.376)
ENAMEL_SPEC, ENAMEL_F0 = spec(1.62)
FIBRE_SPEC, FIBRE_F0 = spec(1.53)
LEATHER_SPEC, LEATHER_F0 = spec(1.50)
RUBBER_SPEC, RUBBER_F0 = spec(1.52)
ABS_SPEC, ABS_F0 = spec(1.54)
GLASS_SPEC, GLASS_F0 = spec(1.52)

# Game's Career::SkinTones() (CareerTypes.cpp:129) in index order, light -> deep. Kept (they sit inside the
# measured Fitzpatrick range from physicallybased.info), annotated with the nearest measured type.
GAME_TONES = [
    (0.80, 0.62, 0.52), (0.72, 0.52, 0.42), (0.62, 0.43, 0.33), (0.52, 0.34, 0.24),
    (0.40, 0.25, 0.17), (0.28, 0.17, 0.11), (0.18, 0.11, 0.07), (0.12, 0.07, 0.045)]
FITZ = ["I", "I-II", "III", "III-IV", "IV", "V", "V-VI", "VI"]
skin_tones = []
for i, c in enumerate(GAME_TONES):
    deep = i / 7.0
    skin_tones.append({
        "index": i, "albedo_linear": list(c), "luminance_Y": lum(c), "nearest_fitzpatrick": FITZ[i],
        # More melanin = less visible red bleed; scatter distance shrinks slightly, transmission tint darkens.
        "sss_mfp_scale": round(1.0 - 0.18 * deep, 3),
        "sss_transmission_tint_linear": [round(0.85 - 0.45 * deep, 3), round(0.22 - 0.14 * deep, 3), round(0.12 - 0.09 * deep, 3)],
        "redness_flush_strength": round(0.30 - 0.22 * deep, 3),
        "vein_tint_strength": round(0.06 - 0.06 * deep, 3),
    })

measured_skin = {
    "source": "physicallybased.info (linear, total diffuse reflectance incl. subsurface), IOR 1.4",
    "Skin I": [0.847, 0.638, 0.552], "Skin II": [0.799, 0.485, 0.347], "Skin III": [0.623, 0.433, 0.343],
    "Skin IV": [0.436, 0.227, 0.131], "Skin V": [0.283, 0.148, 0.079], "Skin VI": [0.09, 0.05, 0.02]}

# Subsurface profile (Burley). Jensen et al. 2001 'Skin1' diffuse mean free path: R 3.67 mm, G 1.37 mm, B 0.68 mm.
sss_base = {
    "enable_burley": True, "enable_mean_free_path": True,
    "mean_free_path_color_linear": [1.0, round(1.37 / 3.67, 3), round(0.68 / 3.67, 3)],
    # Named unit fields. Values follow a common UE convention but are EXPLICITLY UNVERIFIED in UE 5.8
    # (Jarvis: do not guess; match Epic's current Subsurface Profile docs when an editor read is possible).
    "mean_free_path_distance_mm": 3.67,
    "mean_free_path_distance_unit": "mm",
    "mean_free_path_distance_verified_in_ue_5_8": False,
    "world_unit_scale": 0.1,
    "world_unit_scale_unit": "UE SubsurfaceProfile WorldUnitScale",
    "world_unit_scale_verified_in_ue_5_8": False,
    "world_unit_scale_convention_note": "Common convention: WorldUnitScale 0.1 means MFP distance is in mm when 1 uu = 1 cm. UNVERIFIED in the 5.8 editor tooltip / Epic docs — leave as named fields and confirm before --apply.",
    "surface_albedo_linear": "set per tone from skin_tones[].albedo_linear",
    "tint_linear": [1.0, 1.0, 1.0],
    "boundary_color_bleed_linear": [1.0, 1.0, 1.0],
    "dual_specular": {"roughness0": 0.75, "roughness1": 1.30, "lobe_mix": 0.85,
                       "_note": "UE dual-lobe multipliers on material roughness (engine defaults; sharp oily lobe + broad lobe)."},
    "ior": 1.40, "extinction_scale": 1.0, "normalization_scale": 1.0, "scatter_radius_cm_fallback": 1.2}

# Age groups. Sex does not change skin optics; only pores/hair-follicle density and stubble differ.
age_groups = {
    "child_6_12": {"label": "Child (6-12)", "height_cm": [115, 152], "head_heights": [5.5, 6.5],
        "skin_roughness": 0.50, "pore_normal_strength": 0.15, "wrinkle_normal_strength": 0.0, "cavity_ao": 0.35,
        "sss_mfp_scale": 1.08, "cheek_flush": 1.15, "sclera_linear": [0.78, 0.74, 0.72], "pupil_scale": 1.12,
        "hair_grey_fraction": 0.0, "vellus_peach_fuzz_sheen": 0.06,
        "proportions": "Large head relative to body (child head ~5.5-6.5 heads tall vs adult ~7.5), shorter legs, rounder face, no adult musculature. Requires a child-proportioned mesh: never a uniformly scaled adult mesh."},
    "teen_13_17": {"label": "Teen (13-17)", "height_cm": [150, 185], "head_heights": [6.5, 7.2],
        "skin_roughness": 0.47, "pore_normal_strength": 0.30, "wrinkle_normal_strength": 0.0, "cavity_ao": 0.45,
        "sss_mfp_scale": 1.04, "cheek_flush": 1.05, "sclera_linear": [0.75, 0.70, 0.66], "pupil_scale": 1.06,
        "hair_grey_fraction": 0.0, "oily_t_zone_roughness": 0.36, "blemish_mask_strength": 0.15,
        "proportions": "Near-adult height, slimmer frame, ordinary teen build."},
    "adult_18_64": {"label": "Adult (18-64)", "height_cm_female": [152, 178], "height_cm_male": [163, 193],
        "height_mean_cm": {"female": 162, "male": 176}, "head_heights": [7.3, 7.8],
        "skin_roughness": 0.50, "pore_normal_strength": 0.45, "wrinkle_normal_strength": 0.15, "cavity_ao": 0.55,
        "sss_mfp_scale": 1.0, "cheek_flush": 1.0, "sclera_linear": [0.70, 0.62, 0.56], "pupil_scale": 1.0,
        "hair_grey_fraction": [0.0, 0.35], "oily_t_zone_roughness": 0.38, "stubble_shadow_male": [0.0, 0.6],
        "proportions": "Varied builds: slim / average / heavy / athletic in roughly equal shares."},
    "older_65_plus": {"label": "Older adult (65+)", "height_cm_female": [148, 172], "height_cm_male": [158, 186],
        "head_heights": [7.2, 7.6],
        "skin_roughness": 0.56, "pore_normal_strength": 0.55, "wrinkle_normal_strength": 0.6, "cavity_ao": 0.65,
        "sss_mfp_scale": 0.95, "cheek_flush": 0.85, "age_spot_mask_strength": 0.25,
        "sclera_linear": [0.66, 0.58, 0.48], "pupil_scale": 0.82, "hair_grey_fraction": [0.4, 1.0],
        "proportions": "Slight forward posture is an animation job (note for Jarvis); materials add wrinkles, age spots, thinner hair."}}

face_regions_roughness = {"_note": "Multiply skin_roughness per region (roughness mask or region weights).",
    "forehead_nose_t_zone": 0.78, "cheeks": 1.0, "chin": 0.9, "lips": 0.7, "eyelids": 0.9,
    "ears": 1.05, "hands_back": 1.08, "palms": 1.12, "scalp_under_hair": 1.1}

# Hair: Career::HairColors() index order (CareerTypes.cpp:139). UE Hair shading model (cards) / groom melanin model.
HAIR = [
    ("black", (0.015, 0.012, 0.01), 0.98, 0.10, None),
    ("dark_brown", (0.06, 0.035, 0.02), 0.80, 0.18, None),
    ("brown", (0.16, 0.09, 0.045), 0.60, 0.25, None),
    ("light_brown", (0.45, 0.30, 0.14), 0.42, 0.25, None),
    ("blonde", (0.70, 0.55, 0.32), 0.22, 0.18, None),
    ("auburn_red", (0.35, 0.08, 0.03), 0.50, 0.85, None),
    ("grey_white", (0.55, 0.55, 0.56), 0.08, 0.05, None),
    ("dyed_purple", (0.25, 0.05, 0.35), 0.15, 0.0, (0.25, 0.05, 0.35)),
    ("dyed_blue", (0.04, 0.20, 0.45), 0.15, 0.0, (0.04, 0.20, 0.45)),
    ("dyed_pink", (0.60, 0.15, 0.30), 0.12, 0.0, (0.60, 0.15, 0.30))]
hair_colors = []
for i, (n, c, mel, red, dye) in enumerate(HAIR):
    hair_colors.append({"index": i, "name": n, "card_base_color_linear": list(c), "groom_melanin": mel,
        "groom_redness": red, "groom_dye_color_linear": list(dye) if dye else [1.0, 1.0, 1.0],
        "allowed_age_groups": ["teen_13_17", "adult_18_64", "older_65_plus"] if dye else (
            ["adult_18_64", "older_65_plus"] if n == "grey_white" else ["child_6_12", "teen_13_17", "adult_18_64", "older_65_plus"])})

hair_shading = {
    "ior": 1.55, "f0": HAIR_F0, "specular": HAIR_SPEC, "cuticle_tilt_deg": 3.0,
    "roughness_by_type": {"straight": 0.30, "wavy": 0.36, "curly_coily": 0.45, "short_buzz": 0.40, "grey_coarse": 0.42},
    "scatter": {"dark": 0.0, "brown": 0.2, "light": 0.5, "blonde_grey": 0.8},
    "backlit": 0.35, "cards_opacity_mask_clip": 0.33, "dithered_opacity_mask": True,
    "root_darkening": 0.25, "tip_lightening": 0.10, "per_strand_hue_jitter": 0.04,
    "eyebrows_eyelashes": "Same melanin as scalp hair, roughness +0.05; eyelashes darker (melanin +0.1). Children: lighter, finer brows.",
    "facial_hair": "Adult male only (and only 18+). Stubble = skin-shader overlay (darken albedo x0.85, roughness +0.06, normal noise), beards = cards."}

eyes = {
    "shading_model": "Eye (legacy) or Substrate Eye; refractive cornea, separate wet tear-line strip",
    "cornea": {"ior": 1.376, "f0": CORNEA_F0, "specular": CORNEA_SPEC, "roughness": 0.03, "iris_depth_cm": 0.12, "aqueous_ior": 1.336},
    "sclera_default_linear": [0.70, 0.62, 0.56], "sclera_measured_linear": [0.652, 0.5, 0.394], "sclera_roughness": 0.18,
    "sclera_vein_strength": 0.08, "limbal_ring_darkening": 0.35,
    "iris_colors_linear": {"dark_brown": [0.06, 0.03, 0.015], "brown": [0.14, 0.07, 0.03], "hazel": [0.20, 0.14, 0.06],
                           "green": [0.13, 0.17, 0.07], "blue": [0.12, 0.18, 0.27], "grey": [0.18, 0.19, 0.20]},
    "iris_distribution_hint": {"dark_brown": 0.45, "brown": 0.25, "hazel": 0.10, "blue": 0.12, "green": 0.04, "grey": 0.04},
    "iris_radius_fraction": 0.14, "pupil_diameter_mm": {"formula": "D = clamp(7.0 - 0.33 * EV100, 2.0, 8.0) * age_groups[].pupil_scale",
        "arena_ev100_15_1": round(max(2.0, min(8.0, 7.0 - 0.33 * 15.1)), 2), "city_ev100_15_55": round(max(2.0, 7.0 - 0.33 * 15.55), 2),
        "city_overcast_ev100_12_58": round(7.0 - 0.33 * 12.58, 2), "city_night_ev100_3_93": round(7.0 - 0.33 * 3.93, 2),
        "_note": "Fit to Watson & Yellott 2012 unified formula over 1-10000 cd/m^2 (approx). Driven from scene EV100, not from the auto-exposure result, so eyes react to the world rather than the camera."},
    "caruncle_tearline": {"base_color_linear": [0.55, 0.22, 0.20], "roughness": 0.08, "specular": 0.5},
    "eye_occlusion_shell_opacity": 0.45,
    "catchlight": "No fake catchlight textures; real reflection from Lumen + skylight. Cutscenes add the CineCamera-attached eye light below."}

mouth = {"teeth_enamel": {"base_color_linear": [0.72, 0.68, 0.58], "ior": 1.62, "specular": ENAMEL_SPEC, "roughness": 0.22, "sss_mfp_mm": 1.0},
         "gums_tongue": {"base_color_linear": [0.45, 0.12, 0.10], "roughness": 0.20, "sss_mfp_mm": 2.0},
         "fingernails": {"base_color_linear": [0.62, 0.45, 0.40], "roughness": 0.25, "specular": 0.5}}

def fab(name, rough, fuzz, fuzz_amt, spec_=FIBRE_SPEC, albedo_range=(0.02, 0.85), **kw):
    d = {"roughness": rough, "specular": spec_, "fuzz_color": fuzz, "fuzz_amount": fuzz_amt,
         "albedo_range_linear_Y": list(albedo_range), "metallic": 0.0}
    d.update(kw); return d

cloth = {
    "_shading": "Substrate Slab with Fuzz (or legacy Cloth: FuzzColor + Cloth). 'fuzz_color': 'tint' = albedo desaturated 30% and x1.25; or explicit linear.",
    "_dye_rule": "Career::OutfitColors() are the dye albedos (0.02-0.6 linear) and are already physical; no fabric goes below 0.02 or above 0.85 linear Y.",
    "cotton_tshirt": fab("cotton_tshirt", 0.82, "tint", 0.45, weave_normal_strength=0.25, weave_tiling_cm=0.4),
    "cotton_twill_chino": fab("cotton_twill_chino", 0.80, "tint", 0.35, weave_normal_strength=0.35, weave_tiling_cm=0.6),
    "denim": fab("denim", 0.80, "tint", 0.40, base_color_default_linear=[0.03, 0.05, 0.11], weave_normal_strength=0.45,
                 weave_tiling_cm=0.5, wear_fade_to_linear=[0.22, 0.28, 0.40], wear_on="knees, seat, thigh fronts, hem"),
    "wool_coat_knit": fab("wool_coat_knit", 0.92, "tint", 0.70, weave_normal_strength=0.5, weave_tiling_cm=0.8),
    "polyester_sportswear": fab("polyester_sportswear", 0.48, "tint", 0.10, weave_normal_strength=0.15, weave_tiling_cm=0.2, anisotropy=0.2),
    "nylon_shell_jacket": fab("nylon_shell_jacket", 0.40, "tint", 0.05, weave_normal_strength=0.12, weave_tiling_cm=0.15, anisotropy=0.25),
    "linen_shirt": fab("linen_shirt", 0.88, "tint", 0.40, weave_normal_strength=0.4, weave_tiling_cm=0.5),
    "silk_satin_blouse": fab("silk_satin_blouse", 0.32, "tint", 0.15, anisotropy=0.6, weave_normal_strength=0.08, weave_tiling_cm=0.1),
    "school_uniform_polycotton": fab("school_uniform_polycotton", 0.78, "tint", 0.30, weave_normal_strength=0.25, weave_tiling_cm=0.35,
                                     palette_linear=[[0.03, 0.05, 0.12], [0.55, 0.55, 0.56], [0.65, 0.65, 0.66], [0.18, 0.04, 0.04]]),
    "backpack_nylon_cordura": fab("backpack_nylon_cordura", 0.55, "tint", 0.05, weave_normal_strength=0.6, weave_tiling_cm=0.25),
    "police_uniform_poly": fab("police_uniform_poly", 0.62, "tint", 0.15, base_color_default_linear=[0.012, 0.016, 0.035]),
    "ems_uniform": fab("ems_uniform", 0.66, "tint", 0.15, base_color_default_linear=[0.015, 0.02, 0.04]),
    "hi_vis_vest": fab("hi_vis_vest", 0.60, "tint", 0.10, base_color_default_linear=[0.62, 0.85, 0.02],
                       fluorescence_emissive_fraction_of_daylight=0.08,
                       _fluorescence_note="Fluoresces in daylight only: emissive = albedo * 0.08 * MPC sun illuminance/120000 (needs a scalar from the lighting preset). Off at night.",
                       retroreflective_tape={"base_color_linear": [0.55, 0.55, 0.55], "roughness": 0.35,
                                             "retro_boost_max": 8.0, "_note": "Retro boost scales with dot(V, L) of the headlight/flashlight; sun gives ~none."}),
}

leather_rubber = {
    "leather_smooth_shoe_polished": {"roughness": 0.30, "specular": LEATHER_SPEC, "metallic": 0.0, "base_color_default_linear": [0.05, 0.03, 0.02], "crease_normal_strength": 0.5},
    "leather_jacket_worn": {"roughness": 0.50, "specular": LEATHER_SPEC, "metallic": 0.0, "base_color_default_linear": [0.03, 0.025, 0.022], "edge_wear_lighten": 0.15},
    "suede_nubuck": {"roughness": 0.92, "specular": LEATHER_SPEC, "fuzz_amount": 0.6, "fuzz_color": "tint"},
    "canvas_sneaker_upper": {"roughness": 0.85, "specular": FIBRE_SPEC, "fuzz_amount": 0.3, "dirt_on_toe": 0.25},
    "mesh_sneaker_upper": {"roughness": 0.62, "specular": FIBRE_SPEC, "fuzz_amount": 0.1},
    "rubber_sole": {"roughness": 0.72, "specular": RUBBER_SPEC, "base_color_default_linear": [0.035, 0.033, 0.03], "white_sole_linear": [0.70, 0.68, 0.64], "grime_bottom_up": 0.5}}

props = {
    "phone_glass_front": {"roughness": 0.05, "specular": GLASS_SPEC, "base_color_linear": [0.01, 0.01, 0.012], "screen_on_nits": [200, 500], "_note": "Emissive in nits via ima.EmissiveNitsPerUnit (1000 nits per unit)."},
    "phone_aluminium_back": {"metallic": 1.0, "base_color_linear": [0.912, 0.914, 0.920], "roughness": 0.35, "anodized_tint": "multiply 0.2-0.9 dye"},
    "stainless_steel_watch_badge": {"metallic": 1.0, "base_color_linear": [0.67, 0.64, 0.60], "roughness": 0.22},
    "gold_jewellery_adult": {"metallic": 1.0, "base_color_linear": [1.0, 0.766, 0.336], "roughness": 0.18},
    "abs_plastic_hard_hat_helmet": {"metallic": 0.0, "specular": ABS_SPEC, "roughness": 0.35, "palette_linear": [[0.70, 0.45, 0.02], [0.75, 0.75, 0.72], [0.60, 0.10, 0.02]], "scuff_roughness_add": 0.25},
    "eyeglasses_lens": {"shading": "thin translucent", "ior": 1.50, "roughness": 0.02, "opacity": 0.08},
    "sunglasses_lens": {"shading": "thin translucent", "ior": 1.50, "roughness": 0.02, "transmittance_linear": [0.12, 0.12, 0.10]},
    "glasses_frame_acetate": {"metallic": 0.0, "specular": 0.5, "roughness": 0.25},
    "police_duty_belt_nylon": {"roughness": 0.60, "specular": FIBRE_SPEC, "base_color_linear": [0.02, 0.02, 0.02]},
    "kids_water_bottle_pp": {"roughness": 0.40, "specular": spec(1.49)[0], "palette_linear": [[0.05, 0.25, 0.60], [0.60, 0.08, 0.15], [0.10, 0.45, 0.10]]},
    "paper_coffee_cup": {"roughness": 0.70, "specular": 0.5, "base_color_linear": [0.70, 0.66, 0.58]}}

synthezoid_and_holo = {
    "_note": "Story bodies: Vison / JOCASTA are synthezoids (EStoryBody::Synthezoid); Tony's hologram message is EStoryBody::Hologram. Optional; these are characters, not civilians.",
    "synthezoid_skin": {"shading": "Subsurface profile with short MFP (synthetic tissue)", "mean_free_path_distance_mm": 1.2,
                        "roughness": 0.38, "specular": 0.40, "vibranium_inlay": {"metallic": 1.0, "base_color_linear": [0.62, 0.60, 0.58], "roughness": 0.28},
                        "vison_red_linear": [0.45, 0.06, 0.10], "jocasta_linear": [0.62, 0.72, 0.82]},
    "hologram": {"shading": "unlit additive translucent", "color_linear": [0.25, 0.65, 1.0], "nits": [150, 400],
                 "scanline_period_cm": 0.6, "fresnel_rim_power": 3.0, "flicker_hz": [0, 12], "dither_temporal_aa": True,
                 "_exposure_note": "Holograms are emitters: express in nits so they read the same across EV100 3.9 (night) to 15.6 (noon); at EV100 15+ 400 nits is barely visible in direct sun, which is correct for a projector, so keep cutscene hologram shots in shade or interior."}}

lighting_contexts = {
    "_formula": "E_lux = 2.5 * 2^EV100 (lighting_presets.json); mid-grey 0.18 lands at the exposure key.",
    "arena_15_30_wsw": {"scene_ev100": 15.1, "incident_lux_approx": round(2.5 * 2 ** 15.1), "sun": "DirectionalLight pitch -38 / yaw 330 (arena preset, WSW, 15:30), 5900 K",
        "skin_vs_midgrey_stops": "vs mid-grey 0.18 (albedo ratio): tone 0 +1.9, tone 2 +1.4, tone 7 -1.2 stops. All inside the filmic shoulder/toe; do not raise skin albedo to 'help' darker tones.",
        "key_cues": ["low sun gives side light on faces: SSS red bleed at the terminator (nose, ears)", "ear/finger transmission when backlit by the WSW sun",
                     "long shadows: VSM contact shadow on faces needs r.Shadow.Virtual.SMRT on (already)", "marine haze lifts distant crowds; no material change needed"]},
    "city_day_13_00": {"scene_ev100": 15.55, "incident_lux": 120000, "key_cues": ["top light: eye sockets dark; rely on Lumen bounce from pavement (albedo ~0.25) and the SkyLight", "hats shadow faces"]},
    "city_overcast_09_00": {"scene_ev100": 12.58, "incident_lux": 15360, "key_cues": ["soft light reveals SSS most clearly", "wetness 0.5 from MPC_WorldWeather: hair and cloth darken"]},
    "city_night_01_00": {"scene_ev100": 3.93, "incident_lux": 38, "key_cues": ["sodium/LED street lamps are unshadowed (perf_budget §6): add VSM contact shadows only on the sun/moon", "pupils dilate to ~5.7 mm", "wetness 0.8: skin roughness drops (sweat/rain), cloth darker"]},
    "rooftops_storm": {"key_cues": ["rain 0.9: full wet model below"]}}

wetness = {"source": "MPC_WorldWeather.Wetness (Ultron) 0..1; live per-NPC 'Soak' can max() with it",
    "skin": {"roughness_mult_at_1": 0.55, "albedo_mult_at_1": 0.95},
    "hair": {"roughness_mult_at_1": 0.6, "albedo_mult_at_1": 0.7, "clumping": 0.5},
    "cloth_absorbent": {"albedo_power_at_1": 1.6, "roughness_mult_at_1": 0.75, "_note": "Absorbent fibres darken by roughly albedo^1.6 (Lekner & Dorf); polyester/nylon barely darken (power 1.15) but get glossy (roughness x0.6)."},
    "cloth_synthetic": {"albedo_power_at_1": 1.15, "roughness_mult_at_1": 0.6},
    "leather": {"albedo_mult_at_1": 0.8, "roughness_mult_at_1": 0.5}}

grime_damage = {"_note": "City combat. Dust/ash from MPC_WorldWeather.Dust/Ash; per-NPC 'Grime' for flee/cower near explosions.",
    "dust_albedo_linear": [0.30, 0.28, 0.25], "dust_roughness": 0.9, "ash_albedo_linear": [0.10, 0.10, 0.10],
    "rule": "Grime and dust only. No blood, wounds or injury decals on any NPC.",
    "story_characters": "Morgan (and any future story child) never gets grime, damage, ragdoll or a 'dead' visual state. Street-crowd children are not used."}

crowd_mix = {
    "_note": "Proposal for ACityPedestrian::Randomize (Jarvis owns the code). Numbers are shares of spawned street-crowd civilians. Children are not in this mix (Jarvis, 2026-11-10): story characters only.",
    "sex": {"female": 0.5, "male": 0.5},
    # Renormalised after removing child_6_12 0.08: teen 0.08/0.92, adult 0.70/0.92, 65+ 0.14/0.92.
    "age_share": {"teen_13_17": 0.087, "adult_18_64": 0.761, "older_65_plus": 0.152},
    "_age_share_note": "Previous mix was child 0.08, teen 0.08, adult 0.70, 65+ 0.14. After dropping children, the remaining 0.92 is renormalised to 0.087 + 0.761 + 0.152 = 1.000.",
    "story_characters_only": {
        "applies_to": ["Morgan"],
        "required_if_ever_changed": {
            "_note": "Children stay story characters only (Morgan). Do not add children to the killable street crowd, and do not use shrunken adult meshes as kids. If that ever changes, the following safety rules are required.",
            "rules": [
                "Always with an adult (family group of 2-4), never alone",
                "Daytime presets only (city, city_overcast, arena free play); not at city_night 01:00",
                "Gone (fade out or walk off) when the wanted level is >= 1, a boss/enemy wave starts, or combat is within 60 m",
                "Never damageable, never ragdoll, never targeted; excluded from CityLife::Damage",
                "Ordinary age-appropriate everyday clothing only (t-shirts, hoodies, jeans, shorts to the knee, school uniform, sneakers, backpack, sun hat)",
                "No romantic, sexual or suggestive content, poses, outfits or voice lines; same rule for teens",
                "Requires a real child-proportioned body; never a uniformly scaled adult mesh",
            ],
        },
        "outfit_sets": {
            "kid_everyday": {"ages": ["child_6_12"], "pieces": ["cotton_tshirt", "denim|cotton_twill_chino(knee shorts)", "canvas_sneaker_upper", "backpack_nylon_cordura", "kids_water_bottle_pp"]},
            "kid_school": {"ages": ["child_6_12", "teen_13_17"], "pieces": ["school_uniform_polycotton", "backpack_nylon_cordura", "rubber_sole"]},
        },
        "hair_styles": {
            "girls_child": ["Long", "Ponytail", "Bun", "Curly", "Short"],
            "boys_child": ["Short", "Curly", "Buzz"],
        },
    },
    "all_ages_clothing_rule": "Ordinary everyday city/Florida clothing for everyone. No revealing or sexualised outfits for any NPC; swimwear is not used in the street crowd.",
    "outfit_sets": {
        "office": {"ages": ["adult_18_64", "older_65_plus"], "pieces": ["cotton_twill_chino", "linen_shirt|silk_satin_blouse", "leather_smooth_shoe_polished", "wool_coat_knit(overcast only)"]},
        "casual": {"ages": ["teen_13_17", "adult_18_64", "older_65_plus"], "pieces": ["cotton_tshirt", "denim", "canvas_sneaker_upper", "rubber_sole"]},
        "sport": {"ages": ["teen_13_17", "adult_18_64"], "pieces": ["polyester_sportswear", "mesh_sneaker_upper"]},
        "tourist": {"ages": ["adult_18_64", "older_65_plus"], "pieces": ["linen_shirt", "cotton_twill_chino(shorts)", "sunglasses_lens", "abs sun visor"]},
        "worker": {"ages": ["adult_18_64"], "pieces": ["hi_vis_vest", "denim", "abs_plastic_hard_hat_helmet", "leather work boots"]},
        "police": {"ages": ["adult_18_64"], "pieces": ["police_uniform_poly", "police_duty_belt_nylon", "stainless_steel_watch_badge"]},
        "ems": {"ages": ["adult_18_64"], "pieces": ["ems_uniform", "hi_vis_vest(tape only)"]},
        "teen_everyday": {"ages": ["teen_13_17"], "pieces": ["cotton_tshirt|hoodie(cotton fleece)", "denim", "mesh_sneaker_upper", "backpack_nylon_cordura"]}},
    "hair_styles_by_group": {
        "teen_girls": ["Long", "Ponytail", "Bun", "Curly", "Short"], "teen_boys": ["Short", "Curly", "Buzz"],
        "women": ["Long", "Ponytail", "Bun", "Curly", "Short"], "men": ["Short", "Buzz", "Curly", "Bald"],
        "older_women": ["Short", "Bun", "Curly"], "older_men": ["Short", "Bald", "Buzz"],
        "_note": "EAvatarHair names for the street crowd (teens, adults, 65+). Bald and facial hair: adult men only. Grey hair (index 6) weighted by age_groups[].hair_grey_fraction. Dyed colours (7-9) teen+ and rare (<=6%). Child styles live under story_characters_only."}}

scalability = {
    "Epic": {"cvars": {"r.SSS.Quality": 1, "r.SSS.HalfRes": 0, "r.SSS.Burley.Quality": 1, "r.SSS.Checkerboard": 0},
             "material_quality": "High: detail pores/weave normals, dual-lobe spec, eye refraction, hair cards LOD0",
             "cutscene_groom_strands": "allowed for story MetaHumans only", "crowd_lod_bias": 0},
    "High": {"cvars": {"r.SSS.Quality": 0, "r.SSS.HalfRes": 1, "r.SSS.Burley.Quality": 1, "r.SSS.Checkerboard": 1},
             "material_quality": "High (same graph); detail normals fade out beyond 8 m", "cutscene_groom_strands": "off; hair cards (story MetaHumans LOD1)", "crowd_lod_bias": 1,
             "_default_tier": True},
    "Medium": {"cvars": {"r.SSS.Quality": 0, "r.SSS.HalfRes": 1, "r.SSS.Checkerboard": 1, "r.SSS.Burley.Quality": 0},
               "material_quality": "Medium: no detail normal, single spec lobe, eye refraction kept (cheap, big realism win)", "crowd_lod_bias": 2},
    "Low": {"cvars": {"r.SSS.Scale": 0}, "material_quality": "Low: skin falls back to default lit with wrap-diffuse tint", "crowd_lod_bias": 3},
    "_ini_target": "DefaultScalability.ini [ShadingQuality@N] (Jarvis merges). Check overlap with Tools/vison_camera / Content/Story scalability notes and Ultron's overlays before merge. r.SSS.* cvars apply to hero/close-up shading only; the CityKit street crowd is default-lit and does not use Subsurface Profile."}

perf = {
    "target": "RTX 2070 SUPER, 1080p output, High tier, TSR (75% / DynRes 67-100%), 16.67 ms frame",
    "estimates_ms_high_citykit_crowd_default_lit": {
        "12 skinned civilians (GPU skin cache, ~25-40k tris each LOD1)": 0.25,
        "base pass + velocity for character pixels (default lit; no SSS, eye, or hair shading models)": 0.18,
        "VSM shadow pages for characters": 0.12},
    "sum_ms_high_gameplay_crowd": 0.55,
    "estimates_ms_high_hero_closeup": {
        "SSS Burley, half-res + checkerboard, hero skin pixels only": 0.12,
        "hair cards (masked, dithered)": 0.10,
        "eye refraction / translucency (eyes, glasses)": 0.03},
    "cutscene_story_metahuman_closeup_ms": {"High (LOD1, cards)": 1.6, "Epic (LOD0, strands)": 3.5},
    "sum_ms_high_gameplay": 0.55,
    "medium_sum_ms_crowd": 0.45, "epic_sum_ms_gameplay_crowd": 0.7,
    "_status": "All [E] estimates, not measured. Jarvis (2026-11-10): do not spend ~0.8 ms of SSS on all 12 crowd peds on the RTX 2070 SUPER; CityKit stays on cheaper default-lit shading; no characters line was requested from Ultron's 13.5 ms High budget. Hero SSS is for close-ups / cinematics only (combat VFX usually off). Measure with profilegpu (perf_budget §11)."}

camera_post = {
    "exposure": "Auto exposure must not chase faces: keep Vison camera_realism metering (histogram, 3 s eye-adapt). For cutscene close-ups use manual EV100 = preset scene_ev100 so skin reads the same shot to shot.",
    "color_grade": "PhotorealGrade caps (sat <= 1, CA 0) protect skin. Split-tone looks are <= 4% per channel (color_grade.json) so skin hue stays true; do not add a skin-only grade.",
    "sharpen": "r.Tonemapper.Sharpen 0.5 can make pores crunchy at distance: pore normals fade out beyond 8 m (material) rather than lowering global sharpen.",
    "motion_blur": "Per-object velocity on all skinned meshes (r.SkinCache + output velocity) so walking legs don't smear like static mesh. Already on via skin cache; verify bOutputVelocity on NPC components.",
    "dof": "Cutscene CineCamera: 35-85 mm, f/2.0-2.8, focus on the near eye (head socket + 8 cm forward). Requires the GetFocusPoint head-socket fix Jocasta proposed.",
    "cutscene_eye_light": {"type": "rect light attached to CineCamera", "intensity_lux_at_subject": 40, "temperature_k": 5600, "cast_shadows": False,
                           "affects": "lighting channel 2 (characters only)", "_note": "Real film 'eye light': adds catchlight without changing the scene look. Unshadowed, so it fits perf_budget's no-shadow rule."},
    "film_grain": "Cutscene grain 0.15 (PhotorealGrade) hides hair-card dithering; keep."}

shading_policy = {
    "citykit_street_crowd": {
        "applies_to": "CityKit / Quaternius / any AvatarCatalog Tier < 2 pedestrian",
        "shading": "default_lit_mis_only",
        "allow_subsurface_profile": False, "allow_eye_shading": False, "allow_hair_shading": False, "allow_sss": False,
        "skin_mis": "MI_NPC_CrowdSkin_T0..T7 (default-lit, Tint x TintMultiplier=1, no SP_Skin assignment)",
        "also_allowed": ["cloth_mis", "leather_rubber_mis", "prop_mis"],
        "forbidden": ["subsurface_profiles", "hero skin_mis", "hair_mis", "eye_mis"]},
    "hero_closeup": {
        "applies_to": ["player", "Harley", "Morgan", "cinematics"],
        "shading": "subsurface_profile_sss_eye_hair",
        "allow_subsurface_profile": True, "allow_eye_shading": True, "allow_hair_shading": True,
        "profiles": "SP_Skin_T0..T7, SP_Skin_Child, SP_Synthezoid — hero / close-up only",
        "skin_mis": "MI_NPC_Skin_<Age>_T<n>"}}

skin_parent_scan = {
    "do_not_hardcode": "/Game/Characters/Kevin/...",
    "roots": ["/Game/Kevin", "/Game/CharacterCreator", "/Game/Reallusion", "/Game/MetaHumans/<Name>/"],
    "path_tokens": ["Kevin", "CharacterCreator", "Reallusion", "CC_Base"],
    "reference": "Source/IronManArena/Private/PilotAvatar.cpp (AvatarCatalog) and Docs/avatar_characters.md"}

data = {
    "version": 2, "schemaVersion": "vison.npcs.v1.1", "generatedBy": "Tools/vison_npcs/gen_npc_materials.py",
    "colorSpace": "linear Rec.709 albedo; roughness = UE perceptual; Specular = F0/0.08",
    "target": "UE 5.8, Substrate Blendable GBuffer (r.Substrate=1, ProjectGBufferFormat=0), Lumen SW, VSM, TSR",
    "output_folder": "/Game/Vison/NPCs",
    "consistency": {"suits": "Content/Story/suit_materials.json (same colour space, same NITS_PER_UNIT=1000)",
                    "lighting": "Content/Story/lighting_presets.json (EV100 15.1)",
                    "camera": "Tools/vison_camera / Content/Story/camera_realism.json",
                    "weather": "Ultron MPC_WorldWeather.Wetness/Dust/Ash", "game_tables": "Career::SkinTones / HairColors / OutfitColors (CareerTypes.cpp) index-compatible"},
    "shading_policy": shading_policy,
    "skin_parent_scan": skin_parent_scan,
    "ior_specular": {"skin": [1.40, SKIN_F0, SKIN_SPEC], "hair": [1.55, HAIR_F0, HAIR_SPEC], "cornea": [1.376, CORNEA_F0, CORNEA_SPEC],
                     "enamel": [1.62, ENAMEL_F0, ENAMEL_SPEC], "textile_fibre": [1.53, FIBRE_F0, FIBRE_SPEC], "leather": [1.50, LEATHER_F0, LEATHER_SPEC],
                     "rubber": [1.52, RUBBER_F0, RUBBER_SPEC], "abs": [1.54, ABS_F0, ABS_SPEC], "glass": [1.52, GLASS_F0, GLASS_SPEC], "_columns": ["ior", "f0", "ue_specular"]},
    "measured_skin_reference": measured_skin,
    "skin": {"shading_model": "Subsurface Profile (Substrate: Slab SSS profile)", "specular": SKIN_SPEC, "metallic": 0.0,
             "base_roughness": 0.50, "subsurface_profile_base": sss_base, "tones": skin_tones,
             "face_region_roughness": face_regions_roughness, "detail_normal_fade_m": [4.0, 8.0],
             "tint_param_note": "PilotAvatar writes the tone into Tint/BaseColor/Color on every MID. Skip that skin-tone multiply on AvatarCatalog Tier >= 2 (MetaHuman / Kevin): see npc_pilotavatar_skin_tint.patch. Our skin MIs expose 'Tint' as the albedo for untextured CityKit bodies, and 'TintMultiplier' which must default to 1 so textured MetaHuman/Kevin skin stays unscaled if a tint path is ever added."},
    "age_groups": age_groups, "hair": {"shading": hair_shading, "colors": hair_colors}, "eyes": eyes, "mouth_nails": mouth,
    "cloth": cloth, "leather_rubber": leather_rubber, "props": props, "synthezoid_and_hologram": synthezoid_and_holo,
    "wetness": wetness, "grime": grime_damage, "crowd_mix": crowd_mix, "lighting_contexts": lighting_contexts,
    "scalability": scalability, "performance": perf, "camera_post": camera_post,
    "instances": {
        "_note": "What apply_npc_materials.py creates under /Game/Vison/NPCs (dry-run by default). usage=hero gets profiles + hero skin/hair/eye + shared cloth; usage=crowd gets default-lit crowd skin + shared cloth/props only (no SP, eye, hair, or SSS).",
        "subsurface_profiles": ["SP_Skin_T%d" % i for i in range(8)] + ["SP_Skin_Child", "SP_Synthezoid"],
        "skin_mis": ["MI_NPC_Skin_%s_T%d" % (a, i) for a in ["Child", "Teen", "Adult", "Older"] for i in range(8)],
        "crowd_default_lit_skin_mis": ["MI_NPC_CrowdSkin_T%d" % i for i in range(8)],
        "hair_mis": ["MI_NPC_Hair_%s" % h[0] for h in HAIR],
        "eye_mis": ["MI_NPC_Eye_%s" % k for k in ["dark_brown", "brown", "hazel", "green", "blue", "grey"]],
        "cloth_mis": ["MI_NPC_Cloth_%s" % k for k in cloth if not k.startswith("_")],
        "leather_rubber_mis": ["MI_NPC_%s" % k for k in leather_rubber],
        "prop_mis": ["MI_NPC_Prop_%s" % k for k in props],
        "usage": {"hero_closeup": ["subsurface_profiles", "skin_mis", "hair_mis", "eye_mis", "cloth_mis", "leather_rubber_mis", "prop_mis"],
                  "citykit_street_crowd": ["crowd_default_lit_skin_mis", "cloth_mis", "leather_rubber_mis", "prop_mis"]}}}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "npc_materials.json")
with open(out, "w") as f:
    json.dump(data, f, indent=2)
print("wrote", out)
