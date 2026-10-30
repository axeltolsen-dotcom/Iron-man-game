"""
Builds the PBR master materials + instances the C++ uses for realistic rendering
(ArenaVisuals::GetSurfaceMaterial, see Source/IronManArena/Public/ArenaVisuals.h).

    /Game/Realism/Materials/
      M_CarPaint      suit / robot paint: Clear Coat shading model, metal flakes, edge wear,
                      scorch / battle damage, ember glow. Params: Color, WearColor, Metallic,
                      Roughness, ClearCoat, ClearCoatRoughness, FlakeAmount, FlakeScale,
                      EdgeWear, Scorch, Damage, HeatGlow, EmissiveStrength
      M_SurfacePBR    triplanar (world-aligned) textured PBR for environments: BaseColorMap,
                      NormalMap, ARMMap (AO/Rough/Metal), Color (tint), TileSize, NormalStrength,
                      RoughnessScale, MetallicScale, Wetness (darkens + gloss + puddles on floors)
      M_Glass         translucent surface-lit glass (visors, workshop windows, Hall of Armor cases)
      M_ArcGlow       unlit emissive with fresnel core + pulse (arc reactor, repulsors, eyes)
      M_HeatHaze      refraction shimmer for reactor vents / thrusters
      M_ScorchDecal   deferred decal for blaster impacts (Opacity param fades it out)
      MI_SuitHotRod / MI_SuitGold / MI_RobotMetal / MI_<texture set> / MI_<set>_Wet

Every master keeps the "Color" + "EmissiveStrength" parameters, so the existing paint / flash
code drives them unchanged. Idempotent: deletes and rebuilds each asset.

Headless (what the build box does):
  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<proj>/Content/Python/build_realism_materials.py" -unattended -nullrhi
Report: Saved/Realism/materials_report.txt
"""
import os
import traceback
import unreal

import import_cc0_textures

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
REPORT = os.path.join(PROJECT, "Saved", "Realism", "materials_report.txt")
MAT_DIR = "/Game/Realism/Materials"
TEX_DIR = "/Game/Realism/Textures"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LOG = []


def log(msg):
    LOG.append(msg)


