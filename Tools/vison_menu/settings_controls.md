# Settings: Controls tab design (Vison, menu lane)

Requested by Axel via Jarvis ("turn the KEY BINDINGS list into a real Controls tab where every key can be rebound and saved, with gamepad support"; CLAUDE.md "Finish and polish").
Status: **design + Phase 0 patch.** Nothing here has been compiled or run in Unreal (no UE on the box, and Jarvis asked nobody to touch Axel's PC).

Files in this folder: `controls_bindings.json` (source of truth), `gen_controls_table.py` (checks every key name against `../../Source/IronManArena`, then can write `ControlsReference.inl` next to the script), `settings_tabs.patch` (Phase 0 UMG tabs + read-only Controls list).

The generated `ControlsReference.inl` already lives at `Source/IronManArena/Private/Generated/ControlsReference.inl`. This folder does not include a second copy.

Verified against `origin/master` @ `9d50df0`.

## 1. What is in the code today (verified against `origin/master` @ 9d50df0)

- `USettingsWidget::BuildTree` (SettingsWidget.cpp) is one scroll box: VIDEO, CAMERA & CONTROLS (FOV, mouse sensitivity, Invert Y), AUDIO, ACCESSIBILITY, HELMET HUD, then the read-only **KEY BINDINGS** list (`AddControlsReference`, rows from `Private/Generated/ControlsReference.inl`, already merged in commit `8623124`). No tabs.
- Input actions and the mapping context are **built at runtime**: `AIronManCharacter::CreateDefaultInputIfNeeded()` (IronManCharacter.cpp ~L639-738) creates 20 `UInputAction`s and one `IMC_IronMan` with `NewObject`, `SetupPlayerInputComponent` adds it and binds the actions (L741-793). There are **no** `.uasset` actions or contexts, and `Config/DefaultInput.ini` has no action mappings (only mouse capture and the F11 / Alt+Enter flags).
- Many keys are **not** in Enhanced Input at all, they are polled: Left Alt focus (`TickAimFeel`), RMB / LMB / LT / R3 ultra and H / B / R (`USuitSuperComponent::TickInput`), E / U / K / N / M / O and all vehicle keys (`UCareerInput`), plus widget `NativeOnKeyDown` handlers (career screens, main menu, story menu, suit select, settings).
- Only **four** keys are rebindable today, by editing `[IronManArena.Keys]` in GameUserSettings.ini: `SuitExit` (O), `Faceplate` (H), `WebSwing` (B), `WebZip` (R), read by `UCareerInput::Key(Name, Default)` (CareerWidgets.cpp L1884). The list already shows "(rebound)" for them.
- Settings are stored in `UArenaGameUserSettings` (GameUserSettings.ini): `FieldOfView`, `MouseSensitivity`, `bInvertY`, applied by `ApplyGameplaySettings` -> `Player->SetLookSettings`.
- `Gamepad` is real for flight and combat (the IMC maps it). It does **not** exist for E / U / K / N / M / O / Alt-focus / quick suits / vehicles, and `UCareerScreen` (shop, phone, avatar) cannot be closed with a pad.
- PR #1 (`claude/todo-list`, draft) touches only `.gitattributes`, `Scripts/Windows/package.bat` and `WorkshopEasterEggs.cpp`. Nothing in settings, input or NPC code has changed, so there is no overlap with this design yet.

## 2. Tab layout

Tabs, in order: **VIDEO | CONTROLS | AUDIO | ACCESSIBILITY | HELMET HUD**. APPLY / DEFAULTS / BACK stay below the tabs.

```
 SETTINGS
 [ VIDEO ][ CONTROLS ][ AUDIO ][ ACCESSIBILITY ][ HELMET HUD ]      Q / E or LB / RB switch tabs
 +------------------------------------------------------------------------------+
 | CAMERA                                                                         |
 |   Field of view        ----o------  95                                         |
 |   Mouse sensitivity    ---o-------  1.00                                       |
 |   Invert Y             [ ]                                                     |
 | KEY BINDINGS            [ KEYBOARD / MOUSE ]  [ GAMEPAD ]   <- device filter  |
 |  ACTION                 KEYBOARD / MOUSE     GAMEPAD            RESET          |
 |  MOVEMENT & FLIGHT                                                              |
 |   Toggle flight         [ F ]                [ Y ]              (reset)        |
 |   Fly down              [ Left Ctrl ] [ C ]  [ B ]              (reset)        |
 |  COMBAT ...  SUIT ...  CITY & CAREER ...  PHONE ...  SYSTEM ...                |
 |  !  "H" is already used by Faceplate.   [SWAP]  [CANCEL]                        |
 |                                          [ RESET ALL CONTROLS ]                |
 +------------------------------------------------------------------------------+
        [ APPLY ]  [ DEFAULTS ]  [ BACK ]
```

- Pages are `UWidgetSwitcher` children inside the existing scroll box, so every `AddSlider` / `AddCheck` / `AddCombo` call works unchanged; `RefreshFromSettings` is untouched. **FOV, sensitivity and Invert Y move to the top of the Controls tab** (header renamed CAMERA). See `settings_tabs.patch`.
- Tab choice is remembered in a static (`USettingsWidget::LastTab`). `Q` / `E` and `LB` / `RB` cycle tabs (these two pad buttons are free in menus).
- The Controls tab keeps the existing generated list as its **read-only fallback** until the rebinding phases land, so the list is never empty.
- Groups, in this order (matches `controls_bindings.json`): Movement & Flight, Combat, Suit, City & Career, Driving & Piloting, Menus & Cutscenes (Pause, Skip, Resume, Back, Story menu, Suit select, Fullscreen, Game over). Phone sits in City & Career, because that is where the code puts it (N).
- Rows that cannot be rebound (menu navigation, axes, Esc, Menu, F11, Alt+Enter, dev bar) are shown dimmed with a small "fixed" tag, never hidden, so the list is the full truth.

## 3. The real bindings (read from code, nothing invented)

Source of truth is `controls_bindings.json`; `gen_controls_table.py --check` confirmed every key name appears as `EKeys::<Name>` in the cited file on `master` @ 98f0004. Pad names: A/B/X/Y = `FaceButton_Bottom/Right/Left/Top`, LB/RB = shoulders, LT/RT = triggers, L3/R3 = stick clicks, Menu = `Special_Right`, View = `Special_Left`.

**MOVEMENT & FLIGHT**

| Action | Keyboard / mouse | Gamepad | Source | Notes |
|---|---|---|---|---|
| Move / strafe | W A S D | Left stick | `IronManCharacter.cpp` | (IA_Move) |
| Look / aim | Mouse | Right stick | `IronManCharacter.cpp` | (IA_Look) |
| Toggle flight | F | Y | `IronManCharacter.cpp` | (IA_ToggleFlight) |
| Fly up / jump (on ground) | Space | A | `IronManCharacter.cpp` | (IA_Ascend) |
| Fly down | Left Ctrl / C | B | `IronManCharacter.cpp` | (IA_Descend) |
| Boost (hold) | Left Shift | Left stick click | `IronManCharacter.cpp` | (IA_Boost) |
| First / third person | V | Right stick click | `IronManCharacter.cpp` | (IA_ToggleView) |

**COMBAT**

| Action | Keyboard / mouse | Gamepad | Source | Notes |
|---|---|---|---|---|
| Repulsors (hold to keep firing) | Left mouse | RT | `IronManCharacter.cpp` | (IA_Fire) |
| Focus aim (hold) | Left Alt | - | `IronManCharacter.cpp` | TickAimFeel; no gamepad key. Slow hover while shooting gives half focus automatically. |
| Charged shot | Hold Left mouse while focused (Alt) | - | `IronManCharacter.cpp` | Repulsor / red blast / laser suits; full charge after 0.7 s at focus > 85 %. |
| Suit special (tap) | Right mouse | LT | `IronManCharacter.cpp` | (IA_Special) |
| Suit ULTRA / super move | Hold Right mouse 0.3 s, or double-click Left mouse | Hold LT 0.3 s, or LT + right stick click | `SuitSuperComponent.cpp` | Only when the super meter is ready. Double-click window 0.28 s, hold 0.3 s (SuitSuperComponent::TickInput). The gamepad chord LT + R3 also fires IA_ToggleView (R3) - see defaults_overlaps. |
| Hulkbuster | Q | X | `IronManCharacter.cpp` | While Hulkbuster is active, Left mouse punches. (IA_Hulkbuster) |
| Vent reactor | X | D-pad down | `IronManCharacter.cpp` | (IA_Vent) |
| Summon ally | G | D-pad up | `IronManCharacter.cpp` | (IA_Ally) |

**SUIT**

| Action | Keyboard / mouse | Gamepad | Source | Notes |
|---|---|---|---|---|
| Suit menu / wheel (hold, release to pick) | T | LB | `IronManCharacter.cpp` | Left mouse also confirms. (IA_SuitWheel) |
| Suit wheel: next page (wheel open) | Mouse wheel / Tab | RB | `IronManCharacter.cpp` | IA_WheelPage is ignored unless the wheel is open (Input_WheelPage). (IA_WheelPage) |
| Quick suits 1-5 | 1 2 3 4 5 | - | `IronManCharacter.cpp` | (IA_QuickSlot1..5) |
| Faceplate open / close | H | D-pad left | `SuitSuperComponent.cpp` | (legacy ini key `Faceplate`) |
| Exit suit / call suit | O | - | `CareerWidgets.cpp` | On foot: near the suit = suit up, far away = it flies to you. (legacy ini key `SuitExit`) |
| Web swing mode (Iron Spider) | B | D-pad right | `SuitSuperComponent.cpp` | In swing mode: hold Space (pad A) in the air to swing, release to fling. B toggles swing mode / flight. (legacy ini key `WebSwing`) |
| Web-zip (swing mode) | R | X | `SuitSuperComponent.cpp` | (legacy ini key `WebZip`) |

**CITY & CAREER**

| Action | Keyboard / mouse | Gamepad | Source | Notes |
|---|---|---|---|---|
| Interact (vehicles, shops, houses) | E | - | `CareerWidgets.cpp` |  |
| Shop | U | - | `CareerWidgets.cpp` |  |
| Avatar customizer | K | - | `CareerWidgets.cpp` | Also on the title / pause menu. |
| Phone | N | - | `CareerWidgets.cpp` |  |
| Map markers on / off | M | - | `CareerWidgets.cpp` | Toggles HUD bShowMarkers (not a full map screen). |
| Close shop / phone / avatar screen | Esc, or the same key again (U / N / K) | - | `CareerWidgets.cpp` | UCareerScreen::NativeOnKeyDown only knows keyboard keys: there is no gamepad close key (see proposed_gamepad). |

**DRIVING & PILOTING**

| Action | Keyboard / mouse | Gamepad | Source | Notes |
|---|---|---|---|---|
| Throttle / brake | W / S | - | `CareerWidgets.cpp` |  |
| Steer | A / D | - | `CareerWidgets.cpp` |  |
| Climb / descend (aircraft) | Space / Left Ctrl | - | `CareerWidgets.cpp` | On the ground Space is the handbrake. |
| Rudder (aircraft) | X / Q | - | `CareerWidgets.cpp` |  |
| Boost | Left Shift | - | `CareerWidgets.cpp` |  |
| Get out | E | - | `CareerWidgets.cpp` |  |

**MENUS & CUTSCENES**

| Action | Keyboard / mouse | Gamepad | Source | Notes |
|---|---|---|---|---|
| Pause | P / Esc | Menu (Start) | `IronManCharacter.cpp` | (IA_Pause) |
| Skip cutscene / suit-up | Space, Left mouse, Esc or P | A, RT or Menu | `IronManCharacter.cpp` | TrySkipCinematic() runs from IA_Ascend / IA_Fire / IA_Pause (Input_AscendStarted, Input_FireStarted, Input_Pause). |
| Resume (pause menu) | P or Esc | Menu (Start) | `MainMenuWidget.cpp` |  |
| Back / close screen | Esc | B | `SettingsWidget.cpp` |  |
| Story menu: chapter / side mission | Arrow keys | D-pad | `StoryMenuWidget.cpp` |  |
| Story menu: codex | C | View (Back) | `StoryMenuWidget.cpp` |  |
| Suit select: browse / confirm | Arrows or A / D, Enter | D-pad, A | `SuitSelectWidget.cpp` |  |
| Fullscreen | F11 / Alt+Enter | - | `ArenaPlayerController.cpp` |  |
| Game over: restart | Enter or Space | Menu (Start) | `MainMenuWidget.cpp` |  |

Not shown to players: dev bar `F1` / `~` (gated by `UCareerInput::IsDevMenuAllowed`).

### Keys that overlap in the defaults (real, from code)
| Id | Keys | Effect | Suggested fix |
|---|---|---|---|
| `pad_X_hulkbuster_vs_webzip` | Gamepad_FaceButton_Left | In Iron Spider swing mode X fires both when Hulkbuster charges exist. | WebZip pad key -> consume X only while swing mode is on (separate IMC at higher priority with bConsumeInput) or move Hulkbuster to a chord. |
| `pad_R3_view_vs_ultra` | Gamepad_RightThumbstick | The LT + R3 ultra also toggles the camera view. | Ignore IA_ToggleView while Gamepad_LeftTrigger is held (or move ultra to LT hold only; the LT 0.3 s hold already works). |
| `vehicle_pause_double` | Escape, P | While driving both paths can call TogglePauseMenu; check that one press does not open then close the menu. | Verify in PIE; if the pawn keeps input while driving, drop the TickVehicle branch. |
| `career_screens_hardcoded_toggle` | U, N, K | Open and close keys are literal EKeys in two places; rebinding must change both. | Both call ArenaInput::KeyFor(MappingName). |

## 4. Gamepad

**What exists:** flight and combat on pad are real (table above). Pad is **missing** for: focus aim, quick suits 1-5, exit / call suit, interact, shop, avatar, phone, map markers, all driving, and closing career screens with B.

**Proposed layout (NOT in the code, marked proposed everywhere):**
| Action | Proposed pad | Why it does not collide |
|---|---|---|
| Focus aim (hold) | RB (hold) | Gamepad_RightShoulder is only read by IA_WheelPage and that action returns early unless the wheel is open (Input_WheelPage L956). Ignore Focus while bWheelOpen. |
| Career chord modifier | hold View (Back) | One free key; nothing else uses it in gameplay (Menu/Start is pause). HUD shows the four chords while View is held. |
| Phone | View + D-pad up |  |
| Shop | View + D-pad left |  |
| Avatar customizer | View + D-pad right |  |
| Map markers | View + D-pad down |  |
| Exit suit / call suit | View + Y |  |
| Interact / get in / get out | X, but only while an interact prompt is showing or while driving | X is Hulkbuster / web-zip. A higher-priority IMC (priority 10, consume input) is added only while UCareerInput::InteractKind != 0 or Driving is valid, so X never competes outside those moments. |
| Quick suits 1-5 | - | Use the suit wheel (LB) on gamepad; five quick slots have no spare buttons. |
| Driving: throttle / brake | RT / LT |  |
| Driving: steer | Left stick X |  |
| Driving: climb / descend (air) \| handbrake (ground) | A / B |  |
| Driving: rudder (air) | RB / LB |  |
| Driving: boost | L3 |  |
| Driving: get out | X |  |
| Close shop / phone / avatar screen | B or Menu | UCareerScreen::NativeOnKeyDown only checks Escape and the toggle key. |

Chord tech: one `IA_CareerModifier` on `Gamepad_Special_Left`, and `UInputTriggerChordAction` on the five chord mappings so View alone does nothing in gameplay. Contexts: `IMC_Interact` (priority 10, `bConsumeInput`) is added only while `UCareerInput::InteractKind != 0` or the player is driving; `IMC_Vehicle` is added on `Enter` and removed on `Exit`, and it blocks flight actions while driving.

## 5. Rebinding approach (UE 5.8 Enhanced Input User Settings)

Goal: every row editable for keyboard / mouse and gamepad, saved, conflict-checked, resettable, with glyphs. Nothing here needs new art or a new plugin. **Verify exact names against the 5.8 headers (`EnhancedInputUserSettings.h`, `PlayerMappableKeySettings.h`, `InputKeySelector.h`) on Axel's PC before coding** (I only have 5.7 / 5.8 docs, not headers).

### 5.1 One owner for input: `UArenaInputSubsystem` (new `UGameInstanceSubsystem`)
- Move the body of `CreateDefaultInputIfNeeded()` here so the 20 actions and `IMC_IronMan` are built **once**, outer = the subsystem (not each pawn). The character keeps its `UPROPERTY` action pointers (copied from the subsystem) so nothing else changes. Why: the IMC is registered with user settings, and a per-pawn `NewObject` would register a new copy on every respawn.
- API: `FKey KeyFor(FName MappingName, const FKey& Default)`, `FText LabelFor(FName MappingName)` (key name or pad glyph text for the last-used device), `bool Rebind(Name, Slot/device, NewKey, FText& Err)`, `FindConflict(...)`, `ResetRow(Name)`, `ResetAll()`, `bUsingGamepad` (set from `AArenaPlayerController::InputKey` / `FSlateApplication` last event).

### 5.2 Enable and register
1. `Config/DefaultInput.ini`: enable Enhanced Input user settings (Project Settings > Plugins > Enhanced Input > "Enable User Settings"; ini key `bEnableUserSettings=True` under `[/Script/EnhancedInput.EnhancedInputDeveloperSettings]`; confirm the name in the 5.8 editor).
2. For every rebindable mapping in `IMC_IronMan`, create a `UPlayerMappableKeySettings` (`Name` = stable mapping name from `controls_bindings.json -> rebindable_rows`, `DisplayName` = LOCTEXT, `DisplayCategory` = group) and assign it to the key mapping. Use **per-mapping** settings, not per-action: `IA_Move` has four keys (W / S / A / D) and `IA_Descend` has two, so each key needs its own name (`Move.Forward`, `Descend`, `Descend.Alt`, ...).
3. One mapping name per **(action, device)**: `Fire` is the KBM key, `Fire.Pad` the gamepad key, so conflict checks stay inside one device family and a single cell edits exactly one mapping. (Slots work too; this avoids depending on the slot enum.)
4. `UserSettings->RegisterInputMappingContext(IMC)` once, on subsystem init, before any `MapPlayerKey` (known gotcha: without it `MapPlayerKey` finds no row).
5. Modifiers (`Swizzle`, `Negate`, `Scalar`, `ScaleByDeltaTime`) stay on the default mapping; the user setting only swaps the key. Acceptance check below covers W -> another key.

### 5.3 Polled keys go through the same table (phase B)
Today these ignore Enhanced Input: Left Alt (focus), H, B, R, O, E, U, K, N, M, and the **hard-coded** RMB / LMB / LT / R3 / Space / A in `SuitSuperComponent::TickInput` (L776, L910-923, L988). Plan:
- Minimal, safe: replace `UCareerInput::Key(Name, Default)` internals with `UArenaInputSubsystem::KeyFor`, and change every literal `EKeys::` for those actions to `KeyFor(...)`. Polling code stays as it is.
- Cleaner later: turn the new ones into real `IA_*` (Faceplate, SuitExit, WebSwing, WebZip, FocusAim, Interact, Shop, Avatar, Phone, MapMarkers) in `IMC_IronMan` and bind them like the rest.
- Legacy: on first run, if `[IronManArena.Keys]` has `SuitExit` / `Faceplate` / `WebSwing` / `WebZip` and the user profile has no override, call `MapPlayerKey` once so existing players keep their keys. Leave the ini reader as a fallback.
- Ultra follows `Special` (hold) and `Fire` (double-click): if those are rebound, ultra moves with them.
- `UCareerScreen::NativeOnKeyDown` (U / N / K close keys) and `UMainMenuWidget` (K) must call `KeyFor` too. Add `Gamepad_FaceButton_Right` / `Gamepad_Special_Right` as close keys.

### 5.4 Controls tab widgets
- One row per `rebindable_rows` entry: label, KBM cell (`UInputKeySelector`, gamepad disabled), pad cell (`UInputKeySelector`, `bAllowGamepadKeys`), reset-row button. `UInputKeySelector` already does "press a key" capture, shows a localised key name, and Esc cancels (Esc is also reserved, so cancel and bind never collide).
- Phase A rows use `MapPlayerKey`; read current values from `GetCurrentKeyProfile()->GetPlayerMappingRows()` so the list never shows a stale value.
- Rows build from data (the JSON becomes a generated `.inl` of mapping names + labels, the same generator), so adding an action adds a row.

### 5.5 Conflict detection
- **Reserved**, never assignable: Esc, F11, Alt+Enter, F1, `~`, Enter / arrows (menus), `Mouse2D`, `MouseWheelAxis`, both sticks, Menu (Start), View when used as the chord modifier. Esc / Menu always open the pause menu, even if `Pause` is rebound.
- **Context-aware**: same device family AND overlapping context = conflict. `Core` actions all conflict with each other. `Vehicle` may reuse any `Core` key, and `Menu` is fixed.
- On conflict show the banner: "`H` is already used by Faceplate": **SWAP** (exchange the two keys), **REPLACE** (the other row becomes unbound; refuse if the other action is *essential*), **CANCEL**. Essential rows need at least one KBM and one pad key: Move, Look, Fire, Special, Ascend, Pause.
- Allowed duplicates: `Fire` / `Ascend` / `Pause` with the cutscene skip (it is the same action by design).
- Unbind is allowed only for non-essential rows and for the alternates (`Descend.Alt`).

### 5.6 Save, load, reset
- `UEnhancedInputUserSettings` is its own save (not `GameUserSettings.ini`). After a change: `ApplySettings()` then `AsyncSaveSettings()` (queue; `SaveSettings()` on BACK to be sure). Load at subsystem init, before the IMC is added.
- **Reset row** = `ResetAllPlayerKeysInRow`. **Reset all controls** = reset every row of the current key profile. APPLY / DEFAULTS on the page keep their meaning for display settings; DEFAULTS does **not** wipe bindings (the Controls tab has its own RESET ALL CONTROLS with a confirm), so players do not lose keys by resetting graphics.
- Corrupt / missing save: fall back to defaults and log one warning.

### 5.7 Glyphs and live prompts
- No CommonUI in the project (`IronManArena.Build.cs`, `.uproject`), so no new plugin: use text keycaps now (`FKey::GetDisplayName()`), and for gamepad a small table `Gamepad_FaceButton_Bottom -> "A"` (Xbox) / `Cross` (PlayStation) chosen by the last-used device, drawn as a rounded keycap (`UBorder` + text).
- Optional icon pass later: CC0 input-prompt sprites (Kenney "Input Prompts", CC0, **check the licence file when importing**), mapped by key name. Original assets only, as CLAUDE.md requires.
- **Live labels:** `LabelFor(Name)` replaces every literal key in prompts and hints: `[E] get in` and friends (CareerWidgets.cpp L1578, L1978, L1992-2010; WorkshopEasterEggs.cpp L851-859), `[O] exit suit  [H] faceplate  [Alt] focus  [B]  [R]` (CareerWidgets.cpp L1667-1668), HUD `HULKBUSTER READY [Q]` (ArenaHUDWidget.cpp L217) and `[X] VENT` (L314; `UHelmetHUDWidget::VentKeyLabel` exists but is never set), `VENT [X] OR WAIT` (IronManCharacter.cpp L1581), the title / pause controls blurb (MainMenuWidget.cpp L156-160), and the Iron Spider toast (SuitSuperComponent.cpp L928).

### 5.8 Phases (each one can be merged on its own)
| Phase | Scope | Risk |
|---|---|---|
| 0 | `settings_tabs.patch`: tabs + Controls tab holding the read-only list (UMG only) | low |
| 1 | `UArenaInputSubsystem`, enable user settings, register IMC, per-mapping settings for Phase A rows, `UInputKeySelector` cells, conflict banner, reset, save / load | medium |
| 2 | Polled keys through `KeyFor`, legacy ini migration, live labels in HUD / prompts, pad close key for career screens | medium |
| 3 | Proposed gamepad layout (View chord, interact context IMC, focus aim on RB), glyph set | medium |
| 4 | Vehicle rows (own context, optional) | low |

## 6. Files to touch
- `Source/IronManArena/Public|Private/SettingsWidget.h|.cpp` (tabs, rows, banner)
- new `ArenaInputSubsystem.h|.cpp` (IMC + IA creation moved from `IronManCharacter.cpp::CreateDefaultInputIfNeeded`)
- `IronManCharacter.cpp|h` (take actions from the subsystem; `TickAimFeel` Alt -> `KeyFor("FocusAim")`)
- `SuitSuperComponent.cpp` (L776, L910-923, L967, L988: literal keys -> `KeyFor`)
- `CareerWidgets.cpp|h` (`UCareerInput::Key` body, `TickInteract`, `TickVehicle`, `TickKeys`, `UCareerScreen::NativeOnKeyDown`, prompts)
- `MainMenuWidget.cpp` (K, controls blurb), `ArenaHUDWidget.cpp`, `HelmetHUDWidget.cpp` (`VentKeyLabel`), `WorkshopEasterEggs.cpp` (prompt strings)
- `ArenaPlayerController.cpp` (last-used-device tracking)
- `Config/DefaultInput.ini` (`bEnableUserSettings`)
- `IronManArena.Build.cs`: nothing new (EnhancedInput, UMG, Slate are already listed).
- `Source/IronManArena/Private/Generated/ControlsReference.inl` + `Tools/vison_menu/controls_bindings.json` + `Tools/vison_menu/gen_controls_table.py` (regenerate, see `settings_tabs.patch`)

## 7. Acceptance checks (in the editor / PIE, on Axel's PC when Jarvis says it is free)
1. Settings opens on the last tab; tabs switch by mouse, Q / E and LB / RB; FOV, sensitivity and Invert Y sit on CONTROLS and still change the camera live; APPLY / DEFAULTS / BACK work from every tab.
2. The list shows exactly the table in section 3 (43 rows; spot-check ten against play).
3. Rebind `ToggleFlight` F -> J: F does nothing, J toggles flight; restart the game: still J. **RESET** returns F.
4. Rebind `Move.Forward` W -> I: forward still moves forward and `S` still moves back (Swizzle kept).
5. Try to bind `Faceplate` to `T`: banner "T is already used by Suit wheel" with SWAP / REPLACE / CANCEL; SWAP exchanges them.
6. Esc, F11, Alt+Enter, F1 and `~` refuse with a clear message; Esc during capture cancels.
7. Rebind `Special` RMB -> middle mouse: tap special and hold-0.3 s ultra both follow the new key.
8. Old `[IronManArena.Keys] Faceplate=J` in GameUserSettings.ini is picked up once and shows as J.
9. Plug in a pad: prompts switch to pad glyph text; unplug: keyboard keys again. Rebind `Fire.Pad` RT -> RB; the wheel page still uses RB only when the wheel is open.
10. Proposed layout (phase 3): View + D-pad opens phone / shop / avatar, View + Y exits the suit, X gets in a car only when the prompt shows, B closes the shop.
11. Delete the user-settings save: defaults load with one log warning. Respawn 5 times: still one registered IMC, no duplicated inputs.
12. No regression: Esc skips the suit-up, Space / click skip story cutscenes, pause opens with P / Esc / Menu while driving (check the possible double-toggle listed in section 3).

## 8. Open questions for Jarvis / Claude
- Q1: Are hold-to-rebind confirmation and a timeout needed, or is Esc-to-cancel enough? (Proposal: Esc only.)
- Q2: Should DEFAULTS also reset bindings? (Proposal: no, separate RESET ALL CONTROLS.)
- Q3: Keep `[IronManArena.Keys]` as a permanent fallback or remove after a release? (Proposal: keep for one release.)
- Q4: PlayStation glyphs: do we detect the pad type (`FPlatformMisc` / `IPlatformInputDeviceMapper`) or just ship Xbox text keycaps? (Proposal: Xbox keycaps first.)
