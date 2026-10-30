# Iron Man Arena: Unreal Engine 5 Setup Guide

> **Version note (added when the project was packaged): this project now targets Unreal Engine 5.8 (built with 5.8.4).**
> The `.uproject` uses `"EngineAssociation": "5.8"` and both `*.Target.cs` files use `BuildSettingsVersion.Latest` and `EngineIncludeOrderVersion.Latest` (not `Unreal5_4`).
> Where this document says 5.4/5.5, read 5.8. Toolchain versions (Visual Studio / MSVC, Xcode, Linux clang) must match what the UE 5.8 release notes list.
> On **Linux** (the 5.8.4 binary build): `Engine/Build/BatchFiles/Linux/Build.sh IronManArenaEditor Linux Development -Project="$PWD/IronManArena.uproject" -WaitMutex`, then `Engine/Binaries/Linux/UnrealEditor "$PWD/IronManArena.uproject"`. See the project README.
> Class names in the final code: GameMode `AArenaGameMode`, player `AIronManCharacter`, player controller `AArenaPlayerController` (not `AIronManPlayerController`), widgets `UArenaHUDWidget` / `UMainMenuWidget`. All the files marked *(referenced, file not yet present)* below now exist.


This guide takes you from a fresh Windows PC or Mac to pressing **Play** in the Unreal Editor with the Iron Man Arena C++ project. Do the steps in order. Where Windows and macOS differ, each one gets its own sub-section.

> **About the names in this guide**
> These names come from the project files in `IronManArena/` (the `.uproject`, `*.Target.cs`, `IronManArena.Build.cs`, and `Config/*.ini`). The project was still being written when I checked, so a couple of C++ files weren't on disk yet. Those are marked *(referenced, file not yet present)*.
>
> | Thing | Name | Source |
> |---|---|---|
> | Project file | `IronManArena.uproject` (display name "Iron Man Arena") | project root, `DefaultGame.ini` |
> | Engine version | **5.4** (`"EngineAssociation": "5.4"`, `IncludeOrderVersion = Unreal5_4`) | `.uproject`, `*.Target.cs` |
> | Game module | `IronManArena` (export macro `IRONMANARENA_API`) | `Source/IronManArena/IronManArena.Build.cs` |
> | Targets | `IronManArena` (Game), `IronManArenaEditor` (Editor) | `Source/*.Target.cs` |
> | GameMode class | `AArenaGameMode` → `/Script/IronManArena.ArenaGameMode` *(referenced, file not yet present)* | `DefaultEngine.ini` |
> | Player character | `AIronManCharacter` *(referenced, file not yet present)* | `#include`s in `Source/` |
> | Other classes | `AIronProjectile`, `AArenaBlastFX`, `AEnemyBase` (abstract `APawn`), `ADroneEnemy` (header only so far), `APickup`, helpers in `ArenaVisuals.h` | `Source/IronManArena/Public/` |
> | Maps | **No `.umap` ships.** Both default maps are set to the empty engine map `/Engine/Maps/Entry`, and `AArenaGameMode` builds the arena (floor, lights) at runtime when the level has none | `DefaultEngine.ini` |
> | Plugins | `EnhancedInput`, `PythonScriptPlugin`, `EditorScriptingUtilities` (all enabled) | `.uproject` |
> | Module deps | Core, CoreUObject, Engine, InputCore, EnhancedInput, UMG, Slate, SlateCore. **No Niagara or AIModule** | `Build.cs` |
>
> Unreal names a C++ class in menus and ini files by dropping its `A`/`U` prefix. So `AArenaGameMode` shows up in menus as **ArenaGameMode**, and in ini files as `/Script/IronManArena.ArenaGameMode`.

---

## Contents

