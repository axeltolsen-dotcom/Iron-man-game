"""
Import SourceAssets/CityKit/{Cars,People,Buildings,Props} models into /Game/CityKit/...
Updates Content/CityKit/citykit_manifest.json.

  UnrealEditor-Cmd <proj> -run=pythonscript -script="<proj>/Content/Python/import_citykit_models.py" -unattended -nullrhi
"""
import json
import os
import traceback
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceAssets", "CityKit")
MANIFEST_PATH = os.path.join(PROJECT, "Content", "CityKit", "citykit_manifest.json")
REPORT = os.path.join(PROJECT, "Saved", "CityKit", "models_import_report.txt")
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
LOG = []

CATEGORIES = {
    "Cars": "/Game/CityKit/Cars",
    "People": "/Game/CityKit/People",
    "Buildings": "/Game/CityKit/Buildings",
    "Props": "/Game/CityKit/Props",
}


def log(s):
    LOG.append(s)
    try:
        unreal.log("[citykit-models] " + s)
    except Exception:
        pass


def gltf_import(filename, dest, combine=True, skeletal=False):
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
        mp.set_editor_property("build_nanite", not skeletal)
    except Exception:
        pass
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
    tools.import_asset_tasks([t])
    paths = list(t.get_editor_property("imported_object_paths") or [])
    return [p.split(".")[0] for p in paths]


