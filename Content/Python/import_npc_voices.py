"""Import NPC bark WAVs as SoundWaves under /Game/NPC/Voices/<VoiceId>/.

Headless:
  UnrealEditor-Cmd.exe IronManArena.uproject -run=pythonscript -script=Content/Python/import_npc_voices.py
"""
import json
import os
import unreal

sys_path_setup = os.path.dirname(os.path.abspath(__file__))
import sys
sys.path.insert(0, sys_path_setup)
try:
    import setup_sound_classes
except Exception:
    setup_sound_classes = None


def main():
    project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    src_root = os.path.join(project, "SourceAssets", "NPC", "Voices")
    man_path = os.path.join(src_root, "voices_manifest.json")
    if not os.path.isdir(src_root):
        unreal.log_error("No SourceAssets/NPC/Voices at %s" % src_root)
        return

    tasks = []
    for voice_id in sorted(os.listdir(src_root)):
        folder = os.path.join(src_root, voice_id)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.lower().endswith(".wav"):
                continue
            t = unreal.AssetImportTask()
            t.filename = os.path.join(folder, name)
            t.destination_path = "/Game/NPC/Voices/%s" % voice_id
            t.destination_name = os.path.splitext(name)[0]
            t.automated = True
            t.replace_existing = True
            t.save = True
            tasks.append(t)

    unreal.log("Importing %d NPC voice WAVs ..." % len(tasks))
    batch = 40
    for i in range(0, len(tasks), batch):
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks[i:i + batch])
    imported = sum(len(t.imported_object_paths) for t in tasks)
    unreal.log("NPC voice import done: %d / %d" % (imported, len(tasks)))

    # Copy manifest into Content for runtime soft lookup
    content_man = os.path.join(project, "Content", "NPC", "npc_voices_manifest.json")
    os.makedirs(os.path.dirname(content_man), exist_ok=True)
    if os.path.isfile(man_path):
        with open(man_path, "r", encoding="utf-8") as f:
            data = f.read()
        with open(content_man, "w", encoding="utf-8") as f:
            f.write(data)
        unreal.log("Wrote Content/NPC/npc_voices_manifest.json")

    if setup_sound_classes is not None:
        try:
            setup_sound_classes.main()
        except Exception as e:
            unreal.log_warning("setup_sound_classes: %s" % e)


main()
