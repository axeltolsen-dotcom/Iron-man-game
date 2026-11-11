# NPC look pass: real people under real light (Vison)

Requested by Axel ("make look real", and include men, women, boys and girls). This is a proposal for Jarvis to merge. Values are in `Tools/vison_npcs/npc_materials.json`, and the editor script is `Tools/vison_npcs/apply_npc_materials.py`, which is a dry run by default and writes only to `/Game/Vison/NPCs/`.

Lane: materials, shading, how NPCs sit under the lighting presets, and camera/post interaction. AI, behaviour, spawning and animation belong to Jarvis, Jocasta and the npc worker. Where they need changing, it is noted for Jarvis in §9.

All numbers marked [E] are estimates. None of this has been rendered in Unreal yet.

Jarvis decisions (10 Nov 2026) are already applied in the JSON and scripts:
- CityKit street crowd uses **default-lit MIs only** (no Subsurface Profile, eye, hair or SSS shading).
- `SP_Skin` profiles and full skin/eye/hair shading are **hero / close-up only** (player, Harley, Morgan, cinematics).
- **No children in the street crowd.** Children stay story characters (Morgan). Do not use shrunken adult meshes as kids. Safety rules live under `crowd_mix.story_characters_only.required_if_ever_changed`.
- Skip the PilotAvatar skin-tone multiply on AvatarCatalog **Tier >= 2**; `TintMultiplier` defaults to 1.
- Skin-scatter units are **named, explicitly unverified** fields (`*_verified_in_ue_5_8: false`) until an editor read can match Epic's current Subsurface Profile docs.
- Kevin / Character Creator / MetaHuman skin parents are found by a **runtime scan** (see §3), not a hardcoded `/Game/Characters/Kevin/...` path.

## 1. Who the NPCs are (survey, updated 10 Nov 2026)

| Group | Where it lives | Body today | Look today / this pass |
|---|---|---|---|
| City pedestrians (≤12 on screen) | `ACityPedestrian` in `CityLife.cpp`, built by `APilotAvatar::ApplyConfig` | `AvatarCatalog`: MetaHuman, then CC Kevin (if imported), then CityKit Quaternius people, then Manny/Quinn, then basic shapes | MID tint by slot name from `Career::SkinTones/HairColors/OutfitColors`. **This pass: default-lit crowd MIs only** (`MI_NPC_CrowdSkin_T*` plus cloth/prop MIs). No SSS, eye or hair shading on the street crowd |
| Story speakers | `AStoryDirector::CastList` → `AStoryCharacterNPC` | TONY (MetaHuman if present, else Manny), PEPPER (Quinn 0.97), **MORGAN (story child, Quinn scaled 0.88)**, **PLAYER/Harley (teen, Manny 0.93)** | Hero close-up shading (SP_Skin, eyes, hair) once bodies are in. Morgan still needs her own teen/child-proportioned body — a scaled adult mesh is not acceptable |
| Synthezoids | Vison, JOCASTA (`EStoryBody::Synthezoid`) | Placeholder shapes | Optional synthezoid profile (`SP_Synthezoid`) — hero path |
| Hologram | Tony's message (`EStoryBody::Hologram`) | Human placeholder | Translucent blue glow |
| Robots / enemies | U, DUM-E; drones, tanks, Titan | Shapes | **Edith owns** `M_EnemyMaster`. Not touched here. |

Sex split: `Randomize` already picks 50/50 feminine/masculine bodies and matching voices. The street crowd is men, women, boys and girls as **adults and teens** (and 65+). **There are no children in the crowd** (Jarvis). Height for the crowd runs 0.90–1.07; the comment says "teen .. tall adult". Morgan is the only child, and she is currently an adult Quinn mesh shrunk uniformly, which reads as a small adult rather than a child.

The pedestrians are also **damageable and can die** (`ApplyCityDamage`, `EPedState::Dead`). That is why children stay out of the crowd.

No skin, eye or hair master exists elsewhere in the repo (`Content/Story` has suit and lighting data; Edith has suit and enemy masters). The `M_NPC_*` masters are new and deliberately minimal (§3).

