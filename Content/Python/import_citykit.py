"""
Import SourceAssets/CityKit into /Game/CityKit (headless):

  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<proj>/Content/Python/import_citykit.py" -unattended -nullrhi

Layout expected under SourceAssets/CityKit:
  Textures/<Role>/*_{Color,NormalGL,Roughness,AmbientOcclusion,Metalness,Displacement}.jpg
  HDRI/*.hdr
  Props/<slug>/*_2k.gltf (+ textures/ + .bin)

Creates:
  /Game/CityKit/Textures/<Role>/T_<Role>_<Map>
  /Game/CityKit/HDRI/T_<name>
  /Game/CityKit/Props/<slug>/SM_<slug>
  /Game/CityKit/Materials/M_City_PBR + MI_City_<Role>
  Content/CityKit/citykit_manifest.json  (+ Saved/CityKit/citykit_manifest.json, report)

Powered by Poly Haven + ambientCG (CC0).
"""
import json
import os
import traceback
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceAssets", "CityKit")
REPORT = os.path.join(PROJECT, "Saved", "CityKit", "import_report.txt")
MANIFEST_CONTENT = os.path.join(PROJECT, "Content", "CityKit", "citykit_manifest.json")
MANIFEST_SAVED = os.path.join(PROJECT, "Saved", "CityKit", "citykit_manifest.json")

TEX = "/Game/CityKit/Textures"
MAT = "/Game/CityKit/Materials"
HDRI = "/Game/CityKit/HDRI"
PROPS = "/Game/CityKit/Props"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LOG = []
MANIFEST = {
    "version": 1,
    "license_note": "All source assets CC0 (ambientCG + Poly Haven). API credit: Powered by Poly Haven.",
    "sources": [
        {"name": "ambientCG", "license": "CC0 1.0", "url": "https://ambientcg.com/"},
        {"name": "Poly Haven", "license": "CC0", "url": "https://polyhaven.com/"},
    ],
    "materials": {},
    "textures": {},
    "hdri": {},
    "props": {},
    "fallbacks": {
        "Asphalt": "/Game/CityKit/Materials/MI_City_Asphalt_Dry",
        "AsphaltWet": "/Game/CityKit/Materials/MI_City_Asphalt_Wet",
        "Sidewalk": "/Game/CityKit/Materials/MI_City_Concrete_Sidewalk",
        "Curb": "/Game/CityKit/Materials/MI_City_Concrete_Curb",
        "Brick": "/Game/CityKit/Materials/MI_City_Brick",
        "Metal": "/Game/CityKit/Materials/MI_City_Metal_Panels",
        "Roof": "/Game/CityKit/Materials/MI_City_Roof_Gravel",
        "GlassFacade": "/Game/CityKit/Materials/MI_City_Glass_Facade",
        "Facade": "/Game/CityKit/Materials/MI_City_Facade_Alt",
        "HDRI_Day": "/Game/CityKit/HDRI/T_City_Day_urban_street_01_2k",
        "HDRI_Dusk": "/Game/CityKit/HDRI/T_City_Dusk_modern_evening_street_2k",
    },
}

# Role -> (MI name, default tile cm, default wetness)
ROLES = [
    ("Asphalt_Dry", "MI_City_Asphalt_Dry", 400.0, 0.0),
    ("Asphalt_Wet", "MI_City_Asphalt_Wet", 400.0, 0.85),
    ("Concrete_Sidewalk", "MI_City_Concrete_Sidewalk", 200.0, 0.0),
    ("Concrete_Curb", "MI_City_Concrete_Curb", 120.0, 0.0),
    ("Brick", "MI_City_Brick", 250.0, 0.0),
    ("Metal_Panels", "MI_City_Metal_Panels", 180.0, 0.0),
    ("Roof_Gravel", "MI_City_Roof_Gravel", 300.0, 0.0),
    ("Glass_Facade", "MI_City_Glass_Facade", 800.0, 0.0),
    ("Facade_Alt", "MI_City_Facade_Alt", 600.0, 0.0),
]


