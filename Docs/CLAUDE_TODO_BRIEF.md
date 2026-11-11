# IronManArena (UE 5.8 C++): shared brief for every job

Repo: github.com/axeltolsen-dotcom/Iron-man-game. Default branch is `master`; the integration branch is `claude/todo-list`. Paths below are relative to the repo root. `.cpp` names live in `Source/IronManArena/Private/`, `.h` names in `Source/IronManArena/Public/`.

## The situation
- **Nobody can compile here.** There's no Unreal Engine, no Windows and no GPU in this environment. The owner (Axel, a kid) and his Grok bot will build on his Windows PC with MSVC (VS2022, UE 5.8). That makes every compile error expensive. Write code you are *certain* compiles on MSVC with UE 5.8:
  - Prefer engine APIs and include paths that **already appear in this codebase**. Grep for an existing usage before using any engine API.
  - For anything new, use only long-stable UE5 APIs whose exact signatures you are sure of. Add the right `#include`s.
  - If you need a new module, add it to `Source/IronManArena/IronManArena.Build.cs` (e.g. `MoviePlayer`).
- **MSVC treats shadowing warnings as errors** (C4456/C4457/C4458/C4459):
  - Never name a local or parameter the same as a class member, a base-class member, an outer local, a parameter or a global.
  - Never use these names for locals or parameters: `Slot`, `bHidden`, `Owner`, `Instigator`, `Tags`, `Children`, `World`, `Level`, `Controller`, `Mesh`, `RootComponent`, `Role`.
  - When a parameter would clash with a member, prefix it with `In`.
- **Unity builds.** Several `.cpp` files get concatenated into one translation unit, so anonymous-namespace or `static` helpers with the same name in different `.cpp` files can collide. Give new file-local helpers names unique to their file, e.g. a file prefix like `MinimapPaint_*` or a uniquely named namespace.
- **UHT rules.**
  - `UCLASS`/`USTRUCT`/`UENUM`/`UFUNCTION`/`UPROPERTY` only in headers under `Public/` (or Private headers), with `GENERATED_BODY()`.
  - A new `.h` with reflected types needs `#include "X.generated.h"` as its last include.
  - `UPROPERTY` types must be UHT-supported. Use `TObjectPtr<>` for object members, as the codebase does.
- **Generated tables.** Don't hand-edit `Private/Generated/*.inl`.
- **Content.** You can't create `.uasset`s.
  - Visuals are procedural: basic-shape meshes via the `ArenaVisuals` helpers (`AddRuntimePart` / `AddPart` / `PaintAll` / `RepaintPart` / `SetScalarOnAll`, `EArenaSurface` materials, `EmissiveToNits`), `FSlateDrawElement` painting, lights, and the existing materials.
  - Assets are loaded by soft path behind a `DoesPackageExist` check, always with a fallback.
- **Kid-friendly.**
  - No gore and no real-world gun brands.
  - Don't add children to the crowd. The game has combat and pedestrians can be harmed. Leave the crowd at adult and teen sizes.
  - Use only original assets and lines: no Marvel/film quotes and no voice cloning.

## Main classes
- `AArenaGameMode` (ArenaGameMode.h/.cpp): waves, score, Arena and Story modes. `SetupCityPlay` (~line 231) sets up free play.
- `AIronManCharacter` (IronManCharacter.h/.cpp, ~2556 lines):
  - Flight: `TickFlightPhysics` (~2401-2556), `SetFlying` (~1057), `Landed` (~1422), `TickVisuals` (~2074), `ApplyFlightCameraForState` (~2373), `TickAimFeel` (~2312).
  - Input: `CreateDefaultInputIfNeeded` (~639-739, Enhanced Input actions and mapping context built with NewObject), `SetupPlayerInputComponent` (~741).
  - Suit wear: `TickSuitWear` (~1200). Damage: `TakeDamage` (~1999). `TickBoundary` enforces the play area.