def fresh(name, cls, factory, folder=MAT_DIR):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def scalar(mat, name, value, x, y):
    return expr(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector(mat, name, rgb, x, y):
    return expr(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def custom(mat, code, inputs, x, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="Custom"):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out)
    c.set_editor_property("description", desc)
    return c


def link(a, b, pin, out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        log("  ! link failed -> %s.%s" % (b.get_name(), pin))


def prop(e, p, out=""):
    if not mel.connect_material_property(e, out, p):
        log("  ! property link failed -> %s" % p)


def mask(mat, src, x, y, r=False, g=False, b=False, a=False):
    m = expr(mat, unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=a)
    link(src, m, "")
    return m


def local_position(mat, x, y):
    if hasattr(unreal, "MaterialExpressionLocalPosition"):
        return expr(mat, unreal.MaterialExpressionLocalPosition, x, y)
    # fallback: world position - object position
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, x - 200, y)
    op = expr(mat, unreal.MaterialExpressionObjectPositionWS, x - 200, y + 80)
    sub = expr(mat, unreal.MaterialExpressionSubtract, x, y)
    link(wp, sub, "A")
    link(op, sub, "B")
    return sub


def finish(mat, path):
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    log("OK   " + path)


# ----------------------------------------------------------------------------- car paint
def build_car_paint():
    name = "M_CarPaint"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    color = vector(mat, "Color", (0.55, 0.02, 0.02), -1400, -300)
    wear_c = vector(mat, "WearColor", (0.62, 0.62, 0.64), -1400, -150)
    p = {n: scalar(mat, n, v, -1400, i * 70) for i, (n, v) in enumerate([
        ("Metallic", 0.7), ("Roughness", 0.3), ("ClearCoat", 1.0), ("ClearCoatRoughness", 0.04),
        ("FlakeAmount", 0.35), ("FlakeScale", 1.2), ("EdgeWear", 0.12), ("Scorch", 0.0),
        ("Damage", 0.0), ("HeatGlow", 0.0), ("EmissiveStrength", 0.0)])}
    lp = local_position(mat, -1400, 900)
    wear_n = expr(mat, unreal.MaterialExpressionNoise, -1100, 900, scale=0.035, levels=4,
                  output_min=0.0, output_max=1.0, turbulence=True)
    link(lp, wear_n, "Position")
    scorch_n = expr(mat, unreal.MaterialExpressionNoise, -1100, 1050, scale=0.012, levels=5,
                    output_min=0.0, output_max=1.0, turbulence=False)
    link(lp, scorch_n, "Position")
    ins = ["Color", "WearColor", "P", "FlakeScale", "FlakeAmount", "WearN", "EdgeWear", "ScorchN",
           "Scorch", "Damage", "Metallic", "Roughness", "ClearCoat", "HeatGlow", "Emissive"]
    common = """
float3 q = floor(P * FlakeScale);
float flake = frac(sin(dot(q, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float wear = saturate((WearN - (1.0 - saturate(EdgeWear + Damage * 0.45))) * 9.0);
float burn = saturate((ScorchN - (1.0 - saturate(Scorch + Damage * 0.8))) * 4.0);
"""
    base = custom(mat, common + """
float3 paint = Color * (1.0 + (flake - 0.5) * FlakeAmount * 0.35);
float3 c = lerp(paint, WearColor, wear);
return lerp(c, c * 0.06 + float3(0.012, 0.01, 0.009), burn * 0.92);
""", ins, -700, -200, desc="CarPaint BaseColor")
    surf = custom(mat, common + """
float metal = lerp(Metallic, 1.0, wear);
float rough = lerp(Roughness + (flake - 0.5) * FlakeAmount * 0.12, 0.38, wear);
rough = lerp(rough, 0.85, burn);
float coat = ClearCoat * (1.0 - wear) * (1.0 - burn * 0.85);
return float3(metal, saturate(rough), saturate(coat));
""", ins, -700, 100, desc="CarPaint Metal/Rough/Coat")
    emis = custom(mat, common + """
float ember = burn * (1.0 - burn) * 4.0;  // glows at the edge of the burn
return Color * Emissive + float3(1.0, 0.28, 0.04) * ember * HeatGlow * 20.0;
""", ins, -700, 400, desc="CarPaint Emissive")
    for c in (base, surf, emis):
        link(color, c, "Color"); link(wear_c, c, "WearColor"); link(lp, c, "P")
        link(p["FlakeScale"], c, "FlakeScale"); link(p["FlakeAmount"], c, "FlakeAmount")
        link(wear_n, c, "WearN"); link(p["EdgeWear"], c, "EdgeWear"); link(scorch_n, c, "ScorchN")
        link(p["Scorch"], c, "Scorch"); link(p["Damage"], c, "Damage"); link(p["Metallic"], c, "Metallic")
        link(p["Roughness"], c, "Roughness"); link(p["ClearCoat"], c, "ClearCoat")
        link(p["HeatGlow"], c, "HeatGlow"); link(p["EmissiveStrength"], c, "Emissive")
    prop(base, MP.MP_BASE_COLOR)
    prop(mask(mat, surf, -400, 50, r=True), MP.MP_METALLIC)
    prop(mask(mat, surf, -400, 150, g=True), MP.MP_ROUGHNESS)
    prop(mask(mat, surf, -400, 250, b=True), MP.MP_CUSTOM_DATA0)       # clear coat
    prop(p["ClearCoatRoughness"], MP.MP_CUSTOM_DATA1)                  # clear coat roughness
    prop(emis, MP.MP_EMISSIVE_COLOR)
    finish(mat, MAT_DIR + "/" + name)
    return mat


# ----------------------------------------------------------------------------- triplanar PBR
TRI = """
float3 w = pow(abs(N), 4.0); w /= max(dot(w, 1.0), 1e-4);
float3 p = WP / max(TileSize, 1.0);
"""


def build_surface_pbr(default_set):
    name = "M_SurfacePBR"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("tangent_space_normal", False)  # the triplanar normal is world space
    st = unreal.MaterialSamplerType

    def texparam(pname, suffix, sampler, y):
        tex = unreal.load_asset("%s/%s/T_%s_%s" % (TEX_DIR, default_set, default_set, suffix))
        e = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, y, parameter_name=pname, sampler_type=sampler)
        if tex:
            e.set_editor_property("texture", tex)
        else:
            log("  ! default texture missing for %s (%s)" % (pname, suffix))
        return e

    t_col = texparam("BaseColorMap", "diff", st.SAMPLERTYPE_COLOR, -400)
    t_nrm = texparam("NormalMap", "nor_dx", st.SAMPLERTYPE_NORMAL, -200)
    t_arm = texparam("ARMMap", "arm", st.SAMPLERTYPE_MASKS, 0)
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1400, 200)
    nrm = expr(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 300)
    tile = scalar(mat, "TileSize", 300.0, -1400, 400)
    tint = vector(mat, "Color", (1.0, 1.0, 1.0), -1400, 500)
    wet = scalar(mat, "Wetness", 0.0, -1400, 600)
    rs = scalar(mat, "RoughnessScale", 1.0, -1400, 700)
    ms = scalar(mat, "MetallicScale", 1.0, -1400, 800)
    ns = scalar(mat, "NormalStrength", 1.0, -1400, 900)
    es = scalar(mat, "EmissiveStrength", 0.0, -1400, 1000)

    col = custom(mat, TRI + """
float3 c = Texture2DSample(Tex, TexSampler, p.yz).rgb * w.x
         + Texture2DSample(Tex, TexSampler, p.xz).rgb * w.y
         + Texture2DSample(Tex, TexSampler, p.xy).rgb * w.z;
float wetMask = saturate(Wet * lerp(0.55, 1.0, w.z));
return c * Tint * lerp(1.0, 0.45, wetMask);
""", ["Tex", "WP", "N", "TileSize", "Tint", "Wet"], -800, -300, desc="Triplanar BaseColor")
    arm = custom(mat, TRI + """
float3 a = Texture2DSample(Tex, TexSampler, p.yz).rgb * w.x
         + Texture2DSample(Tex, TexSampler, p.xz).rgb * w.y
         + Texture2DSample(Tex, TexSampler, p.xy).rgb * w.z;
float wetMask = saturate(Wet * lerp(0.55, 1.0, w.z));
float puddle = saturate((wetMask - 0.5) * 2.0) * (1.0 - a.r * 0.5) * w.z;
float rough = lerp(saturate(a.g * RoughScale), 0.06, saturate(wetMask * 0.8 + puddle));
return float3(a.r, rough, saturate(a.b * MetalScale));
""", ["Tex", "WP", "N", "TileSize", "Wet", "RoughScale", "MetalScale"], -800, 0, desc="Triplanar ARM + wetness")
    nor = custom(mat, TRI + """
float2 tx = Texture2DSample(Tex, TexSampler, p.yz).xy * 2 - 1;
float2 ty = Texture2DSample(Tex, TexSampler, p.xz).xy * 2 - 1;
float2 tz = Texture2DSample(Tex, TexSampler, p.xy).xy * 2 - 1;
// whiteout-style blend into world space
float3 nx = float3(N.x, tx.x + N.y, tx.y + N.z);
float3 ny = float3(ty.x + N.x, N.y, ty.y + N.z);
float3 nz = float3(tz.x + N.x, tz.y + N.y, N.z);
float3 n = normalize(nx * w.x + ny * w.y + nz * w.z);
float flat = saturate(Wet * 1.2) * w.z;    // standing water flattens the bumps on floors
return normalize(lerp(N, n, saturate(Strength) * (1.0 - flat)));
""", ["Tex", "WP", "N", "TileSize", "Strength", "Wet"], -800, 300, desc="Triplanar world normal")
    for c, t in ((col, t_col), (arm, t_arm), (nor, t_nrm)):
        link(t, c, "Tex"); link(wp, c, "WP"); link(nrm, c, "N"); link(tile, c, "TileSize"); link(wet, c, "Wet")
    link(tint, col, "Tint")
    link(rs, arm, "RoughScale"); link(ms, arm, "MetalScale")
    link(ns, nor, "Strength")
    prop(col, MP.MP_BASE_COLOR)
    prop(mask(mat, arm, -400, -50, r=True), MP.MP_AMBIENT_OCCLUSION)
    prop(mask(mat, arm, -400, 50, g=True), MP.MP_ROUGHNESS)
    prop(mask(mat, arm, -400, 150, b=True), MP.MP_METALLIC)
    prop(nor, MP.MP_NORMAL)
    emis = expr(mat, unreal.MaterialExpressionMultiply, -400, 400)
    link(tint, emis, "A"); link(es, emis, "B")
    prop(emis, MP.MP_EMISSIVE_COLOR)
    finish(mat, MAT_DIR + "/" + name)
    return mat


