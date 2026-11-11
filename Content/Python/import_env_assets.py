"""
Import the city kit (Dum-E's CC0 pack, prepared by Tools/prepare_env_assets.py) into /Game/Env (headless):

  UnrealEditor-Cmd <proj> -run=pythonscript -script="<proj>/Content/Python/import_env_assets.py" -unattended -nullrhi

  SourceAssets/Env/Textures/<id>/  -> /Game/Env/Textures/<id>/T_<id>_{diff,nor_dx,arm}
  SourceAssets/Env/HDRI/<id>.hdr   -> /Game/Env/HDRI/T_<id> (TextureCube)
  SourceAssets/Env/Models/<id>/    -> /Game/Env/Props/<id>/SM_<id>   (props: every glTF node merged into one mesh)
                                      /Game/Env/Kits/<id>/<node>       (facade kits: one mesh per kit piece)
  Materials -> /Game/Env/Materials:
    MI_Env_*  instances of /Game/Realism/Materials/M_SurfacePBR (triplanar, world-aligned)
    M_RoadUV + MI_Env_Road4Lane / MI_Env_Road2Lane: road textures with lane markings laid along the mesh's
              local X axis (object-space UVs: UTiles across the road, VTilesPerMeter along it).
              Texture samples use standard TextureSampleParameter2D nodes; a Custom node only
              computes the float2 UV. Sampling inside Custom HLSL crashed dxcompiler while cooking
              PCD3D_SM5 (null ShaderMap / EXCEPTION_ACCESS_VIOLATION).
glTF is Y-up metres; Interchange converts to Z-up centimetres. Report: Saved/Env/env_report.txt
Kit piece sizes for C++: Saved/Env/kit_pieces.json is also copied to Content/Env/kit_pieces.json.
"""
import json
import os
import traceback
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceAssets", "Env")
REPORT = os.path.join(PROJECT, "Saved", "Env", "env_report.txt")
KIT_JSON = os.path.join(PROJECT, "Content", "Env", "kit_pieces.json")
TEX = "/Game/Env/Textures"
MAT = "/Game/Env/Materials"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LOG = []
KITS = ("modular_urban_apartments_facade", "modular_factory_facade")


def log(s):
    LOG.append(s)
    unreal.log("[env] " + s)


# ------------------------------------------------------------------ textures
def configure(tex, fn):
    if isinstance(tex, unreal.TextureCube):
        return
    low = fn.lower()
    tex.set_editor_property("max_texture_size", 1024)
    if "_nor" in low:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif "_arm" in low:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)


def import_files(pairs):
    """pairs: (filename, dest folder, asset name)"""
    tasks = []
    for fn, dest, name in pairs:
        t = unreal.AssetImportTask()
        t.filename = fn
        t.destination_path = dest
        t.destination_name = name
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    if tasks:
        tools.import_asset_tasks(tasks)
    for t in tasks:
        paths = list(t.imported_object_paths or [])
        if not paths:
            log("FAIL import " + t.filename)
            continue
        a = unreal.load_asset(paths[0])
        if isinstance(a, unreal.Texture):
            configure(a, os.path.basename(t.filename))
        eal.save_asset(paths[0].split(".")[0], only_if_is_dirty=False)
        log("OK   " + paths[0].split(".")[0])


def import_textures():
    pairs = []
    skip_existing = os.environ.get("ENV_REIMPORT_TEXTURES") != "1"
    root = os.path.join(SRC, "Textures")
    for tid in sorted(os.listdir(root)):
        for fn in sorted(os.listdir(os.path.join(root, tid))):
            if fn.endswith(".jpg") and not (skip_existing and eal.does_asset_exist("%s/%s/T_%s" % (TEX, tid, os.path.splitext(fn)[0]))):
                pairs.append((os.path.join(root, tid, fn), TEX + "/" + tid, "T_" + os.path.splitext(fn)[0]))
    hroot = os.path.join(SRC, "HDRI")
    for fn in sorted(os.listdir(hroot)) if os.path.isdir(hroot) else []:
        if fn.endswith(".hdr") and not (skip_existing and eal.does_asset_exist("/Game/Env/HDRI/T_" + os.path.splitext(fn)[0])):
            pairs.append((os.path.join(hroot, fn), "/Game/Env/HDRI", "T_" + os.path.splitext(fn)[0]))
    import_files(pairs)


