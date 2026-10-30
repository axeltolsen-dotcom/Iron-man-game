"""
Create the game's Sound Class hierarchy and route every voice line through SC_Voice.

    /Game/Audio/Classes/SC_Master
        SC_SFX     (also the project's default sound class, see Config/DefaultEngine.ini [/Script/Engine.AudioSettings])
        SC_Voice   (every SoundWave under /Game/Audio and /Game/StoryAudio)
        SC_Music

UArenaGameUserSettings drives the SFX / Voice / Music sliders through a sound mix with per-class overrides
(Master stays on the audio device). The C++ also sets SC_Voice as SoundClassOverride on voice components,
so a re-imported line that lost its class still lands in the Voice slider.

Idempotent. Import_jarvis_audio.py calls it after importing, or run it alone:
  * Editor:   Tools > Execute Python Script... > Content/Python/setup_sound_classes.py
  * Headless: UnrealEditor-Cmd <path>/IronManArena.uproject -run=pythonscript -script="<path>/Content/Python/setup_sound_classes.py" -unattended -nullrhi
"""
import unreal

CLASS_DIR = "/Game/Audio/Classes"
MASTER = "SC_Master"
CHILDREN = ["SC_SFX", "SC_Voice", "SC_Music"]
VOICE_ROOTS = ["/Game/Audio", "/Game/StoryAudio"]

eal = unreal.EditorAssetLibrary


def load_or_create(name):
    path = CLASS_DIR + "/" + name
    if eal.does_asset_exist(path):
        asset = eal.load_asset(path)
    else:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, CLASS_DIR, unreal.SoundClass, unreal.SoundClassFactory())
    if not isinstance(asset, unreal.SoundClass):
        raise RuntimeError("could not create or load sound class " + path)
    return asset


def setup_classes():
    master = load_or_create(MASTER)
    kids = {n: load_or_create(n) for n in CHILDREN}
    children = list(master.get_editor_property("child_classes"))
    for name in CHILDREN:
        kid = kids[name]
        if kid not in children:
            children.append(kid)
            # One child per edit: SoundClass::PostEditChangeProperty parents only the first new child it finds.
            master.set_editor_property("child_classes", children)
    for asset in [master] + list(kids.values()):
        eal.save_loaded_asset(asset, False)
    for name, kid in kids.items():
        parent = kid.get_editor_property("parent_class")
        unreal.log("Sound class %s/%s parent: %s" % (CLASS_DIR, name, parent.get_name() if parent else "NONE"))
    return master, kids


def route_voices(voice_class):
    changed = 0
    total = 0
    for root in VOICE_ROOTS:
        if not eal.does_directory_exist(root):
            continue
        for path in eal.list_assets(root, recursive=True, include_folder=False):
            if "/Classes/" in path:
                continue
            asset = eal.load_asset(path)
            if not isinstance(asset, unreal.SoundWave):
                continue
            total += 1
            if asset.get_editor_property("sound_class_object") != voice_class:
                asset.set_editor_property("sound_class_object", voice_class)
                eal.save_loaded_asset(asset, False)
                changed += 1
    unreal.log("Voice routing: %d SoundWaves under %s, %d newly set to SC_Voice" % (total, ", ".join(VOICE_ROOTS), changed))
    return total, changed


def main():
    _, kids = setup_classes()
    route_voices(kids["SC_Voice"])


if __name__ == "__main__":
    main()
