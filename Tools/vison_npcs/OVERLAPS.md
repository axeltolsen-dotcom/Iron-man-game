# NPC look: overlaps with other agents (Vison)

| Agent / folder | What they own | Overlap with Tools/vison_npcs | Resolution |
|---|---|---|---|
| npc worker (`Content/NPC/npc_manifest.json`) | UAL anims, Piper voices, manifest | None (no materials there) | NPC MIs can be referenced from npc_manifest looks later; Jarvis decides |
| Jocasta (`humans_plan.md`) | Kevin/MetaHuman plan, ACivilianNPC proposal, idle/face anim | Her step 4 "fix up Kevin materials" (SSS, eye, hair) | My hero values fill her step 4; script parents to a scanned Kevin/CC/MetaHuman skin if present. Civilians-never-take-damage stays her call; street-crowd children are not used (C3 resolved) |
| Edith (enemies, suits) | M_EnemyMaster, M_SuitMaster | None (no human surfaces) | Robots/enemies untouched |
| U / Dum-E / Friday | suit + env materials, textures | Dum-E CC0 fabric textures (Fabric004) could feed weave normals | Optional; ask Dum-E for paths |
| Ultron (world_realism, perf_budget) | MPC_WorldWeather, budgets, overlays | Read Wetness/Dust/Ash; budget line | **C1 decided:** no 0.8 ms SSS line on the 12 CityKit peds; crowd stays default-lit; no characters cut was requested from the High budget. Never write his MPC |
| Vison camera/lighting/suits | Tools/vison_camera, Content/Story/{camera_realism,lighting_presets,suit_materials}.json | Same EV100, nits/unit 1000, grade caps | Consistent by construction |
| Source (Jarvis) | CityLife, PilotAvatar, StoryDirector, Career tables | Tint path (C2), children (C3) | **C2 resolved:** skip PilotAvatar skin-tone multiply on Tier >= 2; TintMultiplier defaults to 1. **C3 resolved:** no children in the street crowd; Morgan is story-only; safety rules live under `crowd_mix.story_characters_only.required_if_ever_changed` |
