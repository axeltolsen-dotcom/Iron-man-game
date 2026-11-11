# Claude's plan for the CLAUDE.md to-do list (paused)

The work was paused to save usage, before any job code was written. This file and its two companions hold the whole plan, so it can restart from here:
- `Docs/CLAUDE_TODO_BRIEF.md` is the shared brief every job reads first: codebase map, MSVC/UHT/unity-build rules, conventions.
- `Docs/CLAUDE_TODO_REVIEWS.md` holds Friday's and Ultron's review items from PR #1, to do after the merge.

## How it runs
1. Each job gets its own sparse git worktree on branch `todo/<key>`, made from `claude/todo-list`.
2. One implementer builds it, then two reviewers check it: one acting as the MSVC compiler and UHT, one on gameplay. Then one fixer applies what they found.
3. Merge every `todo/<key>` into `claude/todo-list`.
4. Apply the review items from `CLAUDE_TODO_REVIEWS.md`, checking each against the code first.
5. Run a cross-cutting compile and merge audit, then a final fix.
6. Push to PR #1 with a test checklist for Grok and Axel to build on the Windows PC.

Nothing here has been compiled. A build on Axel's PC is the real test.

## The 13 jobs

### `buildfix`: Windows build safety, packaging, settings and performance
1. **Unity-build safety.** Find every file-local name used in more than one `.cpp` and rename it to something unique to its file. Known clashes:
   - `PropCull` in CityLayout and DowntownDistrict
   - `Data()` in CityLayout and NpcManifest
   - `Kit()` in CityKit and SuitArmorKit
   - `Num`/`Str` in CityLayout and SuitArmorKit
   - `Resolve`, `Pick` and `MakeText`

   Also fix other MSVC-only risks, such as C4459 and designated initializers.
2. **Packaging.** Stage every JSON file the game reads at runtime in `DefaultGame.ini` under `DirectoriesToAlwaysStageAsNonUFS`. The NPC, Audio and CityKit manifests are known to be missing.
3. **Graphics settings get overwritten.** `UCareerSubsystem::Tick` applies `ApplyScalabilityPreset(2)` on every launch and forces `r.Lumen.HardwareRayTracing` and `r.ScreenPercentage` with `ECVF_SetByCode`. Apply that preset only when the dev bar asks for it, and never override the player's Settings.
4. **Performance.** Make safe per-frame wins, aiming for 60 fps at 1080p on a 2070 SUPER.

### `midtown`: bring Midtown (the living city) into free play, with a day/night clock and rain
- **Spawn Midtown.** `ADowntownDistrict::SpawnDistrict()` is never called. Spawn it in free play so `ACityLife` starts: traffic, pedestrians, the wanted level, the shops and the airstrip.
- **Play area.** Widen it to cover Harbor and Midtown, which sits about 1.95 km west. Keep a boundary and a ceiling.
- **Travel.** Make the dev bar's TP CITY and the phone taxi work.
- **Day/night clock.** A full day takes about 40 minutes and drives `SetTimeOfDay` and `SetNight`. Save the time of day.
- **Rain.** Cheap procedural rain at night and in overcast weather, which also wets the streets.
- **Cars at night.** Headlights and taillights, with real lights only on the nearest few cars. Traffic density follows the hour.

### `flight`: better suit flight feel
- **Data file.** Move the tuning numbers into `Content/Story/flight_params.json`, keeping Ultron's values as the defaults (see `CLAUDE_TODO_REVIEWS.md`).
- **Turning.** Banking that comes from the turn rate, with camera roll inside Ultron's limits.
- **Boost.** An FOV kick, camera shake, speed lines, and a sonic-boom ring with a sound.
- **Landing.**
  - Auto-land when you touch down slowly.
  - On a hard landing: a superhero landing pose, a dust or shockwave ring, and a camera dip.
- **Control.** An air-brake, and a throttle curve for the gamepad.

### `onfoot`: out of the suit, walking as yourself, and getting into cars
- **Camera and body.** A third-person over-the-shoulder camera, with V for first person at a human eye height of about 165 instead of 74. A capsule the size of a person.
- **Movement.**
  - Walk at 300, jog at 420, sprint at about 650.
  - Jumping.
  - A walk animation that matches the speed.
- **Getting into cars.** Press E (Y on gamepad) near a stopped traffic car. The driver calmly gets out and walks away (no violence), and the car becomes something you can drive.
- **Suiting back up.** Smooth, and everything goes back to how the suit had it.