def meshes_in(folder, cls=None):
    out = []
    for p in eal.list_assets(folder, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if cls is None:
            if isinstance(a, (unreal.StaticMesh, unreal.SkeletalMesh)):
                out.append(a)
        elif isinstance(a, cls):
            out.append(a)
    return out


def import_single_model(path, dest_root, skeletal=False):
    base = os.path.splitext(os.path.basename(path))[0]
    # sanitize
    base = base.replace(" ", "_").replace("-", "_")
    dest = dest_root + "/" + base
    ext = os.path.splitext(path)[1].lower()
    try:
        if eal.does_directory_exist(dest):
            eal.delete_directory(dest)
        if ext in (".gltf", ".glb", ".fbx", ".obj"):
            gltf_import(path, dest, combine=True, skeletal=skeletal)
        else:
            asset_import_task(path, dest, "SM_" + base if not skeletal else "SK_" + base)
        meshes = meshes_in(dest)
        if not meshes:
            log("FAIL no mesh " + path)
            return None
        # Prefer skeletal for people
        if skeletal:
            sk = [m for m in meshes if isinstance(m, unreal.SkeletalMesh)]
            main = sk[0] if sk else meshes[0]
            prefix = "SK_" if isinstance(main, unreal.SkeletalMesh) else "SM_"
        else:
            statics = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
            main = statics[0] if statics else meshes[0]
            prefix = "SM_"
            if isinstance(main, unreal.StaticMesh):
                try:
                    unreal.EditorStaticMeshLibrary.add_simple_collisions(
                        main, unreal.ScriptCollisionShapeType.BOX
                    )
                except Exception:
                    pass
        target = dest + "/" + prefix + base
        cur = main.get_path_name().split(".")[0]
        if cur != target:
            try:
                eal.rename_asset(cur, target)
            except Exception:
                target = cur
        for p in eal.list_assets(dest, recursive=True, include_folder=False):
            eal.save_asset(p.split(".")[0], only_if_is_dirty=True)
        log("OK   " + target)
        return target
    except Exception:
        log("FAIL " + path + "\n" + traceback.format_exc())
        return None


def import_folder_tree(src_folder, dest_root, skeletal=False):
    """Import either flat files or subfolders containing gltf."""
    results = {}
    if not os.path.isdir(src_folder):
        log("NO DIR " + src_folder)
        return results
    for entry in sorted(os.listdir(src_folder)):
        full = os.path.join(src_folder, entry)
        if os.path.isfile(full) and entry.lower().endswith((".glb", ".gltf", ".fbx", ".obj")):
            # skip already-imported prior props that live as folders too
            path = import_single_model(full, dest_root, skeletal=skeletal)
            if path:
                results[entry] = path
        elif os.path.isdir(full):
            # find gltf/glb inside
            model = None
            for root, _, files in os.walk(full):
                for f in files:
                    if f.lower().endswith((".gltf", ".glb")):
                        model = os.path.join(root, f)
                        break
                if model:
                    break
            if not model:
                # fbx in folder?
                for root, _, files in os.walk(full):
                    for f in files:
                        if f.lower().endswith(".fbx"):
                            model = os.path.join(root, f)
                            break
                    if model:
                        break
            if not model:
                log("SKIP empty " + entry)
                continue
            # Skip if this slug already imported in prior CityKit Props run (street_lamp_01 etc)
            existing = dest_root + "/" + entry
            if eal.does_directory_exist(existing) and meshes_in(existing):
                # re-import to refresh
                pass
            path = import_single_model(model, dest_root, skeletal=skeletal)
            if path:
                results[entry] = path
    return results


def load_manifest():
    if os.path.isfile(MANIFEST_PATH):
        with open(MANIFEST_PATH) as f:
            return json.load(f)
    return {"version": 1, "materials": {}, "textures": {}, "hdri": {}, "props": {}, "fallbacks": {}}


def main():
    man = load_manifest()
    man.setdefault("cars", {})
    man.setdefault("people", {})
    man.setdefault("buildings", {})
    man.setdefault("props", {})
    man.setdefault("models_meta", [])

    # Cars
    cars = import_folder_tree(os.path.join(SRC, "Cars"), CATEGORIES["Cars"], skeletal=False)
    man["cars"].update(cars)
    # People (skeletal)
    people = import_folder_tree(os.path.join(SRC, "People"), CATEGORIES["People"], skeletal=True)
    man["people"].update(people)
    # Buildings
    buildings = import_folder_tree(os.path.join(SRC, "Buildings"), CATEGORIES["Buildings"], skeletal=False)
    man["buildings"].update(buildings)
    # Props — only NEW ones not already listed, but import_folder_tree handles all in Props/
    # Avoid re-importing huge prior set: import only files/dirs that are new markers
    props_src = os.path.join(SRC, "Props")
    # Import everything under Props that is Kenney_*, Traffic*, rescue*, or polyhaven slug folders listed
    if os.path.isdir(props_src):
        for entry in sorted(os.listdir(props_src)):
            full = os.path.join(props_src, entry)
            # Skip if already in prior props and is street_lamp_01 / painted_wooden_bench etc from first pass
            prior = {"street_lamp_01", "painted_wooden_bench", "metal_trash_can", "concrete_road_barrier_02", "tree_small_02"}
            if entry in prior and entry in man.get("props", {}):
                log("SKIP prior " + entry)
                continue
            if os.path.isfile(full) and entry.lower().endswith((".glb", ".gltf", ".fbx")):
                path = import_single_model(full, CATEGORIES["Props"], skeletal=False)
                if path:
                    man["props"][entry] = path
            elif os.path.isdir(full):
                model = None
                for root, _, files in os.walk(full):
                    for f in files:
                        if f.lower().endswith((".gltf", ".glb")):
                            model = os.path.join(root, f)
                            break
                    if model:
                        break
                if model:
                    path = import_single_model(model, CATEGORIES["Props"], skeletal=False)
                    if path:
                        man["props"][entry] = path

    # Soft paths + fallbacks for city generator
    fb = man.setdefault("fallbacks", {})
    if man["cars"]:
        fb["Car_Sedan"] = man["cars"].get("Kenney_sedan.glb") or next(iter(man["cars"].values()), None)
        fb["Car_SUV"] = man["cars"].get("Kenney_suv.glb")
        fb["Car_Taxi"] = man["cars"].get("Kenney_taxi.glb")
        fb["Car_Police"] = man["cars"].get("Kenney_police.glb")
        fb["Car_Photoreal"] = man["cars"].get("covered_car")
    if man["people"]:
        fb["Ped_Male"] = man["people"].get("Quaternius_Suit_Male.fbx")
        fb["Ped_Female"] = man["people"].get("Quaternius_Suit_Female.fbx")
        fb["Ped_Worker_Male"] = man["people"].get("Quaternius_Worker_Male.fbx")
        fb["Ped_Worker_Female"] = man["people"].get("Quaternius_Worker_Female.fbx")
        fb["Armor_Male"] = man["people"].get("Quaternius_Knight_Golden_Male.fbx")
        fb["Armor_Female"] = man["people"].get("Quaternius_Knight_Golden_Female.fbx")
    if man["buildings"]:
        fb["Building_Skyscraper"] = man["buildings"].get("Kenney_building_skyscraper_a.glb")
        fb["Building_Facade"] = man["buildings"].get("modular_urban_apartments_facade")
    fb["Helicopter"] = man["props"].get("rescue_helicopter")
    fb["TrafficLight"] = man["props"].get("Kenney_traffic_light.glb") or man["props"].get("TrafficLight_Quaternius.glb")
    fb["FireHydrant"] = man["props"].get("fire_hydrant")

    man["models_meta"] = [
        {"source": "Kenney Car Kit", "license": "CC0", "url": "https://kenney.nl/assets/car-kit"},
        {"source": "Kenney City Kit Commercial", "license": "CC0", "url": "https://kenney.nl/assets/city-kit-commercial"},
        {"source": "Kenney City Kit Roads", "license": "CC0", "url": "https://kenney.nl/assets/city-kit-roads"},
        {"source": "Quaternius Ultimate Animated Character Pack", "license": "CC0", "url": "https://quaternius.com/packs/ultimatedanimatedcharacter.html"},
        {"source": "Poly Haven models", "license": "CC0", "url": "https://polyhaven.com/models"},
        {"source": "Rescue Helicopter (OGA)", "license": "CC-BY", "url": "https://opengameart.org/content/rescue-helicopter-low-poly", "credit": "See AUTHOR file; credit required"},
        {"source": "Traffic Light Quaternius via Poly Pizza", "license": "CC0", "url": "https://poly.pizza/m/lg9AKWejnF"},
    ]

    soft = {}
    for section in ("cars", "people", "buildings", "props", "materials"):
        for k, v in (man.get(section) or {}).items():
            if isinstance(v, str) and v.startswith("/Game/"):
                soft[k] = "%s.%s" % (v, v.split("/")[-1])
    man["soft_object_paths"] = soft

    os.makedirs(os.path.dirname(MANIFEST_PATH), exist_ok=True)
    with open(MANIFEST_PATH, "w") as f:
        json.dump(man, f, indent=2)
    saved = os.path.join(PROJECT, "Saved", "CityKit", "citykit_manifest.json")
    os.makedirs(os.path.dirname(saved), exist_ok=True)
    with open(saved, "w") as f:
        json.dump(man, f, indent=2)
    log("manifest updated " + MANIFEST_PATH)

    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as f:
        f.write("\n".join(LOG) + "\n")


main()
