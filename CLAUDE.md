# Iron Man Game: list for Claude

**Project:** Unreal Engine 5.8, C++. Uproject: `IronManArena.uproject`. Default map: `/Game/Maps/L_IronManArena` (the game mode builds the city at runtime).
**Axel's PC:** Windows, RTX 2070 SUPER, 16 GB RAM. The project lives at `D:\IronManGame\IronManArena\`, and the engine at `C:\Program Files\Epic Games\UE_5.8`.
**Build:** close Unreal first (Live Coding blocks it), then run `D:\IronManGame\build.bat` (IronManArenaEditor Win64 Development). It logs to `D:\IronManGame\build-win.log`.
Read `README.md` and `Docs/` first.

## Rules
- Use only original assets: no ripped Marvel/Disney models, sounds or film dialogue, and no voice cloning of actors.
- MSVC treats C4458 as an error, so never name a local `Slot`, `bHidden`, `Owner`, `Instigator`, `Tags` or `Children`.
- Voices are OFF by default.
- Story: Harley Keener is the hero, and Morgan is the guy-in-the-chair who wears Rescue in the finale.
- Controls: T suit menu, H faceplate, O exit suit, left mouse repulsors, hold right mouse or double-click left mouse for the suit ultra, Space/click skips cutscenes. A private dev menu for Axel has infinite money. The first-launch character creator, CUSTOMIZE AVATAR menu entry and K customizer hotkey are disabled; the default pilot is Harley.

## Do first
1. Get it compiling on Windows. The latest changes were only ever built on Linux, so fix any MSVC errors in `build-win.log`.
2. Package a standalone Windows game: RunUAT BuildCookRun Win64 Development (cook, build, stage, pak, archive) into `D:\IronManGame\WindowsApp`. Then point the desktop shortcut `Iron Man Game.lnk` at the packaged `IronManArena.exe -windowed -ResX=1600 -ResY=900`.
3. Play-test it on the GPU. It has never been seen running with real graphics, so check the lighting (Lumen), the frame rate and the sound.

## Big goal: a realistic city like GTA
- A living city with real lighting, wet streets at night, busy traffic, and pedestrians walking and reacting.
- Suits flying through the city, with better flight feel (speed, banking, boost, landing).
- Press O to leave the suit and walk around as yourself.
- Buy cars, bikes, houses and planes with in-game credits, and actually drive and fly them (cars and vans exist only as purchases right now).
- A wanted level and police response (a first pass already exists).

## Finish and polish
- Settings: turn the KEY BINDINGS list into a real Controls tab where every key can be rebound and saved, with gamepad support.
- Combat: hit feedback, enemy variety, and boss fights (Ultron boss effects are hooked up and need Niagara effects).
- Cutscenes: camera work, pacing and subtitles for the story chapters and the finale.
- NPCs: better faces, skin and clothing (see Vison's NPC notes on PR 1). There are no children in the crowd.
- Suits: better materials, the suit-up animation, damage and scratches.
- HUD (FRIDAY): clean, readable callouts and a minimap.
- Main menu: make it look finished, with a loading screen.
- Performance: hold 60 fps at 1080p on an RTX 2070 SUPER.
- Easter eggs: swap the film quote "I am inevitable" for an original line.

## Other bots' notes
The other bots (Friday, Edith, Vison, Jocasta, U, Dum-E, Ultron) post their notes as comments on PR 1, signed with their names. Read them before working in their areas.

## Steam
To sell it on Steam, everything Marvel has to go: the name, Iron Man, Tony, Harley, Morgan and the suit designs. Rename it to an original hero before release.