- `AArenaPlayerController`: owns the HUDs, menus, `UJarvisComponent` and `UCareerInput`. `PresentScreen` / `ShowMenu`.
- `UCareerSubsystem` (CareerSubsystem.cpp): a GameInstanceSubsystem and FTickableGameObject that autosaves every 15 s.
  - `Tick` (~519) calls `ApplyScalabilityPreset(2)` on every launch and sets cvars with `ECVF_SetByCode` (~666-683). That overrides the user's Settings. It's a known bug for the buildfix job.
  - Also: `SetTimeOfDay` (~573), `BuyVehicle` / `BuyHouse`, and `DevSetInfiniteMoney`.
- `UArenaGameUserSettings` (config=GameUserSettings): video, audio and accessibility settings. `ApplyRenderingSettings` (~180).
- `AArenaEnvironment` is placed in `L_IronManArena` and spawns `ACityLayout` (FRIDAY's Harbor District from `Content/City/city_layout.json`). See `SpawnArenaCity()` (~303) and `SetArenaDressingVisible()` (~145).
- `ADowntownDistrict` (Midtown, DowntownDistrict.cpp):
  - `SpawnDistrict()` (line ~91) is **never called**. Midtown's BeginPlay spawns `ACityLife` (traffic, pedestrians and wanted level), plus houses, dealership, casino, barber, clothes, airstrip, street lamps and night/wet lighting (`SetNight(bNight, bWet)` ~976).
  - Midtown sits about 1.95 km west of Harbor (OffsetX -195000).
  - `SetPlayArea` is a circle around Harbor; `AIronManCharacter::TickBoundary` pushes the player back.
- `ACityLife` / `ACityCar` / `ACityPedestrian` (CityLife.h/.cpp, ~980 lines):
  - Traffic: up to 8 kinematic cars on Midtown's 9x9 road grid, in `TickTraffic` / `PlaceTrafficCar`.
  - Pedestrians: a pool of 12 with states Wander / Idle / Watch / Flee / Dead.
  - Wanted level: `AddCrime` / `SetWanted` / `ClearWanted` / `TickWanted` / `SpawnResponders` (~671-863). Thresholds are 50 / 150 / 350 / 700 / 1200 heat.
  - Responders are reused enemies (drone, gunship drone, tank) with a red/blue `PoliceLight`, a siren and a PA. The table is in `WantedPressure::ForStars` (WantedPressure.h), with a 12-enemy cap.
- `ACityVehicle` (CityVehicle.h/.cpp): a drivable `AActor`, not a pawn.
  - Arcade handling in `TickGround` / `TickPlane` / `TickHeli`, with a chase camera.
  - The character stays possessed but hidden and attached. `UCareerInput::TickVehicle` (CareerWidgets.cpp ~2054) forwards raw hard-coded keys: W/S/A/D/Space/Ctrl/X/Q/Shift/E.
  - Delivered by the phone (N) or the shop through `DeliverVehicle`.
  - The catalog is `Career::Vehicles()` (14 entries) and `Career::Houses()` (6), both in CareerTypes.cpp.
- `UCareerInput` (CareerWidgets.cpp):
  - Raw-polled keys via `WasInputKeyJustPressed`: U, K, N, M, O, E, F1/` and the vehicle keys.
  - `Key()` reads `[IronManArena.Keys]` from GameUserSettings.ini, then DefaultGame.ini. Only 4 keys are rebindable and nothing writes that section.
  - `SetSuitOff` (~1841) / `RequestSuitUp` (~1895) / `TickKeys` (~2090): O exits the suit, leaving an `AParkedSuit` and attaching an `APilotAvatar`. Walk speed is 420, but the camera, capsule and eye height (74) stay the suit's.
  - The dev menu is `UCareerScreen::BuildDevBar` (~402), gated by `IsDevMenuAllowed` (~1705).
- `UCareerHUD::NativePaint` (CareerWidgets.cpp ~1454): immediate-mode paint of credits, wanted stars, "LOSING THEM %", toasts, world markers (M toggles them), prompts and the speedometer.
- `USettingsWidget` (SettingsWidget.cpp): a single UMG scroll list built through WidgetTree. KEY BINDINGS (`AddControlsReference` ~165) is read-only and uses `Generated/ControlsReference.inl`; its rows have `RebindName` / `RebindDefault` and a `CONTROLS_RESERVED_KEY` list.
- `USuitSuperComponent` polls H, B, R and the D-pad. Gamepad X (FaceButton_Left) is bound to both Hulkbuster and Web-zip, which is a bug.
- `UHelmetHUDWidget` (HelmetHUDWidget.cpp, ~2968 lines): built from `Content/UI/hud_layout.json`.
  - Threat radar (enemy blips only), `PushWarning`, `SubtitleBar`.
  - Callouts: `FHelmetHUDCallouts` (HelmetHUDCallouts.cpp).
- `UArenaHUDWidget`: health, energy, heat, wave, boss bar, banners and subtitles.
- Combat:
  - `ArenaCombat` handles hitscan, tracers, hit markers and kill streak; `UArenaFeelSubsystem` handles hit-stop, trauma shake and decals.
  - Effects: `AArenaBlastFX` is pooled by `UArenaFXPool`.
  - Enemies: `ADroneEnemy`, `ATankBot` (no navmesh) and `ABossTitan` (phases plus a cannon/missile kit). All derive from `AEnemyBase`, with `TakeDamage` / `TickEnemy`.
  - Waves come from `AArenaGameMode::StartWave` / `SpawnFromQueue`.
  - `UBossVFXComponent` (BossVFXComponent.cpp ~36-47) has soft references to Niagara systems under `/Game/FX/...` that don't exist, so it falls back to BlastFX.
- Cutscenes: `AStoryDirector` (StoryDirector.cpp ~1302-1760: `PlayCutscene`, `BeginShot`, `UpdateCamera`, `TickCutscene`).
  - One `ACineCameraActor`, framed by keyword-matching each shot's camera note.
  - Pacing: voice length, or text at 16 chars/s plus 1.2 s, clamped between 2.5 and 14 s.
  - `UCinematicOverlayWidget::SetCaption` (~130): captions at a fixed 20 pt with no background plate; they ignore `SubtitleScale` and long lines aren't split.
  - `ASuitUpCinematic` handles the suit-up sequence.
- NPC looks: `APilotAvatar` (PilotAvatar.h/.cpp) builds a basic-shape person with faces, hair and clothes. Pedestrians are randomised in `ACityPedestrian::Randomize` (CityLife.cpp ~337).
- Menus: `UMainMenuWidget` (Title / Paused / GameOver), `AMenuCameraActor`, `USuitSelectWidget`, `UStoryMenuWidget`. **There's no loading screen.** The city builds synchronously in BeginPlay.

## UI patterns (there's no AHUD::DrawHUD)
1. UMG built in C++: `RebuildWidget()` calls `BuildTree()`, which uses `WidgetTree->ConstructWidget<...>` and the `ArenaUI::MakeText` / `MakeButton` helpers.
2. Raw Slate: `SNew(...)` returned from `UUserWidget::RebuildWidget`, with the `Txt` / `Btn` / `Selector` / `Frame` helpers in CareerWidgets.cpp's anonymous namespace. Used by the `UCareerScreen` pages.
3. Immediate-mode `NativePaint` with `FSlateDrawElement`. Used by `UCareerHUD` and the damage arcs.

Z-orders: ArenaHUD 0, HelmetHUD 1, menus 10-12, CareerHUD 40, career screens 60.

## Conventions
- **Formatting:** tabs, many one-line `{ ... }` bodies, a `LOCTEXT_NAMESPACE` per file, header comments describing the design.
- **Lookups:** `AFoo::Find(WorldContext)` via `TActorIterator`; `UFoo::Get(WorldContext)` for subsystems.
- **JSON:** `FFileHelper::LoadFileToString(FPaths::ProjectContentDir() / "Story/x.json")` plus `FJsonSerializer`. Cache it in a function-local static, parse tolerantly, and fall back to compiled-in defaults. If you add a new loose JSON data folder, it must be staged in `Config/DefaultGame.ini` `+DirectoriesToAlwaysStageAsNonUFS`.
- **Headless switches:** `-ArenaSmoke`, `-StoryAutoplay`, `GIsAutomationTesting`. Keep them working.
- **Input:** every new keyboard control needs a gamepad equivalent where it makes sense.