### Honest limit
Materials can't make low-poly Quaternius people look photoreal. They can make them look consistent and well lit. Real-looking hero characters need real bodies: CC Kevin, MetaHumans, or City Sample Crowd. This pass gives hero/close-up characters physically correct skin, eyes, hair and cloth, and the street crowd a cheaper default-lit set that still sits in the same lighting. The script parents hero skin to a scanned Kevin/CC/MetaHuman skin when one is present.

## 2. Per-surface PBR (full values in `Tools/vison_npcs/npc_materials.json`)

Colours are linear Rec.709 albedo. UE Specular = F0/0.08.

| Surface | Model | Key values | Who uses it |
|---|---|---|---|
| **Skin (hero)** | Subsurface Profile (Burley, MFP); Substrate Slab SSS | IOR 1.40 → Specular **0.347**; roughness 0.50 (T-zone ×0.78, lips ×0.7, palms ×1.12); dual lobe 0.75/1.30, mix 0.85; MFP R:G:B = 3.67:1.37:0.68 mm (Jensen skin1), scaled per tone 1.0→0.82. **MFP distance and WorldUnitScale are named, unverified 5.8 unit fields** | player, Harley, Morgan, cinematics |
| **Skin (crowd)** | Default lit only | Same tone albedos and IOR-derived Specular 0.347, roughness 0.50; `Tint` × `TintMultiplier` (default 1). No SP_Skin, no SSS | CityKit street crowd |
| Skin tones | 8, index-compatible with `Career::SkinTones()` | Game table kept: it sits inside measured Fitzpatrick I–VI (physicallybased.info). Lightest Y 0.65, deepest Y 0.079 | both |
| Age layers | child / teen / adult / 65+ | roughness 0.50/0.47/0.50/0.56; pores 0.15/0.30/0.45/0.55; wrinkles 0/0/0.15/0.6; MFP ×1.08/1.04/1.0/0.95 (hero graphs only). Child age is for Morgan / story, not the crowd | hero ages; crowd uses the adult roughness on `MI_NPC_CrowdSkin_*` |
| **Eyes** | Eye model, refractive cornea | cornea IOR 1.376 → Specular 0.31, roughness 0.03; iris depth 0.12 cm; sclera (0.70,0.62,0.56); 6 iris colours; limbal ring 0.35 | hero / close-up only |
| Pupils | driven by scene EV100 | D = clamp(7 − 0.33·EV100, 2, 8) mm × age scale: arena ~2.0 mm, overcast ~2.8, city night ~5.7. Children ×1.12, 65+ ×0.82 | hero / close-up only |
| **Hair** | Hair model (cards), groom melanin for MetaHumans | IOR 1.55 → Specular 0.58; roughness 0.30 straight to 0.45 coily; 10 colours mapped to `Career::HairColors()` | hero / close-up only |
| Teeth / mouth / nails | default lit + SSS | enamel (0.72,0.68,0.58), roughness 0.22; gums (0.45,0.12,0.10) | hero |
| **Cloth** | Substrate fuzz / legacy Cloth | cotton roughness 0.82, fuzz 0.45; denim (0.03,0.05,0.11), 0.80; wool 0.92, fuzz 0.7; polyester 0.48; nylon shell 0.40; school uniform polycotton 0.78; police navy (0.012,0.016,0.035) | both |
| Hi-vis | albedo + daylight fluorescence | (0.62,0.85,0.02), emissive 8% of daylight, off at night; retroreflective tape boosts toward headlights | both |
| **Leather / rubber** | default lit | polished shoe 0.30, worn jacket 0.50, suede 0.92; rubber sole (0.035,0.033,0.03), roughness 0.72 | both |
| **Metal / props** | default lit | phone aluminium M1 (0.912,0.914,0.920) R0.35; steel badge/watch R0.22; gold (adult jewellery) R0.18; ABS hard hat R0.35; glasses lens IOR 1.5 | both |
| Synthezoid / hologram | SSS short MFP / unlit additive | synthezoid MFP 1.2 mm, roughness 0.38; hologram 150–400 nits, scanlines 0.6 cm | story / hero |

