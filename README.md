# Iron Man Arena (Unreal Engine 5.8, C++)

A 3-D Iron Man game written in C++ for **Unreal Engine 5.8** (built and tested with **5.8.4**). The project runs without
any map or art assets. Everything it draws is generated from code: suits, arena, enemies, HUD and menus.

## Build status (what the logs show)

| When (MDT, 2026-10-29) | Who | What | Result |
|---|---|---|---|
| 22:08 - 22:10 | compile worker | `IronManArenaEditor Linux Development`, three `-Werror` fixes (see below) | `Result: Succeeded` (editor-2/3.log) |
| 22:11 | compile worker | Linux package | built (`/workspace/ironman-unreal/package-linux`) |
| later the same night | this project's author | incremental editor builds after the settings, menus, HUD, story and MetaHuman work | `Result: Succeeded`, no warnings (build-check/editor-me1..me5.log) |
| 22:28 | this project's author | headless voice import (`-run=pythonscript`, import_jarvis_audio.py) | 825 / 825 WAVs imported as `.uasset` (build-check/import-audio.log) |
| 22:32 | this project's author | 120 s `-game -nullrhi` smoke run of the editor build | no crash or errors; runtime arena built; `AI voice: 532 / 532 lines have audio` (build-check/smoke-game.log) |
| 22:50 | this project's author | editor build after the code-review fixes (sound classes, prompt unbind, ClearArena guard) | `Result: Succeeded`, no warnings (build-check/editor-me6.log) |
| 22:51 | this project's author | headless re-import from `SourceAudio/` + `setup_sound_classes.py` + `create_emissive_material.py` | scripts ran; SC_Master -> SC_SFX / SC_Voice / SC_Music, 825 / 825 voice SoundWaves on SC_Voice, M_ArenaEmissive created (build-check/import-audio2.log, emissive.log, verify-audio.txt) |
| 22:57 | this project's author | editor build with the startup story-status log | `Result: Succeeded`, no warnings (build-check/editor-me7.log) |
| 23:00 | this project's author | fresh `RunUAT BuildCookRun` Linux Development (`-nullrhi`, old package deleted first) | `BUILD SUCCESSFUL`, ExitCode=0, 0 warnings (build-check/package3.log); manifest has 536 Audio + 293 StoryAudio `.uasset`, the material, and `Content/Story/*.json` as loose files, no `.wav` |
| 23:00 - 23:02 | this project's author | 120 s headless run of the **packaged** game (`IronManArena.sh -nullrhi -nosound -unattended`) | no errors or warnings; `Story mode: ENABLED - 'Iron Legacy: The Workshop Kid', 9 chapters; story voice: 189 / 189 script takes, 104 / 104 MORGAN comms takes have audio`; `AI voice: 532 / 532 lines have audio` (build-check/smoke-packaged.log). Title screen only; the chapters were not played through |

These are Linux (clang, warnings treated as errors) editor builds. Windows/MSVC and macOS/Xcode builds have **not** been run. The 22:11 package was built before at least the MetaHuman hook, the cloak change and the imported voices, so rebuild it from this zip. The smoke run stayed on the title screen: story chapters, the GPU look and audio playback have not been played through.
The compile fixes are logged in `contrib/compile-fixes.md`, next to the project folder:
- a single-iterator sun check in `ArenaGameMode.cpp`;
- `CoreFlicker` in `IronManCharacter.cpp`;
- iterating JSON maps by value in `StoryLibrary.cpp`.

For UE 5.8 code style:
- `FJsonObject::Values` keys are `UE::TSharedString`, so iterate by value or with `const auto&`, never with `const TPair<FString,...>&`.
- Avoid `-Wshadow` names.
- Avoid loops that always break on their first pass.

## Quick start