1. [Hardware and disk requirements](#1-hardware-and-disk-requirements)
2. [Install the Epic Games Launcher and Unreal Engine](#2-install-the-epic-games-launcher-and-unreal-engine)
3. [Install a C++ toolchain: Windows](#3-install-a-c-toolchain-windows)
4. [Install a C++ toolchain: macOS (Xcode)](#4-install-a-c-toolchain-macos-xcode)
5. [Get the project onto your machine](#5-get-the-project-onto-your-machine)
6. [Associate the engine version](#6-associate-the-engine-version)
7. [Generate project files](#7-generate-project-files)
8. [Build (Development Editor)](#8-build-development-editor)
9. [First launch](#9-first-launch)
10. [Make a simple test map](#10-make-a-simple-test-map)
11. [Set the GameMode and default maps](#11-set-the-gamemode-and-default-maps)
12. [Press Play](#12-press-play)
13. [Live Coding vs. a full rebuild](#13-live-coding-vs-a-full-rebuild)
14. [Package a build (optional)](#14-package-a-build-optional)
15. [Quick checklist](#15-quick-checklist)

If something goes wrong at any step, see **TROUBLESHOOTING.md** in this folder.

---

## 1. Hardware and disk requirements

These are Epic's published figures for UE 5.4/5.5, rounded off. You *can* get by with less, but compiling and shader builds will be slow.

### Windows

| | Minimum (realistic) | Recommended |
|---|---|---|
| OS | Windows 10 64-bit (22H2) | Windows 11 64-bit |
| CPU | 4-core, 2.5 GHz+ | 8+ cores (C++ compile time scales with cores) |
| RAM | 16 GB | 32 GB or more |
| GPU | DirectX 12, Shader Model 6 | Recent NVIDIA RTX / AMD RX with up-to-date drivers |
| Disk | SSD | NVMe SSD |

### macOS

| | Minimum | Recommended |
|---|---|---|
| OS | macOS Ventura 13.5 (UE 5.5) | Latest macOS Sonoma 14 |
| CPU | Apple Silicon M1 (Intel Macs work but are slow) | Apple Silicon M2/M3 or newer |
| RAM | 16 GB | 32 GB or more |
| GPU | Metal-capable (all Apple Silicon is) | |

### Disk space (both platforms)

Plan for **at least 150 GB free on an SSD**:

- Engine install: roughly **40–60 GB**, depending on which components you pick (see Step 2).
- *Editor symbols for debugging* (optional): about **another 60–80 GB** on Windows.
- Visual Studio + Windows SDK: **~20–40 GB**. Xcode: **~15–40 GB**.
- The project's `Intermediate/`, `Binaries/`, and `DerivedDataCache/` folders can each grow to several GB after the first build and shader compile.

> 💡 Put the engine **and** the project on an SSD. On a spinning hard drive the first shader compile can take hours.

---

## 2. Install the Epic Games Launcher and Unreal Engine

### 2.1 Install the launcher

1. Go to <https://www.unrealengine.com/download> and download the **Epic Games Launcher**.
   - **Windows:** run the `.msi` installer.
   - **macOS:** open the `.dmg` and drag **Epic Games Launcher** into **Applications**.
2. Open the launcher and sign in with (or create) an Epic Games account.

### 2.2 Install the engine

1. In the launcher, click **Unreal Engine** in the left sidebar, then the **Library** tab at the top.
2. Next to **ENGINE VERSIONS**, click the **+** button.
3. Use the version dropdown on the new tile to pick **5.4.x**. This project's `.uproject` says `"EngineAssociation": "5.4"`, so the latest 5.4 hotfix is the smoothest choice. 5.5 should also work (the Target files use `BuildSettingsVersion.V5` / `EngineIncludeOrderVersion.Unreal5_4`, both still valid in 5.5), but opening the project in 5.5 upgrades it. See Step 6.
4. Click **Install**, then pick an install location if asked. Defaults:
   - Windows: `C:\Program Files\Epic Games\UE_5.4`
   - macOS: `/Users/Shared/Epic Games/UE_5.4`
5. Before or after it installs, open the dropdown arrow on the engine tile and choose **Options** to pick components:

| Component | Needed? | Notes |
|---|---|---|
| **Core Components** | ✅ Required | Always installed |
| **Starter Content** | 👍 Recommended | Free materials/meshes, handy for a test map |
| **Templates and Feature Packs** | 👍 Recommended | |
| **Engine Source** | Optional | Lets you step into engine code while debugging; also gives better code navigation in VS/Rider |
| **Editor symbols for debugging** | Optional (large) | Only if you want readable call stacks in the debugger or crash reports. Windows only. |
| **Target Platforms** | Optional | Only for building to Android, iOS, Linux, etc. Your own desktop platform is always included. |

6. Wait for the download and install to finish. This can take a while.

---

## 3. Install a C++ toolchain: Windows

This project is **C++**, so the editor needs a compiler. You have two options:

- **Option A:** Visual Studio 2022 (free Community edition). This is the most common setup and the one shown below.
- **Option B:** JetBrains Rider as the editor/IDE. Rider **still needs** Microsoft's MSVC compiler and Windows SDK installed (through Visual Studio or the *Build Tools for Visual Studio 2022*), so do the component part of Option A either way.

### 3.1 Which versions?

From Epic's Visual Studio compatibility table:

| Engine | Visual Studio 2022 | MSVC toolset | Windows SDK |
|---|---|---|---|
| UE 5.4 | 17.4 or later (17.8 recommended) | **14.38** is the preferred version | 10.0.18362 or newer (Epic's table lists 10.0.19041 min / 10.0.22621 recommended) |
| UE 5.5 | 17.8 or later (17.10 recommended) | **14.38** min; newer works | 10.0.22621 recommended |

Visual Studio 2019 is **not supported** for UE 5.4+.

> ⚠️ If the build log says *"Visual Studio 2022 compiler version 14.xx is not a preferred version. Please use the latest preferred version 14.38..."*, install the **MSVC v143 – VS 2022 C++ x64/x86 build tools (v14.38-17.8)** individual component (Step 3.3). It can sit alongside newer toolsets.

### 3.2 Install Visual Studio 2022 with the right workloads

1. Download **Visual Studio 2022 Community** from <https://visualstudio.microsoft.com/downloads/>.
2. Run the installer. On the **Workloads** tab, tick:
   - ✅ **Game development with C++**
   - ✅ **Desktop development with C++**
   - ✅ **.NET desktop development** (Unreal's build tools are .NET programs. UE 5.4/5.5 ship their own .NET 8 runtime, but Epic still lists this workload.)
3. With **Game development with C++** selected, look at the **Installation details** panel on the right and make sure these are ticked:
   - ✅ **C++ profiling tools**
   - ✅ **Windows 10 SDK** or **Windows 11 SDK** (10.0.18362 or newer; 10.0.22621 is a safe pick)
   - ✅ **Unreal Engine installer** (adds VS's Unreal integration)
   - ☐ C++ AddressSanitizer (optional)
   - ✅ **IDE support for Unreal Engine** / *Unreal Engine Test Adapter* if listed. These are optional, but they give you Blueprint references and better UE IntelliSense.

### 3.3 Add the pinned MSVC toolset (recommended for UE 5.4)

1. In the installer, go to the **Individual components** tab.
2. Search for `MSVC v143`.
3. Tick **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.38-17.8)**. Leave the "(Latest)" one ticked too.
4. Search for `Windows 11 SDK` (or `Windows 10 SDK`) and confirm one version is ticked.
5. Click **Install** (or **Modify**) and wait for it to finish. **Restart Windows** afterwards.

### 3.4 Recommended Visual Studio tweaks (optional)

- **Tools > Options > Text Editor > C/C++ > Advanced:** set *Disable Error Reporting* to **True** if the IntelliSense red squiggles bother you. Real errors still appear in the **Output** window. See TROUBLESHOOTING.md.
- Add the **Solution Configurations** dropdown to the toolbar and widen it so you can read the full name (e.g. `Development Editor`).

### 3.5 Option B: JetBrains Rider

1. Install the Visual Studio components from 3.2/3.3 first. **Build Tools for Visual Studio 2022** with the same C++ workload also works if you don't want the full VS IDE.
2. Install **Rider** from <https://www.jetbrains.com/rider/>. Rider has been free for non-commercial use since late 2024.
3. On first run, let Rider install the **RiderLink** plugin when it offers, into the engine or the project.
4. In Rider you can **open the `.uproject` directly** (File > Open). You don't need to generate a `.sln` first.

---

## 4. Install a C++ toolchain: macOS (Xcode)

### 4.1 Which Xcode?

From Epic's macOS requirements table:

| Engine | Minimum macOS | Minimum Xcode | Recommended Xcode |
|---|---|---|---|
| UE 5.4 | macOS 13 Ventura | Xcode 14.1 | Xcode 15.x works well in practice (15.2–15.4) |
| UE 5.5 | macOS 13.5 Ventura | Xcode 15.2 | **Xcode 15.4** |

> ⚠️ **Xcode 16 and the Launcher build of UE 5.5:** lots of forum reports describe *"Platform Mac is not a valid platform to build"* or SDK errors with Xcode 16.x on macOS Sequoia. If you get that, install **Xcode 15.4** next to it (see 4.3) and select it with `xcode-select`.

### 4.2 Install Xcode

1. **Simple route:** install **Xcode** from the Mac App Store. This gives you the latest version, which may be newer than you want.
2. **Exact-version route (recommended):** sign in at <https://developer.apple.com/download/all/> with a free Apple ID, search for `Xcode 15.4`, download the `.xip`, double-click to expand it, and drag `Xcode.app` into `/Applications`. Rename it (e.g. `Xcode_15.4.app`) if you keep more than one version.

### 4.3 Command line tools, first launch, and license

Open **Terminal** and run the following. Change the path if you renamed Xcode:

```bash
# Point the command-line tools at the Xcode you want UE to use
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer

# Accept the Xcode license (UE builds fail with "xcrun: error" until you do)
sudo xcodebuild -license accept

# Install the extra components Xcode wants on first launch
sudo xcodebuild -runFirstLaunch

# Check what's active
xcode-select -p
xcodebuild -version
xcrun --sdk macosx --show-sdk-path
```

If `xcode-select --install` offers to install the standalone **Command Line Tools**, that's fine, but **UE needs the full Xcode.app**, not just the Command Line Tools.

Finally, **open Xcode once** from Applications and let it finish installing components. If it asks which platforms to download, **macOS** is all you need (it's built in). iOS is only needed if you build for iPhone.

---

## 5. Get the project onto your machine

Pick whichever matches how the project is shared with you:

- **Git:**
  ```bash
  git clone <repo-url> ironman-unreal
  cd ironman-unreal/IronManArena
  ```
  If the repo uses **Git LFS** for `.uasset`/`.umap` files, run `git lfs install` once beforehand and `git lfs pull` after cloning.
- **Zip:** unzip it somewhere with a **short path and no special characters**, e.g. `C:\Dev\IronManArena` or `~/Dev/IronManArena`. Windows has path-length limits, and deeply nested folders cause odd build errors.

The project folder should look roughly like this:

```
IronManArena/
├── IronManArena.uproject
├── Config/
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   └── DefaultInput.ini
├── Content/                      (empty except .gitkeep; no maps or assets ship)
└── Source/
    ├── IronManArena.Target.cs
    ├── IronManArenaEditor.Target.cs
    └── IronManArena/
        ├── IronManArena.Build.cs
        ├── Public/   (IronManArena.h, IronProjectile.h, ArenaBlastFX.h, EnemyBase.h, Pickup.h, ArenaVisuals.h, ...)
        └── Private/  (matching .cpp files)
```

You **won't** have `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, or `*.sln`/`*.xcworkspace` yet. They're generated on your machine and normally kept out of source control.

> ⚠️ Avoid putting the project inside OneDrive, iCloud Drive, or Dropbox-synced folders. Syncing locks files mid-build.

---

## 6. Associate the engine version

Open `IronManArena.uproject` in a text editor. It's plain JSON. Look for:

```json
"EngineAssociation": "5.4",
```

- If it matches an installed version (e.g. `5.4`), you're done.
- If you have a different version installed, or the field is blank or a long `{GUID}` (meaning it came from a source build), switch it as follows.

### Windows

1. In File Explorer, right-click `IronManArena.uproject`.
   - On **Windows 11** you'll first need **Show more options** (or Shift+right-click) to see Unreal's entries.
2. Choose **Switch Unreal Engine version...**
3. Pick your installed version from the dropdown and click **OK**. This rewrites `EngineAssociation` and also generates the Visual Studio project files.

> If those right-click entries are missing, launch the engine once from the Epic Games Launcher (this registers the file association), or double-click `Engine\Binaries\Win64\UnrealVersionSelector.exe` inside the engine folder.

### macOS

Finder's right-click menu doesn't reliably include *Switch Unreal Engine version* with Launcher installs. Use one of these instead:

- **Edit the file:** set `"EngineAssociation": "5.5"` (or your version) by hand and save.
- **Or let the editor do it:** double-click the `.uproject` (or open it from the Launcher's **Library > My Projects**). If the version doesn't match, the editor offers to open it with the installed version.

> ⚠️ Opening a project in a **newer** engine version upgrades its assets, and you can't easily go back. Make a copy (or commit in Git) first.

---

## 7. Generate project files

This creates the Visual Studio solution (`IronManArena.sln`) or the Xcode workspace from the `*.Build.cs`/`*.Target.cs` files. Re-run it any time you **add, remove, or rename** a `.h`/`.cpp` file or change a `Build.cs`.

### Windows

**Option 1: right-click (easiest)**
1. Right-click `IronManArena.uproject` (Windows 11: **Show more options**).
2. Choose **Generate Visual Studio project files**.
3. After a few seconds, `IronManArena.sln` appears next to the `.uproject`.

**Option 2: command line** (Command Prompt; adjust the paths to yours)

```bat
"C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="C:\Dev\IronManArena\IronManArena.uproject" -game -rocket -progress
```

**Option 3: from inside the editor:** **Tools > Refresh Visual Studio Project** (only once the editor opens).

### macOS

**Option 1: from inside the editor (most reliable on Launcher installs)**
- Once the editor opens, use **Tools > Refresh Xcode Project**. If it isn't there yet, **Tools > Generate Xcode Project**. The wording varies slightly between versions.

**Option 2: command line** (Terminal; adjust the paths to yours)

```bash
UE="/Users/Shared/Epic Games/UE_5.4"
PROJ="$HOME/Dev/IronManArena/IronManArena.uproject"

# Generate the Xcode workspace
"$UE/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" -project="$PROJ" -game
```

If that script complains on a Launcher (installed) build, call UnrealBuildTool's project-file mode through the Mac `Build.sh` wrapper instead:

```bash
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" -projectfiles -project="$PROJ" -game -rocket -progress
```

You should get a workspace named something like `IronManArena (Mac).xcworkspace` next to the `.uproject`. UE 5.4+ uses Xcode's "modern" workspace layout by default, and the exact name can differ slightly.

**Option 3: Finder:** on some setups, right-clicking the `.uproject` shows **Services > Generate Xcode Project**. If you don't see it, use Option 1 or 2.

---

## 8. Build (Development Editor)

You're building the project's **editor module** (the DLL/dylib the editor loads). The configuration you want is **Development Editor**.

### Windows: Visual Studio

1. Double-click `IronManArena.sln`.
2. In the toolbar, set:
   - **Solution Configuration:** `Development Editor`
   - **Solution Platform:** `Win64`
3. In **Solution Explorer**, under **Games**, right-click **IronManArena** and choose **Set as Startup Project**.
4. **Build > Build Solution** (Ctrl+Shift+B), or right-click **IronManArena > Build** to build only the game project. That's faster and doesn't try to rebuild the engine.
5. Watch the **Output** window (View > Output, "Show output from: Build"). The last lines should read something like:
   ```
   ========== Build: 1 succeeded, 0 failed, ... ==========
   ```
6. Press **F5** (Debug > Start Debugging) to launch the editor with the debugger attached, or Ctrl+F5 to launch it without.

### Windows: Rider

1. **File > Open** and select `IronManArena.uproject` (or the `.sln`).
2. Set the run configuration to **IronManArena** with **Development Editor | Win64**.
3. **Build > Build Solution**, then **Run** (or **Debug**).

### macOS: Xcode

1. Double-click the generated `.xcworkspace`.
2. In the scheme selector at the top (next to the Play/Stop buttons), choose the **IronManArenaEditor** scheme with destination **My Mac**.
3. Check the configuration: **Product > Scheme > Edit Scheme... > Run > Info > Build Configuration**, set to **Development Editor** if listed (in the modern workspace the editor scheme normally defaults to a Development configuration).
4. **Product > Build** (⌘B). The first build takes a while.
5. **Product > Run** (⌘R) to launch the editor.

### macOS (or Windows): command line build

```bash
# macOS
"/Users/Shared/Epic Games/UE_5.4/Engine/Build/BatchFiles/Mac/Build.sh" \
  IronManArenaEditor Mac Development -project="$HOME/Dev/IronManArena/IronManArena.uproject" -waitmutex
```

```bat
:: Windows
"C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" IronManArenaEditor Win64 Development -project="C:\Dev\IronManArena\IronManArena.uproject" -waitmutex
```

(`IronManArenaEditor` is the editor target name, i.e. the class in `IronManArenaEditor.Target.cs`. Development + an Editor target = "Development Editor".)

### Shortcut: let the editor build for you

Double-click `IronManArena.uproject`. If the C++ modules haven't been built, you'll see:

> *The following modules are missing or built with a different engine version: IronManArena. Would you like to rebuild them now?*

Click **Yes**. The editor builds the modules in the background (this needs Visual Studio/Xcode installed) and opens when it's done. If it fails with *"could not be compiled. Try rebuilding from source manually"*, build from the IDE instead so you can see the real error. See TROUBLESHOOTING.md.

> ⏱ **First compile:** expect anywhere from **2–20+ minutes** for the project's C++, depending on your CPU. Later builds only recompile what changed.

---

## 9. First launch

1. The splash screen shows progress. On the first open the editor **compiles shaders** (the bottom-right corner says *"Compiling Shaders (N)"*). This can take **10–60 minutes** the first time. Later opens are much faster because results are cached in `DerivedDataCache/`.
2. If prompted to **enable plugins** or **restart**, accept.
3. If prompted about **missing or new engine version assets**, read carefully. Converting a project to a newer engine version is a one-way trip (see Step 6).
4. Open **Window > Output Log**. Warnings in yellow are usually fine. Red **errors** are worth reading.
5. In the **Content Browser** (Ctrl+Space, or **Window > Content Browser**), click **Settings** (gear) and enable **Show C++ Classes**. You should see a **C++ Classes > IronManArena** folder containing the GameMode, character/pawn classes, etc. This confirms the module loaded.

---

## 10. Make a simple test map

**For this project, this step is optional.** No map ships with it. `DefaultEngine.ini` points both the editor startup map and the game default map at the empty engine map `/Engine/Maps/Entry`, and `AArenaGameMode` builds the arena (floor, lights) at runtime when the level has none. So you can go straight to Step 12 and press Play.

Make your own map when you want to place things by hand or keep a saved layout. It takes about 5 minutes. Heads-up: if your map already has a floor and lights, the GameMode will probably skip its runtime arena-building, which is the point of the "detects that there is no floor/light" logic described in the ini comment.

1. **File > New Level...**
2. Choose **Basic**, which already has a floor, a directional light, a sky atmosphere, a sky light, exponential height fog, and volumetric clouds. (Choose **Empty Level** to build it all by hand.)
3. If you started from **Empty Level**, add these through the **Quick Add** (the "+ cube" icon) toolbar menu or **Window > Place Actors**:
   - **Shapes > Plane** or **Cube**, scaled up (e.g. X/Y scale 50, Z scale 1) as the arena floor
   - **Lights > Directional Light** (the sun)
   - **Visual Effects > Sky Atmosphere**
   - **Lights > Sky Light**, with **Real Time Capture** ticked in Details
   - **Visual Effects > Exponential Height Fog** (optional, looks nicer)
   - **Visual Effects > Volumetric Cloud** (optional)
4. **PlayerStart:** in **Place Actors**, search for **Player Start** and drag it onto the floor, slightly above it. The game spawns the player pawn here. A blue arrow shows which way the player will face. If you see a "Bad size" warning, lift it a bit above the floor.
5. **NavMesh (only if AI needs it):** the current code doesn't use `AIModule` or `NavigationSystem` (enemies derive from `AEnemyBase : APawn` and appear to move themselves), so you can **skip this** for now. If you later add `AAIController::MoveTo`-style ground AI:
   - Place Actors > search **Nav Mesh Bounds Volume** and drag it in.
   - Scale it to cover the whole floor, with some height above and below.
   - Press **P** in the viewport to show the NavMesh. Walkable areas turn green.
   - Flying-only AI may not need it, but most `AAIController::MoveTo` calls do.
6. Add a few cubes as cover/obstacles so you can see movement and flight.
7. **Save:** **File > Save Current Level As...** Create a folder **Content/Maps** and name the level **Arena** (so its path is `/Game/Maps/Arena`). Use the same name in Step 11.

---

## 11. Set the GameMode and default maps

The **GameMode** decides which pawn spawns, which HUD is used, the player controller, and the game rules. If it isn't set, you'll spawn as the default flying-camera "spectator" pawn instead of Iron Man.

You can set it in three places. **World Settings beats Project Settings**, and Project Settings writes to the ini.

### 11.1 Per map: World Settings > GameMode Override

1. With your Arena map open: **Window > World Settings** (it opens as a tab next to Details).
2. Find the **Game Mode** section.
3. Set **GameMode Override** to **ArenaGameMode** *(`AArenaGameMode`)* or a Blueprint subclass of it, e.g. `BP_ArenaGameMode`, if the project has one.
4. Expand the **Selected GameMode** section underneath to confirm the **Default Pawn Class** is the Iron Man pawn/character, not `DefaultPawn` or `SpectatorPawn`.
5. **Save** the map (Ctrl+S).

### 11.2 Project-wide: Project Settings > Maps & Modes

1. **Edit > Project Settings...**
2. In the left column, under **Project**, click **Maps & Modes**.
3. **Default Modes > Default GameMode:** choose **ArenaGameMode** (or its Blueprint).
4. **Default Maps:**
   - **Editor Startup Map:** `Arena` (the map that opens with the editor)
   - **Game Default Map:** `Arena` (the map a packaged game starts on)
5. Settings save automatically into `Config/DefaultEngine.ini`.

### 11.3 Or directly in `Config/DefaultEngine.ini`

The project **already ships** with this, so the GameMode is set project-wide out of the box:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Engine/Maps/Entry
EditorStartupMap=/Engine/Maps/Entry
GlobalDefaultGameMode=/Script/IronManArena.ArenaGameMode
```

Once you've saved your own map (Step 10), close the editor and point the two map lines at it:

```ini
EditorStartupMap=/Game/Maps/Arena.Arena
GameDefaultMap=/Game/Maps/Arena.Arena
```

- `/Script/<ModuleName>.<ClassNameWithoutPrefix>` is the C++ class path: module `IronManArena`, class `AArenaGameMode`.
- For a **Blueprint** GameMode, the path ends in `_C`, e.g. `/Game/Blueprints/BP_ArenaGameMode.BP_ArenaGameMode_C`.
- Map paths are `/Game/<folder>/<Map>.<Map>` (the name appears twice).

---

## 12. Press Play

1. Click the green **Play** ▶ button in the toolbar (or press **Alt+P**). Use the **⋮** menu next to it to choose **Selected Viewport**, **New Editor Window (PIE)**, or **Standalone Game**.
2. Click inside the viewport to give it mouse/keyboard focus.
3. **Shift+F1** frees the mouse cursor while playing. **Esc** stops Play.
4. If you spawn as a floating camera rather than Iron Man, or input doesn't work, see *"Unable to find a game mode / wrong pawn spawns"* and *"Enhanced Input not working"* in TROUBLESHOOTING.md.

---

## 13. Live Coding vs. a full rebuild

**Live Coding** patches your C++ changes into the running editor without restarting it. It's **Windows only**. On macOS you close the editor and rebuild.

### Using Live Coding (Windows)

1. Edit a `.cpp` in Visual Studio/Rider and save it.
2. In the editor press **Ctrl+Alt+F11**, or click the **Compile** button (the stacked-blocks icon) at the bottom-right of the editor window.
3. A Live Coding console window shows progress, ending with *"Live coding succeeded"*.

Live Coding is **on by default** in UE5. To check or change it, use the options menu next to the Compile button, or **Edit > Editor Preferences > General > Live Coding**.

> ⚠️ **While Live Coding is enabled, you can't build from Visual Studio with the editor open.** UBT refuses with *"Unable to build while Live Coding is active. Exit the editor and game, or press Ctrl+Alt+F11..."*. Either use Ctrl+Alt+F11, or close the editor and build in VS.

### When Live Coding is fine

- Changing code **inside function bodies** in `.cpp` files.

### When you should close the editor and do a full rebuild

- Adding or removing **`UPROPERTY`**, **`UFUNCTION`**, or **`UCLASS`/`USTRUCT`/`UENUM`** declarations, or changing anything in a header that UHT reads. (UE 5.x has experimental reinstancing for some of this, but it's flaky, so don't count on it.)
- Changing **constructors** or default values (they affect the Class Default Object).
- Adding **new files** (re-run *Generate project files* first, see Step 7).
- Changing a **`Build.cs`** or **`Target.cs`**.
- Something "isn't taking effect" or Blueprints referencing your class look broken.

**Full rebuild procedure:** close the editor → (re-generate project files if you added files) → build **Development Editor** in VS/Xcode → reopen the editor.

---

## 14. Package a build (optional)

Packaging cooks the content and makes a standalone game you can run without the editor.

### From the editor

1. Make sure **Project Settings > Maps & Modes > Game Default Map** is set (Step 11.2). It defaults to `/Engine/Maps/Entry` here, which is fine because the GameMode builds the arena at runtime. Optionally add your maps under **Project Settings > Project > Packaging > Advanced > List of maps to include in a packaged build**.
2. In the main toolbar, open the **Platforms** dropdown.
3. Pick **Windows** (or **Mac**) and set the **Binary Configuration** to **Development** for testing or **Shipping** for release.
4. **Platforms > Windows > Package Project** (or **Mac > Package Project**) and choose an output folder, e.g. `C:\Builds\IronManArena`.
5. Watch the progress toast and the **Output Log**. When it finishes:
   - Windows: run `Windows\IronManArena.exe`
   - macOS: run `Mac/IronManArena.app` (named after the game target)

### From the command line (RunUAT)

```bat
:: Windows
"C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="C:\Dev\IronManArena\IronManArena.uproject" ^
  -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -archive -archivedirectory="C:\Builds\IronManArena"
```

```bash
# macOS
"/Users/Shared/Epic Games/UE_5.4/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$HOME/Dev/IronManArena/IronManArena.uproject" \
  -platform=Mac -clientconfig=Development \
  -build -cook -stage -pak -archive -archivedirectory="$HOME/Builds/IronManArena"
```

> 💡 Mac builds you hand to *other* people need code signing/notarization. For running on your own Mac, an unsigned Development build is fine.

---

## 15. Quick checklist

**Install**
- [ ] 150 GB+ free on an SSD
- [ ] Epic Games Launcher installed and signed in
- [ ] Unreal Engine **5.4+** installed (Core Components; Starter Content recommended)
- [ ] **Windows:** VS 2022 with *Game development with C++*, *Desktop development with C++*, *.NET desktop development*, a Windows 10/11 SDK, and **MSVC v14.38** component (UE 5.4) → rebooted
- [ ] **Windows (Rider):** Rider installed *and* the MSVC/Windows SDK components above
- [ ] **macOS:** Xcode **15.4** (or 15.2+) installed, `xcode-select` pointing at it, license accepted, `-runFirstLaunch` done, Xcode opened once

**Project**
- [ ] Project in a short, non-synced path
- [ ] `EngineAssociation` in the `.uproject` matches your installed engine
- [ ] Project files generated (`.sln` / `.xcworkspace` exists)
- [ ] Built **Development Editor** successfully (or accepted the "rebuild modules?" prompt)
- [ ] Editor opens; **C++ Classes > IronManArena** visible in the Content Browser

**Play**
- [ ] (Optional for this project) Test map with floor, lights, Sky Atmosphere, **PlayerStart**, saved as `/Game/Maps/Arena`. Otherwise just Play on the default `/Engine/Maps/Entry`
- [ ] GameMode = **ArenaGameMode** (already set in `DefaultEngine.ini`; set **World Settings > GameMode Override** too on any custom map)
- [ ] **Project Settings > Maps & Modes**: Default GameMode + Editor Startup Map + Game Default Map set
- [ ] **Play** spawns the Iron Man pawn and input works
- [ ] Know the difference: **Ctrl+Alt+F11** (Live Coding, body-only changes, Windows) vs. **close editor + full rebuild** (headers/UPROPERTY/new files)

Stuck? → **TROUBLESHOOTING.md** 🛠️
