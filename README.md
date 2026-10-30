# Iron Man Arena (Unreal Engine 5.8, C++)

A third-person / first-person Iron Man action game for **Unreal Engine 5.8** (built and tested on Linux with **5.8.4**).
You play Harley Keener: a story campaign ("Iron Legacy: The Workshop Kid", prologue + 9 chapters + finale + 5 side
missions) and an endless arena mode in a runtime-built city. Gameplay, suits, armour, city, HUD and menus are C++;
materials, textures, HDRIs, fonts, music and voices are imported `.uasset`s in `Content/`.

**Map:** `/Game/Maps/L_IronManArena` (the game default map; `AArenaGameMode` builds everything else at runtime).

## What the logs show (2026-10-30, times MDT)

| Check | Result | Log |
|---|---|---|
| Editor build `IronManArenaEditor Linux Development` (clang, warnings as errors) | `Result: Succeeded` (r13 had 4 errors, fixed in r14; latest r20) | `build-check/editor-r*.log` |
| Headless automation, all `IronManArena.*` tests (`-game -nullrhi -nosound`) | 19 / 20 pass on r15: chapters 1-9, side missions 1,2,4,5, ArmorKit, CityLayout, Data, FullRun (prologue + finale played), LightingPresets, SideData. `sm_synthezoid_signal` failed (ally never summoned); fixed and re-run alone on r16: pass | `/tmp/story2.log`, `/tmp/sm3.log` |
| Arena smoke `-ArenaSmoke -ArenaSmokeQuit` (editor `-game`) | PASS: 1 wave, 4/4 drones killed by repulsor shots. 5 waves: PASS, 44 enemies (38 drones, 5 tanks, the Titan boss in wave 4), 38 killed by shots, 6 by the smoke's fallback damage after 75 s | `/tmp/arena.log`, `/tmp/arena5.log` |
| Linux package + packaged smokes | see the "Packaged build" line at the end of this table | `build-check/package-r20.log` |

**Not verified:** there is no GPU on the build box (everything ran `-nullrhi`), so the look, Lumen, TSR and
frame rate have never been seen or measured. Audio playback was never heard (`-nosound`); only the asset
references were checked. **No Windows (MSVC) build has been run**; the code avoids C4458/-Wshadow names, but
that is not a proof.

## Quick start

```bash
UE=/path/to/UnrealEngine/5.8
$UE/Engine/Build/BatchFiles/Linux/Build.sh IronManArenaEditor Linux Development -Project="$PWD/IronManArena.uproject" -WaitMutex
$UE/Engine/Binaries/Linux/UnrealEditor "$PWD/IronManArena.uproject"
```
- **Windows:** right-click `IronManArena.uproject` → *Generate Visual Studio project files*, build `IronManArenaEditor`
  / *Development Editor*, press Play. The source drop has no `Binaries/` or `Intermediate/`, so the first open compiles.
- **MetaHuman plugins:** `MetaHumanCoreTech`, `MetaHumanSDK` and `MetaHumanCharacter` are enabled in the `.uproject`
  as `"Optional": true`, so engines without them (like the build box) still load. Tony's MetaHuman is used when
  `/Game/MetaHumans/Tony/BP_Tony` exists (see `Docs/ADDING_METAHUMANS.md`); otherwise the procedural suit / placeholder.
- Headless tests: `UnrealEditor IronManArena.uproject -game -nullrhi -nosound -unattended -ExecCmds="Automation RunTests IronManArena" -TestExit="Automation Test Queue Empty"`.
- Arena smoke: add `-ArenaSmoke [-ArenaSmokeWaves=N] -ArenaSmokeQuit`; story autoplay: `-StoryAutoplay -StoryAutoplayQuit`.

## How to play

Title menu (a slow cinematic camera circling the city) → **Story** or **Arena** → suit select → suit-up cinematic.

- **Story:** the prologue is Tony's last stand as a flashback (below), then 9 chapters (Dust and Blueprints, First
  Flight, Static in the Wires, Eyes in the Dark, The Titan Wakes, Heavy Lifting, Siege of the Skyline, Legacy Protocol,
  Ghost in the Machine), the finale and 5 side missions. Objectives: flight trials, drones, tanks, waves, the Obsidian
  Titan boss, Hulkbuster and energy runs; win/lose screens and checkpoints. Vison is a summonable ally (G).
