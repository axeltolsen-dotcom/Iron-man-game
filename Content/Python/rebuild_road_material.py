"""Rebuild only M_RoadUV and MI_Env_Road4Lane / MI_Env_Road2Lane with the cook-safe graph.

Does not reimport meshes or textures. Run headless:

  UnrealEditor-Cmd <proj>.uproject -run=pythonscript -script="<proj>/Content/Python/rebuild_road_material.py" -unattended -nullrhi

Report: Saved/Env/rebuild_road_material.txt
"""
import os
import traceback
import unreal

import import_env_assets as env

REPORT = os.path.join(env.PROJECT, "Saved", "Env", "rebuild_road_material.txt")


def main():
    try:
        road = env.build_road_master()
        env.make_mi("MI_Env_Road4Lane", road, {"UTiles": 1.0, "VTileMeters": 14.0}, "Road007")
        env.make_mi("MI_Env_Road2Lane", road, {"UTiles": 1.0, "VTileMeters": 9.0}, "Road008A")
        env.log("rebuild_road_material: done")
    except Exception:
        env.log("FAIL rebuild_road_material\n%s" % traceback.format_exc())
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as f:
        f.write("\n".join(env.LOG) + "\n")


# UE -run=pythonscript may exec this file with a name other than __main__.
main()
