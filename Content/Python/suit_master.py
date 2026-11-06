"""
suit_master.py - the game's single suit master (Edith's M_SuitMaster, merged 2026-10-30 from
contrib/edith/suits/create_suit_materials.py, extended with Vison's gap list from
contrib/vison/suits/master_material_spec.md) and the two live suit MIs reparented to it:
    /Game/Edith/Suits/M_SuitMaster
    /Game/Realism/Materials/MI_SuitHotRod, MI_SuitGold   (parent: M_SuitMaster; were M_CarPaint)
Added over Edith's version: Specular (dielectric F0; 0.65 for oxide coats), Anisotropy + AnisotropyAngle
(Quality Switch: off at Low), geometric specular AA (normal-curvature -> roughness floor, notes.md §3),
Jocasta's live params Wetness / GrimeLive / ScorchLive. Per-suit values are applied at RUNTIME by
SuitRig from Content/Story/suit_materials.json (one table), so the 198 per-suit MIs are optional:
pass "-SuitMIs" in the command line to also build them (Edith's MI_Suit_<Suit>_<Slot>).
M_CarPaint stays for non-suit metal (robots, props). Texture slots (UseDetailTextures) are a TODO.

Original header:
create_suit_materials.py - builds M_SuitMaster + one MI per suit slot under /Game/Edith/Suits.
Edith (materials/look layer). UE 5.8 editor Python. Procedural only: no textures.

M_SuitMaster is a drop-in SUPERSET of /Game/Realism/Materials/M_CarPaint: every parameter the
game already drives (Color, WearColor, Metallic, Roughness, ClearCoat, ClearCoatRoughness,
FlakeAmount, FlakeScale, EdgeWear, Scorch, Dirt, Damage, HeatGlow, EmissiveStrength, CoreWhite)
keeps its name and meaning, and it adds: procedural panel lines (normal + cavity darkening),
curvature/vertex-colour edge wear to bare titanium, micro scratches, BattleDamage-driven dirt/
scorch/soot, emissive slots (EmissiveMask / EmissiveColor / EmissiveIntensity / pulse / hot core)
and blackbody repulsor overheat (HeatGlow x HeatZone) with heat-tint on the metal.

Per-suit values come from U's suit_materials.json via suit_values.py (33 suits incl. Hulkbuster).
MIs: MI_Suit_<Suit>_{Primary,Secondary,Accent,Reactor,Eyes,Repulsor}.

Headless:
  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash -nosound -stdout
Idempotent: the master's graph is cleared and rebuilt in place, MIs are re-parented and re-set.
Prints "[SuitMat] done: N ok, M failed".
"""
import os
import sys
import traceback

import unreal  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else \
    os.path.join(unreal.Paths.project_content_dir(), "Python")
for p in (HERE, os.path.dirname(HERE)):
    if p not in sys.path:
        sys.path.insert(0, p)

import unreal  # noqa: E402
import edith_matlib as L  # noqa: E402
try:
    import suit_values  # noqa: E402  (only needed for -SuitMIs)
except Exception:
    suit_values = None

FOLDER = "/Game/Edith/Suits"
MASTER = "M_SuitMaster"
log = L.Log("[SuitMat]")

SCALARS = [  # name, default (Mark III primary-ish)
    ("Metallic", 0.7), ("Roughness", 0.3), ("ClearCoat", 1.0), ("ClearCoatRoughness", 0.04),
    ("FlakeAmount", 0.35), ("FlakeScale", 1.6), ("EdgeWear", 0.12), ("Scorch", 0.0), ("Dirt", 0.0),
    ("Damage", 0.0), ("BattleDamage", 0.0), ("HeatGlow", 0.0), ("HeatZone", 0.1), ("HeatNits", 3000.0),
    ("EmissiveStrength", 0.0), ("EmissiveMask", 0.0), ("EmissiveIntensity", 0.0), ("NitsPerUnit", 1000.0),
    ("PulseSpeed", 2.2), ("PulseAmount", 0.08), ("CoreWhite", 0.5),
    ("PanelSize", 12.0), ("PanelWidth", 0.12), ("PanelDepth", 0.12),
    ("ScratchAmount", 0.2), ("ScratchScale", 0.45), ("CurvatureScale", 6.0), ("VertexColorWear", 0.0),
    ("NormalStrength", 1.0),
    # Vison gap list
    ("Specular", 0.5), ("Anisotropy", 0.0), ("AnisotropyAngle", 0.0), ("SpecularAA", 1.0),
    ("Wetness", 0.0), ("GrimeLive", 0.0), ("ScorchLive", 0.0),
]
VECTORS = [
    ("Color", (0.44, 0.02, 0.02)), ("WearColor", (0.542, 0.497, 0.449)), ("EmissiveColor", (0.58, 0.87, 1.0)),
    ("DirtColor", (0.045, 0.038, 0.03)), ("SootColor", (0.012, 0.01, 0.009)),
]
GEOM = ["LP", "N", "AX", "AY", "AZ", "WPc", "CamV", "T", "VC", "WearN", "ScorchN", "DirtN"]

