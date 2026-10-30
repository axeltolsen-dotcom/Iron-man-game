# Story mode: "Iron Legacy: The Workshop Kid"

## Data

- **`Content/Story/story.json`**: title, default player name, speakers, prologue, 9 chapters and the finale.
- **`Content/Story/morgan.json`**: Morgan's "guy in the chair" comms chatter, organised by event.

Both are the contrib files, unmodified. They are read at runtime by `UStoryLibrary` and are not baked into C++. Edit them and restart. Errors go to the log (`LogIronManArena`). If the file is missing or fails to parse, story mode is disabled; arena mode still works.

### Chapters

Each chapter has these fields:
- `id`, `title`, `location`
- `intro` / `outro`: shots of `speaker`, `line`, `camera`
- `objectives[]`
- `unlockSuit`
- `reactor{stage,note}`
- optional `unlockAlly`
- optional **`lore`**: a string shown in the story menu (see LORE.md)

### Objectives

- **Types:** `ground` / `flight` (reach a beacon, marked `<> N m` on the HUD), `drone`, `tank`, `wave`, `boss`, `hulkbuster` and `energy` (collect energy cells).
- **Comms triggers:** each objective has `comms{start, progress, complete}`, played by `AStoryDirector`.
- **Ally:** `allyAvailable` (bool or a name) lets you call the ally with G. Vison is available in ch7 and ch9, and is unlocked in ch5.

### Reactor stages

These are applied per chapter to Harley's makeshift unit: `makeshift` → `unstable` → `failed` (ch5) → `upgraded` (ch7).

### Tokens

- **`{playerName}`:**
  - The default is Harley.
  - The player can edit it in the Story menu ("YOUR NAME") or at the `promptHeroName` prompt in the finale.
- **`{heroName}`:** the hero name the player types at that finale prompt.
- **Saving:** both names are stored in `ArenaSaveGame`.

### Finale

- **Ally:** `allySuit` is MORGAN in `rescue`. Morgan flies beside you as an AI ally.
- **Comms only:** MORGAN is a speaker, not a selectable AI persona.

## Flow

1. **Title → STORY:** the Story menu offers CONTINUE / NEW STORY / chapter select. Chapter select covers the chapters reached so far and the finale once it is unlocked.
2. **Prologue:** Tony (Mark 85) and the snap. Then Harley's workshop.
3. **Each chapter:** intro cutscene → objectives → outro → suit unlock → save.
   - Space or a click skips a shot; Esc skips the cutscene.
4. **Pause menu:** RESTART CHAPTER / QUIT TO TITLE. On game over, use RETRY CHAPTER.

## Voices

`Tools/generate_story_audio.py` (piper TTS + numpy) writes `SourceAudio/StoryAudio/<SPEAKER>/<lineId>.wav`, plus `<lineId>_noname.wav` for lines with name tokens, and `story_lines.csv`.

**Line ids:**

| Lines | Id format |
|---|---|
| Prologue | `prologue_NN` |
| Chapter intros / outros | `<chapterId>_intro_NN`, `<chapterId>_outro_NN` |
| Objective comms | `<chapterId>_obj<N>_<start\|progress\|complete>` |
| Finale | `finale_NN` |
| Morgan chatter | `MORGAN_<event>_<n>` |

**Playback:**
- At runtime the default take plays when the player's name is the default, Harley. Otherwise the `_noname` take plays, so the voice never says the wrong name.
- Subtitles always show the real name.
- Shot length is the WAV length, or reading time if there is no audio, with a minimum of `MinShotSeconds`.

**Regenerating:**
```bash
python Tools/generate_story_audio.py            # only missing takes
python Tools/generate_story_audio.py --force    # everything
python Tools/generate_story_audio.py --csv-only # just story_lines.csv
```
Then re-import the WAVs with `Content/Python/import_jarvis_audio.py` (README, manual step 1). This writes `/Game/StoryAudio/<SPEAKER>/<lineId>` and assigns the `SC_Voice` sound class.
