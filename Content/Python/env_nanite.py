"""Nanite pass (Ultron perf budget §3/§4): enable Nanite on every static mesh under /Game/Env
and make project-owned Nanite copies of the engine basic shapes at /Game/Env/Shapes/SM_<Shape>
(ArenaVisuals::GetShapeMesh prefers them). Run headless:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi -nocrashreports
Report: Saved/Env/nanite_report.txt"""
import os
import unreal

eal = unreal.EditorAssetLibrary
# The StaticMeshEditorSubsystem is not available under -run=pythonscript; set_editor_property
# fires PostEditChangeProperty, which rebuilds the mesh with the new Nanite settings.
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
log = []

def enable(mesh):
    s = mesh.get_editor_property("nanite_settings")
    if s.get_editor_property("enabled"):
        return "already"
    s.set_editor_property("enabled", True)
    if sms:
        sms.set_nanite_settings(mesh, s, apply_changes=True)
    else:
        mesh.set_editor_property("nanite_settings", s)
    return "enabled"

for shape in ("Cube", "Sphere", "Cylinder", "Cone"):
    dst = "/Game/Env/Shapes/SM_" + shape
    if not eal.does_asset_exist(dst):
        eal.duplicate_asset("/Engine/BasicShapes/" + shape, dst)
    m = eal.load_asset(dst)
    if m:
        log.append("OK   %s %s" % (dst, enable(m)))
        eal.save_asset(dst, only_if_is_dirty=False)
    else:
        log.append("FAIL %s" % dst)

for path in eal.list_assets("/Game/Env", recursive=True, include_folder=False):
    data = eal.find_asset_data(path)
    if str(data.asset_class_path.asset_name) != "StaticMesh" or "/Env/Shapes/" in path:
        continue
    m = eal.load_asset(path)
    try:
        r = enable(m)
        eal.save_asset(path.split(".")[0], only_if_is_dirty=False)
        log.append("OK   %s %s" % (path.split(".")[0], r))
    except Exception as e:
        log.append("FAIL %s %s" % (path, e))

out = os.path.join(PROJECT, "Saved", "Env")
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "nanite_report.txt"), "w") as f:
    f.write("\n".join(log) + "\n")
unreal.log("env_nanite: %d meshes" % len(log))