Wetness reads Ultron's `MPC_WorldWeather.Wetness`. At full wet: skin roughness ×0.55, hair ×0.6 and darker, cotton albedo^1.6, synthetics glossier, leather ×0.5. Grime and dust read `Dust`/`Ash`. **No blood or injury decals on anyone.**

## 3. Asset plan (`/Game/Vison/NPCs/`)

- Hero only: `SP_Skin_T0..T7`, `SP_Skin_Child`, `SP_Synthezoid` (SubsurfaceProfile).
- Hero: 32 skin MIs (4 ages × 8 tones), 10 hair, 6 eye.
- Crowd: 8 default-lit skin MIs (`MI_NPC_CrowdSkin_T0..T7`).
- Shared: 13 cloth, 6 leather/rubber, 11 prop MIs.
- That's 96 items in the full plan (88 hero-path + 8 crowd skin; cloth/props are shared). `apply_npc_materials.py --usage hero` plans 88; `--usage crowd` plans 38 (8 crowd skin + 30 cloth/leather/prop).
- Hero skin parents: **runtime scan**, same roots and tokens as `PilotAvatar.cpp` / `Docs/avatar_characters.md`:
  - `/Game/Kevin`, `/Game/CharacterCreator`, `/Game/Reallusion`
  - any material path containing `Kevin`, `CharacterCreator`, `Reallusion`, or `CC_Base`
  - MetaHumans at `/Game/MetaHumans/<Name>/`
  - fallback: Vison's minimal `M_NPC_Skin` (built only with `--build-masters --apply`)
- Crowd skin parent: `M_NPC_SkinDefaultLit` (default lit, never a Subsurface Profile master).
- Other fallbacks: `M_NPC_Hair/Eye/Cloth/Generic`.
  - Those masters are parameter-only: Tint × TintMultiplier (default **1**), Roughness, Specular, and fuzz/scatter/backlit.
  - The eye master is a stand-in (default lit iris + glossy cornea values). For true refraction, use the MetaHuman eye material as the parent once it's in the project.
  - The richer pieces (pore/weave detail normals, flush and vein masks, pupil driver, wetness) are spec'd in the JSON for whoever builds the full graph.
- **Tint compatibility.** `PilotAvatar` writes the tone into `Tint`/`BaseColor`/`Color` on every MID. On untextured bodies (Quaternius, Manny) `Tint` *is* the albedo, so the game's tone table works unchanged. Textured MetaHuman or Kevin skin must not be multiplied by a 0.12–0.80 albedo. **C2 resolved:** skip the skin-tone multiply when `E.Tier >= 2` (`Tools/vison_npcs/npc_pilotavatar_skin_tint.patch`). `TintMultiplier` defaults to 1.

## 4. How NPCs sit under the lighting

| Preset | EV100 | What to expect | Material implication |
|---|---|---|---|
| **Arena 15:30, WSW sun (pitch −38 / yaw 330), 5900 K** | 15.1 (~88k lux) | Low side light; red SSS bleed at the shadow terminator on noses and ears (hero close-ups); backlit ears and fingers glow | Skin vs mid-grey: tone 0 +1.9, tone 2 +1.4, tone 7 −1.2 stops. All in range: **don't brighten deep tones**; Lumen bounce and skylight carry them |
| City 13:00 | 15.55 (120k lux) | Top light, dark eye sockets, hats shade faces | Relies on pavement bounce (albedo ~0.25); no fill light hacks |
| City overcast | 12.58 | Soft light: SSS reads best on hero close-ups | Wetness 0.5 |
| City night 01:00 | 3.93 (38 lux) | Unshadowed street lamps (perf_budget rule) | Pupils ~5.7 mm (hero); wetness 0.8 gives glossier skin and hair |
| Storm | — | Rain 0.9 | Full wet model |

Sun 120000 lux, 5900 K and EV100 are unchanged. I'm not proposing any lighting edits for NPCs. Lighting data: `Content/Story/lighting_presets.json`.

