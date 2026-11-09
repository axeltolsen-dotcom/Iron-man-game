"""
Import SourceAssets/Suits/{Classic,Nanotech,SpiderLegs,Heavy,Gunner,Rescue}
into /Game/Models/Suits/<Name>/ with Nanite when valid.
Creates simple candy-red / gold material instances when a master exists or via basic MI.

  UnrealEditor-Cmd <proj> -run=pythonscript -script="<proj>/Content/Python/import_suit_models.py" -unattended -nullrhi
"""
import json
import os
import traceback
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceAssets", "Suits")
MANIFEST_CONTENT = os.path.join(PROJECT, "Content", "Models", "suit_models_manifest.json")
MANIFEST_CONTRIB = os.path.join(PROJECT, "contrib", "models", "suit_models_manifest.json")
REPORT = os.path.join(PROJECT, "Saved", "Suits", "import_report.txt")
DEST_ROOT = "/Game/Models/Suits"

# soft path key -> folder name under SourceAssets/Suits and /Game/Models/Suits
ROLES = {
    "Suit_Classic": "Classic",
    "Suit_Nanotech": "Nanotech",
    "Suit_SpiderLegs": "SpiderLegs",
    "Suit_Heavy": "Heavy",
    "Suit_Gunner": "Gunner",
    "Suit_Rescue": "Rescue",
}

# Primary source filename per role (imported first / tagged in manifest update)
PRIMARIES = {
    "Classic": "ShockBot.fbx",
    "Nanotech": "RobotEnemy.glb",
    "SpiderLegs": "RobotEnemy_LegsGun.glb",
    "Heavy": "ColoradoMech.fbx",
    "Gunner": "RobotEnemy_LargeGun.glb",
    "Rescue": "CharacterHazmat.glb",
}

# Candy red / gold linear approximations for MI Color params
RECOLOR = {
    "Classic": {"Color": (0.694, 0.071, 0.149), "Secondary": (0.773, 0.627, 0.157)},
    "Nanotech": {"Color": (0.694, 0.071, 0.149), "Secondary": (0.773, 0.627, 0.157)},
    "SpiderLegs": {"Color": (0.694, 0.071, 0.149), "Secondary": (0.773, 0.627, 0.157)},
    "Heavy": {"Color": (0.694, 0.071, 0.149), "Secondary": (0.773, 0.627, 0.157)},
    "Gunner": {"Color": (0.18, 0.21, 0.23), "Secondary": (0.773, 0.627, 0.157)},
    "Rescue": {"Color": (0.773, 0.627, 0.157), "Secondary": (0.91, 0.91, 0.91)},
}

MESH_EXTS = {".fbx", ".glb", ".gltf", ".obj"}
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
LOG = []


def log(s):
    LOG.append(s)
    try:
        unreal.log("[suit-models] " + s)
    except Exception:
        pass


def ensure_dir(content_path):
    if not eal.does_directory_exist(content_path):
        eal.make_directory(content_path)


def gltf_import(filename, dest, combine=True, skeletal=True):
    pipe = unreal.new_object(unreal.InterchangeGenericAssetsPipeline)
    mp = pipe.get_editor_property("mesh_pipeline")
    if combine:
        mp.set_editor_property(
            "combine_static_meshes_behavior",
            unreal.InterchangeCombineStaticMeshesBehavior.ALL,
        )
    else:
        mp.set_editor_property(
            "combine_static_meshes_behavior",
            unreal.InterchangeCombineStaticMeshesBehavior.DO_NOT_COMBINE,
        )
    try:
        mp.set_editor_property("build_nanite", True)
    except Exception:
        pass
    try:
        mp.set_editor_property("import_as_skeletal", skeletal)
    except Exception:
        try:
            mp.set_editor_property("import_skeletal_meshes", skeletal)
        except Exception:
            pass
    try:
        pipe.get_editor_property("animation_pipeline").set_editor_property(
            "import_animations", skeletal
        )
    except Exception:
        pass
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines = [unreal.SoftObjectPath(pipe.get_path_name())]
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    src = unreal.InterchangeManager.create_source_data(filename)
    mgr.import_asset(dest, src, params)