# ----------------------------------------------------------------------------- glass / glow / haze / decal
def build_glass():
    name = "M_Glass"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    mat.set_editor_property("two_sided", True)
    color = vector(mat, "Color", (0.6, 0.75, 0.8), -800, -200)
    op = scalar(mat, "Opacity", 0.18, -800, 0)
    rough = scalar(mat, "Roughness", 0.03, -800, 100)
    es = scalar(mat, "EmissiveStrength", 0.0, -800, 200)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -800, 300, exponent=4.0, base_reflect_fraction=0.04)
    opf = custom(mat, "return saturate(Op + F * 0.6);", ["Op", "F"], -400, 0,
                 out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, desc="Glass opacity")
    link(op, opf, "Op"); link(fres, opf, "F")
    emis = expr(mat, unreal.MaterialExpressionMultiply, -400, 200)
    link(color, emis, "A"); link(es, emis, "B")
    metal = expr(mat, unreal.MaterialExpressionConstant, -400, 300, r=0.0)
    spec = expr(mat, unreal.MaterialExpressionConstant, -400, 380, r=0.5)
    prop(color, MP.MP_BASE_COLOR); prop(opf, MP.MP_OPACITY); prop(rough, MP.MP_ROUGHNESS)
    prop(metal, MP.MP_METALLIC); prop(spec, MP.MP_SPECULAR); prop(emis, MP.MP_EMISSIVE_COLOR)
    finish(mat, MAT_DIR + "/" + name)