## 5. Camera / post interaction

- **Exposure:** gameplay keeps the existing camera metering in `Tools/vison_camera` / `Content/Story/camera_realism.json` (3 s adaptation). Cutscene close-ups use manual EV100 = the preset value, so skin matches across cuts.
- **Grade:** PhotorealGrade (sat ≤ 1, CA 0) and my ≤ 4% split-tones keep skin hue true. There is no skin-specific grade.
- **Sharpen 0.5:** pore normals fade out from 4 to 8 m so faces in a crowd don't get crunchy. Global sharpen is unchanged.
- **Motion blur:** per-bone velocity through the skin cache. Verify that NPC components output velocity.
- **DOF / eye light (cutscenes):** 35–85 mm, f/2–2.8, focus on the near eye. This needs Jocasta's head-socket `GetFocusPoint` fix. There's an optional unshadowed camera-attached eye light (40 lux at the subject, 5600 K, lighting channel 2) for catchlights.
- **Grain:** the cutscene grain of 0.15 hides hair-card dither. Keep it.

## 6. Scalability and cost (RTX 2070 SUPER, 1080p, TSR) [E]

| Tier | Hero SSS (close-ups only) | Crowd material | Crowd LOD bias | Gameplay cost (crowd) |
|---|---|---|---|---|
| Epic | full-res Burley | default lit | 0 | ~0.7 ms [E] |
| **High (default)** | half-res + checkerboard | default lit; no SP / eye / hair models | 1 | **~0.55 ms** [E] |
| Medium | half-res, Burley quality 0 | default lit, no detail normals | 2 | ~0.45 ms [E] |
| Low | `r.SSS.Scale 0` | default lit fallback | 3 | ~0.4 ms [E] |

The High crowd breakdown: skinning 0.25 + default-lit base/velocity 0.18 + VSM 0.12 ms. Hero close-ups add SSS 0.12 + hair 0.10 + eyes 0.03 ms on top of the subject, not on all 12 peds. Cutscene MetaHuman close-ups cost about 1.6 ms on High (LOD1, cards) and 3.5 ms on Epic (strands), with no combat VFX running.

**Do not spend ~0.8 ms of SSS on all 12 crowd peds.** Jarvis declined a characters cut from Ultron's 13.5 ms High budget for that. Measure with perf_budget §11 `profilegpu`.

The cvars are written per tier in the JSON, for `[ShadingQuality@N]`. They apply to hero/close-up shading. Jarvis merges them, checking against `Tools/vison_camera` and Ultron's overlays.

## 7. Men, women, boys and girls

- **Street crowd:** 50/50 female/male; **teens 8.7%, adults 76.1%, aged 65+ 15.2%** (the previous 8% child share was removed and the rest renormalised). Boys and girls appear as teens (and as adults); children are not spawned in the crowd.
- **Proportions.** Adult heights are 152–178 cm for women (mean 162) and 163–193 cm for men (mean 176), about 7.5 heads tall. Teens are 150–185 cm, about 6.5–7.2 heads. Children 6–12 (story only) are 115–152 cm, about 5.5–6.5 heads, with a larger head, shorter legs and a rounder face.
  - **Morgan needs a real child- or teen-proportioned body.** A uniformly scaled adult mesh is not acceptable.
  - Mixed builds for adults (slim, average, heavy, athletic). 65+ gets wrinkles, age spots and greyer, thinner hair.
- **Skin** optics are the same for every sex. Age changes pores, wrinkles, smoothness and flush (hero graphs). Stubble and beards are for adult men only.
- **Hair** styles per group use the existing `EAvatarHair` set, with grey weighted by age. Dyed colours are teen and up only, and rare. Full hair shading is hero/close-up only; the crowd keeps the existing slot tint.
- **Clothing** is ordinary everyday city/Florida wear for every age in the crowd (office, casual, sport, tourist, worker, police, EMS, teen hoodie/jeans). Kid outfits (t-shirts, knee shorts, school uniform, backpack) live under `story_characters_only` for Morgan.
  - **No revealing or sexualised outfits for anyone, and no swimwear in the street crowd.**