# --------------------------------------------------------------------------- HLSL
# Shared prologue (pasted into every Custom node; the compiler CSEs identical math).
COMMON = r"""
#ifndef EDITH_SUIT_HASH
#define EDITH_SUIT_HASH 1
#define EH3(p) frac(sin(dot((p), float3(12.9898, 78.233, 37.719))) * 43758.5453)
#define EH2(p) frac(sin(dot((p), float2(127.1, 311.7))) * 43758.5453)
#endif
// object-oriented position in real centimetres (local position x component scale)
float3 S = max(float3(length(AX), length(AY), length(AZ)), 1e-4);
float3 P = LP * S;
float3 nl = float3(dot(N, AX / S.x), dot(N, AY / S.y), dot(N, AZ / S.z));
float3 tw = pow(abs(nl), 4.0); tw /= max(dot(tw, 1.0), 1e-4);
// ---- panel lines: staggered plate grid per projection plane, distance to seam in cm
float ps = max(PanelSize, 1.0);
float pw = max(PanelWidth, 0.02);
float2 g0 = P.yz / ps; g0.x += floor(g0.y) * 0.37;
float2 g1 = P.xz / ps; g1.x += floor(g1.y) * 0.61 + 0.13;
float2 g2 = P.xy / ps; g2.y += floor(g2.x) * 0.29 + 0.41;
float2 f0 = abs(frac(g0) - 0.5); float2 f1 = abs(frac(g1) - 0.5); float2 f2 = abs(frac(g2) - 0.5);
float d0 = (0.5 - max(f0.x, f0.y)) * ps; float d1 = (0.5 - max(f1.x, f1.y)) * ps; float d2 = (0.5 - max(f2.x, f2.y)) * ps;
float seam = (1.0 - smoothstep(pw * 0.35, pw, d0)) * tw.x + (1.0 - smoothstep(pw * 0.35, pw, d1)) * tw.y
           + (1.0 - smoothstep(pw * 0.35, pw, d2)) * tw.z;
float seamHalo = (1.0 - smoothstep(pw, pw * 4.0, d0)) * tw.x + (1.0 - smoothstep(pw, pw * 4.0, d1)) * tw.y
               + (1.0 - smoothstep(pw, pw * 4.0, d2)) * tw.z;      // wear / grime creeps out of seams
seam *= step(0.001, PanelDepth);
seamHalo *= step(0.001, PanelDepth);
// ---- micro scratches: short random-angle strokes in 1/ScratchScale cm cells, two octaves
float D = saturate(max(Damage, BattleDamage));
float DirtT = saturate(Dirt + GrimeLive);
float ScorchT = saturate(Scorch + ScorchLive);
float2 suv = (tw.x > tw.y && tw.x > tw.z) ? P.yz : ((tw.y > tw.z) ? P.xz : P.xy);
float scr = 0.0;
[unroll] for (int o = 0; o < 2; o++)
{
    float2 uv = suv * ScratchScale * (o == 0 ? 1.0 : 2.7) + (o == 0 ? 0.0 : 17.3);
    float2 cell = floor(uv);
    float a = EH2(cell) * 6.2831853;
    float2 q = frac(uv) - 0.5;
    float2 dir = float2(cos(a), sin(a));
    float along = dot(q, dir);
    float across = dot(q, float2(-dir.y, dir.x));
    float len = 0.2 + 0.28 * EH2(cell + 7.1);
    float on = step(1.0 - saturate(ScratchAmount * 0.6 + D * 0.5), EH2(cell + 3.3));
    scr = max(scr, on * (1.0 - smoothstep(0.006, 0.03, abs(across))) * (1.0 - smoothstep(len * 0.7, len, abs(along))));
}
// ---- curvature (screen-space normal change per cm) for edge wear
float3 dPx = ddx(WPc); float3 dPy = ddy(WPc);
float curv = (length(ddx(N)) + length(ddy(N))) / max(length(dPx) + length(dPy), 1e-3);
float edge = saturate(curv * CurvatureScale * 0.1);
float wearRaw = edge + seamHalo * 0.25 + VC * VertexColorWear;
float wear = saturate((wearRaw * (0.4 + WearN * 1.2) - (1.0 - saturate(EdgeWear + D * 0.5))) * 5.0) * (1.0 - EmissiveMask);
float burn = saturate((ScorchN - (1.0 - saturate(ScorchT + D * 0.8))) * 4.0) * (1.0 - EmissiveMask * 0.7);
float dirt = saturate(seamHalo * DirtT * 1.3 + (DirtN - (1.0 - saturate(DirtT * 0.6 + D * 0.35))) * 2.0) * (1.0 - EmissiveMask);
float flake = EH3(floor(P * FlakeScale));
float ht = saturate(HeatGlow) * saturate(HeatZone);
"""