def build_arc_glow():
    name = "M_ArcGlow"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = vector(mat, "Color", (0.35, 0.9, 1.0), -800, -200)
    es = scalar(mat, "EmissiveStrength", 8.0, -800, 0)
    pulse = scalar(mat, "PulseSpeed", 2.2, -800, 100)
    core = scalar(mat, "CoreWhite", 0.6, -800, 200)
    t = expr(mat, unreal.MaterialExpressionTime, -800, 300)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -800, 400, exponent=2.5, base_reflect_fraction=0.0)
    c = custom(mat, """
float beat = 0.88 + 0.12 * sin(T * Pulse);
float centre = 1.0 - F;                    // faces looking at the camera = hot white core
return (lerp(Color, float3(1, 1, 1), saturate(centre * centre * Core)) * Strength * beat);
""", ["Color", "Strength", "Pulse", "Core", "T", "F"], -400, 0, desc="Arc glow")
    link(color, c, "Color"); link(es, c, "Strength"); link(pulse, c, "Pulse"); link(core, c, "Core")
    link(t, c, "T"); link(fres, c, "F")
    prop(c, MP.MP_EMISSIVE_COLOR)
    finish(mat, MAT_DIR + "/" + name)


def build_heat_haze():
    name = "M_HeatHaze"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    color = vector(mat, "Color", (1.0, 0.45, 0.12), -800, -200)
    strength = scalar(mat, "Strength", 1.0, -800, 0)
    es = scalar(mat, "EmissiveStrength", 0.4, -800, 100)
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -800, 200)
    t = expr(mat, unreal.MaterialExpressionTime, -800, 300)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -800, 400, exponent=1.5, base_reflect_fraction=0.0)
    ior = custom(mat, """
float3 q = WP * 0.02 + float3(0, 0, -T * 3.0);
float n = sin(q.x * 3.1 + sin(q.z * 2.3)) * sin(q.y * 2.7 + q.z * 1.7);
return 1.0 + (0.04 + 0.03 * n) * Strength * (1.0 - F);
""", ["WP", "T", "Strength", "F"], -400, 0, out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, desc="Haze IOR")
    for n, e in (("WP", wp), ("T", t), ("Strength", strength), ("F", fres)):
        link(e, ior, n)
    op = custom(mat, "return 0.04 * Strength * (1.0 - F);", ["Strength", "F"], -400, 200,
                out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, desc="Haze opacity")
    link(strength, op, "Strength"); link(fres, op, "F")
    emis = expr(mat, unreal.MaterialExpressionMultiply, -400, 300)
    link(color, emis, "A"); link(es, emis, "B")
    prop(ior, MP.MP_REFRACTION); prop(op, MP.MP_OPACITY); prop(emis, MP.MP_EMISSIVE_COLOR)
    finish(mat, MAT_DIR + "/" + name)