def asset_import_task(filename, dest, name=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    if name:
        t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    # Prefer skeletal for character FBX when possible
    try:
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", True)
        options.set_editor_property("import_animations", True)
        options.set_editor_property("import_materials", True)
        options.set_editor_property("import_textures", True)
        t.set_editor_property("options", options)
    except Exception:
        pass
    tools.import_asset_tasks([t])
    paths = list(t.get_editor_property("imported_object_paths") or [])
    return [p.split(".")[0] for p in paths]


def meshes_in(folder):
    out = []
    for p in eal.list_assets(folder, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if isinstance(a, (unreal.StaticMesh, unreal.SkeletalMesh)):
            out.append(a)
    return out


def try_enable_nanite(mesh):
    if not isinstance(mesh, unreal.StaticMesh):
        return False
    try:
        mesh.set_editor_property("nanite_settings", mesh.get_editor_property("nanite_settings"))
        ns = mesh.get_editor_property("nanite_settings")
        ns.enabled = True
        mesh.set_editor_property("nanite_settings", ns)
        return True
    except Exception:
        try:
            # older API
            unreal.EditorStaticMeshLibrary.set_nanite_settings(mesh, True)
            return True
        except Exception:
            return False


def apply_recolor_mi(mesh, role_folder, dest):
    """Create /Game/Models/Suits/<Role>/MI_SuitRecolor_<Role> and assign if mesh has slots."""
    colors = RECOLOR.get(role_folder)
    if not colors:
        return None
    mi_name = "MI_SuitRecolor_" + role_folder
    mi_path = dest + "/" + mi_name
    # Prefer existing Edith / Realism car-paint masters if present
    master_candidates = [
        "/Game/Edith/Suits/M_SuitMaster",
        "/Game/Realism/Materials/M_CarPaint",
        "/Game/Realism/Materials/M_SuitPaint",
    ]
    master = None
    for c in master_candidates:
        if eal.does_asset_exist(c):
            master = unreal.load_asset(c)
            break
    if master is None:
        # Create a minimal constant-color material
        try:
            mf = unreal.MaterialFactoryNew()
            mat_path = dest + "/M_SuitSolid_" + role_folder
            if eal.does_asset_exist(mat_path):
                eal.delete_asset(mat_path)
            master = tools.create_asset(
                "M_SuitSolid_" + role_folder, dest, unreal.Material, mf
            )
            # Can't easily build graph headless; leave master empty and skip MI
            if master:
                eal.save_asset(mat_path)
            log("NOTE no paint master; solid material stub " + mat_path)
            return None
        except Exception:
            log("WARN recolor skip (no master): " + role_folder)
            return None
    try:
        if eal.does_asset_exist(mi_path):
            eal.delete_asset(mi_path)
        factory = unreal.MaterialInstanceConstantFactoryNew()
        mi = tools.create_asset(mi_name, dest, unreal.MaterialInstanceConstant, factory)
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, master)
        for pname, rgb in colors.items():
            try:
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                    mi, pname, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)
                )
            except Exception:
                pass
        # Common alias
        try:
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                mi, "Color", unreal.LinearColor(
                    colors["Color"][0], colors["Color"][1], colors["Color"][2], 1.0
                )
            )
        except Exception:
            pass
        eal.save_asset(mi_path)
        # Assign to first material slot if possible
        try:
            if isinstance(mesh, unreal.StaticMesh):
                mats = mesh.static_materials
                if mats and len(mats) > 0:
                    sm = mats[0]
                    sm.set_editor_property("material_interface", mi)
                    mesh.set_editor_property("static_materials", [sm] + list(mats[1:]))
            elif isinstance(mesh, unreal.SkeletalMesh):
                # UE5 materials via skeletal mesh editor subsystem when available
                try:
                    subsystem = unreal.get_engine_subsystem(unreal.SkeletalMeshEditorSubsystem)
                    # best-effort: leave textures; MI available for BP assignment
                except Exception:
                    pass
        except Exception:
            log("WARN could not assign MI to mesh slots: " + mesh.get_path_name())
        log("MI   " + mi_path)
        return mi_path
    except Exception:
        log("WARN MI failed\n" + traceback.format_exc())
        return None