# ------------------------------------------------------------------ models
def gltf_import(filename, dest, combine):
    pipe = unreal.new_object(unreal.InterchangeGenericAssetsPipeline)
    mp = pipe.get_editor_property("mesh_pipeline")
    mp.set_editor_property("combine_static_meshes_behavior",
                           unreal.InterchangeCombineStaticMeshesBehavior.ALL if combine
                           else unreal.InterchangeCombineStaticMeshesBehavior.DO_NOT_COMBINE)
    try:
        mp.set_editor_property("build_nanite", True)  # static environment meshes: Nanite (Ultron perf budget)
    except Exception:
        pass
    try:
        mp.set_editor_property("import_skeletal_meshes", False)
    except Exception:
        pass
    try:
        pipe.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
    except Exception:
        pass
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipe.get_path_name())]
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    src = unreal.InterchangeManager.create_source_data(filename)
    mgr.import_asset(dest, src, params)


def meshes_in(folder):
    out = []
    for p in eal.list_assets(folder, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            out.append(a)
    return out


def bounds(m):
    b = m.get_bounding_box()
    return b.min, b.max


def import_models():
    root = os.path.join(SRC, "Models")
    kit = {}
    for mid in sorted(os.listdir(root)):
        folder = os.path.join(root, mid)
        if not os.path.isdir(folder):
            continue
        gl = [f for f in os.listdir(folder) if f.endswith(".gltf")]
        if not gl:
            continue
        is_kit = mid in KITS
        if is_kit and os.environ.get("ENV_IMPORT_KITS") != "1":
            # The facade kits cost ~38 MB of content and the city does not place them yet
            # (buildings are procedural masses), so they are opt-in.
            log("SKIP kit %s (set ENV_IMPORT_KITS=1)" % mid)
            continue
        dest = ("/Game/Env/Kits/" if is_kit else "/Game/Env/Props/") + mid
        try:
            if eal.does_directory_exist(dest):
                eal.delete_directory(dest)
            gltf_import(os.path.join(folder, gl[0]), dest, combine=not is_kit)
            ms = meshes_in(dest)
            if not ms:
                log("FAIL %s: no meshes" % mid)
                continue
            if is_kit:
                pieces = {}
                for m in ms:
                    lo, hi = bounds(m)
                    pieces[m.get_name()] = {"path": m.get_path_name(), "min": [lo.x, lo.y, lo.z], "max": [hi.x, hi.y, hi.z]}
                kit[mid] = pieces
                log("OK   kit %s: %d pieces" % (mid, len(pieces)))
            else:
                ms.sort(key=lambda m: (bounds(m)[1] - bounds(m)[0]).length(), reverse=True)
                main = ms[0]
                target = dest + "/SM_" + mid
                if main.get_path_name().split(".")[0] != target:
                    eal.rename_asset(main.get_path_name().split(".")[0], target)
                m = unreal.load_asset(target)
                try:
                    unreal.EditorStaticMeshLibrary.add_simple_collisions(m, unreal.ScriptCollisionShapeType.BOX)
                except Exception:
                    pass
                eal.save_asset(target, only_if_is_dirty=False)
                lo, hi = bounds(m)
                log("OK   %s  min(%.0f,%.0f,%.0f) max(%.0f,%.0f,%.0f)  meshes=%d" % (target, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, len(ms)))
            for p in eal.list_assets(dest, recursive=True, include_folder=False):
                eal.save_asset(p.split(".")[0], only_if_is_dirty=True)
        except Exception:
            log("FAIL %s\n%s" % (mid, traceback.format_exc()))
    os.makedirs(os.path.dirname(KIT_JSON), exist_ok=True)
    with open(KIT_JSON, "w") as f:
        json.dump(kit, f, indent=1)


# ------------------------------------------------------------------ materials
def fresh(name, cls, factory, folder=MAT):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin, out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        log("  ! link failed -> %s.%s" % (b.get_name(), pin))


def tex_param(mat, name, path, sampler, y):
    e = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1200, y, parameter_name=name, sampler_type=sampler)
    t = unreal.load_asset(path)
    if t:
        e.set_editor_property("texture", t)
    return e