def build_scorch_decal():
    name = "M_ScorchDecal"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    color = vector(mat, "Color", (0.02, 0.018, 0.016), -800, -200)
    op = scalar(mat, "Opacity", 1.0, -800, 0)
    glow = scalar(mat, "EmissiveStrength", 0.0, -800, 100)
    uv = expr(mat, unreal.MaterialExpressionTextureCoordinate, -800, 200)
    m = custom(mat, """
float2 d = UV - 0.5;
float r = length(d) * 2.0;
float a = atan2(d.y, d.x);
float jag = 0.82 + 0.12 * sin(a * 7.0) + 0.06 * sin(a * 17.0 + 1.3);
return saturate((jag - r) * 4.0) * Op;
""", ["UV", "Op"], -400, 0, out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, desc="Scorch mask")
    link(uv, m, "UV"); link(op, m, "Op")
    em = custom(mat, """
float r = length(UV - 0.5) * 2.0;
return float3(1.0, 0.35, 0.05) * saturate(1.0 - r * 2.2) * Glow;
""", ["UV", "Glow"], -400, 200, desc="Scorch ember")
    link(uv, em, "UV"); link(glow, em, "Glow")
    rough = expr(mat, unreal.MaterialExpressionConstant, -400, 300, r=0.92)
    prop(color, MP.MP_BASE_COLOR); prop(m, MP.MP_OPACITY); prop(rough, MP.MP_ROUGHNESS); prop(em, MP.MP_EMISSIVE_COLOR)
    finish(mat, MAT_DIR + "/" + name)


# ----------------------------------------------------------------------------- instances
def make_mi(name, parent, scalars=None, vectors=None, textures=None):
    mi = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in (textures or {}).items():
        tex = unreal.load_asset(v)
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, k, tex)
        else:
            log("  ! %s: missing texture %s" % (name, v))
    mel.update_material_instance(mi)
    eal.save_asset(MAT_DIR + "/" + name, only_if_is_dirty=False)
    log("OK   %s/%s" % (MAT_DIR, name))