BASE = COMMON + r"""
float3 paint = Color * (1.0 + (flake - 0.5) * FlakeAmount * 0.35);
// heat tint (temper colours: straw -> bronze -> blue) on overheated zones
float3 temper = lerp(float3(0.78, 0.6, 0.32), float3(0.22, 0.25, 0.55), saturate(ht * 1.6 - 0.4));
float3 c = lerp(paint, temper * 0.55, ht * 0.4 * (1.0 - EmissiveMask));
c = lerp(c, WearColor * (0.92 + 0.16 * flake), wear);                    // chipped to bare titanium
c = lerp(c, lerp(c, WearColor, 0.55), scr * 0.75 * (1.0 - EmissiveMask)); // scratches through the coat
c = lerp(c, DirtColor, dirt * 0.8);
c = lerp(c, SootColor, burn * 0.92);
c *= 1.0 - seam * 0.6 * (1.0 - wear);                                    // seam cavity
c *= 1.0 - saturate(Wetness) * 0.2 * (1.0 - Metallic);                   // wet dielectric darkens a little
return c;
"""

SURF = COMMON + r"""
float metal = lerp(Metallic, 1.0, max(wear, scr * 0.5 * (1.0 - EmissiveMask)));
metal = lerp(metal, 0.0, max(dirt * 0.7, burn * 0.85));
float rough = Roughness + (flake - 0.5) * FlakeAmount * 0.12;
rough = lerp(rough, 0.33 + WearN * 0.12, wear);
rough = lerp(rough, max(rough, 0.45), scr);
rough = lerp(rough, 0.82, dirt);
rough = lerp(rough, 0.92, burn);
rough += seam * 0.2;
float coat = ClearCoat * (1.0 - wear) * (1.0 - burn * 0.85) * (1.0 - dirt * 0.6) * (1.0 - seam * 0.5);
float coatRough = ClearCoatRoughness + scr * 0.25 + dirt * 0.4 + D * 0.08;
// live wetness: a water film = glossy coat over everything not burnt
float wet = saturate(Wetness) * (1.0 - burn);
coat = lerp(coat, 1.0, wet * 0.8);
coatRough = lerp(coatRough, 0.03, wet * 0.8);
rough = lerp(rough, rough * 0.6, wet);
// geometric specular AA (Kaplanyan/Tokuyoshi): normal variance across the pixel -> roughness floor.
// Keeps sub-pixel sun glints on small curved plates from flickering under TSR.
float3 dNx = ddx(N); float3 dNy = ddy(N);
float kern = min(2.0 * 0.25 * (dot(dNx, dNx) + dot(dNy, dNy)), 0.18) * saturate(SpecularAA);
rough = sqrt(saturate(rough * rough + kern));
coatRough = max(sqrt(saturate(coatRough * coatRough + kern)), 0.03);
return float4(saturate(metal), saturate(rough), saturate(coat), saturate(coatRough));
"""

NORMAL = COMMON + r"""
// height field (cm): seams are grooves, chips step down through the paint, scratches are fine cuts,
// battle damage adds broad dents. Screen-space surface-gradient bump (Mikkelsen 2010) -> world normal.
float h = -seam * PanelDepth - wear * 0.015 - scr * 0.006 - (WearN - 0.5) * D * 0.25;
float3 r1 = cross(dPy, N);
float3 r2 = cross(N, dPx);
float det = dot(dPx, r1);
float3 grad = sign(det) * (ddx(h) * r1 + ddy(h) * r2);
return normalize(abs(det) * N - grad * NormalStrength);
"""

