"""Import Quaternius UAL GLBs into /Game/NPC/Anims/.

Headless:
  UnrealEditor-Cmd.exe IronManArena.uproject -run=pythonscript -script=Content/Python/import_npc_anims.py

Source files: SourceAssets/NPC/Anims/UAL1_Standard.glb, UAL2_Standard.glb (CC0, Quaternius).
Creates Animation Sequences under /Game/NPC/Anims/UAL1 and /Game/NPC/Anims/UAL2 when the
interchange/GLTF pipeline exposes them. Retargeting to SK_Mannequin is documented in
contrib/npc/docs/RETARGET_AND_GAS.md (IK Retargeter usually needs a one-time editor click).
"""
import os
import unreal

SRC_REL = os.path.join("SourceAssets", "NPC", "Anims")
FILES = [
    ("UAL1_Standard.glb", "/Game/NPC/Anims/UAL1"),
    ("UAL2_Standard.glb", "/Game/NPC/Anims/UAL2"),
    ("Mannequin_F.glb", "/Game/NPC/Anims/Mannequin_F"),
]


def import_one(path, dest):
    if not os.path.isfile(path):
        unreal.log_warning("Missing %s" % path)
        return None
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.automated = True
    t.replace_existing = True
    t.save = True
    # Prefer Interchange if available; AssetImportTask still works for GLTF in 5.x
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return t


def main():
    project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    src = os.path.join(project, SRC_REL)
    unreal.log("NPC anim import from %s" % src)
    for name, dest in FILES:
        path = os.path.join(src, name)
        unreal.log("Importing %s -> %s" % (path, dest))
        t = import_one(path, dest)
        if t is None:
            continue
        for p in t.imported_object_paths or []:
            unreal.log("  imported %s" % p)
    # Tag a marker data asset / json note via EditorAssetLibrary
    note = "/Game/NPC/Anims/IMPORT_NOTE"
    if not unreal.EditorAssetLibrary.does_asset_exist(note):
        # Just ensure folders exist
        unreal.EditorAssetLibrary.make_directory("/Game/NPC/Anims")
    unreal.log("NPC anim import finished. Retarget to UE5 Manny via IK Retargeter if bone names differ — see RETARGET_AND_GAS.md")


main()
