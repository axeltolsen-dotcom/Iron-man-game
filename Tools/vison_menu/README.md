# Tools/vison_menu (Vison)

Settings Controls tab design: tabs (Video, Controls, Audio, Accessibility, Helmet HUD), a bindings table taken from the C++ source, and a rebinding / gamepad proposal.

## Files

- `settings_controls.md` — design
- `controls_bindings.json` — source of truth for the key list
- `gen_controls_table.py` — verifies every key name against `../../Source/IronManArena`
- `settings_tabs.patch` — Phase 0 UMG tabs + read-only Controls list (applies to Source/). One comment in this patch still mentions the old `contrib/vison/menu` path; the live files are here.

`ControlsReference.inl` already lives at `Source/IronManArena/Private/Generated/ControlsReference.inl`. This folder does not include a second generated copy. If you regenerate, run the script without `--check` and copy the new `.inl` over that Source path (the script only writes next to itself).

From this directory:

    python3 gen_controls_table.py --check
