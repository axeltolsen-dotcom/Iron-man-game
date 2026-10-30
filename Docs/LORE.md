# Lore hooks and Vision Quest nods

The story is a fan "what if": Harley Keener picks up Tony's legacy after the snap. The game leaves room for more lore without code changes.

- **Per-chapter `lore` (story.json):** an optional string per chapter, shown under the chapter info in the Story menu. Use it for archive notes, Stark Industries file entries or Easter eggs, for example:
  ```json
  { "id": "ch7_skyline", "lore": "ARCHIVE: Mark L nanotech - stored in the housing Harley's rig was built around." }
  ```
  The shipped script leaves it empty, because the contrib story.json is authoritative.
- **Vison:**
  - Spelled as in the contrib script, as a nod to the *Vision Quest* series.
  - The ally (`AStoryAlly`) uses red/green synthezoid shapes, a gold forehead gem and a cape.
  - The cast `Synthezoid` body has its own placeholder. Swap in a mesh or MetaHuman per Docs/ADDING_METAHUMANS.md.
  - The voice is a lowered "alan" voice with an echo.
- **Speakers and personas:**
  - Story speakers are open-ended (`speakers` in story.json). A new speaker only needs a voice entry in `Tools/generate_story_audio.py` (VOICES) and, optionally, a cast entry.
  - Suit personas (JARVIS/FRIDAY/EDITH/ULTRON/WAR MACHINE) come from `voices/*.json`. Lines tagged as lore can be added there for more banter.
- **Suits:** `suits.json` has a `description` for every suit, shown on suit select. Longer histories can go there.
- **Room for more:**
  - Ideas: a "Vision Quest" side chapter, a Westview-style location, Ultron's suit lore in ch9.
  - All three are data-only: add a chapter to story.json; reuse the existing objective types and locations.