- **Children are story characters only (Morgan), not a sexual or background crowd presence.** That means no romantic or suggestive content, poses, outfits or voice lines, and the same rule applies to teens.
- **If children are ever added to the crowd**, the rules under `crowd_mix.story_characters_only.required_if_ever_changed` are required: always with an adult; daytime only; gone when wanted / waves / combat is near; never damageable, ragdoll, or targetable; excluded from `CityLife::Damage`; real child-proportioned body, never a shrunken adult.

## 8. Verification checklist (in the editor)

1. `python3 -m py_compile Tools/vison_npcs/apply_npc_materials.py` passes, and an offline dry run lists **96 items** for `--usage all` (88 hero-path + 8 crowd skin), **88** for `--usage hero`, **38** for `--usage crowd`, all under `/Game/Vison/NPCs/`. **Done on the box.**
2. In the editor, run a dry run and read the log. It should show no writes outside `/Game/Vison/NPCs`, with "skip: not on parent" only where expected. Hero skin parent should come from the runtime scan, or `M_NPC_Skin` if nothing is imported yet — never from a hardcoded `/Game/Characters/Kevin/...` path.
3. Run `--build-masters --apply`, then open `M_NPC_Skin` and confirm the shading model is still Subsurface Profile after the Substrate conversion. Confirm `M_NPC_SkinDefaultLit` is default lit.
4. Check `SP_Skin_T*` in the profile editor. Confirm what the World Unit Scale and MFP units mean in 5.8. The JSON fields are named and marked `*_verified_in_ue_5_8: false` until that read happens.
5. Lookdev level: a grey card (0.18) plus tones 0, 2, 5 and 7 under the arena preset, on a **hero** close-up. Expect skin about +1.9, +1.4, 0, −1.2 stops in `HighResShot` with the pixel inspector.
6. Swap the preset to city_night, then overcast, then storm. Check that wetness darkens cloth and hair and that hero skin isn't grey or waxy.
7. Backlight test: put the WSW sun behind a hero head. Ears should show a red transmission rim and no black ears.
8. Crowd at 5, 15 and 30 m on High. Default-lit CityKit people should stay cheap (no SSS pass for the 12 peds). `stat gpu` in line with the ~0.55 ms crowd estimate [E].
9. Cutscene: Morgan and Harley close-up with manual EV100, focus on the eye, eye light on or off, **hero** materials assigned. Then Medium tier: refraction kept, no SSS banding.
10. Confirm the street crowd still has **no children**, and that Morgan is the only child and is not damageable.

## 9. Conflicts and notes for Jarvis

- **C1, budget — decided:** do not spend ~0.8 ms of SSS on all 12 crowd peds. CityKit stays default-lit. No characters line was requested from Ultron's 13.5 ms High budget. Hero SSS is for close-ups only.
- **C2, tint vs textured skin — resolved:** skip the PilotAvatar skin-tone multiply on `E.Tier >= 2` (`npc_pilotavatar_skin_tint.patch`). `TintMultiplier` on the MIs defaults to 1 so MetaHuman/Kevin stay unscaled.
- **C3, children — resolved:** no children in the street crowd. Morgan is story-only and still needs a real child/teen-proportioned body (please ask Claude not to ship her as a scaled adult mesh). Safety rules are filed under `story_characters_only.required_if_ever_changed` if that ever changes.
- **C4, animation (not my lane):** older-adult posture, child/teen gait and stride, idle variety, and look-at. The npc worker and Jocasta's `humans_plan.md` cover these. UAL clips are adult-only.
- **C5, scalability ini:** `[ShadingQuality@N]` SSS cvars sit alongside the camera/realism ini and Ultron's overlays, and apply to hero shading only. There are no known key collisions (neither sets `r.SSS.*`), but check during the merge.
- Edith (enemies, suits), U, Dum-E and Friday (suits, env) have no human-material work, so there's no overlap. Ultron's `MPC_WorldWeather` is only read here, never written.