def log(s):
    LOG.append(s)
    try:
        unreal.log("[citykit] " + s)
    except Exception:
        pass


def fresh(name, cls, factory, folder):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def configure_tex(tex, fn):
    if isinstance(tex, unreal.TextureCube):
        return
    low = fn.lower()
    tex.set_editor_property("max_texture_size", 2048)
    if "normal" in low:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        try:
            tex.set_editor_property("flip_green_channel", False)  # OpenGL normals
        except Exception:
            pass
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif any(k in low for k in ("roughness", "ambientocclusion", "metalness", "displacement", "ao")):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)


def import_files(pairs):
    tasks = []
    for fn, dest, name in pairs:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", fn)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    if tasks:
        tools.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        paths = list(t.get_editor_property("imported_object_paths") or [])
        if not paths:
            log("FAIL import " + t.get_editor_property("filename"))
            continue
        a = unreal.load_asset(paths[0])
        if isinstance(a, unreal.Texture):
            configure_tex(a, os.path.basename(t.get_editor_property("filename")))
        soft = paths[0].split(".")[0]
        eal.save_asset(soft, only_if_is_dirty=False)
        log("OK   " + soft)
        out.append(soft)
    return out


def import_textures():
    root = os.path.join(SRC, "Textures")
    if not os.path.isdir(root):
        log("NO Textures at " + root)
        return
    pairs = []
    for role in sorted(os.listdir(root)):
        folder = os.path.join(root, role)
        if not os.path.isdir(folder):
            continue
        for fn in sorted(os.listdir(folder)):
            if not fn.lower().endswith((".jpg", ".png")):
                continue
            base = os.path.splitext(fn)[0]
            # Asphalt_Dry_Color -> T_Asphalt_Dry_Color
            name = "T_" + base
            pairs.append((os.path.join(folder, fn), TEX + "/" + role, name))
    imported = import_files(pairs)
    for soft in imported:
        role = soft.split("/")[-2] if "/" in soft else "unknown"
        MANIFEST["textures"].setdefault(role, []).append(soft)


def import_hdris():
    root = os.path.join(SRC, "HDRI")
    if not os.path.isdir(root):
        log("NO HDRI at " + root)
        return
    pairs = []
    for fn in sorted(os.listdir(root)):
        if not fn.lower().endswith((".hdr", ".exr")):
            continue
        name = "T_" + os.path.splitext(fn)[0]
        pairs.append((os.path.join(root, fn), HDRI, name))
    for soft in import_files(pairs):
        MANIFEST["hdri"][soft.split("/")[-1]] = soft


def gltf_import(filename, dest):
    pipe = unreal.new_object(unreal.InterchangeGenericAssetsPipeline)
    mp = pipe.get_editor_property("mesh_pipeline")
    mp.set_editor_property(
        "combine_static_meshes_behavior",
        unreal.InterchangeCombineStaticMeshesBehavior.ALL,
    )
    try:
        mp.set_editor_property("build_nanite", True)
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


def import_props():
    root = os.path.join(SRC, "Props")
    if not os.path.isdir(root):
        log("NO Props at " + root)
        return
    for slug in sorted(os.listdir(root)):
        folder = os.path.join(root, slug)
        if not os.path.isdir(folder):
            continue
        gltfs = [f for f in os.listdir(folder) if f.endswith(".gltf")]
        if not gltfs:
            log("SKIP prop %s (no gltf)" % slug)
            continue
        dest = PROPS + "/" + slug
        try:
            if eal.does_directory_exist(dest):
                eal.delete_directory(dest)
            gltf_import(os.path.join(folder, gltfs[0]), dest)
            meshes = []
            for p in eal.list_assets(dest, recursive=True, include_folder=False):
                a = unreal.load_asset(p)
                if isinstance(a, unreal.StaticMesh):
                    meshes.append(a)
            if not meshes:
                log("FAIL prop %s: no meshes" % slug)
                continue
            meshes.sort(key=lambda m: (m.get_bounding_box().max - m.get_bounding_box().min).length(), reverse=True)
            main = meshes[0]
            target = dest + "/SM_" + slug
            cur = main.get_path_name().split(".")[0]
            if cur != target:
                eal.rename_asset(cur, target)
            m = unreal.load_asset(target)
            try:
                unreal.EditorStaticMeshLibrary.add_simple_collisions(m, unreal.ScriptCollisionShapeType.BOX)
            except Exception:
                pass
            eal.save_asset(target, only_if_is_dirty=False)
            for p in eal.list_assets(dest, recursive=True, include_folder=False):
                eal.save_asset(p.split(".")[0], only_if_is_dirty=True)
            MANIFEST["props"][slug] = target
            log("OK   " + target)
        except Exception:
            log("FAIL prop %s\n%s" % (slug, traceback.format_exc()))


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin, out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        log("  ! link failed -> %s.%s" % (b.get_name(), pin))


