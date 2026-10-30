"""Import only The Second Signal's voice takes (Jocasta side mission) without re-importing every line.

    SourceAudio/Audio/JOCASTA/JOCASTA_<event>_<n>.wav                 -> /Game/Audio/JOCASTA/...
    SourceAudio/StoryAudio/<SPEAKER>/sm_jocasta_second_signal_*.wav   -> /Game/StoryAudio/<SPEAKER>/...

Generate the WAVs first:  /workspace/kokoro-venv/bin/python Tools/generate_story_audio.py --missions sm_jocasta_second_signal
Headless: UnrealEditor-Cmd <path>/IronManArena.uproject -run=pythonscript -script="<path>/Content/Python/import_jocasta_audio.py" -unattended -nullrhi
Routes the new SoundWaves to SC_Voice (setup_sound_classes)."""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_sound_classes  # noqa: E402

PREFIX = "sm_jocasta_second_signal_"


def task(path, dest, name):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    return t


def main():
    src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceAudio")
    tasks = []
    ev = os.path.join(src, "Audio", "JOCASTA")
    if os.path.isdir(ev):
        for n in sorted(os.listdir(ev)):
            if n.lower().endswith(".wav"):
                tasks.append(task(os.path.join(ev, n), "/Game/Audio/JOCASTA", os.path.splitext(n)[0]))
    story = os.path.join(src, "StoryAudio")
    for folder in sorted(os.listdir(story)) if os.path.isdir(story) else []:
        d = os.path.join(story, folder)
        if not os.path.isdir(d):
            continue
        for n in sorted(os.listdir(d)):
            if n.startswith(PREFIX) and n.lower().endswith(".wav"):
                tasks.append(task(os.path.join(d, n), "/Game/StoryAudio/" + folder, os.path.splitext(n)[0]))
    if not tasks:
        unreal.log_error("import_jocasta_audio: no WAVs found under %s" % src)
        return
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    done = sum(len(t.imported_object_paths) for t in tasks)
    unreal.log("import_jocasta_audio: %d / %d assets imported" % (done, len(tasks)))
    setup_sound_classes.main()


main()