```bash
# Linux (binary 5.8.4 engine)
UE=/path/to/UnrealEngine/5.8
$UE/Engine/Build/BatchFiles/Linux/Build.sh IronManArenaEditor Linux Development -Project="$PWD/IronManArena.uproject" -WaitMutex
$UE/Engine/Binaries/Linux/UnrealEditor "$PWD/IronManArena.uproject"
```
- **Windows:** right-click the `.uproject`, choose *Generate Visual Studio project files*, then build `IronManArenaEditor` in *Development Editor*.
- **Full toolchain notes:** see `Docs/UNREAL_SETUP_GUIDE.md` and `Docs/TROUBLESHOOTING.md`.
- **Engine settings:**
  - `.uproject` has `"EngineAssociation": "5.8"`.
  - Both targets use `BuildSettingsVersion.Latest` and `EngineIncludeOrderVersion.Latest`.

Open any map, or a new empty level, and press Play. `AArenaGameMode` is the default game mode. It builds the arena, lights and sky if the level has none.

## What's in it

- **Arena mode:**
  - Enemy waves: drones, tank bots, and the Obsidian Titan boss.
  - Pickups and score.
  - A Hulkbuster call-in.
- **Story mode, "Iron Legacy: The Workshop Kid":**
  - Data comes from `Content/Story/story.json` and `morgan.json`. Those files are the contrib versions, unmodified.
  - The story runs as a Tony "snap" prologue, then 9 chapters, then a finale. Harley is the hero. Morgan is the guy in the chair; she flies in Rescue in the finale.
  - Objectives include ground/flight trials, drone, tank and boss fights, waves, the Hulkbuster and energy runs. Each objective has comms triggers for start, progress and complete.
  - Each chapter can unlock a suit.
  - Vison is an ally in chapters 7 and 9 (press G).
  - The `{playerName}` and `{heroName}` tokens are filled in from the save.
  - Cutscene NPCs use skeletal meshes (Manny/Quinn or MetaHumans) and fall back to shape placeholders when those assets are missing.
- **32 data-driven suits:**
  - They come from the contrib `suits.json`, via `Tools/generate_suits_and_voices.py` → `Private/Generated/SuitTable.inl`.
  - Suit select screen.
  - Hold T for the suit wheel; 1-5 are quick suits.
  - Cinematic suit-up.
  - First and third person (V).
- **On-board AI personas** (JARVIS, FRIDAY, EDITH, ULTRON, WAR MACHINE):
  - Each has its own lines, voice, HUD tint and subtitle colour.
  - Handovers are keyed by the outgoing persona.
  - There are 532 TTS takes (imported to `Content/Audio/<PERSONA>`; raw WAVs and `lines.csv` in `SourceAudio/Audio`).
  - Story mode adds 293 TTS takes (imported to `Content/StoryAudio/<SPEAKER>`; raw WAVs and `story_lines.csv` in `SourceAudio/StoryAudio`). Name-token lines also get a `_noname` take.
  - All voice lines play through the `SC_Voice` sound class (see Settings below).
  - MORGAN is a comms speaker, not a selectable persona.
- **Harley's makeshift arc reactor** (`UReactorComponent`):
  - Mounted on the spine or forearm, never the chest. It is visible in third person, and in first person on the forearm.
  - Covers energy, heat, overheat, venting (X) and per-chapter story stages: makeshift → unstable → failed (ch5) → upgraded (ch7).
  - It has upgrade levels. All tunables are `EditAnywhere` on the component.
- **Settings menu** (`USettingsWidget` + `UArenaGameUserSettings`, saved to `GameUserSettings.ini`):
  - Video: window mode, resolution, VSync, frame cap, overall quality.
  - Camera and controls: FOV, mouse sensitivity, invert Y.
  - Audio: Master, SFX, Voice and Music volumes. Master is set on the audio device. SFX, Voice and Music use the sound classes `/Game/Audio/Classes/SC_Master` > `SC_SFX` / `SC_Voice` / `SC_Music`, through a sound mix. Every voice line is assigned to `SC_Voice`, and sounds without a class default to `SC_SFX` (`[/Script/Engine.AudioSettings]`). If the class assets are missing, the code skips them quietly, and the Voice slider then goes on the voice sources.
  - Subtitles.
  - The default is **windowed fullscreen** (`Config/DefaultGameUserSettings.ini`).
  - **F11** and **Alt+Enter** toggle fullscreen; the engine's own handling of those keys is disabled.