### `vehicles`: gamepad driving, a garage that remembers, and damage
- **Gamepad driving.** Stick to steer, triggers for throttle and brake, A for the handbrake, Y to get out. Keyboard keys go through `UCareerInput::Key` so they can be rebound.
- **Garage.** Where you leave a vehicle is saved. You can call it with the phone.
- **Damage.**
  - Smoke and sparks, then the vehicle wrecks.
  - It comes back at the garage after a short delay, for a small repair fee.
- **Feel.** Steering that depends on speed, drifting with the handbrake, and engine pitch that rises with speed.

### `police`: police that chase you, roadblocks, getting busted
- **Same wanted system.** All of this goes into `ACityLife`'s existing `SpawnResponders` and `TickWanted`.
- **Cruisers.** Built from basic shapes, with a light bar, siren and driver. They chase you on the road grid and ram your car.
- **Bigger response at higher stars.**
  - At 3★ and up, roadblocks.
  - At 4★ and up, a helicopter with a searchlight.
  - Police drones stay for when you're flying in the suit.
- **BUSTED.** Caught while out of the suit or stopped in a car means you lose 10% of your credits, your stars are cleared, and you respawn at home.
- **Limits.** Respect the 12-enemy cap.

### `controls`: a real Controls tab
- **Saved bindings.** Keyboard and gamepad maps in `UArenaGameUserSettings`.
- **The tab.** Settings gets a tab where you click a binding and press the new key or button. It checks for conflicts, has a reset, and works with a gamepad.
- **Live updates.** The Enhanced Input mapping context rebuilds right away when a binding changes.
- **One table for everything.** Every raw-polled key reads the same table: the ones in `UCareerInput`, `USuitSuperComponent` H/B/R and the D-pad, and Left Alt.
- **Bug fix.** Gamepad X is currently both Hulkbuster and Web-zip.

### `combat`: hit feedback, more enemy types, better boss fights
- **Hit feedback.** Damage numbers, a hit flash, stagger, a stronger kill effect, and sparks when armour breaks.
- **New enemies.** Two or three new `AEnemyBase` types: an Interceptor jet drone, a Shield Drone, and a rooftop Missile Turret. Mix them into the waves.
- **The Titan boss.** Clearer phases and attacks you can see coming.
- **Effects.** A good-looking procedural fallback for every `UBossVFXComponent` cue, since the Niagara assets don't exist. Keep the soft Niagara paths.

### `cutscenes`: camera work, pacing, subtitles
- **Captions.** A background plate, `SubtitleScale`, line wrapping, speaker colours and fades.
- **Camera.** Eased moves: dolly, orbit, crane and push-in. Handheld noise, and rack focus on close-ups.
- **Pacing.**
  - Hold each shot a little after the line ends.
  - Quicker back-and-forth.
  - A "Space: next / Esc: skip" hint.
- Keep `-StoryAutoplay` working.

### `npcs`: better faces, skin and clothing, livelier crowds
- **People.**
  - A realistic, varied range of skin tones.
  - Better faces: eyes with whites and irises, brows, lips, ears, hairlines, beards, glasses and freckles.
  - Adult and teen sizes only: **no children**.
- **Clothing.** Many outfit combinations with materials that look different from each other.
- **Life.**
  - People walk at different speeds and in different styles.
  - Some walk in pairs and chat; some stop to look at their phones.
  - When the suit flies by they point, wave or take photos.

### `suits`: better materials, a better suit-up, real damage marks
- **Damage marks.**
  - Scorch, scratch or dent marks where a hit lands, capped and recycled.
  - Sparks on every hit, and smoke at low health.
  - Marks clear on repair.
- **Materials.** A more premium look for each suit, using the existing material parameters.
- **Suit-up.** Plates lock on one after another with sparks and sounds, then the faceplate closes last and the eyes ignite.

### `hud`: FRIDAY minimap and cleaner callouts
- **Minimap.**
  - A rotating map showing Midtown and Harbor.
  - Markers for the player, north, enemies, police, points of interest, houses, vehicles and the objective.
  - It zooms out with speed. Map data is cached instead of rebuilt each frame.
  - A key (rebindable, D-pad on gamepad) opens a big map with a legend.
- **Callouts.** Clean and readable, with priorities and fades, keeping the existing anti-spam rules.
- **HUD modes.** A HUD for walking on foot and for driving.

### `menus`: a finished main menu and a loading screen
- **Main menu.**
  - A title treatment, styled buttons with hover and focus states, and keyboard and gamepad navigation.
  - Animated accents, a footer, and fade transitions.
- **Loading screen.** A MoviePlayer loading screen with tips while the map loads.
- **City-build overlay.** An overlay that's painted before the city builds, so you never see a frozen frame. `-ArenaSmoke` and `-StoryAutoplay` keep working.
