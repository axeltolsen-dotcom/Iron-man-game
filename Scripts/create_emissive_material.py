"""
Optional: creates /Game/IronManArena/Materials/M_ArenaEmissive, the material the C++ code
prefers for glowing parts (arc reactor, repulsors, thrusters, enemy eyes, projectiles).

Without it the code falls back to /Engine/BasicShapes/BasicShapeMaterial (colour only) and
relies on point lights for glow. With it you get real emissive bloom.

Parameters (names must match ArenaVisuals.cpp):
    Color            (Vector)  -> Base Color, and Emissive = Color * EmissiveStrength
    EmissiveStrength (Scalar)  -> 0 for plain painted parts

Run inside the editor: Tools > Execute Python Script... > pick this file
(or in the Output Log, switch the command box to "Python" and run:
     py "<Project>/Scripts/create_emissive_material.py" )
Requires the Python Editor Script Plugin + Editor Scripting Utilities (both enabled in the .uproject).
"""
import unreal

PACKAGE_PATH = "/Game/IronManArena/Materials"
ASSET_NAME = "M_ArenaEmissive"
FULL_PATH = PACKAGE_PATH + "/" + ASSET_NAME

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

if eal.does_asset_exist(FULL_PATH):
    unreal.log_warning(FULL_PATH + " already exists - delete it first if you want to regenerate it.")
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())

    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.8, 0.1, 0.08, 1.0))

    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    strength.set_editor_property("parameter_name", "EmissiveStrength")
    strength.set_editor_property("default_value", 0.0)

    mult = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 200)
    mel.connect_material_expressions(color, "", mult, "A")
    mel.connect_material_expressions(strength, "", mult, "B")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Slightly metallic, fairly smooth: reads as painted armour.
    metal = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, -150)
    metal.set_editor_property("r", 0.6)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, -50)
    rough.set_editor_property("r", 0.35)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_asset(FULL_PATH)
    unreal.log("Created " + FULL_PATH + ". Restart PIE; the C++ code picks it up automatically.")
