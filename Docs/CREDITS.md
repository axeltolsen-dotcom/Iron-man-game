# Credits

## Textures and HDRIs - Poly Haven (CC0 1.0)

Downloaded by `Tools/fetch_cc0_assets.py` from https://polyhaven.com at 2K and imported by
`Content/Python/import_cc0_textures.py`. CC0 = public domain dedication: free to use, modify and
redistribute, no attribution required. Credited anyway, with thanks.

| Asset | Used for | Source |
|---|---|---|
| garage_floor | workshop floor (Tony's lake-house garage) | https://polyhaven.com/a/garage_floor |
| asphalt_02 | city streets | https://polyhaven.com/a/asphalt_02 |
| brushed_concrete | rooftops / concrete | https://polyhaven.com/a/brushed_concrete |
| concrete_block_wall | building walls | https://polyhaven.com/a/concrete_block_wall |
| brick_wall_001 | city building walls (variety) | https://polyhaven.com/a/brick_wall_001 |
| metal_plate | workshop props, Hall of Armor plinths, robot arms | https://polyhaven.com/a/metal_plate |
| rusty_metal_02 | ruins props, wrecked vehicles | https://polyhaven.com/a/rusty_metal_02 |
| rubble | ruins ground | https://polyhaven.com/a/rubble |
| burned_ground_01 | Titan's battlefield ground | https://polyhaven.com/a/burned_ground_01 |
| industrial_sunset_02_puresky | sky / reflection fallback for outdoor chapters | https://polyhaven.com/a/industrial_sunset_02_puresky |

## Textures and HDRIs - Dume's list (contrib/dume/assets.json, all CC0 1.0)

Fetched at 2K by `Tools/fetch_dume_assets.py` (no login; the script refuses any non-CC0 entry), imported by
`Content/Python/import_cc0_textures.py` into `/Game/Realism/Textures/<id>/`. ambientCG sets are repacked
into one ARM map (R = AO, G = Roughness, B = Metalness). ambientCG (https://ambientcg.com) and Poly Haven
(https://polyhaven.com) publish under CC0 1.0: no attribution needed, credited with thanks.

| Asset | Source | Used for | Page |
|---|---|---|---|
| hangar_concrete_floor | Poly Haven | Tony's workshop / lab floor | https://polyhaven.com/a/hangar_concrete_floor |
| Concrete034 | ambientCG | Alternate garage floor / walls | https://ambientcg.com/view?id=Concrete034 |
| Metal009 | ambientCG | Armor-rack trims, workbench & machinery steel | https://ambientcg.com/view?id=Metal009 |
| garage | Poly Haven | Workshop/garage interior lighting & reflections | https://polyhaven.com/a/garage |
| IndoorEnvironmentHDRI010 | ambientCG | Industrial garage/workshop ambient lighting | https://ambientcg.com/view?id=IndoorEnvironmentHDRI010 |
| tarred_gravel | Poly Haven | Stark Tower & city rooftop surfaces | https://polyhaven.com/a/tarred_gravel |
| Gravel022 | ambientCG | Loose rooftop gravel ballast | https://ambientcg.com/view?id=Gravel022 |
| Asphalt025C | ambientCG | Rain-soaked night streets | https://ambientcg.com/view?id=Asphalt025C |
| asphalt_02 | Poly Haven | Dry city streets and parking lots | https://polyhaven.com/a/asphalt_02 |
| red_brick_03 | Poly Haven | Older city building exteriors | https://polyhaven.com/a/red_brick_03 |
| Facade001 | ambientCG | Reflective glass skyscraper exteriors | https://ambientcg.com/view?id=Facade001 |
| Facade017 | ambientCG | Office tower window grids | https://ambientcg.com/view?id=Facade017 |
| rebar_reinforced_concrete | Poly Haven | Battle-damaged walls with exposed rebar | https://polyhaven.com/a/rebar_reinforced_concrete |
| damaged_concrete_floor | Poly Haven | Cracked floors in destroyed zones | https://polyhaven.com/a/damaged_concrete_floor |
| rubble | Poly Haven | Debris piles after repulsor/missile impacts | https://polyhaven.com/a/rubble |
| Ground111 | ambientCG | Urban construction debris ground cover | https://ambientcg.com/view?id=Ground111 |
| kloofendal_48d_partly_cloudy_puresky | Poly Haven | Daytime flight sky | https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky |
| the_sky_is_on_fire | Poly Haven | Sunset flight / cinematic sky | https://polyhaven.com/a/the_sky_is_on_fire |
| kloofendal_overcast_puresky | Poly Haven | Overcast / stormy flight sky | https://polyhaven.com/a/kloofendal_overcast_puresky |

## Fonts (SIL Open Font License 1.1)

Downloaded by `Tools/fetch_ui_fonts.py` from the google/fonts repository; each family's OFL.txt is kept
next to the font in `SourceAssets/Fonts/<family>/` and copied to `Docs/licenses/`. Imported as
`/Game/UI/Fonts/F_*` for EDITH's helmet HUD (Roboto, the engine font, is the fallback).

(copyright lines from each OFL.txt)
- Rajdhani - Copyright (c) 2014, Indian Type Foundry
- Share Tech Mono - Copyright (c) 2012, Carrois Type Design, Ralph du Carrois (Reserved Font Name 'Share')
- Exo 2 - Copyright 2013 The Exo 2 Project Authors
- Orbitron - Copyright 2018 The Orbitron Project Authors (Reserved Font Name "Orbitron")
- Saira Stencil One - Copyright 2019 The Saira Stencil Project Authors

## Lighting, HUD, suit and story data (team contributions)

- `Content/Story/lighting_presets.json` - Vison (physical sun 120000 lux / 5900 K, EV100 exposure presets).
- `Content/UI/hud_layout.json`, `HelmetHUD*` C++ - EDITH (contrib/edith).
- `Content/Story/suit_materials.json` - U. `Content/Story/codex.json`, `side_missions.json` - Friday.
- `contrib/dume/assets.json` (the CC0 asset list above) - Dume.
- `Content/Story/armor_kit.json` (procedural armour plates, generated at runtime by `SuitArmorKit.cpp`) - U.
- `Content/UI/Materials/M_HUD_*` / `MI_HUD_*` (made by `contrib/edith/create_hud_materials.py`) - EDITH.
- `Content/City/city_layout.json` (2738 buildings, 404 roads, 196 blocks, 175 spawn points; built at runtime by `CityLayout.cpp`) - FRIDAY.
- Perf budget (max 12 active enemies, scalability tiers) - Ultron.

## Engine content

- UE5 Manny / Quinn mannequins, engine basic shapes and materials: Epic Games, used under the
  Unreal Engine EULA (engine content, redistributable inside Unreal Engine projects).

## Music

- `Content/Music/S_FlashbackSwell` - original, procedurally synthesised (numpy additive pad, no samples) by
  `Tools/make_flashback_score.py`; released CC0 with this project.

## Voices

- All voice lines are generated by `Tools/generate_suits_and_voices.py` / `Tools/generate_story_audio.py`
  (offline TTS on the build box); no third-party recordings.

Iron Man, Vision and related names are Marvel characters; this is a non-commercial fan project.
