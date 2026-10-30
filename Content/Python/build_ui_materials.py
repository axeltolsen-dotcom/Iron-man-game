"""
Build the helmet-HUD UI materials from Edith's hud_spec.md §15.2 (headless):

  UnrealEditor-Cmd <proj> -run=pythonscript -script="<proj>/Content/Python/build_ui_materials.py" -unattended -nullrhi

Creates /Game/UI/Materials/M_HUD_<X> (UI domain, translucent) + MI_HUD_<X> with exactly the
parameter names in the spec. Each material is one Custom HLSL node returning float4(rgb, alpha);
UV = widget brush UV, Time = real time. Report: Saved/UI/ui_materials_report.txt.
"""
import os
import unreal

DIR = "/Game/UI/Materials"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LOG = []

COMMON = """
#define HASH(p) frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453)
float2 c = UV - 0.5;
"""

# name: (scalars, vectors, textures, hlsl)  -- hlsl must `return float4(rgb, a);`
MATS = {
    "VisorWarp": (
        dict(BarrelK1=0.06, BarrelK2=0.02, OverscanScale=1.03, ChromaticStart=0.6, ChromaticEdgePx=2.5, BloomIntensity=0.35,
             BloomRadiusPx=3.0, ScanlineDensity=540.0, ScanlineIntensity=0.06, NoiseIntensity=0.03, NoiseFPS=24.0, EdgeFade=0.06,
             GlitchAmount=0.0, TearCount=3.0, TearSeed=0.0, VignetteIntensity=0.25),
        dict(VignetteColor=(0.0, 0.0, 0.0), PersonaTint=(0.11, 0.9, 1.0)), ["Texture"],
        """
float r2 = dot(c, c);
float2 uv = 0.5 + c * (1.0 + BarrelK1 * r2 + BarrelK2 * r2 * r2) / OverscanScale;
float band = floor(UV.y * max(1.0, TearCount * 4.0));
float tear = step(1.0 - GlitchAmount * 0.5, HASH(float2(band, floor(Time * 12.0) + TearSeed)));
uv.x += tear * (HASH(float2(band, TearSeed)) - 0.5) * 0.06 * GlitchAmount;
float ca = saturate((sqrt(r2) * 2.0 - ChromaticStart) / max(0.01, 1.0 - ChromaticStart)) * ChromaticEdgePx / 1920.0;
float4 s = Texture2DSample(Texture, TextureSampler, uv);
float rr = Texture2DSample(Texture, TextureSampler, uv + c * ca).r;
float bb = Texture2DSample(Texture, TextureSampler, uv - c * ca).b;
float3 col = float3(rr, s.g, bb);
float2 px = BloomRadiusPx / float2(1920.0, 1080.0);
float3 bloom = 0;
bloom += Texture2DSample(Texture, TextureSampler, uv + float2(px.x, 0)).rgb;
bloom += Texture2DSample(Texture, TextureSampler, uv - float2(px.x, 0)).rgb;
bloom += Texture2DSample(Texture, TextureSampler, uv + float2(0, px.y)).rgb;
bloom += Texture2DSample(Texture, TextureSampler, uv - float2(0, px.y)).rgb;
col += bloom * 0.25 * BloomIntensity * PersonaTint;
col *= 1.0 - ScanlineIntensity * (0.5 + 0.5 * sin(UV.y * ScanlineDensity * 6.2831));
col += (HASH(UV * 1000.0 + floor(Time * NoiseFPS)) - 0.5) * NoiseIntensity;
float edge = saturate(min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y)) / max(0.001, EdgeFade));
float vig = saturate(r2 * 2.0) * VignetteIntensity;
col = lerp(col, VignetteColor, vig);
return float4(col, s.a * edge);
"""),
    "VisorFrame": (dict(GlintIntensity=0.25, EdgeDarkness=0.85), dict(FrameColor=(0.02, 0.02, 0.025)), [],
        """
float2 a = abs(c) * 2.0;
float rim = smoothstep(0.82, 1.0, max(a.x * 0.92, a.y)) + smoothstep(0.9, 1.05, length(c * float2(1.6, 2.0)));
float glint = pow(saturate(1.0 - abs(UV.x + UV.y - 0.35 - frac(Time * 0.05) * 2.0) * 8.0), 4.0) * rim;
return float4(FrameColor + glint * GlintIntensity, saturate(rim) * EdgeDarkness);
"""),
    "Vignette": (dict(VignetteIntensity=0.0, Pulse=0.0), dict(VignetteColor=(1.0, 0.04, 0.04)), [],
        """
float r = length(c * float2(1.3, 1.0)) * 1.6;
float p = 1.0 - Pulse * (0.5 + 0.5 * sin(Time * 8.0));
return float4(VignetteColor, saturate(smoothstep(0.55, 1.1, r) * VignetteIntensity * p));
"""),
    "Scanlines": (dict(ScanlineDensity=540.0, ScanlineIntensity=0.05, NoiseIntensity=0.03, NoiseFPS=24.0), {}, [],
        """
float s = (0.5 + 0.5 * sin(UV.y * ScanlineDensity * 6.2831)) * ScanlineIntensity;
float n = HASH(UV * 900.0 + floor(Time * NoiseFPS)) * NoiseIntensity;
return float4(float3(n, n, n) * 4.0, saturate(s + n));
"""),
    "Glitch": (dict(GlitchAmount=0.0, TearCount=4.0, TearSeed=0.0, BlockSize=24.0, Desaturate=0.0, Blackout=0.0), {}, [],
        """
float t = floor(Time * 15.0) + TearSeed;
float2 blk = floor(UV * float2(1920.0, 1080.0) / max(1.0, BlockSize));
float on = step(1.0 - GlitchAmount * 0.35, HASH(blk + t));
float band = step(1.0 - GlitchAmount * 0.3, HASH(float2(floor(UV.y * TearCount * 6.0), t)));
float3 col = lerp(float3(0.9, 0.1, 0.2), float3(0.1, 0.9, 1.0), HASH(blk * 1.7 + t));
col = lerp(col, dot(col, float3(0.3, 0.59, 0.11)).xxx, Desaturate);
float a = saturate((on + band * 0.6) * GlitchAmount);
return float4(col * (1.0 - Blackout), saturate(a + Blackout));
"""),
    "Gauge": (dict(Fill=0.5, SegmentCount=20.0, WarnStart=0.7, CritStart=0.9, RecoverMarker=0.4, Pulse=0.0, Jitter=0.0, Dropout=0.0, Chevrons=0.0),
        dict(FillColor=(0.11, 0.9, 1.0), BackColor=(0.02, 0.05, 0.07), GlowColor=(0.4, 0.95, 1.0)), [],
        """
float x = UV.x + (HASH(float2(floor(Time * 30.0), UV.y)) - 0.5) * Jitter * 0.02;
float seg = frac(x * SegmentCount);
float gap = step(0.12, seg) * step(seg, 0.9);
float drop = step(Dropout, HASH(float2(floor(x * SegmentCount), floor(Time * 10.0))));
float filled = step(x, Fill) * gap * drop;
float3 fc = FillColor;
fc = lerp(fc, float3(1.0, 0.58, 0.07), step(WarnStart, x));
fc = lerp(fc, float3(1.0, 0.04, 0.04), step(CritStart, x));
float chev = Chevrons * step(0.5, frac((UV.x - UV.y * 0.5) * 12.0 - Time * 2.0));
float pulse = 1.0 + Pulse * 0.5 * sin(Time * 10.0);
float marker = (1.0 - smoothstep(0.0, 0.006, abs(UV.x - RecoverMarker))) * step(0.001, RecoverMarker);
float3 col = lerp(BackColor, fc * pulse + GlowColor * 0.15, filled) + chev * fc * 0.3 + marker;
return float4(col, saturate(0.35 + filled * 0.65 + marker));
"""),
    "Bracket": (dict(Progress=0.0, CornerLength=0.28, Thickness=0.06, State=0.0, Pulse=0.0), dict(Color=(0.11, 0.9, 1.0)), [],
        """
float2 a = abs(c) * 2.0;
float edge = step(1.0 - Thickness, max(a.x, a.y));
float corner = step(1.0 - CornerLength - Progress * 0.4 * step(State, 0.5), min(a.x, a.y));
float lockedRing = step(1.5, State) * (1.0 - smoothstep(0.0, Thickness * 0.5, abs(length(c) * 2.0 - 0.75)));
float p = 1.0 + Pulse * 0.6 * sin(Time * 12.0);
float3 col = lerp(Color, float3(1.0, 0.1, 0.1), step(1.5, State));
return float4(col * p, saturate(edge * corner + lockedRing));
"""),
    "Reticle": (dict(LockProgress=0.0, Spread=0.0), dict(Color=(0.11, 0.9, 1.0)), [],
        """
float r = length(c) * 2.0;
float ang = atan2(c.y, c.x) / 6.2831 + 0.5;
float ring = 1.0 - smoothstep(0.0, 0.03, abs(r - 0.55 - Spread * 0.3));
float ticks = step(0.92, frac(ang * 4.0 + 0.125)) * step(0.3, r) * step(r, 0.45);
float dot0 = 1.0 - smoothstep(0.03, 0.06, r);
float lockArc = (1.0 - smoothstep(0.0, 0.04, abs(r - 0.8))) * step(ang, LockProgress);
return float4(lerp(Color, float3(1.0, 0.2, 0.15), step(0.999, LockProgress)), saturate(ring * 0.6 + ticks + dot0 + lockArc));
"""),
    "Radar": (dict(GridAlpha=0.25, SweepAngle=0.0, RingCount=3.0), dict(Tint=(0.11, 0.9, 1.0)), [],
        """
float r = length(c) * 2.0;
float inside = step(r, 1.0);
float rings = (1.0 - smoothstep(0.0, 0.02, abs(frac(r * RingCount) - 0.5) - 0.48)) * GridAlpha;
float cross0 = (1.0 - smoothstep(0.0, 0.006, min(abs(c.x), abs(c.y)))) * GridAlpha;
float ang = frac(atan2(c.x, -c.y) / 6.2831 - SweepAngle / 360.0);
float sweep = pow(saturate(1.0 - ang * 5.0), 2.0) * 0.5;
float rim = 1.0 - smoothstep(0.0, 0.02, abs(r - 0.99));
return float4(Tint, saturate((rings + cross0 + sweep + rim) * inside + 0.08 * inside));
"""),
    "Glyph": (dict(GlyphIndex=0.0, Speaking=0.0, Pulse=0.0, Morph=0.0), dict(Color=(0.11, 0.9, 1.0)), [],
        """
float r = length(c) * 2.0;
float ang = atan2(c.y, c.x);
float g = 0;
float s = 1.0 + Speaking * 0.15 * sin(Time * 18.0);
if (GlyphIndex < 0.5)      { g = (1.0 - smoothstep(0.0, 0.04, abs(frac(r * 3.0 / s) - 0.5) - 0.42)) * step(r, 0.95); }   // JARVIS rings
else if (GlyphIndex < 1.5) { float h = max(abs(c.x) * 1.73 + abs(c.y), abs(c.y) * 2.0) * 2.0; g = (1.0 - smoothstep(0.0, 0.05, abs(h - 0.8 * s))) * step(0.04, abs(c.x)); } // FRIDAY split hex
else if (GlyphIndex < 2.5) { float d = (abs(c.x) + abs(c.y)) * 2.0; float legs = step(0.9, frac(ang / 6.2831 * 8.0 + 0.5)) * step(0.3, r) * step(r, 0.95); g = (1.0 - smoothstep(0.0, 0.04, abs(d - 0.5 * s))) + legs; } // EDITH diamond
else if (GlyphIndex < 3.5) { float eye = 1.0 - smoothstep(0.0, 0.05, abs(length(c * float2(1.0, 3.0)) * 2.0 - 0.6 * s)); g = eye + (1.0 - smoothstep(0.08, 0.12, r)); } // ULTRON eye
else                       { float2 a = abs(c) * 2.0; g = (1.0 - smoothstep(0.0, 0.04, abs(max(a.x, a.y) - 0.7 * s))) + step(max(a.x, a.y), 0.2); } // WAR MACHINE square
float p = 1.0 + Pulse * 0.4 * sin(Time * 6.0);
return float4(Color * p, saturate(g) * (1.0 - Morph * 0.5));
"""),
    "IntegrityRegion": (dict(Damage=0.0, Hatch=0.0, Flash=0.0), dict(Color=(0.11, 0.9, 1.0)), [],
        """
float2 a = abs(c) * 2.0;
float box = 1.0 - smoothstep(0.85, 0.95, max(a.x, a.y));
float outline = box - (1.0 - smoothstep(0.7, 0.8, max(a.x, a.y)));
float hatch = Hatch * step(0.5, frac((UV.x + UV.y) * 10.0));
float3 col = lerp(Color, float3(1.0, 0.58, 0.07), saturate(Damage * 2.0));
col = lerp(col, float3(1.0, 0.04, 0.04), saturate(Damage * 2.0 - 1.0));
col += Flash;
return float4(col, saturate(outline + box * (0.25 + 0.5 * Damage) + hatch * box * 0.4));
"""),
    "CommsFeed": (dict(PixelSize=2.0, Desaturate=0.2, ScanlineIntensity=0.15, SignalLoss=0.0, Jitter=0.0, Speaking=0.0),
        dict(Tint=(1.0, 0.75, 0.45)), ["Feed"],
        """
float2 res = float2(256.0, 144.0) / max(1.0, PixelSize);
float2 uv = (floor(UV * res) + 0.5) / res;
uv.x += (HASH(float2(floor(UV.y * 40.0), floor(Time * 20.0))) - 0.5) * Jitter * 0.03;
float3 col = Texture2DSample(Feed, FeedSampler, uv).rgb;
col = lerp(col, dot(col, float3(0.3, 0.59, 0.11)).xxx, Desaturate) * Tint;
col *= 1.0 - ScanlineIntensity * (0.5 + 0.5 * sin(UV.y * 144.0 * 6.2831));
float loss = step(1.0 - SignalLoss, HASH(floor(UV * float2(32.0, 18.0)) + floor(Time * 12.0)));
col = lerp(col, HASH(UV * 500.0 + Time).xxx, loss);
float2 a = abs(c) * 2.0;
float frame = step(0.94, max(a.x, a.y)) * (0.6 + 0.4 * Speaking);
return float4(col + frame * Tint, 1.0);
"""),
    "Bar": (dict(Fill=1.0, Invulnerable=0.0), dict(FillColor=(1.0, 0.04, 0.04), BackColor=(0.05, 0.01, 0.01)), [],
        """
float filled = step(UV.x, Fill);
float shine = 0.15 * (1.0 - abs(UV.y - 0.3) * 3.0);
float3 col = lerp(BackColor, FillColor + shine, filled);
float stripes = Invulnerable * step(0.5, frac((UV.x * 8.0 - UV.y) - Time));
col = lerp(col, float3(0.6, 0.6, 0.65), stripes * 0.6);
return float4(col, 0.4 + filled * 0.6);
"""),
}


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def build(name, scalars, vectors, textures, code):
    asset = "M_HUD_" + name
    path = DIR + "/" + asset
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = tools.create_asset(asset, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    cust = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
    inputs = ["UV", "Time"] + list(scalars) + list(vectors) + textures
    cis = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        cis.append(ci)
    cust.set_editor_property("inputs", cis)
    cust.set_editor_property("code", COMMON + code)
    cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    cust.set_editor_property("description", "HUD " + name)
    y = 0
    def wire(e, pin):
        if not mel.connect_material_expressions(e, "", cust, pin):
            LOG.append("  ! link %s.%s" % (asset, pin))
    wire(expr(mat, unreal.MaterialExpressionTextureCoordinate, -600, -200), "UV")
    wire(expr(mat, unreal.MaterialExpressionTime, -600, -100), "Time")
    for n, v in scalars.items():
        wire(expr(mat, unreal.MaterialExpressionScalarParameter, -600, y, parameter_name=n, default_value=v), n); y += 60
    for n, v in vectors.items():
        wire(expr(mat, unreal.MaterialExpressionVectorParameter, -600, y, parameter_name=n,
                  default_value=unreal.LinearColor(v[0], v[1], v[2], 1.0)), n); y += 60
    default_tex = unreal.load_asset("/Engine/EngineResources/DefaultTexture")
    for n in textures:
        t = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -600, y, parameter_name=n)
        if default_tex:
            t.set_editor_property("texture", default_tex)
        wire(t, n); y += 60
    rgb = expr(mat, unreal.MaterialExpressionComponentMask, 300, -50, r=True, g=True, b=True, a=False)
    al = expr(mat, unreal.MaterialExpressionComponentMask, 300, 50, r=False, g=False, b=False, a=True)
    mel.connect_material_expressions(cust, "", rgb, "")
    mel.connect_material_expressions(cust, "", al, "")
    ok = mel.connect_material_property(rgb, "", MP.MP_EMISSIVE_COLOR) and mel.connect_material_property(al, "", MP.MP_OPACITY)
    if not ok:
        LOG.append("  ! property link " + asset)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_asset(path, only_if_is_dirty=False)
    mi_name = "MI_HUD_" + name
    mi_path = DIR + "/" + mi_name
    if eal.does_asset_exist(mi_path):
        eal.delete_asset(mi_path)
    mi = tools.create_asset(mi_name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    eal.save_asset(mi_path, only_if_is_dirty=False)
    LOG.append("OK   %s (+%s) params: %s" % (path, mi_name, ", ".join(inputs[2:])))


def main():
    for n, (s, v, t, code) in MATS.items():
        try:
            build(n, s, v, t, code)
        except Exception as e:  # noqa
            LOG.append("FAIL %s: %s" % (n, e))
    out = os.path.join(unreal.Paths.project_saved_dir(), "UI")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "ui_materials_report.txt"), "w") as f:
        f.write("\n".join(LOG) + "\n")
    for l in LOG:
        unreal.log(l)


if __name__ != "build_ui_materials":
    main()
