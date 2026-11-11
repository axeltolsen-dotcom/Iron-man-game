# Fixes from the PR #1 review (Friday's HUD, callout and sound review), for after the merge

Line numbers refer to head 5f2c656, before the 13 jobs landed. Re-locate everything by function name. **Check every claim against the current code before changing anything**, and skip any item that's wrong or already fixed by a job.

## Overlaps with the jobs: only finish them if the job left them undone
1. **Duplicate subtitles in first person.** `UArenaHUDWidget` calls `TickSubtitles` unconditionally, and `UHelmetHUDWidget::UpdateSubtitle` draws `SubtitleBar` in the same bottom-centre spot.
   - Fix: collapse the ArenaHUD subtitle labels when `AreGaugesHandedOff()` is true.
   - Call `TickHelmet` before `TickSubtitles`, because `bGaugesHandedOff` is set in `TickHelmet`.
2. **Cutscene captions** (`CinematicOverlayWidget`): wrap at about 60% of viewport width, put a 0.55-opacity plate behind them, apply `SubtitleScale`, and show the speaker's name in their colour.
   - Make the intent explicit for Voices off + Subtitles off. In `StoryDirector`, `Sound` is null when Voices are off, so the caption still shows. Keep the caption when there's no voice; otherwise the cutscene is silent. Say so in the Settings label.
3. **Late hit feedback.** `UHelmetHUDWidget` (around the `!bVisible` early return, near `LastSeenHitSerial`) delays it.
   - Update `LastSeenHitSerial` before the early return.
   - Only play the SFX or callout if the hit is fresh (`HUDDataSubsystem::SecondsSinceHit < 0.3`).
4. **Respawn replays the mission-start line.** In `HelmetHUDCallouts`, `Reset()` on pawn change also clears the mission state (`StartedChapter`, `bHaveMissionState`). Reset the mission state only on a chapter or level change.
5. **`FillStateFromGame` per-frame cost** (`HelmetHUDWidget`):
   - It rebuilds `FHelmetHUDState` arrays every frame. Reuse them with `Reset()`.
   - It line-traces AGL every frame. Trace at about 10 Hz.
   - It runs two `TActorIterator` scans every frame (enemies and pickups). Cache them, either by registering with `UHUDDataSubsystem` in `BeginPlay`/`EndPlay` or by refreshing at 5 Hz.
10. **Stage `Content/Audio`** in `DefaultGame.ini` with `+DirectoriesToAlwaysStageAsNonUFS=(Path="Audio")`, so the sfx manifest is in the package.

## New items, not covered by any job
6. **Voice audio components pile up when Voices are on.** `JarvisComponent` and `StoryDirector` call `CreateSound2D(..., bAutoDestroy=false)` for every line, and the old component is only stopped or overwritten. Keep one `UAudioComponent` per owner and use `SetSound` + `Play`.
7. **`ArenaAudio` mutates the shared `USoundWave::bLooping`** in `Attach` with `bLoop` and in `Loop2D`, and never resets it. Only loop events the manifest flags `"looping": true`, and log a warning instead of mutating otherwise.
8. **`DamageDirectionWidget` allocates in Paint.** It builds a `TArray<FVector2f>` per segment per arc every frame. Use a `mutable` scratch array on the widget and `Reset()` it each time.
9. **The HUD Scale slider uses `SetRenderScale` on the whole widget** (`ArenaPlayerController`, about lines 112-114).
   - Edge-anchored elements drift on or off screen.
   - ArenaHUD isn't scaled at all.
   - Fix: wrap the root canvas in a full-screen scaling container with the user's scale, and apply the same scale to both HUDs.

## Polish to include where it's safe
- **`PowerWarnAt`:** `HelmetHUDCallouts` hard-codes `PowerWarnAt = 0.30`, but `hud_layout.json` has `power.low = 0.20` and `critical = 0.10`. Read the JSON.
- **The `lowpower` banter line** ("Charge pad, two blocks north") states something that may not be true. Make it generic.
- **`Scripts/Windows/package.bat`:**
  - Add `-prereqs -nocompileeditor -unattended -nosplash`.
  - Quote paths safely in the inline PowerShell. A path with an apostrophe breaks the single quotes; passing values through environment variables avoids that.
  - Keep CRLF line endings.
- **HUD beeps:** add a `USoundConcurrency` group capped at 3, created in code, so beeps don't stack.
- **Wire `bIncomingMissile` and `bPullUp`.** At the end of `FillStateFromGame` they're marked "not wired".
  - `bIncomingMissile`: set it from enemy missiles or projectiles homing on the player.
  - `bPullUp`: use the real AGL when diving fast near the ground.

# Ultron's flight and wanted hand-off (PR #1 comment): constraints and fixes for after the merge

