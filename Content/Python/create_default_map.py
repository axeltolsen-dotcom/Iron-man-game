"""
Creates the default map /Game/Maps/L_IronManArena (GameDefaultMap + EditorStartupMap):
  * AArenaEnvironment at the origin (floor, walls, pillars, sun, sky, fog, post) so the editor
    viewport shows the arena and Play has a lit level; Story mode adds its chapter sets at runtime.
  * PlayerStart 1.2 m above the floor facing +X.
  * World Settings GameMode Override = AArenaGameMode.
Re-running replaces the map.

  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<proj>/Content/Python/create_default_map.py" -unattended -nullrhi
"""
import unreal

MAP = "/Game/Maps/L_IronManArena"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    unreal.EditorAssetLibrary.delete_asset(MAP)
ok = les.new_level(MAP, False)
if not ok:
    raise RuntimeError("new_level failed for " + MAP)

env = eas.spawn_actor_from_class(unreal.ArenaEnvironment, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
env.set_actor_label("ArenaEnvironment")
ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 120), unreal.Rotator(0, 0, 0))
ps.set_actor_label("PlayerStart")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.ArenaGameMode)

if not les.save_current_level():
    raise RuntimeError("save_current_level failed")
actors = [a.get_class().get_name() for a in eas.get_all_level_actors()]
unreal.log("create_default_map: saved %s with %d actors: %s" % (MAP, len(actors), ", ".join(sorted(set(actors)))))