## Controls

| Key | Action |
|---|---|
| WASD, mouse | move / look |
| F | toggle flight |
| Space / Ctrl | up / down |
| Shift | boost |
| LMB / RMB | repulsor fire / beam |
| X | vent the reactor |
| V | first / third person |
| Q | Hulkbuster |
| 1-5 | quick suits |
| hold T | suit wheel (wheel/Tab pages) |
| G | call the ally |
| Space / click | skip a shot |
| Esc | skip the cutscene |
| P / Esc | pause |
| F11 / Alt+Enter | fullscreen |

Gamepad works through Enhanced Input (mappings are created in code).

## Manual editor steps

1. **Voice assets.** The project zip already contains the imported voice `.uasset` files (routed to `SC_Voice`), the sound classes and `M_ArenaEmissive`, all made headlessly with UE 5.8.4. The raw WAVs are **not** in `Content/`. They are in the top-level `SourceAudio/` folder, shipped separately as `IronManArena-SourceAudio.zip`; unzip it next to the `.uproject`. You only need them to re-import. The generators (`Tools/generate_suits_and_voices.py`, `Tools/generate_story_audio.py`) write there too. To re-import, use either:
   - In the editor: *Tools → Execute Python Script → Content/Python/import_jarvis_audio.py*.
   - Headless: `UnrealEditor-Cmd IronManArena.uproject -run=pythonscript -script="$PWD/Content/Python/import_jarvis_audio.py" -unattended -nullrhi`.

   `import_jarvis_audio.py` ends by running `Content/Python/setup_sound_classes.py`, which creates or repairs the sound classes and assigns `SC_Voice`. You can also run that script on its own.
   `Config/DefaultGame.ini` always cooks `/Game/Audio`, `/Game/StoryAudio` and `/Game/IronManArena`, because these assets are loaded by path. Without that, packaged builds would be silent.
2. **Story data** is read from disk. `Content/Story` is staged loose (`DirectoriesToAlwaysStageAsNonUFS`). Edit the JSON and restart; no rebuild is needed.
3. **Mannequins (optional):** add the *Third Person* template content (Add → Add Feature or Content Pack → Third Person) to get Manny/Quinn cutscene bodies. Without it you get the shape placeholders.
4. **MetaHumans (optional):** see `Docs/ADDING_METAHUMANS.md`.
5. **Emissive material:** `/Game/IronManArena/Materials/M_ArenaEmissive` is already included; `Scripts/create_emissive_material.py` made it headlessly. Rerun the script only if you delete the material. The code falls back to the engine's basic material when it is missing.
6. **Packaging:** use *Platforms → Linux/Windows → Package Project*, or BuildCookRun. No maps need to be listed; the default map is used.

## Layout

```
IronManArena.uproject
Config/            DefaultEngine/Game/Input/GameUserSettings.ini
Source/IronManArena/
  Public|Private/  game mode, character, controller, reactor, suits, JARVIS, enemies, story, UMG widgets (built in C++)
  Private/Generated/SuitTable.inl, AIVoiceLines.inl   (generated from contrib JSON)
Content/Story/     story.json, morgan.json          (runtime data, staged loose)
Content/Audio/     persona voice .uasset (SoundWaves, SC_Voice) + Classes/ (SC_Master, SC_SFX, SC_Voice, SC_Music)
Content/StoryAudio/ story voice .uasset
Content/IronManArena/Materials/M_ArenaEmissive.uasset
Content/Python/    import_jarvis_audio.py, setup_sound_classes.py
SourceAudio/       raw WAVs + lines.csv / story_lines.csv (separate zip; only needed to re-import)
Tools/             generate_suits_and_voices.py, generate_story_audio.py (piper TTS, numpy)
Scripts/           create_emissive_material.py
Docs/              UNREAL_SETUP_GUIDE, TROUBLESHOOTING, ADDING_METAHUMANS, STORY_MODE, LORE
```

All voices are synthetic placeholders (piper TTS) and no film audio is used. Iron Man and related names belong to Marvel; this is a non-commercial fan project.