## Flight: keep these behaviours exactly (IronManCharacter.cpp TickFlightPhysics / SetFlying / ApplyFlightCameraForState / TickBoundary)
The new flight work (data file, auto-land, landing pose and effects, boost and sonic-boom effects) must sit on top of these. Revert any part of the flight job that changes them.
- **Speed caps:** Hover 900 / Cruise 3500 / Boost 7500 × K.
- **Hover↔cruise hysteresis:**
  - Enter cruise at HoverMax×1.05, or after 0.15 s of stick Y > 0.55 with Speed > 500.
  - Leave cruise only below HoverMax×0.72 with stick Y < 0.35.
  - Never go back to a single-frame flip at HoverMax×0.9.
- **Boost auto-forward:** add Look×0.35 only when the stick is dead (size < 0.12). Never add it when In.Y < −0.2.
- **Banking:** one bank write (physics interp 8). Camera roll is FlightBank×0.22, clamped to ±10°.
- **Chase-camera lag per state**, applied when FlightState changes: hover 12/120/14, cruise 9/180/12, boost 6/260/9.
- **Soft ceiling:** bleed speed off in the last 200 cm, and hard-zero V.Z only 50 cm past the ceiling.
- **Landing flare:**
  - Clamp V.Z to −900.
  - When DistToFloor < 350, clamp to −420 instead and apply 0.12 s of XY brake, then MOVE_Falling.
- **Soft takeoff:** blend toward 320 instead of hard-setting 250.

## Wanted level: no second wanted system
Cruisers, roadblocks, the helicopter and BUSTED must extend ACityLife's SpawnResponders and TickWanted. Keep all of this:
- **Heat per crime:** scrape 12, explode 70, ped 90, responder 35, gunship 110.
- **Star thresholds:** 50 / 150 / 350 / 700 / 1200.
- **Spawn limits:**
  - SpawnResponders trims so that NonResponderAlive + Responders ≤ 12.
  - It uses the star composition table: 5★ = 5 drones + 1 tank + 1 gunship, capped at 7.
  - One toggling police light.
  - Respawn time goes from 5.2 s at 1★ down to 3.75 s at 5★.
- **Waves:** MaxAlive=9 stays for non-responders. The boss is never spawned as a responder.

## Fix these three gaps
1. **Responders block wave-clear.** ArenaGameMode::GetHostilesRemaining and TickWaves count every living AEnemyBase. Filter them with `!CityLife::IsResponder(*It)`, so wanted stars don't block wave-clear or fight MaxAlive=9.
2. **The WantedPressure multipliers are never used.**
   - Only when `CityLife::IsResponder(this)`, apply `WantedPressure::Get(this)` multipliers:
     - DroneEnemy::TickEnemy: Lead (currently hard-coded at 0.4), OrbitDistance, DroneSpring and FireInterval.
     - TankBot::TickEnemy: MoveSpeed and FireInterval.
   - Example at 5★: FireInterval×0.70, Lead×1.00, OrbitDist×0.80, DroneSpring×1.50, TankMove×1.30.
   - Leave wave (non-responder) AI unchanged.
3. **Stars clear too early.** CityLife::TickWanted clears the stars as soon as Heat drops below 50, which skips the half-escape-time gate. Keep Stars at least 1 until HiddenTime ≥ 0.5×(8+4×Stars), then call ClearWanted, so the "lost the cops" toast shows.

## Ultron's exact fixes (three follow-up comments)
**A. GetHostilesRemaining and TickWaves.** Both loops have `if (!It->IsDead()) { ++Alive; }`. Change both to:
```cpp
if (!It->IsDead() && !CityLife::IsResponder(*It)) { ++Alive; }
```
- Include CityLife.h.
- MaxAlive=9 stays as the non-responder wave cap.

**B. TickWanted.** Inside the search radius and unseen, let Stars fall with the heat thresholds, but never below 1 until the timer is met:
```cpp
if (NewStars <= 0)
{
    if (HiddenTime >= 0.5f * (8.f + 4.f * float(Stars)))
    {
        // existing LostQuiet toast + ClearWanted
        return;
    }
    Stars = 1; // stay wanted until the unseen timer
}
else
{
    Stars = NewStars;
}
```
- Replace the code that clears as soon as Heat < 50.
- Leave the leave-zone path alone: HiddenTime >= 8+4*Stars → ClearWanted.
- Heat cools at 6/s while you're unseen inside the zone.

**C. Responder AI.** Only when `CityLife::IsResponder(this)`, use `WantedPressure::Get(this)`. It returns identity otherwise, so wave enemies stay as they are.

DroneEnemy::TickEnemy:
- `FireCooldown *= Press.FireInterval` (1.00 at 1★ to 0.70 at 5★)
- The lead factor becomes `Press.Lead` instead of the hard-coded 0.4 (0.40 at 1★ to 1.00 at 5★).
- `OrbitDistance *= Press.OrbitDist` (1.00 to 0.80)
- `Spring *= Press.DroneSpring` (1.00 to 1.50)

TankBot::TickEnemy:
- The Want speed uses `MoveSpeed * Press.TankMove` (1.00 to 1.30).
- `FireCooldown *= Press.FireInterval`
