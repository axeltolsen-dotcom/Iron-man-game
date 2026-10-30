# Adding MetaHumans (or any skeletal mesh) to story mode

Story cutscenes spawn one `AStoryCharacterNPC` per speaker. Each NPC is an `ACharacter`, so the stock Manny/Quinn animation blueprints work on it.
`AStoryDirector::CastList` (an array of `FStoryCastMember`) decides what each NPC looks like.

The director picks a look in this order:

1. **`MetaHumanClass`**: a full MetaHuman Blueprint (`BP_<Name>`).
   - It is spawned feet-down, attached to the NPC, with collision off.
   - It is hidden and shown together with the NPC, and destroyed with it.
   - The NPC's own mesh and placeholder shapes are hidden.
2. **`Mesh` + `AnimClass`**: any `USkeletalMesh` on the NPC's mesh component.
   - With no AnimClass, it tries the Third Person template's ABP_Unarmed / ABP_Manny.
   - The defaults are `/Game/Characters/Mannequins/Meshes/SKM_Manny` and `SKM_Quinn`.
3. **Placeholder shapes**: human, robot arm (U, DUM-E), synthezoid (Vison), or a SuitRig suit (Tony in the Mark 85 for the prologue, Morgan in Rescue for the finale).

A missing asset never breaks the game. The director checks `FPackageName::DoesPackageExist` and falls back to the next option.

## Default cast

| Speaker (story.json) | Body | Default mesh | Scale |
|---|---|---|---|
| TONY | Human (Mark 85 suit in the prologue, blue hologram in "recording" shots) | Manny | 1.00 |
| PEPPER | Human | Quinn | 0.97 |
| MORGAN | Human (Rescue suit in the finale) | Quinn | 0.88 |
| PLAYER (Harley, out of the suit) | Human | Manny | 0.93 |
| Vison | Synthezoid | placeholder | 1.02 |
| U, DUM-E | Robot | placeholder | 1.00 |

Suit AIs (JARVIS, FRIDAY, ...) are voice only.

## Steps

1. **Get the MetaHumans.**
   - Make them in MetaHuman Creator.
   - In UE 5.8, either add them through Fab or Quixel Bridge (*Window → Fab / Quixel Bridge → MetaHumans → Add*), or use the in-editor MetaHuman Creator plugin and export.
   - They land in `/Game/MetaHumans/<Name>/BP_<Name>`.
   - Open each one once and let the editor compile its shaders. This is slow the first time.
2. **Create a director Blueprint.**
   - Content Browser: *Add → Blueprint Class → All Classes → StoryDirector*. Name it `BP_StoryDirector` and save it in `/Game/Story/`.
3. **Fill in the cast.**
   - Open `BP_StoryDirector`, then *Class Defaults → Story|Cutscene → Cast List*.
   - Edit an entry, or add one. The *Speaker* must match the story.json speaker key exactly; it is case-sensitive (`Vison`, `DUM-E`).
   - For a MetaHuman, set **Meta Human Class** = `BP_<Name>` and leave *Body* = Human.
   - For a plain mesh, set **Mesh** and, optionally, **Anim Class**.
   - Keep *Scale* at 1 for MetaHumans. Their proportions come from the Blueprint. The teen scale only applies to meshes and placeholders.
4. **Tell the game mode to use your director.**
   - Add this to `Config/DefaultGame.ini`:
     ```ini
     [/Script/IronManArena.ArenaGameMode]
     StoryDirectorClass=/Game/Story/BP_StoryDirector.BP_StoryDirector_C
     ```
   - Or set *Story Director Class* on a Blueprint subclass of ArenaGameMode.
   - Note: `/Game/Story/` in the Content Browser is `Content/Story/`. That folder is staged loose for story.json, so keep **only** `BP_StoryDirector` there and put the MetaHumans in `/Game/MetaHumans`.
5. **Cook them.**
   - Cast assets are soft references, so add their folders to `Config/DefaultGame.ini`:
     ```ini
     [/Script/UnrealEd.ProjectPackagingSettings]
     +DirectoriesToAlwaysCook=(Path="/Game/MetaHumans")
     +DirectoriesToAlwaysCook=(Path="/Game/Characters/Mannequins")
     ```
   - Alternatively, list them in *Project Settings → Packaging → Additional Asset Directories to Cook*.

## Animation and faces

- The NPC does not drive a MetaHuman's face or body; the Blueprint's own idle/AnimBP runs.
- For lip sync:
  - add a Level Sequence or a MetaHuman Animator performance to `BP_<Name>`, or
  - drive the face AnimBP from the playing `UAudioComponent` with Audio-to-Facial / audio-driven animation in 5.8.
- The story voice lines are `/Game/StoryAudio/<SPEAKER>/<lineId>`, so you can pre-bake a performance per line id.
- Camera framing uses `AStoryCharacterNPC::GetFocusPoint()`: eye height ~160 cm × scale (125 cm for robots). If a MetaHuman's head sits noticeably higher or lower, adjust its *Scale* or override `GetFocusPoint` in a C++ subclass.

## Performance

MetaHumans are heavy. Use their LOD sync (*Forced LOD* 1–2 for cutscenes on mid-range PCs) or the "Optimized" MetaHuman export. Only the speakers in the current cutscene are visible.
