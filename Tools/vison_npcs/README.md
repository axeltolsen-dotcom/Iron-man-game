# Tools/vison_npcs (Vison)

NPC material pack for Jarvis / Claude to merge. Values live in `npc_materials.json` (regenerate with `gen_npc_materials.py`). The editor script `apply_npc_materials.py` is a dry run by default and writes only under `/Game/Vison/NPCs/`.

Look notes: `Docs/npc_look.md`. Overlaps: `OVERLAPS.md`. PilotAvatar tint skip on AvatarCatalog Tier >= 2: `npc_pilotavatar_skin_tint.patch` (against `origin/master` @ `9d50df0`).

Jarvis (10 Nov 2026):

- CityKit street crowd: default-lit MIs only (no Subsurface Profile, eye, hair, or SSS).
- `SP_Skin` and full skin/eye/hair shading: hero / close-up only (player, Harley, Morgan, cinematics).
- No children in the street crowd (story characters only; Morgan). If that ever changes, the required safety rules are under `crowd_mix.story_characters_only.required_if_ever_changed`.
- Skip the PilotAvatar skin-tone multiply on Tier >= 2; `TintMultiplier` defaults to 1.
- Skin-scatter units are named and explicitly unverified in UE 5.8.
- Kevin / Character Creator / MetaHuman skin parents are found by a runtime scan of `/Game/Kevin`, `/Game/CharacterCreator`, `/Game/Reallusion`, `/Game/MetaHumans/<Name>/`, and paths containing `Kevin` / `CharacterCreator` / `Reallusion` / `CC_Base`. Do not hardcode `/Game/Characters/Kevin/...`.

From this directory:

    python3 gen_npc_materials.py
    python3 apply_npc_materials.py                # offline dry run, usage=all
    python3 apply_npc_materials.py --usage crowd  # CityKit default-lit only
    python3 apply_npc_materials.py --usage hero