def import_one(path, dest_root, role_folder):
    base = os.path.splitext(os.path.basename(path))[0]
    base = base.replace(" ", "_").replace("-", "_")
    dest = dest_root + "/" + base
    ext = os.path.splitext(path)[1].lower()
    try:
        if eal.does_directory_exist(dest):
            eal.delete_directory(dest)
        ensure_dir(dest)
        if ext in (".gltf", ".glb"):
            gltf_import(path, dest, combine=True, skeletal=True)
        elif ext == ".fbx":
            # Interchange for FBX when available, else AssetImportTask
            try:
                gltf_import(path, dest, combine=True, skeletal=True)
            except Exception:
                asset_import_task(path, dest, "SK_" + base)
        elif ext == ".obj":
            asset_import_task(path, dest, "SM_" + base)
        else:
            log("SKIP non-mesh " + path)
            return None
        meshes = meshes_in(dest)
        if not meshes:
            # textures-only import may land in dest; still OK
            log("WARN no mesh after import " + path)
            return None
        # Prefer skeletal
        sk = [m for m in meshes if isinstance(m, unreal.SkeletalMesh)]
        main = sk[0] if sk else meshes[0]
        for m in meshes:
            if isinstance(m, unreal.StaticMesh):
                if try_enable_nanite(m):
                    log("NANITE " + m.get_path_name())
        mi = apply_recolor_mi(main, role_folder, dest_root)
        for p in eal.list_assets(dest, recursive=True, include_folder=False):
            eal.save_asset(p.split(".")[0], only_if_is_dirty=True)
        eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
        result = {
            "source": path,
            "asset": main.get_path_name().split(".")[0],
            "class": main.get_class().get_name(),
            "mi": mi,
            "primary": os.path.basename(path) == PRIMARIES.get(role_folder),
        }
        log("OK   " + result["asset"])
        return result
    except Exception:
        log("FAIL " + path + "\n" + traceback.format_exc())
        return None


def import_role(role_key, folder):
    src_folder = os.path.join(SRC, folder)
    dest = DEST_ROOT + "/" + folder
    ensure_dir(dest)
    results = []
    if not os.path.isdir(src_folder):
        log("MISSING source " + src_folder)
        return results
    # Import primary first
    primary = PRIMARIES.get(folder)
    ordered = []
    for root, _dirs, files in os.walk(src_folder):
        for f in files:
            ext = os.path.splitext(f)[1].lower()
            if ext in MESH_EXTS:
                ordered.append(os.path.join(root, f))
    ordered.sort(key=lambda p: (0 if os.path.basename(p) == primary else 1, os.path.basename(p).lower()))
    for path in ordered:
        r = import_one(path, dest, folder)
        if r:
            results.append(r)
    return results


def update_manifest(imported):
    path = MANIFEST_CONTENT if os.path.isfile(MANIFEST_CONTENT) else MANIFEST_CONTRIB
    data = {}
    if os.path.isfile(path):
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
    data["imported"] = imported
    data["import_dest_root"] = DEST_ROOT
    soft = data.get("soft_paths") or {}
    for soft_key, folder in ROLES.items():
        entry = soft.get(soft_key) or {}
        entry["folder"] = folder
        entry["soft_path"] = DEST_ROOT + "/" + folder
        # find primary imported asset
        prim = None
        for item in imported.get(soft_key, []):
            if item.get("primary"):
                prim = item.get("asset")
                break
        if not prim and imported.get(soft_key):
            prim = imported[soft_key][0].get("asset")
        entry["imported_primary"] = prim
        soft[soft_key] = entry
    data["soft_paths"] = soft
    os.makedirs(os.path.dirname(MANIFEST_CONTENT), exist_ok=True)
    with open(MANIFEST_CONTENT, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    # Mirror to contrib if present on PC
    try:
        os.makedirs(os.path.dirname(MANIFEST_CONTRIB), exist_ok=True)
        with open(MANIFEST_CONTRIB, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except Exception:
        pass
    log("MANIFEST " + MANIFEST_CONTENT)


def main():
    ensure_dir(DEST_ROOT)
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    log("SRC " + SRC)
    imported = {}
    for soft_key, folder in ROLES.items():
        log("==== " + soft_key + " -> " + folder)
        imported[soft_key] = import_role(soft_key, folder)
    update_manifest(imported)
    with open(REPORT, "w", encoding="utf-8") as f:
        f.write("\n".join(LOG))
    ok = sum(len(v) for v in imported.values())
    log("DONE imported_meshes=%d" % ok)
    print("[suit-models] done: %d meshes" % ok)


if __name__ == "__main__":
    main()