def custom(mat, code, inputs, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -600, y)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out)
    return c


def const_f(mat, value, x, y):
    return expr(mat, unreal.MaterialExpressionConstant, x, y, r=value)


def tex_sample(mat, name, path, sampler, x, y, uv_node):
    """Regular TextureSampleParameter2D. Do not sample textures inside Custom HLSL — that
    path made dxcompiler crash (EXCEPTION_ACCESS_VIOLATION) while cooking M_RoadUV for PCD3D_SM5."""
    e = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y,
             parameter_name=name, sampler_type=sampler)
    t = unreal.load_asset(path)
    if t:
        e.set_editor_property("texture", t)
    if not mel.connect_material_expressions(uv_node, "", e, "Coordinates"):
        if not mel.connect_material_expressions(uv_node, "", e, "UVs"):
            log("  ! UV link failed for %s" % name)
    return e


def build_road_master():
    name = "M_RoadUV"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew())
    # Tangent-space normals + standard texture samples (no GetPrimitiveData in custom HLSL).
    mat.set_editor_property("tangent_space_normal", True)
    st = unreal.MaterialSamplerType
    d = "Road007"
    lp = expr(mat, unreal.MaterialExpressionLocalPosition, -1200, 300) if hasattr(unreal, "MaterialExpressionLocalPosition") else None
    if lp is None:
        wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1400, 300)
        tr = expr(mat, unreal.MaterialExpressionTransformPosition, -1200, 300)
        tr.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        tr.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        link(wp, tr, "")
        lp = tr
    os_ = expr(mat, unreal.MaterialExpressionObjectBounds, -1200, 400)
    ut = expr(mat, unreal.MaterialExpressionScalarParameter, -1200, 500, parameter_name="UTiles", default_value=1.0)
    vt = expr(mat, unreal.MaterialExpressionScalarParameter, -1200, 600, parameter_name="VTileMeters", default_value=8.0)
    tint = expr(mat, unreal.MaterialExpressionVectorParameter, -1200, 700, parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
    wet = expr(mat, unreal.MaterialExpressionScalarParameter, -1200, 800, parameter_name="Wetness", default_value=0.0)
    es = expr(mat, unreal.MaterialExpressionScalarParameter, -1200, 900, parameter_name="EmissiveStrength", default_value=0.0)
    # Object-space UV only: U across the road (local Y), V along it (local X). No texture samples here.
    uv = custom(mat, """
float3 ext = max(Bounds, 1.0);
float u = (LP.y / 100.0 + 0.5) * UT;
float v = (LP.x / 100.0) * (ext.x / 100.0) / max(VT, 0.001);
return float2(u, v);
""", ["LP", "Bounds", "UT", "VT"], -800, out=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    link(lp, uv, "LP"); link(os_, uv, "Bounds"); link(ut, uv, "UT"); link(vt, uv, "VT")
    base = tex_sample(mat, "BaseColorMap", "%s/%s/T_%s_diff" % (TEX, d, d), st.SAMPLERTYPE_COLOR, -400, -300, uv)
    nrm = tex_sample(mat, "NormalMap", "%s/%s/T_%s_nor_dx" % (TEX, d, d), st.SAMPLERTYPE_NORMAL, -400, -100, uv)
    arm = tex_sample(mat, "ARMMap", "%s/%s/T_%s_arm" % (TEX, d, d), st.SAMPLERTYPE_MASKS, -400, 100, uv)
    sat = expr(mat, unreal.MaterialExpressionSaturate, -200, 800)
    link(wet, sat, "")
    dark = expr(mat, unreal.MaterialExpressionLinearInterpolate, -50, -300)
    link(const_f(mat, 1.0, -200, -360), dark, "A")
    link(const_f(mat, 0.5, -200, -320), dark, "B")
    link(sat, dark, "Alpha")
    tinted = expr(mat, unreal.MaterialExpressionMultiply, -50, -240)
    link(base, tinted, "A", "RGB")
    link(tint, tinted, "B")
    col = expr(mat, unreal.MaterialExpressionMultiply, 150, -240)
    link(tinted, col, "A")
    link(dark, col, "B")
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    ao = expr(mat, unreal.MaterialExpressionComponentMask, -50, 40, r=True)
    link(arm, ao, "", "RGB")
    mel.connect_material_property(ao, "", MP.MP_AMBIENT_OCCLUSION)
    rough_s = expr(mat, unreal.MaterialExpressionComponentMask, -50, 120, g=True)
    link(arm, rough_s, "", "RGB")
    wet_a = expr(mat, unreal.MaterialExpressionMultiply, -50, 200)
    link(sat, wet_a, "A")
    link(const_f(mat, 0.85, -200, 200), wet_a, "B")
    rough = expr(mat, unreal.MaterialExpressionLinearInterpolate, 150, 120)
    link(rough_s, rough, "A")
    link(const_f(mat, 0.08, -50, 260), rough, "B")
    link(wet_a, rough, "Alpha")
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    metal = expr(mat, unreal.MaterialExpressionComponentMask, -50, 320, b=True)
    link(arm, metal, "", "RGB")
    mel.connect_material_property(metal, "", MP.MP_METALLIC)
    mel.connect_material_property(nrm, "RGB", MP.MP_NORMAL)
    em = expr(mat, unreal.MaterialExpressionMultiply, 150, 400)
    link(tint, em, "A")
    link(es, em, "B")
    mel.connect_material_property(em, "", MP.MP_EMISSIVE_COLOR)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_asset(MAT + "/" + name, only_if_is_dirty=False)
    log("OK   %s/%s (cook-safe samples)" % (MAT, name))
    return mat


def make_mi(name, parent, scalars, tex_set):
    mi = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for pname, suffix in (("BaseColorMap", "diff"), ("NormalMap", "nor_dx"), ("ARMMap", "arm")):
        t = unreal.load_asset("%s/%s/T_%s_%s" % (TEX, tex_set, tex_set, suffix))
        if t:
            mel.set_material_instance_texture_parameter_value(mi, pname, t)
        else:
            log("  ! %s: missing %s" % (name, suffix))
    mel.update_material_instance(mi)
    eal.save_asset(MAT + "/" + name, only_if_is_dirty=False)
    log("OK   %s/%s" % (MAT, name))


SURFACES = [  # (MI, texture set, tile size cm)
    ("MI_Env_Pavement", "concrete_pavement", 240.0),
    ("MI_Env_Tactile", "TactilePaving001", 60.0),
    ("MI_Env_GlassTower", "Facade018A", 1400.0),
    ("MI_Env_ConcreteTiles", "concrete_tile_facade", 360.0),
    ("MI_Env_ConcreteWall", "concrete_wall_008", 360.0),
    ("MI_Env_Debris", "concrete_debris", 220.0),
    ("MI_Env_Ground", "Ground107", 400.0),
    ("MI_Env_Metal", "Metal012", 160.0),
]


def build_materials():
    surf = unreal.load_asset("/Game/Realism/Materials/M_SurfacePBR")
    if not surf:
        log("FAIL no M_SurfacePBR (run build_realism_materials.py)")
    else:
        for mi, ts, tile in SURFACES:
            make_mi(mi, surf, {"TileSize": tile}, ts)
    road = build_road_master()
    make_mi("MI_Env_Road4Lane", road, {"UTiles": 1.0, "VTileMeters": 14.0}, "Road007")
    make_mi("MI_Env_Road2Lane", road, {"UTiles": 1.0, "VTileMeters": 9.0}, "Road008A")


def main():
    for step in (import_textures, import_models, build_materials):
        try:
            step()
        except Exception:
            log("FAIL %s\n%s" % (step.__name__, traceback.format_exc()))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as f:
        f.write("\n".join(LOG) + "\n")


if __name__ == "__main__":
    main()
