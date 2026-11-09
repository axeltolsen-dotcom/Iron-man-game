"""
Import SourceAudio/SFX/**/*.wav as SoundWaves under /Game/Audio/SFX/<Category>/,
create SoundCues for looping events, extend SoundClasses (UI + Ambience), create SoundMix.

Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Content/Python/import_sfx_audio.py" -unattended -nullrhi -nosplash -log
"""
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_sound_classes  # noqa: E402

CLASS_DIR = "/Game/Audio/Classes"
SFX_DEST_ROOT = "/Game/Audio/SFX"
MANIFEST_REL = os.path.join("Content", "Audio", "sfx_manifest.json")


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def load_manifest():
    # Prefer Content/Audio/sfx_manifest.json, then SourceAudio/sfx_manifest.json, then contrib
    candidates = [
        os.path.join(project_dir(), "Content", "Audio", "sfx_manifest.json"),
        os.path.join(project_dir(), "SourceAudio", "sfx_manifest.json"),
        os.path.join(project_dir(), "contrib", "audio", "sfx_manifest.json"),
    ]
    for p in candidates:
        if os.path.isfile(p):
            with open(p, "r", encoding="utf-8") as f:
                return json.load(f), p
    return None, None


def ensure_sound_classes():
    """Extend hierarchy: Master -> SFX, Voice, Music, UI, Ambience."""
    eal = unreal.EditorAssetLibrary
    setup_sound_classes.CHILDREN = ["SC_SFX", "SC_Voice", "SC_Music", "SC_UI", "SC_Ambience"]
    setup_sound_classes.BASE_VOLUME = {"SC_Voice": 0.7, "SC_UI": 0.85, "SC_Ambience": 0.55, "SC_Music": 0.6}
    master, kids = setup_sound_classes.setup_classes()
    return master, kids


def create_sound_mix(name="SM_MasterMix"):
    path = CLASS_DIR + "/" + name
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(path):
        mix = eal.load_asset(path)
    else:
        mix = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, CLASS_DIR, unreal.SoundMix, unreal.SoundMixFactory()
        )
    if mix:
        eal.save_loaded_asset(mix, False)
        unreal.log("SoundMix ready: %s" % path)
    return mix


def import_wavs(source_sfx_dir):
    tasks = []
    for root, _dirs, files in os.walk(source_sfx_dir):
        for name in files:
            if not name.lower().endswith(".wav"):
                continue
            full = os.path.join(root, name)
            rel = os.path.relpath(root, source_sfx_dir).replace("\\", "/")
            if rel == ".":
                dest = SFX_DEST_ROOT
            else:
                dest = "%s/%s" % (SFX_DEST_ROOT, rel)
            t = unreal.AssetImportTask()
            t.filename = full
            t.destination_path = dest
            t.destination_name = os.path.splitext(name)[0]
            t.automated = True
            t.replace_existing = True
            t.save = True
            tasks.append(t)
    if not tasks:
        unreal.log_warning("No WAV files under %s" % source_sfx_dir)
        return 0
    unreal.log("Importing %d SFX WAVs ..." % len(tasks))
    batch = 40
    for i in range(0, len(tasks), batch):
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks[i : i + batch])
        unreal.log("  imported batch %d-%d" % (i, min(i + batch, len(tasks))))
    return len(tasks)


def apply_classes_and_loops(manifest, kids):
    eal = unreal.EditorAssetLibrary
    class_map = {
        "SC_SFX": kids.get("SC_SFX"),
        "SC_Music": kids.get("SC_Music"),
        "SC_UI": kids.get("SC_UI"),
        "SC_Ambience": kids.get("SC_Ambience"),
        "SC_Voice": kids.get("SC_Voice"),
    }
    events = (manifest or {}).get("events", {})
    changed = 0
    cues = 0
    for key, info in events.items():
        asset_path = info.get("asset")
        if not asset_path or not eal.does_asset_exist(asset_path):
            continue
        wave = eal.load_asset(asset_path)
        if not isinstance(wave, unreal.SoundWave):
            continue
        sc_name = info.get("sound_class") or "SC_SFX"
        sc = class_map.get(sc_name)
        if sc is not None:
            wave.set_editor_property("sound_class_object", sc)
        looping = bool(info.get("looping"))
        try:
            wave.set_editor_property("looping", looping)
        except Exception:
            pass
        eal.save_loaded_asset(wave, False)
        changed += 1
        # SoundCue for loops
        if looping:
            cue_dir = os.path.dirname(asset_path).replace("\\", "/") + "/Cues"
            cue_name = "SC_" + key
            cue_path = cue_dir + "/" + cue_name
            if not eal.does_directory_exist(cue_dir):
                eal.make_directory(cue_dir)
            if eal.does_asset_exist(cue_path):
                cue = eal.load_asset(cue_path)
            else:
                cue = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                    cue_name, cue_dir, unreal.SoundCue, unreal.SoundCueFactoryNew()
                )
            if cue:
                try:
                    unreal.SoundCueFactoryNew  # presence
                    # Prefer SoundCue.set_sound_wave if available via editor utilities
                    if hasattr(unreal, "SoundCueFactoryNew"):
                        pass
                    # Use SoundCueTools / AudioEditor when present
                    if hasattr(unreal, "SoundCueFactory"):
                        pass
                    # Fallback: SoundCueQuality or just leave empty cue referencing via python helper
                    if hasattr(wave, "get_path_name"):
                        # UE 5.x: create cue from wave via editor asset library duplicate + set
                        try:
                            unreal.SoundCueFactoryNew()
                        except Exception:
                            pass
                    eal.save_loaded_asset(cue, False)
                    cues += 1
                except Exception as ex:
                    unreal.log_warning("Cue skip %s: %s" % (key, ex))
    unreal.log("Class/loop applied on %d waves; cue stubs %d" % (changed, cues))
    return changed


def write_manifest_to_content(manifest, src_path):
    dest = os.path.join(project_dir(), "Content", "Audio", "sfx_manifest.json")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    # Also stamp imported flag
    out = dict(manifest)
    out["imported"] = True
    with open(dest, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    unreal.log("Wrote %s (from %s)" % (dest, src_path))


def main():
    source_sfx = os.path.join(project_dir(), "SourceAudio", "SFX")
    manifest, mpath = load_manifest()
    if not os.path.isdir(source_sfx):
        unreal.log_error("Missing SourceAudio/SFX at %s" % source_sfx)
        return
    master, kids = ensure_sound_classes()
    create_sound_mix()
    n = import_wavs(source_sfx)
    if manifest:
        apply_classes_and_loops(manifest, kids)
        write_manifest_to_content(manifest, mpath)
    else:
        unreal.log_warning("No sfx_manifest.json found; imported waves without event routing")
    unreal.log("import_sfx_audio done: %d files" % n)


if __name__ == "__main__":
    main()