def build_master():
    """UV-tiled PBR master: BaseColor, Normal (GL), Roughness, AO, Metallic, Wetness, TileSize."""
    name = "M_City_PBR"
    mat = fresh(name, unreal.Material, unreal.MaterialFactoryNew(), MAT)
    st = unreal.MaterialSamplerType

    # default texture paths (Asphalt_Dry)
    def tpath(role, suffix):
        return "%s/%s/T_%s_%s" % (TEX, role, role, suffix)

    tc = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, -200,
              parameter_name="BaseColorMap", sampler_type=st.SAMPLERTYPE_COLOR)
    t = unreal.load_asset(tpath("Asphalt_Dry", "Color"))
    if t:
        tc.set_editor_property("texture", t)

    tn = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, 0,
              parameter_name="NormalMap", sampler_type=st.SAMPLERTYPE_NORMAL)
    t = unreal.load_asset(tpath("Asphalt_Dry", "NormalGL"))
    if t:
        tn.set_editor_property("texture", t)

    tr = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, 200,
              parameter_name="RoughnessMap", sampler_type=st.SAMPLERTYPE_MASKS)
    t = unreal.load_asset(tpath("Asphalt_Dry", "Roughness"))
    if t:
        tr.set_editor_property("texture", t)

    ta = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, 400,
              parameter_name="AOMap", sampler_type=st.SAMPLERTYPE_MASKS)
    t = unreal.load_asset(tpath("Asphalt_Dry", "AmbientOcclusion"))
    if t:
        ta.set_editor_property("texture", t)

    tm = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1400, 600,
              parameter_name="MetallicMap", sampler_type=st.SAMPLERTYPE_MASKS)
    # may be missing on many sets; leave unset (black/0)

    tile = expr(mat, unreal.MaterialExpressionScalarParameter, -1400, 800,
                parameter_name="TileSize", default_value=400.0)
    wet = expr(mat, unreal.MaterialExpressionScalarParameter, -1400, 900,
               parameter_name="Wetness", default_value=0.0)
    tint = expr(mat, unreal.MaterialExpressionVectorParameter, -1400, 1000,
                parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
    nstr = expr(mat, unreal.MaterialExpressionScalarParameter, -1400, 1100,
                parameter_name="NormalStrength", default_value=1.0)

    # World-XY UV from world position / TileSize (cm)
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1100, -400)
    uv_code = """
float ts = max(TileSize, 1.0);
float2 uv = WP.xy / ts;
"""
    # Sample helpers via Custom
    def custom(code, inputs, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
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

    col = custom(
        uv_code + "float3 c = Texture2DSample(Tex, TexSampler, uv).rgb * Tint.rgb; return c * lerp(1.0, 0.55, saturate(Wet));",
        ["Tex", "WP", "TileSize", "Tint", "Wet"], -200)
    link(tc, col, "Tex"); link(wp, col, "WP"); link(tile, col, "TileSize"); link(tint, col, "Tint"); link(wet, col, "Wet")
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)

    rough = custom(
        uv_code + "float r = Texture2DSample(Tex, TexSampler, uv).r; return lerp(r, min(r, 0.12), saturate(Wet) * 0.9);",
        ["Tex", "WP", "TileSize", "Wet"], 50, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(tr, rough, "Tex"); link(wp, rough, "WP"); link(tile, rough, "TileSize"); link(wet, rough, "Wet")
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)

    ao = custom(
        uv_code + "return Texture2DSample(Tex, TexSampler, uv).r;",
        ["Tex", "WP", "TileSize"], 200, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(ta, ao, "Tex"); link(wp, ao, "WP"); link(tile, ao, "TileSize")
    mel.connect_material_property(ao, "", MP.MP_AMBIENT_OCCLUSION)

    metal = custom(
        uv_code + "return Texture2DSample(Tex, TexSampler, uv).r;",
        ["Tex", "WP", "TileSize"], 350, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(tm, metal, "Tex"); link(wp, metal, "WP"); link(tile, metal, "TileSize")
    mel.connect_material_property(metal, "", MP.MP_METALLIC)

    nor = custom(
        uv_code + """
float3 nts = Texture2DSample(Tex, TexSampler, uv).xyz * 2.0 - 1.0; // OpenGL: +Y up
nts.xy *= Strength;
nts = normalize(nts);
// flatten toward geometric normal when wet
nts = normalize(lerp(nts, float3(0,0,1), saturate(Wet) * 0.35));
return nts;
""",
        ["Tex", "WP", "TileSize", "Strength", "Wet"], 500)
    link(tn, nor, "Tex"); link(wp, nor, "WP"); link(tile, nor, "TileSize"); link(nstr, nor, "Strength"); link(wet, nor, "Wet")
    mel.connect_material_property(nor, "", MP.MP_NORMAL)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_asset(MAT + "/" + name, only_if_is_dirty=False)
    log("OK   %s/%s" % (MAT, name))
    MANIFEST["materials"]["M_City_PBR"] = MAT + "/M_City_PBR"
    return mat


def make_mi(mi_name, parent, role, tile, wetness):
    mi = fresh(mi_name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew(), MAT)
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_scalar_parameter_value(mi, "TileSize", tile)
    mel.set_material_instance_scalar_parameter_value(mi, "Wetness", wetness)
    for pname, suffix in (
        ("BaseColorMap", "Color"),
        ("NormalMap", "NormalGL"),
        ("RoughnessMap", "Roughness"),
        ("AOMap", "AmbientOcclusion"),
        ("MetallicMap", "Metalness"),
    ):
        path = "%s/%s/T_%s_%s" % (TEX, role, role, suffix)
        t = unreal.load_asset(path)
        if t:
            mel.set_material_instance_texture_parameter_value(mi, pname, t)
        elif suffix != "Metalness" and suffix != "AmbientOcclusion":
            log("  ! %s missing %s" % (mi_name, path))
    mel.update_material_instance(mi)
    soft = MAT + "/" + mi_name
    eal.save_asset(soft, only_if_is_dirty=False)
    MANIFEST["materials"][mi_name] = soft
    log("OK   " + soft)


def build_materials():
    master = build_master()
    for role, mi_name, tile, wet in ROLES:
        try:
            make_mi(mi_name, master, role, tile, wet)
        except Exception:
            log("FAIL MI %s\n%s" % (mi_name, traceback.format_exc()))


def write_manifest():
    # also soft-path form with duplicate name for SoftObjectPath style
    soft_paths = {}
    for k, v in MANIFEST["materials"].items():
        soft_paths[k] = "%s.%s" % (v, v.split("/")[-1])
    MANIFEST["soft_object_paths"] = soft_paths
    for path in (MANIFEST_CONTENT, MANIFEST_SAVED):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(MANIFEST, f, indent=2)
        log("manifest -> " + path)


def main():
    if not os.path.isdir(SRC):
        log("NO SOURCE: %s" % SRC)
    else:
        for step in (import_textures, import_hdris, import_props, build_materials, write_manifest):
            try:
                step()
            except Exception:
                log("FAIL %s\n%s" % (step.__name__, traceback.format_exc()))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as f:
        f.write("\n".join(LOG) + "\n")


main()
