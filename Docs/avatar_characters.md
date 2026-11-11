# Player avatar: real character bodies

**Disabled:** the first-launch “CREATE YOUR CHARACTER” overlay, the **CUSTOMIZE AVATAR** main-menu entry and the **K** customizer hotkey are off. The game goes straight to the title menu with the default pilot (Harley). On-foot still uses that default (or a previously saved look).

The avatar creator previously (first launch over the title screen, **K** anywhere, or **CUSTOMIZE AVATAR [K]** on the title / pause menu)
showed a live 3D preview of the real body the pawn uses, turning on a stand (ROTATE < >).

## Which body is used (AvatarCatalog, scanned from the asset registry at runtime)
1. **Epic / Fab character tech** when it is in the project:
   * MetaHuman: any skeletal mesh under `/Game/MetaHumans/<Name>/` whose name contains `Body` (face mesh in the same folder follows via leader pose).
   * Character Creator **Kevin** (Reallusion): any skeletal mesh whose path contains `Kevin`, `CharacterCreator`, `Reallusion` or `CC_Base`.
2. Otherwise the **CityKit people** (`/Game/CityKit/People/*/SK_*`, 7 male + 7 female Quaternius bodies) with their own Idle / Walk clips.
3. Only if neither exists: the built-in shape body (`ima.AvatarCatalog 0` forces it).

The creator shows a note under the preview saying which tech is active.

## Add Kevin (Axel, one-time, needs your Epic login)
Epic Games Launcher -> **Fab Library** (Library tab) -> **Character Creator: Kevin** -> **Add to Project** -> pick **IronManArena** (UE 5.8).
Restart the editor/game; the creator switches to Kevin automatically (no code change). MetaHumans work the same way
(MetaHuman Creator in 5.8 -> assemble -> it lands in /Game/MetaHumans).

## Controls in the creator
Boy / Girl, Character (body model), Skin tone, Face, Hair style + colour (hair colour tints the body's hair material;
hair *style* only changes the shape body), Height, Outfit pieces + colours (tint the body's Shirt / Pants / Shoes / Hat slots),
suit paint, helmet, voice. SAVE CHARACTER (or START on first launch) saves and immediately rebuilds the on-foot pilot
and the suit. In the suit, press **O** to step out and see the pilot.
