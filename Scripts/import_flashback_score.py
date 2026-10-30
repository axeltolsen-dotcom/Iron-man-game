"""Import SourceMusic/flashback_swell.wav as /Game/Music/S_FlashbackSwell (headless: -run=pythonscript)."""
import os
import unreal

src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceMusic", "flashback_swell.wav")
task = unreal.AssetImportTask()
task.filename = src
task.destination_path = "/Game/Music"
task.destination_name = "S_FlashbackSwell"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset = unreal.load_asset("/Game/Music/S_FlashbackSwell")
if asset:
    try:
        asset.set_editor_property("looping", False)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    except Exception as exc:  # property names differ between versions; the default is non-looping anyway
        unreal.log_warning("[Score] could not set looping: %s" % exc)
    unreal.log("[Score] imported %s (%.1f s)" % (asset.get_path_name(), asset.get_editor_property("duration")))
else:
    unreal.log_error("[Score] import failed for %s" % src)