# texture set -> (MI name, tile size cm, tint, roughness scale, also make a wet variant)
SURFACES = [
    ("garage_floor",        "MI_GarageFloor", 260.0, (1.0, 1.0, 1.0), 1.0, False),
    ("asphalt_02",          "MI_Asphalt",     350.0, (1.0, 1.0, 1.0), 1.0, True),
    ("brushed_concrete",    "MI_Concrete",    400.0, (1.0, 1.0, 1.0), 1.0, True),
    ("concrete_block_wall", "MI_BlockWall",   300.0, (1.0, 1.0, 1.0), 1.0, False),
    ("brick_wall_001",      "MI_Brick",       280.0, (1.0, 1.0, 1.0), 1.0, True),
    ("metal_plate",         "MI_MetalPlate",  200.0, (1.0, 1.0, 1.0), 0.9, False),
    ("rusty_metal_02",      "MI_RustyMetal",  220.0, (1.0, 1.0, 1.0), 1.0, False),
    ("rubble",              "MI_Rubble",      320.0, (1.0, 1.0, 1.0), 1.0, False),
    ("burned_ground_01",    "MI_BurnedGround", 450.0, (1.0, 1.0, 1.0), 1.0, False),
]


def main():
    try:
        LOG.extend(["== textures =="] + import_cc0_textures.run())
    except Exception:
        log("texture import crashed:\n" + traceback.format_exc())
    LOG.append("== materials ==")
    steps = [("car paint", build_car_paint), ("glass", build_glass), ("arc glow", build_arc_glow),
             ("heat haze", build_heat_haze), ("scorch decal", build_scorch_decal)]
    for label, fn in steps:
        try:
            fn()
        except Exception:
            log("FAIL %s:\n%s" % (label, traceback.format_exc()))
    try:
        build_surface_pbr("brushed_concrete")
    except Exception:
        log("FAIL surface pbr:\n" + traceback.format_exc())
    LOG.append("== instances ==")
    try:
        paint = unreal.load_asset(MAT_DIR + "/M_CarPaint")
        make_mi("MI_SuitHotRod", paint, {"FlakeAmount": 0.4, "EdgeWear": 0.1},
                {"Color": (0.42, 0.012, 0.01), "WearColor": (0.7, 0.68, 0.66)})
        make_mi("MI_SuitGold", paint, {"Metallic": 1.0, "Roughness": 0.22, "FlakeAmount": 0.25, "EdgeWear": 0.08},
                {"Color": (0.9, 0.6, 0.18), "WearColor": (0.75, 0.72, 0.7)})
        make_mi("MI_RobotMetal", paint, {"Metallic": 0.95, "Roughness": 0.42, "ClearCoat": 0.15, "FlakeAmount": 0.0,
                                         "EdgeWear": 0.3}, {"Color": (0.2, 0.21, 0.24), "WearColor": (0.65, 0.65, 0.68)})
        surf = unreal.load_asset(MAT_DIR + "/M_SurfacePBR")
        for tex_set, mi, tile, tint, rough, wet in SURFACES:
            tx = {"BaseColorMap": "%s/%s/T_%s_diff" % (TEX_DIR, tex_set, tex_set),
                  "NormalMap": "%s/%s/T_%s_nor_dx" % (TEX_DIR, tex_set, tex_set),
                  "ARMMap": "%s/%s/T_%s_arm" % (TEX_DIR, tex_set, tex_set)}
            make_mi(mi, surf, {"TileSize": tile, "RoughnessScale": rough}, {"Color": tint}, tx)
            if wet:
                make_mi(mi + "_Wet", surf, {"TileSize": tile, "RoughnessScale": rough, "Wetness": 0.85}, {"Color": tint}, tx)
    except Exception:
        log("FAIL instances:\n" + traceback.format_exc())
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as fh:
        fh.write("\n".join(LOG) + "\n")


main()