- **Arena:** endless waves in FRIDAY's runtime city (start on Legacy Plaza by Stark Tower): drones, tanks, a Titan boss
  every 4th wave, health / energy / Hulkbuster pickups; at most 12 enemies active (Ultron's budget); the play area has a
  boundary and ceiling, so you can't fall into the void.

## Controls

| Key | Action |
|---|---|
| WASD, mouse | move / look |
| F | toggle flight (hover when idle) |
| Space / Ctrl | up / down |
| Shift | boost |
| LMB | repulsors |
| RMB | unibeam |
| X | vent heat (the HUD heat bar overheats if you keep firing) |
| V | first / third person |
| Q | Hulkbuster |
| 1-5 / hold T | quick suits / suit wheel (wheel or Tab pages) |
| G | summon the ally (when offered) |
| Space / click | next cutscene shot |
| Esc | skip the cutscene |
| P / Esc | pause |
| F11 / Alt+Enter | fullscreen |

Gamepad works through Enhanced Input (mappings created in code).

## Prologue: Tony's last stand (flashback)

- Battlefield at dusk (`flashback_dusk` preset: low sun, warm fog, embers, smoke), muted grade (saturation 0.62,
  grain, vignette), slow camera, shallow depth of field (f/1.4 manual focus; Settings → cinematic DoF).
- Tony in a damaged Mark 85 (procedural nanotech armour with soot); the arc reactor flickers, drops out and fades
  to dark over the last shots and never comes back.
- Final words to Pepper and about "the Tennessee kid", then a memory insert of young Harley in the snowy garage,
  then a "YEARS LATER / Rural Tennessee" card and chapter 1 in the garage.
- Dialogue is original (no film lines). The lines are the existing Kokoro TTS takes `StoryAudio/TONY|PEPPER|FRIDAY/prologue_*`.
- Score: `/Game/Music/S_FlashbackSwell`, an original procedurally synthesised string pad (`Tools/make_flashback_score.py`), on `SC_Music`.
- Skippable shot by shot (Space/click) or whole (Esc).

## Realism layer

- CC0 PBR textures and HDRIs (Poly Haven, ambientCG; list in `Docs/CREDITS.md`), physical sun/sky, volumetric clouds,
  height fog, Lumen GI/reflections, TSR, filmic post and per-location lighting presets (Vison's `lighting_presets.json`).
- City: FRIDAY's `Content/City/city_layout.json` (2738 buildings, 404 roads, 196 blocks, 175 spawns), built at runtime
  as instanced meshes with box collision.
- Armour: U's `armor_kit.json` plates extruded at runtime (Mark III resolves 135 plates), U's suit materials.
- HUD: EDITH's layout and the 13 `M_HUD_*` / `MI_HUD_*` materials.

## Known gaps

- Nothing has been seen on a GPU or heard through speakers; 60 fps on an RTX 2070 SUPER is a target, not a measurement.
- No Windows build has been compiled.
- City buildings are textured boxes (no facade kit pieces or window bays placed); no water shader, street trees or props;
  no HLOD / World Partition; no navmesh (tanks drive straight at you). Story chapters still use their own chapter sets.
- Armour plates have no bevels, rivets or sliding mechanics; the Manny/Quinn skeleton path is untested.
- No Chaos destruction / rubble.
- In the 5-wave smoke, 6 of 44 enemies needed the test's fallback damage after 75 s (the autopilot doesn't fly or dodge).
- The MetaHuman Tony (if you add one) can't show the suit damage; the flashback score is a synth placeholder.

## Assets and size

`Content/` is about 320 MB. All third-party assets are CC0 or OFL fonts; see `Docs/CREDITS.md` and `Docs/licenses/`.
Voices are Kokoro-82M stock voices (Apache-2.0), generated by `Tools/generate_*.py`; raw WAVs live in `SourceAudio/`
(shipped separately). No film audio. Iron Man and related names belong to Marvel; this is a non-commercial fan project.

## Layout

```
IronManArena.uproject
Config/            DefaultEngine/Game/Input/GameUserSettings.ini
Source/IronManArena/  game mode, character, reactor, suits, armour kit, city layout, enemies, story director, UMG (C++), tests
Content/Maps/      L_IronManArena
Content/Story/     story.json, morgan.json, side_missions.json, lighting/suit/armour data (staged loose)
Content/City/      city_layout.json (staged loose)
Content/Audio/, StoryAudio/, Music/   voice takes, sound classes, flashback score
Content/Env/, Realism/, UI/           CC0 textures, HDRIs, materials, HUD materials, fonts
Tools/, Scripts/, Content/Python/     generators and headless import scripts
Docs/              CREDITS, STORY_MODE, LORE, ADDING_METAHUMANS, UNREAL_SETUP_GUIDE, TROUBLESHOOTING
```