EMIS = COMMON + r"""
float pulse = 1.0 + PulseAmount * sin(T * PulseSpeed);
float facing = saturate(dot(N, CamV));
float core = saturate(facing * facing * facing * CoreWhite);
float3 glow = lerp(EmissiveColor, float3(1.0, 1.0, 1.0), core) * EmissiveIntensity * NitsPerUnit * pulse * EmissiveMask;
// blackbody overheat: dull red -> orange -> yellow-white, strongest in seams and at burn edges
float heat = saturate(HeatGlow);
float ember = burn * (1.0 - burn) * 4.0;
float zone = saturate(saturate(HeatZone) * (0.35 + seamHalo * 0.65) + ember * heat);
float3 bb = lerp(float3(1.0, 0.1, 0.01), float3(1.0, 0.62, 0.25), heat * heat);
float3 heatE = bb * zone * heat * heat * HeatNits;
return glow + heatE + Color * EmissiveStrength;   // last term = M_CarPaint compatibility (hit flash)
"""


def build_master():
    mat = L.load_or_create_material(FOLDER, MASTER, log)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    mat.set_editor_property("tangent_space_normal", False)       # Custom node outputs a world-space normal
    mat.set_editor_property("use_material_attributes", True)     # ClearCoat pins only reachable via MakeMaterialAttributes
    g = L.G(mat, log)
    params = {}
    for i, (n, v) in enumerate(SCALARS):
        params[n] = g.scalar(n, v, -2200, -900 + i * 60, group="Suit")
    for i, (n, v) in enumerate(VECTORS):
        params[n] = g.vector(n, v, -2200, -1300 + i * 80, group="Suit")
    geo = {
        "LP": g.local_position(-2200, 1000),
        "N": g.expr(unreal.MaterialExpressionVertexNormalWS, -2200, 1100),
        "AX": g.local_axis_ws((1.0, 0.0, 0.0), -2200, 1200),
        "AY": g.local_axis_ws((0.0, 1.0, 0.0), -2200, 1300),
        "AZ": g.local_axis_ws((0.0, 0.0, 1.0), -2200, 1400),
        "WPc": g.camera_relative_wp(-2200, 1500),
        "CamV": g.expr(unreal.MaterialExpressionCameraVectorWS, -2200, 1600),
        "T": g.expr(unreal.MaterialExpressionTime, -2200, 1700),
    }
    vc = g.expr(unreal.MaterialExpressionVertexColor, -2400, 1800)
    geo["VC"] = g.mask(vc, -2200, 1800, r=True)
    # object-space centimetre position for the engine noise (wear / scorch / dirt breakup)
    objp = g.custom("float3 S = max(float3(length(AX), length(AY), length(AZ)), 1e-4); return LP * S;",
                    ["LP", "AX", "AY", "AZ"], -1900, 1900, desc="Object cm position")
    for n in ("LP", "AX", "AY", "AZ"):
        g.link(geo[n], objp, n)
    for key, scale, levels, turb, y in (("WearN", 0.09, 4, True, 2000), ("ScorchN", 0.025, 5, False, 2100),
                                        ("DirtN", 0.05, 4, False, 2200)):
        nz = g.expr(unreal.MaterialExpressionNoise, -1600, y, scale=scale, levels=levels, output_min=0.0,
                    output_max=1.0, turbulence=turb)
        g.link(objp, nz, "")
        geo[key] = nz
    inputs = GEOM + [n for n, _ in SCALARS] + [n for n, _ in VECTORS]
    nodes = {
        "base": g.custom(BASE, inputs, -900, -400, "float3", "SuitMaster BaseColor"),
        "surf": g.custom(SURF, inputs, -900, 0, "float4", "SuitMaster Metal/Rough/Coat/CoatRough"),
        "nrm": g.custom(NORMAL, inputs, -900, 400, "float3", "SuitMaster world normal"),
        "emis": g.custom(EMIS, inputs, -900, 800, "float3", "SuitMaster emissive"),
    }
    for c in nodes.values():
        for n in inputs:
            src = geo.get(n) or params.get(n)
            g.link(src, c, n)
    mma = g.expr(unreal.MaterialExpressionMakeMaterialAttributes, -300, 0)
    g.link(nodes["base"], mma, "BaseColor")
    g.link(g.mask(nodes["surf"], -600, -50, r=True), mma, "Metallic")
    g.link(g.mask(nodes["surf"], -600, 50, g=True), mma, "Roughness")
    g.link(g.mask(nodes["surf"], -600, 150, b=True), mma, "ClearCoat")
    g.link(g.mask(nodes["surf"], -600, 250, a=True), mma, "ClearCoatRoughness")
    g.link(nodes["nrm"], mma, "Normal")
    # Specular (dielectric F0 scale), straight from the parameter
    g.link(params["Specular"], mma, "Specular")
    # Anisotropy behind a Quality Switch (Low = 0; r.AnisotropicMaterials in [ShadingQuality@N] gates it too),
    # brushed direction = vertex tangent rotated by AnisotropyAngle (degrees) about the normal.
    tan = g.custom("float a = radians(AnisotropyAngle); float3 t = normalize(VT - N * dot(VT, N));"
                   " float3 b = cross(N, t); return normalize(t * cos(a) + b * sin(a));",
                   ["VT", "N", "AnisotropyAngle"], -900, 1200, "float3", "SuitMaster anisotropy tangent")
    vt = g.expr(unreal.MaterialExpressionVertexTangentWS, -1200, 1200)
    g.link(vt, tan, "VT"); g.link(geo["N"], tan, "N"); g.link(params["AnisotropyAngle"], tan, "AnisotropyAngle")
    qs = g.expr(unreal.MaterialExpressionQualitySwitch, -600, 1300)
    zero = g.expr(unreal.MaterialExpressionConstant, -800, 1400, r=0.0)
    g.link(params["Anisotropy"], qs, "Default")
    g.link(zero, qs, "Low")
    g.link(qs, mma, "Anisotropy")
    g.link(tan, mma, "Tangent")
    g.link(nodes["emis"], mma, "EmissiveColor")
    g.prop(mma, L.MP.MP_MATERIAL_ATTRIBUTES)
    ok = g.finish(FOLDER + "/" + MASTER, [n for n, _ in SCALARS], [n for n, _ in VECTORS])
    return mat, ok


