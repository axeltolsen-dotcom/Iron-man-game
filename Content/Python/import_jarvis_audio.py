"""
Import the generated AI voice WAVs as SoundWave assets.

    SourceAudio/Audio/<PERSONA>/<LineId>.wav        ->  /Game/Audio/<PERSONA>/<LineId>        (SoundWave)
    SourceAudio/StoryAudio/<SPEAKER>/<LineId>.wav   ->  /Game/StoryAudio/<SPEAKER>/<LineId>  (story mode)

The raw WAVs live OUTSIDE Content/ (top-level SourceAudio/, shipped separately as
IronManArena-SourceAudio.zip); only the imported .uasset files are in Content. After importing,
setup_sound_classes.py creates /Game/Audio/Classes/SC_Master + SC_SFX/SC_Voice/SC_Music and
routes every imported line to SC_Voice.

UJarvisComponent fills every line's Sound slot at BeginPlay by looking up exactly that path
(/Game/Audio/<PersonaFolder>/<LineId>), so once this has run every line has its voice - there is
no manual slot assignment. UStoryLibrary does the same for story lines under /Game/StoryAudio
(see SourceAudio/StoryAudio/story_lines.csv, written by Tools/generate_story_audio.py).

Run it either way:
  * Editor: Tools > Execute Python Script... > Content/Python/import_jarvis_audio.py
    (or in the Output Log's "Python" console:  py "<project>/Content/Python/import_jarvis_audio.py")
  * Headless:
      UnrealEditor-Cmd <path>/IronManArena.uproject -run=pythonscript -script="<path>/Content/Python/import_jarvis_audio.py"
    (on Windows: UnrealEditor-Cmd.exe; on Linux: Engine/Binaries/Linux/UnrealEditor-Cmd)

Requires the Python Editor Script Plugin (enabled in IronManArena.uproject).
Re-running re-imports (replaces) existing assets.
"""
import csv
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_sound_classes  # noqa: E402  (Content/Python)

PERSONAS = ["JARVIS", "FRIDAY", "EDITH", "ULTRON", "WARMACHINE"]


def make_tasks(src_root, dest_root, folders):
    tasks = []
    for folder_name in folders:
        folder = os.path.join(src_root, folder_name)
        if not os.path.isdir(folder):
            unreal.log_warning("missing audio folder %s" % folder)
            continue
        for name in sorted(os.listdir(folder)):
            if not name.lower().endswith(".wav"):
                continue
            t = unreal.AssetImportTask()
            t.filename = os.path.join(folder, name)
            t.destination_path = "%s/%s" % (dest_root, folder_name)
            t.destination_name = os.path.splitext(name)[0]
            t.automated = True
            t.replace_existing = True
            t.save = True
            tasks.append(t)
    return tasks


def check_csv(csv_path, dest_root):
    if not os.path.exists(csv_path):
        return
    missing = 0
    with open(csv_path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            asset = "%s/%s/%s" % (dest_root, os.path.dirname(row["file"]), row["id"])
            if not unreal.EditorAssetLibrary.does_asset_exist(asset):
                missing += 1
    if missing:
        unreal.log_warning("%d lines in %s have no imported asset" % (missing, csv_path))


def main():
    project_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    source_dir = os.path.join(project_dir, "SourceAudio")
    audio_dir = os.path.join(source_dir, "Audio")
    story_audio_dir = os.path.join(source_dir, "StoryAudio")

    tasks = []
    if os.path.isdir(audio_dir):
        tasks += make_tasks(audio_dir, "/Game/Audio", PERSONAS)
    else:
        unreal.log_error("No SourceAudio/Audio folder at %s - unzip IronManArena-SourceAudio.zip or run Tools/generate_suits_and_voices.py" % audio_dir)
    if os.path.isdir(story_audio_dir):
        speakers = sorted(d for d in os.listdir(story_audio_dir) if os.path.isdir(os.path.join(story_audio_dir, d)))
        tasks += make_tasks(story_audio_dir, "/Game/StoryAudio", speakers)
    else:
        unreal.log_warning("No SourceAudio/StoryAudio folder - unzip IronManArena-SourceAudio.zip or run Tools/generate_story_audio.py")

    if not tasks:
        unreal.log_warning("No WAV files found - only (re)creating the sound classes")
        setup_sound_classes.main()
        return

    unreal.log("Importing %d voice lines ..." % len(tasks))
    with unreal.ScopedSlowTask(len(tasks), "Importing voice lines") as slow:
        slow.make_dialog(True)
        batch = 50
        for i in range(0, len(tasks), batch):
            if slow.should_cancel():
                break
            chunk = tasks[i:i + batch]
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(chunk)
            slow.enter_progress_frame(len(chunk))

    imported = sum(len(t.imported_object_paths) for t in tasks)
    unreal.log("Voice import done: %d / %d assets" % (imported, len(tasks)))

    # Sanity check against the CSVs so missing files are obvious.
    check_csv(os.path.join(audio_dir, "lines.csv"), "/Game/Audio")
    check_csv(os.path.join(story_audio_dir, "story_lines.csv"), "/Game/StoryAudio")

    # Sound classes + route the voice lines to SC_Voice (the import replaces assets, so always redo it).
    setup_sound_classes.main()


main()