LIVE_MIS = {  # the MIs SuitRig paints from (ArenaVisuals EArenaSurface::SuitPaint / SuitGold); same values as
    # build_realism_materials.py gave them on M_CarPaint (M_CarPaint defaults filled in)
    "MI_SuitHotRod": ({"Metallic": 0.7, "Roughness": 0.3, "ClearCoat": 1.0, "ClearCoatRoughness": 0.04, "FlakeAmount": 0.4,
                       "FlakeScale": 1.6, "EdgeWear": 0.1}, {"Color": (0.42, 0.012, 0.01), "WearColor": (0.7, 0.68, 0.66)}),
    "MI_SuitGold": ({"Metallic": 1.0, "Roughness": 0.22, "ClearCoat": 0.8, "ClearCoatRoughness": 0.05, "FlakeAmount": 0.25,
                     "EdgeWear": 0.08}, {"Color": (0.9, 0.6, 0.18), "WearColor": (0.75, 0.72, 0.7)}),
}


def reparent_live(master):
    n = 0
    for name, (scalars, vectors) in LIVE_MIS.items():
        if L.make_mi("/Game/Realism/Materials", name, master, scalars, vectors, log):
            n += 1
    return n


def main():
    ok_n, fail_n = 0, 0
    try:
        L.ensure_dir(FOLDER)
        master, mok = build_master()
        if mok and not log.errors:
            ok_n += 1
        else:
            fail_n += 1
        r = reparent_live(master)
        ok_n += r; fail_n += len(LIVE_MIS) - r
        want_mis = "-suitmis" in unreal.SystemLibrary.get_command_line().lower() and suit_values is not None
        table = {}
        if want_mis:
            table, meta = suit_values.build_table()
            log("source: %s" % meta)
        for sid, d in table.items():
            for slot, vals in d["slots"].items():
                name = suit_values.mi_name(sid, slot)
                try:
                    if L.make_mi(FOLDER, name, master, vals["scalars"], vals["vectors"], log):
                        ok_n += 1
                    else:
                        fail_n += 1
                except Exception:
                    fail_n += 1
                    log.err("%s: %s" % (name, traceback.format_exc()))
    except Exception:
        fail_n += 1
        log.err(traceback.format_exc())
    report = os.path.join(unreal.Paths.project_saved_dir(), "suit_master_report.txt")
    try:
        with open(report, "w") as f:
            f.write("\n".join(log.lines) + "\n")
    except Exception:
        pass
    log("done: %d ok, %d failed" % (ok_n, fail_n))


main()
