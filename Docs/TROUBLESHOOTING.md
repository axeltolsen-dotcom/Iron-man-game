# Iron Man Arena: Troubleshooting (UE 5.4 / 5.5)

> **Version note (added when the project was packaged): this project now targets Unreal Engine 5.8 (built with 5.8.4).**
> The `.uproject` uses `"EngineAssociation": "5.8"` and both `*.Target.cs` files use `BuildSettingsVersion.Latest` and `EngineIncludeOrderVersion.Latest` (not `Unreal5_4`).
> Where this document says 5.4/5.5, read 5.8. Toolchain versions (Visual Studio / MSVC, Xcode, Linux clang) must match what the UE 5.8 release notes list.
> On **Linux** (the 5.8.4 binary build): `Engine/Build/BatchFiles/Linux/Build.sh IronManArenaEditor Linux Development -Project="$PWD/IronManArena.uproject" -WaitMutex`, then `Engine/Binaries/Linux/UnrealEditor "$PWD/IronManArena.uproject"`. See the project README.
> Class names in the final code: GameMode `AArenaGameMode`, player `AIronManCharacter`, player controller `AArenaPlayerController` (not `AIronManPlayerController`), widgets `UArenaHUDWidget` / `UMainMenuWidget`. All the files marked *(referenced, file not yet present)* below now exist.


When something breaks, don't panic. Almost every Unreal C++ problem falls into one of the buckets below. Each entry lists the **symptom** (what you'll see), the **cause**, and the **fix**.

> **About the names in this doc**
> These names come from the project files: project `IronManArena.uproject` (engine **5.4**), module `IronManArena` (`IRONMANARENA_API`), targets `IronManArena` / `IronManArenaEditor`, GameMode `AArenaGameMode` (`/Script/IronManArena.ArenaGameMode` in `DefaultEngine.ini`), and player character `AIronManCharacter`. The project was still being written when I checked, and the `ArenaGameMode.h` and `IronManCharacter.h` files weren't on disk yet even though other files `#include` them. Class names used in code samples (e.g. `FireRepulsor`) are illustrative.

**Paths used below**

| | Windows | macOS |
|---|---|---|
| Engine (Launcher) | `C:\Program Files\Epic Games\UE_5.x` | `/Users/Shared/Epic Games/UE_5.x` |
| Project | `C:\Dev\IronManArena` (example) | `~/Dev/IronManArena` (example) |
| UBT log | `%LOCALAPPDATA%\UnrealBuildTool\Log.txt` | `~/Library/Logs/Unreal Engine/UnrealBuildTool/Log.txt` |
| Editor log | `<Project>\Saved\Logs\IronManArena.log` | `<Project>/Saved/Logs/IronManArena.log` |
| Crash reports | `<Project>\Saved\Crashes\` | `<Project>/Saved/Crashes/` (plus macOS's own reports in `~/Library/Logs/DiagnosticReports/`) |

---

## Contents

0. [First: how to read the build log and find the first real error](#0-first-how-to-read-the-build-log-and-find-the-first-real-error)
1. ["The following modules are missing or built with a different engine version"](#1-the-following-modules-are-missing-or-built-with-a-different-engine-version)
2. ["Could not be compiled. Try rebuilding from source manually"](#2-could-not-be-compiled-try-rebuilding-from-source-manually)
3. [Missing or wrong MSVC toolset / Windows SDK](#3-missing-or-wrong-msvc-toolset--windows-sdk)
4. [LNK2019 / LNK2001 unresolved external symbol](#4-lnk2019--lnk2001-unresolved-external-symbol)
5. [C1083 cannot open include file / .generated.h must be the last include](#5-c1083-cannot-open-include-file--generatedh-must-be-the-last-include)
6. [UHT (Unreal Header Tool) errors](#6-uht-unreal-header-tool-errors)
7. ["Running .../UnrealBuildTool ... failed"](#7-running-unrealbuildtool--failed)
8. [Live Coding errors and stale DLLs](#8-live-coding-errors-and-stale-dlls)
9. [Safely deleting Binaries / Intermediate / DerivedDataCache / .vs / Saved](#9-safely-deleting-binaries--intermediate--deriveddatacache--vs--saved)
10. [macOS: Xcode signing, "xcrun: error", wrong Xcode version](#10-macos-xcode-signing-xcrun-error-wrong-xcode-version)
11. ["Unable to find a game mode" / wrong pawn spawns](#11-unable-to-find-a-game-mode--wrong-pawn-spawns)
12. [Enhanced Input not working](#12-enhanced-input-not-working)
13. [Editor crashes on open](#13-editor-crashes-on-open)
14. [IntelliSense red squiggles that aren't real errors](#14-intellisense-red-squiggles-that-arent-real-errors)
15. [Long first compile and shader compilation](#15-long-first-compile-and-shader-compilation)
16. [Still stuck?](#16-still-stuck)

---

## 0. First: how to read the build log and find the first real error

**The golden rule: fix the *first* error, not the last one.** A single missing include can cause 200 follow-on errors, and the last line ("Build failed", "ExitCode=6", "MSB3073") only tells you that *something* failed.

### In Visual Studio (Windows)

1. Open **View > Output** (Ctrl+Alt+O) and set **Show output from:** to **Build**.
2. **Don't trust the Error List as-is.** It mixes in IntelliSense guesses. If you use it at all, set its dropdown to **Build Only**.
3. In the Output window, press **Ctrl+F** and search for `error` (try ` error ` with spaces, or `error C`, `error LNK`, `: Error:`).
4. Jump to the **first hit** after `Building IronManArenaEditor...` / `Parsing headers for IronManArenaEditor`. That's the real error.
5. A typical real error looks like this:
   ```
   C:\Dev\IronManArena\Source\IronManArena\IronManCharacter.cpp(42): error C2065: 'InputComponent': undeclared identifier
   ```
   This is **file(line): error CODE: message**. Double-click it in the Output window to jump to the line.
6. Lines to **ignore**, which only summarize the real failure:
   ```
   error MSB3073: The command "...\Build.bat IronManArenaEditor Win64 Development ..." exited with code 6.
   ========== Build: 0 succeeded, 1 failed ==========
   ```

### In Rider

Open the **Build** tool window and turn off "show warnings" to filter. Click the first red entry.

### In Xcode (macOS)

1. Open the **Report navigator** (⌘9) and select the latest **Build**.
2. Click **All** and **Errors** at the top to filter.
3. For the full text, right-click a log entry and choose **Expand All Transcripts**, or use the "hamburger" icon on a step. Search for `error:`.

### In the Unreal Editor

- **Window > Output Log**, then use the filter/search box (type `Error`).
- **Window > Developer Tools > Message Log** groups compile, load, and map-check errors.

### The raw UnrealBuildTool log

If the IDE output is cut off, open the UBT log directly (paths in the table at the top). Search for `error`. It also shows *which* MSVC and Windows SDK versions UBT picked, which is very useful for section 3.

### Command-line build (gives the cleanest output)

```bat
"C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" IronManArenaEditor Win64 Development -project="C:\Dev\IronManArena\IronManArena.uproject" -waitmutex > build.txt 2>&1
findstr /n /i /c:"error" build.txt
```

```bash
"/Users/Shared/Epic Games/UE_5.4/Engine/Build/BatchFiles/Mac/Build.sh" IronManArenaEditor Mac Development \
  -project="$HOME/Dev/IronManArena/IronManArena.uproject" -waitmutex 2>&1 | tee build.txt
grep -n -i "error" build.txt | head -20
```

---

## 1. "The following modules are missing or built with a different engine version"

**Symptom**
A dialog when you double-click the `.uproject`:
```
The following modules are missing or built with a different engine version:

    IronManArena

Would you like to rebuild them now?
```

**Cause**
The editor can't find a compiled `Binaries/Win64/UnrealEditor-IronManArena.dll` (Mac: `Binaries/Mac/UnrealEditor-IronManArena.dylib`), or the one it found was built with a different engine version/hotfix. This is **normal** for a freshly cloned C++ project, since binaries usually aren't committed. It also happens right after you switch engine versions.

**Fix**
1. Click **Yes**. The editor will compile the module (this needs VS 2022 or Xcode installed, see the setup guide).
2. If that fails (see section 2), build from the IDE instead: open the `.sln`/`.xcworkspace`, choose **Development Editor**, build, then open the editor from the IDE.
3. If you never installed a C++ toolchain, the rebuild can't work at all. Install one first.
4. If it keeps coming back after a successful build, make sure you're opening the project with **the same engine version** you built against (check `EngineAssociation` in the `.uproject`).

---

## 2. "Could not be compiled. Try rebuilding from source manually"

**Symptom**
After clicking **Yes** in section 1:
```
IronManArena could not be compiled. Try rebuilding from source manually.
```

**Cause**
The background compile failed. The dialog doesn't tell you *why*. The reason is always one of the other errors in this document: a C++ error, a missing toolset, a missing module, and so on.

**Fix**
1. Find the real error with one of these:
   - Open the project in VS/Rider/Xcode and **build Development Editor**, then read the Output (section 0).
   - Read `<Project>/Saved/Logs/IronManArena.log`, or the UBT log (paths at the top). Search for `error`.
2. Fix that error, then re-open the `.uproject`.
3. If the log says something like *"Unable to build while Live Coding is active"* or a file is locked, close **all** editor instances (check Task Manager / Activity Monitor for `UnrealEditor`) and retry.

---

## 3. Missing or wrong MSVC toolset / Windows SDK

**Symptoms** (any of these)
```
Visual Studio 2022 compiler version 14.39.33523 is not a preferred version. Please use the latest preferred version 14.38.33130
```
```
Unable to find valid C++ toolchain for VisualStudio2022 x64
```
```
Windows SDK must be installed in order to build this target.
```
```
Could not find NetFxSDK install dir; this will prevent SwarmInterface from installing.
```
```
Platform Win64 is not a valid platform to build. Check that the SDK is installed properly.
```
…or lots of strange errors inside **engine** headers (not your code), often after a VS update.

**Cause**
- The **Game development with C++** / **Desktop development with C++** workloads aren't installed, or no Windows SDK is selected.
- VS auto-updated to a newer MSVC that this engine version wasn't tested with. The "not a preferred version" message on its own is only a **warning**, but newer toolsets sometimes do break engine headers.
- You installed only VS 2019, which isn't supported in UE 5.4+.

**Fix**
1. Open **Visual Studio Installer > Modify** (on VS 2022).
2. **Workloads:** tick *Game development with C++*, *Desktop development with C++*, *.NET desktop development*.
3. **Individual components:** search `MSVC v143` and tick **MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.38-17.8)**. Search `Windows 11 SDK` / `Windows 10 SDK` and tick one (10.0.22621 is a good choice). For the *NetFxSDK* warning, also tick **.NET Framework 4.8 SDK** / **4.6.2 targeting pack**. That warning is harmless for most game projects.
4. **Restart Windows.** Then **regenerate project files** (right-click `.uproject` > *Generate Visual Studio project files*) and rebuild.
5. **Optional: pin the versions.** Create `<Project>/Saved/UnrealBuildTool/BuildConfiguration.xml` (or `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` for all projects):
   ```xml
   <?xml version="1.0" encoding="utf-8" ?>
   <Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
     <WindowsPlatform>
       <CompilerVersion>14.38.33130</CompilerVersion>
       <!-- Optional: <WindowsSdkVersion>10.0.22621.0</WindowsSdkVersion> -->
     </WindowsPlatform>
   </Configuration>
   ```
   Use the exact version folder you have under `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\`.
6. Check what UBT actually picked by searching the UBT log for `Available x64 toolchains` and `Using Visual Studio`.

---

## 4. LNK2019 / LNK2001 unresolved external symbol

**Symptom**
```
error LNK2019: unresolved external symbol "__declspec(dllimport) public: void __cdecl UEnhancedInputLocalPlayerSubsystem::AddMappingContext(...)" referenced in function "protected: virtual void __cdecl AIronManCharacter::BeginPlay(void)"
error LNK2001: unresolved external symbol "public: static class UClass * __cdecl UUserWidget::StaticClass(void)"
fatal error LNK1120: 3 unresolved externals
```
On Mac (clang), the same problem reads:
```
Undefined symbols for architecture arm64:
  "UUserWidget::StaticClass()", referenced from: ...
ld: symbol(s) not found for architecture arm64
```

**What it means**
The code **compiled**, since the header was found, but the **linker** couldn't find where the function actually lives. There are three common causes.

### 4a. Missing module in `Build.cs` (by far the most common)

You `#include`d something from an engine module but didn't tell UBT to link against that module.

**Fix:** open `Source/IronManArena/IronManArena.Build.cs` and add the module to `PublicDependencyModuleNames` (or `PrivateDependencyModuleNames`).

The project currently lists **Core, CoreUObject, Engine, InputCore, EnhancedInput, UMG, Slate, SlateCore**. It deliberately **doesn't** list `Niagara` (the effects are built from basic shapes and point lights), `AIModule`, or `NavigationSystem`. So the moment someone adds a `UNiagaraComponent` or an `AAIController`, expect exactly this linker error. Here's an expanded version with the extra modules marked:

```csharp
using UnrealBuildTool;

public class IronManArena : ModuleRules
{
    public IronManArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "EnhancedInput",   // (already present) UInputAction, UInputMappingContext, UEnhancedInputComponent
            "UMG",             // (already present) UUserWidget, UTextBlock, UProgressBar... (HUD)
            "Slate", "SlateCore", // (already present) FSlateFontInfo, FSlateColor, FCoreStyle
            "Niagara",         // ADD if you use UNiagaraComponent / UNiagaraFunctionLibrary (and enable the Niagara plugin)
            "AIModule",        // ADD if you use AAIController, UBehaviorTree, UBlackboardComponent
            "NavigationSystem",// ADD if you use UNavigationSystemV1 / nav queries
            "GameplayTasks",   // often needed alongside AIModule
        });
    }
}
```

Quick lookup: *which module do I need?*

| You use… | Add module |
|---|---|
| `UInputAction`, `UInputMappingContext`, `UEnhancedInputComponent`, `UEnhancedInputLocalPlayerSubsystem` | `EnhancedInput` |
| `UUserWidget`, `UWidgetComponent`, `UTextBlock`, `UProgressBar`, `UImage` | `UMG` (often also `Slate`, `SlateCore`) |
| `UNiagaraComponent`, `UNiagaraSystem`, `UNiagaraFunctionLibrary` | `Niagara` |
| `AAIController`, `UBehaviorTreeComponent`, `UBlackboardComponent`, `UAIPerceptionComponent` | `AIModule` (+ `GameplayTasks`) |
| `UNavigationSystemV1`, `UNavigationPath` | `NavigationSystem` |
| `UCameraShakeBase` etc. | `Engine` (already there) |

**After editing Build.cs:** close the editor, **regenerate project files**, and do a **full build** (Live Coding can't pick up Build.cs changes).

Plugin modules (EnhancedInput and Niagara are plugins) also have to be **enabled** in **Edit > Plugins**. Enhanced Input and Niagara are enabled by default in UE 5.4/5.5. If a plugin is disabled you can also list it in the `.uproject`:
```json
"Plugins": [ { "Name": "EnhancedInput", "Enabled": true } ]
```

### 4b. Missing `MODULE_API` export

**Symptom:** the unresolved symbol is one of **your** classes, used from **another** module (e.g. a second module or plugin in the project) or from a test module.

**Cause:** classes and functions are only visible outside their DLL if they're exported. For the `IronManArena` module the macro is `IRONMANARENA_API` (the module name in UPPERCASE + `_API`).

**Fix:**
```cpp
UCLASS()
class IRONMANARENA_API AArenaGameMode : public AGameModeBase
{
    GENERATED_BODY()
    ...
};

// free functions too:
IRONMANARENA_API void SpawnRepulsorBlast(UWorld* World, const FTransform& Xf);
```
Also add `"IronManArena"` to the **other** module's `Build.cs` dependency list.

### 4c. Declared but never defined

**Symptom:** the unresolved symbol is **your own** function, in the **same** module, e.g. `AIronManCharacter::FireRepulsor`.

**Causes and fixes**
- You declared it in the `.h` but never wrote the body in the `.cpp`. Add it.
- Typo or signature mismatch between `.h` and `.cpp` (`const`, parameter types, class name). Make them match exactly.
- A `UFUNCTION(BlueprintNativeEvent)` or `UFUNCTION(Server, Reliable)` needs the body named **`FireRepulsor_Implementation`** in the `.cpp`, not `FireRepulsor` (and server RPCs with `WithValidation` also need `_Validate`).
- A **new `.cpp` file isn't in the build.** Regenerate project files so it gets picked up. Files have to live under `Source/IronManArena/`.
- A `static` class member was declared but never defined in a `.cpp`.

---

## 5. C1083 cannot open include file / .generated.h must be the last include

### 5a. C1083: Cannot open include file

**Symptom**
```
fatal error C1083: Cannot open include file: 'EnhancedInputComponent.h': No such file or directory
fatal error C1083: Cannot open include file: 'IronManCharacter.generated.h': No such file or directory
```
Mac/clang: `fatal error: 'EnhancedInputComponent.h' file not found`

**Causes and fixes**
| Missing file | Fix |
|---|---|
| An **engine/plugin** header (`EnhancedInputComponent.h`, `NiagaraComponent.h`, `Blueprint/UserWidget.h`, `AIController.h`) | Add the module to `Build.cs` (section 4a), regenerate project files, rebuild. Check the include path: UMG is `#include "Blueprint/UserWidget.h"`, and Niagara is `#include "NiagaraComponent.h"`. |
| A `*.generated.h` | UHT didn't run or failed earlier. Scroll **up** for a UHT error (section 6). Also check that the `#include "X.generated.h"` filename matches the header's own filename exactly, including capitalization. |
| **Your own** header in a subfolder | Use the path relative to the module's `Public/`/`Private/`/root folder, e.g. `#include "Weapons/RepulsorComponent.h"`, or add the folder to `PublicIncludePaths` in Build.cs. |
| Works on Windows, fails on Mac | **Case sensitivity.** `#include "ironmancharacter.h"` ≠ `IronManCharacter.h` on some Mac/Linux file systems. Match the case exactly. |

### 5b. ".generated.h must be the last include"

**Symptom** (from UHT or the compiler)
```
error : #include found after .generated.h file - the .generated.h file should always be the last #include in a header
```

**Fix:** in every header with a `UCLASS`/`USTRUCT`/`UENUM`, the `.generated.h` include must be the **last** `#include`:
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "IronManCharacter.generated.h"   // <- always last

UCLASS()
class IRONMANARENA_API AIronManCharacter : public ACharacter
{
    GENERATED_BODY()
    ...
};
```
Move any extra includes **above** it, or into the `.cpp`. In headers, prefer forward declarations (`class UInputAction;`).

---

## 6. UHT (Unreal Header Tool) errors

UHT runs **before** the C++ compiler and parses your `UCLASS`/`UPROPERTY`/`UFUNCTION` macros. Its errors look like this:
```
C:\Dev\IronManArena\Source\IronManArena\IronManCharacter.h(25): error : ...
Error: UnrealHeaderTool failed for target 'IronManArenaEditor' ...
```
(In UE 5.4/5.5, UHT runs inside UBT, so you'll see it in the same build output.)

| Symptom (typical text) | Cause | Fix |
|---|---|---|
| `Expected a GENERATED_BODY() at the start of the class` / weird errors at the class's closing brace | Missing `GENERATED_BODY()` | Put `GENERATED_BODY()` as the first line inside every `UCLASS`/`USTRUCT` body. |
| `Missing UCLASS / USTRUCT ... ` / `Class 'X' must have a UCLASS() ...` / error near `UCLASS` | `UCLASS()` not directly above the `class` line, inside a namespace, or in a `.cpp` | `UCLASS()` goes on the line right before `class IRONMANARENA_API AFoo : public ...`, at global scope, in a **header**. UObject types can't live in namespaces. |
| `Unable to find 'class', 'delegate', 'enum', or 'struct' with name 'FMyThing'` | A type used in a `UPROPERTY`/`UFUNCTION` isn't a reflected type, or its header isn't included | Make it a `USTRUCT(BlueprintType)` / `UENUM(BlueprintType)`, and `#include` its header (forward declarations are fine for pointers only). |
| `Type 'std::vector<...>'` / `'unsigned long'` / `'int*' is not supported by blueprint` / `Inappropriate '*' on variable of type ...` | Unsupported type in `UPROPERTY` | Use UE types: `TArray`, `TMap`, `TSet`, `FString`, `FName`, `FText`, `int32`, `float`/`double`, `uint8` for enums. Raw pointers to non-UObjects, `std::` containers, and nested containers (`TArray<TArray<...>>`) can't be `UPROPERTY`. Use `TObjectPtr<UFoo>` for object pointers in UE5. |
| `BlueprintReadWrite should not be used on private members` | Blueprint access specifier on a `private:` member | Move it to `protected:`/`public:`, or add `meta=(AllowPrivateAccess="true")`. |
| `Class 'AArenaGameMode' has the same name as ...` / duplicate class | Two classes with the same name in any module or plugin | Rename one of them. Class names are global in UE. |
| `uint8 bFoo : 1` errors / `bool` bitfields | Bitfields need `uint8 bFoo : 1;` | Use that exact form, or a plain `bool`. |
| `UFUNCTION ... BlueprintNativeEvent ... must not be virtual` style errors | Declaring `virtual` on the event itself | Declare `UFUNCTION(BlueprintNativeEvent) void OnHit();` and implement `virtual void OnHit_Implementation();` |

**After fixing a UHT error, do a full build.** Don't use Live Coding for header/reflection changes.

---

## 7. "Running .../UnrealBuildTool ... failed"

**Symptom**
```
Running C:/Program Files/Epic Games/UE_5.4/Engine/Build/BatchFiles/Build.bat  -projectfiles -project="C:/Dev/IronManArena/IronManArena.uproject" -game -rocket -progress
...
UnrealBuildTool failed / Running .../UnrealBuildTool.dll ... failed with ExitCode=6
```
Or on Mac, when generating project files:
```
Running /Users/Shared/Epic Games/UE_5.4/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh ... failed
```
This often appears in a pop-up after *Generate Visual Studio project files* or *Switch Unreal Engine version*.

**Causes and fixes**
1. **No C++ toolchain** (VS workloads/Windows SDK, or Xcode not installed or license not accepted). See section 3 / section 10.
2. **A `Build.cs` or `Target.cs` won't compile.** UBT compiles these C# files first. Look in the UBT log for `error CS....` and fix the C# typo (missing `;`, wrong module name string, wrong class name; the class name must match the file name: `IronManArena.Build.cs` → `public class IronManArena : ModuleRules`).
3. **Target settings conflict** (UE 5.4+), e.g.:
   ```
   IronManArena modifies the values of properties: [ bStrictConformanceMode ]. This is not allowed, as IronManArena has build products in common with UnrealGame/UnrealEditor.
   ```
   Remove the overridden setting from `*.Target.cs`, or use the defaults:
   ```csharp
   DefaultBuildSettings = BuildSettingsVersion.Latest;
   IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
   ```
   (Launcher engines can't change shared build-environment settings.)
4. **Wrong engine association** / engine moved or uninstalled. Re-run **Switch Unreal Engine version...** (section 6 of the setup guide).
5. **Path problems.** Very long paths or non-ASCII characters in the project path. Move the project to something like `C:\Dev\IronManArena`.
6. **Another build is running / files locked.** Close the editor and VS, check for stray `UnrealEditor`, `dotnet`, `cl.exe` processes, and retry.
7. **Antivirus** quarantining `UnrealBuildTool` or locking `Intermediate/`. Add the engine and project folders to your antivirus exclusions.

Always open the **UBT log** (paths at the top). The pop-up rarely shows the real reason.

---

## 8. Live Coding errors and stale DLLs

**Symptoms**
```
Unable to build while Live Coding is active. Exit the editor and game, or press Ctrl+Alt+F11 if iterating on code in the editor or game
```
```
LogLiveCoding: Error: Live coding failed, please see Live console for more information
```
- Your change "doesn't do anything," or old behavior comes back after restarting the editor.
- `LNK1104: cannot open file '...\Binaries\Win64\UnrealEditor-IronManArena.dll'` (or `...-0001.dll`).
- Variables you added in the header don't show up in the Details panel.

**Causes**
- You built from VS while the editor was open with Live Coding on. UBT refuses to do that.
- You changed a header, `UPROPERTY`, or constructor, which Live Coding can't (reliably) patch.
- A Live Coding patch was applied in memory but **never saved to disk**. When you restart, the editor loads the *old* DLL.
- The editor or a crashed instance still holds the DLL open.

**Fix: the clean way**
1. **Close the editor completely.** Check Task Manager (Windows) / Activity Monitor (Mac) for leftover `UnrealEditor` processes and end them.
2. In VS/Xcode, **build Development Editor** (Build, not Rebuild, is usually enough).
3. Reopen the editor.

**Rule of thumb:** Live Coding (**Ctrl+Alt+F11**, Windows only) is fine for changes inside `.cpp` function bodies. For headers, `UPROPERTY`/`UFUNCTION`, constructors, new files, or Build.cs changes: **close the editor → build → reopen**.

You can turn Live Coding off if you prefer always building from the IDE: **Edit > Editor Preferences > General > Live Coding > Enable Live Coding** (or the options menu next to the Compile button at the bottom-right of the editor).

If that still doesn't help, do a clean build (section 9).

---

## 9. Safely deleting Binaries / Intermediate / DerivedDataCache / .vs / Saved

When things are truly tangled (stale DLLs, broken IntelliSense, odd UHT errors after renaming files, switching engine versions), wipe the **generated** folders. They're all rebuilt automatically.

> ⚠️ **Close the editor, Visual Studio/Rider, and Xcode first.**
> ⚠️ **Never delete `Content/`, `Config/`, `Source/`, or the `.uproject`.** Those are the actual project.
> ⚠️ Commit in Git (or zip the project) before deleting anything if you're unsure.

| Folder / file | Safe to delete? | What you lose |
|---|---|---|
| `Binaries/` | ✅ Yes | Compiled DLLs/dylibs, rebuilt on next build |
| `Intermediate/` | ✅ Yes | UHT-generated code, build artifacts, IntelliSense data. Next build is a full one |
| `DerivedDataCache/` | ✅ Yes | Cached shaders/cooked data. **Next editor launch recompiles shaders (slow)** |
| `.vs/` (Windows) | ✅ Yes | VS's IntelliSense database and window layout |
| `*.sln`, `*.xcworkspace` | ✅ Yes | Regenerate with *Generate project files* |
| `Saved/` | ⚠️ Mostly | Logs, crash dumps, autosaves, **per-user editor settings**, and local config overrides. Back up `Saved/Autosaves/` if you have unsaved work you might want. Prefer deleting only `Saved/Config/` and `Saved/Intermediate/` if you just need to reset config |
| `Plugins/*/Binaries`, `Plugins/*/Intermediate` | ✅ Yes | Same as above, for project plugins |

**Windows (Command Prompt, run in the project folder):**
```bat
cd /d C:\Dev\IronManArena
rmdir /s /q Binaries Intermediate DerivedDataCache .vs
del /q IronManArena.sln
:: optional: rmdir /s /q Saved
```

**macOS (Terminal, run in the project folder):**
```bash
cd ~/Dev/IronManArena
rm -rf Binaries Intermediate DerivedDataCache
rm -rf "IronManArena (Mac).xcworkspace" IronManArena.xcworkspace
# optional: rm -rf Saved
```

Then **Generate project files** → **build Development Editor** → open the editor.

---

## 10. macOS: Xcode signing, "xcrun: error", wrong Xcode version

### 10a. `xcrun: error`

**Symptoms**
```
xcrun: error: unable to find utility "xcodebuild", not a developer tool or in PATH
xcrun: error: invalid active developer path (/Library/Developer/CommandLineTools), missing xcrun at: ...
xcrun: error: SDK "macosx" cannot be located
```

**Cause:** only the standalone Command Line Tools are selected (not the full Xcode), Xcode was moved or renamed, or the license or first-launch setup was never completed.

**Fix:**
```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer   # or your Xcode_15.4.app
sudo xcodebuild -license accept
sudo xcodebuild -runFirstLaunch
xcode-select -p                      # should print .../Xcode.app/Contents/Developer
xcrun --sdk macosx --show-sdk-path   # should print a MacOSX.sdk path
```
Then regenerate project files and rebuild.

### 10b. Wrong Xcode version / "Platform Mac is not a valid platform to build"

**Symptoms**
```
Platform Mac is not a valid platform to build. Check that the SDK is installed properly.
```
```
LogTurnkeySupport: Warning: Turnkey failed to run properly ...
```
Or clang errors inside engine headers right after an Xcode update.

**Cause:** your Xcode is outside the range your engine version supports. Epic's table: UE 5.5 needs **Xcode 15.2+ (15.4 recommended)**, and UE 5.4 lists Xcode 14.1 as the minimum (Xcode 15.x is widely used with it). There are many reports of the **Launcher** UE 5.5 failing with **Xcode 16.x on macOS Sequoia**.

**Fix:**
1. Download **Xcode 15.4** from <https://developer.apple.com/download/all/>, expand it, and put it in `/Applications/Xcode_15.4.app`. It can sit alongside a newer Xcode.
2. Select it: `sudo xcode-select --switch /Applications/Xcode_15.4.app/Contents/Developer`
3. Run `sudo xcodebuild -license accept` and `sudo xcodebuild -runFirstLaunch` again.
4. Delete `Intermediate/` (section 9), regenerate project files, and rebuild.
5. Note that some newer macOS versions refuse to launch older Xcode versions. If that happens, check the Epic forums for your exact engine/macOS/Xcode combination, or upgrade the engine to a version that supports your Xcode.

### 10c. Code signing errors

**Symptoms**
```
Signing for "IronManArenaEditor" requires a development team. Select a development team in the Signing & Capabilities editor.
```
```
error: No signing certificate "Mac Development" found
code object is not signed at all
```

**Cause:** Xcode is trying to sign a target, which is usually unnecessary for running the editor locally.

**Fix:**
- For local **editor** builds you normally don't need a team. Build the **IronManArenaEditor** scheme from the generated workspace, which UBT configures for you, and don't add signing in the Xcode UI.
- If Xcode insists: select the project in the navigator, then the target, then the **Signing & Capabilities** tab. Choose your **Personal Team** (add your Apple ID under **Xcode > Settings > Accounts**) or set signing to *Sign to Run Locally*. The option names vary by Xcode version.
- For **packaged** builds you'll distribute: set up signing in **Project Settings > Platforms > Mac** / the Xcode Projects section (UE 5.4+ "modern Xcode"), and use a proper Developer ID for notarization. This isn't needed for testing on your own Mac.
- Gatekeeper blocking your own packaged `.app` ("cannot be opened because the developer cannot be verified"): right-click the app → **Open** → **Open**, or allow it under **System Settings > Privacy & Security**.

---

## 11. "Unable to find a game mode" / wrong pawn spawns

**Symptoms**
- When you press Play, you're a **floating camera** (`DefaultPawn`/`SpectatorPawn`) instead of Iron Man, or **nothing** spawns.
- Output Log: `LogGameMode: ... ` / `Unable to find GameMode class ...` / `Failed to find object 'Class /Script/IronManArena.ArenaGameMode'`.
- Log warnings like `LogLoad: Warning: Failed to find PlayerStart` / `Spawning at 0,0,0`.

**Causes and fixes**
1. **GameMode not set or overridden.** This project already sets `GlobalDefaultGameMode=/Script/IronManArena.ArenaGameMode` in `DefaultEngine.ini`. If you still get the wrong pawn, check that a custom map's **World Settings > GameMode Override** isn't pointing somewhere else. Places to set it:
   - **Window > World Settings > Game Mode > GameMode Override** = ArenaGameMode (`AArenaGameMode`), *or*
   - **Edit > Project Settings > Project > Maps & Modes > Default GameMode**, *or*
   - `Config/DefaultEngine.ini`:
     ```ini
     [/Script/EngineSettings.GameMapsSettings]
     GlobalDefaultGameMode=/Script/IronManArena.ArenaGameMode
     ```
   World Settings override **beats** the project default, so check both.
2. **Wrong class path in the ini.** It's `/Script/<Module>.<ClassWithoutPrefix>`: no `A`, and no `_C` for C++ classes. Blueprints need the full asset path ending in `_C`.
3. **The module didn't load** (e.g. the build failed and you clicked "No" on the rebuild prompt), or `ArenaGameMode.h/.cpp` hasn't been added to `Source/` yet. Either way, the class literally doesn't exist. Rebuild (sections 1–2).
4. **GameMode's Default Pawn Class isn't set.** In C++:
   ```cpp
   AArenaGameMode::AArenaGameMode()
   {
       DefaultPawnClass = AIronManCharacter::StaticClass();   // or a Blueprint via ConstructorHelpers::FClassFinder
       // PlayerControllerClass = AIronManPlayerController::StaticClass();
       // HUDClass = ...
   }
   ```
   Or override it in a Blueprint child of the GameMode (Class Defaults > **Default Pawn Class**). In **World Settings**, expand **Selected GameMode** to see what will actually spawn.
5. **No PlayerStart**, or it's inside geometry. Add one (Place Actors > *Player Start*), lift it slightly above the floor, and rebuild paths (**Build > Build Paths**) if you see a "BADsize" label.
6. **A pawn already in the level has "Auto Possess Player = Player 0"**, so you possess that one instead of the GameMode's pawn. Set it to *Disabled* (Details > Pawn > Auto Possess Player) or delete it.
7. **PIE "Spawn player at"** is set to *Current Camera Location* (the Play ⋮ menu). That's fine, but it ignores the PlayerStart.
8. Changed the constructor defaults? **Close the editor and do a full build.** Live Coding may not update the class defaults.

---

## 12. Enhanced Input not working

**Symptoms:** keys and mouse do nothing; movement and flight are dead; log is quiet, or `LogEnhancedInput` warnings appear.

**Checklist (most common first)**

1. **The Input Mapping Context was never added.** Enhanced Input does *nothing* until a mapping context is registered with the local player's subsystem. Typically you do this in the character's `BeginPlay` (or `PawnClientRestart`/`PossessedBy`), or the PlayerController's `BeginPlay`:
   ```cpp
   #include "EnhancedInputSubsystems.h"
   #include "EnhancedInputComponent.h"

   void AIronManCharacter::BeginPlay()
   {
       Super::BeginPlay();
       if (APlayerController* PC = Cast<APlayerController>(GetController()))
       {
           if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                   ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
           {
               Subsystem->AddMappingContext(DefaultMappingContext, /*Priority*/ 0);
           }
       }
   }
   ```
   If `GetController()` is null in `BeginPlay` (the pawn is possessed later), move this to `PossessedBy` / `PawnClientRestart`, or to the PlayerController.
2. **The mapping context or `UInputAction` objects are null, or have no key mappings.** This project has no input assets in `Content/`. Its `Build.cs` notes that `UInputAction`/`UInputMappingContext` are **created at runtime in C++** (presumably in `AIronManCharacter`, whose source wasn't on disk yet when I checked). So check that the code creating them runs *before* `AddMappingContext` and `BindAction`, and that it calls `MapKey(...)` for each key. If you later switch to editor-made assets (`IMC_Default`, `IA_Move`...), remember to assign them in the Blueprint/Class Defaults. Add a quick check:
   ```cpp
   ensureMsgf(DefaultMappingContext, TEXT("DefaultMappingContext not set on %s"), *GetName());
   ```
3. **Actions not bound.** In `SetupPlayerInputComponent`:
   ```cpp
   if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
   {
       EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AIronManCharacter::Move);
   }
   ```
   If the `Cast` fails, the project isn't using the Enhanced Input component. Check **Project Settings > Engine > Input > Default Classes**: *Default Player Input Class* = `EnhancedPlayerInput` and *Default Input Component Class* = `EnhancedInputComponent`. In `Config/DefaultInput.ini`:
   ```ini
   [/Script/Engine.InputSettings]
   DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
   DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
   ```
4. **Module or plugin missing.** `"EnhancedInput"` must be in `Build.cs` (section 4a), and the **Enhanced Input** plugin enabled (**Edit > Plugins**). Both are already true in this project. The `DefaultInput.ini` lines above also ship with the project. Note that it sets `bCaptureMouseOnLaunch=True` and `CapturePermanently_IncludingInitialMouseDown`, so **Shift+F1** is how you get your mouse cursor back during Play.
5. **Wrong trigger or value type.** For axis-type movement, the Input Action's **Value Type** should be `Axis2D (Vector2D)` for WASD/stick, with **Swizzle**/**Negate** modifiers on the W/S/A/D keys in the mapping context. A `Pressed` trigger on a held key fires only once.
6. **Viewport doesn't have focus.** Click inside the PIE viewport. If you used `SetInputMode(FInputModeUIOnly())` for a menu, switch back to `FInputModeGameOnly()`.
7. **Wrong pawn possessed.** See section 11. Input bound on a pawn you aren't controlling does nothing.
8. **Debugging:** while playing, open the console (`~`) and type `showdebug enhancedinput` to see active mapping contexts and action values.

---

## 13. Editor crashes on open

**Symptoms:** splash screen then crash; the **Unreal Engine Crash Reporter** window appears; or the editor just disappears.

**Where to look**
| What | Windows | macOS |
|---|---|---|
| Latest editor log | `<Project>\Saved\Logs\IronManArena.log` (older ones: `IronManArena-backup-<date>.log`) | `<Project>/Saved/Logs/IronManArena.log` |
| Crash folders (log + minidump + context) | `<Project>\Saved\Crashes\UECC-Windows-...\` | `<Project>/Saved/Crashes/` and `~/Library/Logs/DiagnosticReports/` |
| Crash Reporter window | Shows the call stack. Click **Copy** or read it before closing | Same |

**How to read it:** open the `.log` and scroll to the **bottom**. Look for `Fatal error`, `Assertion failed`, `Unhandled Exception`, or `=== Critical error: ===`. The call stack right below shows which function crashed. If you see `UnrealEditor_IronManArena!...` in it, it's **your** code.

**Common causes and fixes**
1. **Your C++ crashes during load.** Common culprits are a null pointer in a **constructor**, a `check()` failing, or `ConstructorHelpers::FObjectFinder` pointing to an asset path that doesn't exist (`CDO Constructor ... Failed to find /Game/...`).
   - Fix the code. Never dereference other actors/world in constructors, so move that logic to `BeginPlay`.
   - To debug: in VS, set the configuration to **DebugGame Editor** and press **F5**. The debugger stops on the crashing line.
2. **The startup map crashes the editor.** This project starts on the empty `/Engine/Maps/Entry`, so a crash right at startup is more likely to be C++ (cause 1) than the map. If you've pointed `EditorStartupMap` at your own map and it crashes, close the editor, set `EditorStartupMap=/Engine/Maps/Entry` again in `Config/DefaultEngine.ini`, and reopen. Then fix the map.
3. **Stale binaries vs. changed headers.** Do a clean build (section 9: delete `Binaries/` + `Intermediate/`, regenerate, rebuild).
4. **Corrupt editor layout or config.** Delete `<Project>/Saved/Config/` (it's rebuilt).
5. **GPU or driver crash** (`D3D device removed`, `GPU crashed`, `DXGI_ERROR_DEVICE_HUNG`). Update the GPU driver. As a test, launch with DX11 or without RHI features:
   ```bat
   "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Dev\IronManArena\IronManArena.uproject" -dx11
   ```
6. **Plugin incompatible with the engine version.** The log mentions a plugin by name. Disable it in the `.uproject` (`"Enabled": false`) and reopen.
7. **Out of memory during shader compile.** Close other apps, or add RAM/page file. The log shows `Ran out of memory` or the machine thrashes.

---

## 14. IntelliSense red squiggles that aren't real errors

**Symptoms:** VS underlines `UCLASS`, `GENERATED_BODY()`, `UPROPERTY`, `TObjectPtr`, or `#include "X.generated.h"` in red; the **Error List** shows hundreds of "errors"… but the **build succeeds**.

**Cause:** IntelliSense is a separate, simplified parser. It doesn't fully understand Unreal's macros and generated headers, especially before the first build or after adding files. **Only the Output window (Build) counts.**

**Fixes / ways to make it calmer**
1. Build once. Generated headers then exist and many squiggles go away.
2. **Regenerate project files** after adding or renaming files.
3. Set the **Error List** dropdown to **Build Only**.
4. Install the **IDE support for Unreal Engine** component (Visual Studio Installer, under *Game development with C++*) for better UE awareness.
5. Turn off IntelliSense error squiggles: **Tools > Options > Text Editor > C/C++ > Advanced > IntelliSense > Disable Error Reporting = True** (or *Disable Squiggles = True*).
6. Still broken? Close VS, delete `.vs/` and the `.sln`, regenerate, reopen.
7. Alternatives: **Rider** or VS with **Visual Assist** handle UE macros much better.

On Mac, Xcode's live issues can show similar false errors. Trust the **build** result.

---

## 15. Long first compile and shader compilation

**What's normal**
- **First C++ build:** a few minutes to 20+ minutes for the project module (Launcher engines are precompiled, so you're only building your game code). Building the engine from source takes over an hour.
- **First editor launch:** "**Compiling Shaders (12,345)**" in the bottom-right corner can take **10–60+ minutes** and pin the CPU at 100%. That's expected. Results go into `DerivedDataCache/` and the engine's shared DDC, so later launches are fast.
- Opening a new map, or the first time you use a new material/Niagara system, triggers more shader compiles.

**Speed-ups**
- Put the engine, the project, **and** the DDC on an **SSD/NVMe**.
- Don't delete `DerivedDataCache/` unless you have to (section 9).
- Close other heavy apps and give UE the RAM.
- Let the first compile finish once, overnight if needed, instead of cancelling it.
- Add the engine and project folders to your **antivirus exclusions** (Windows Defender scanning every `.obj` adds a lot of time).
- **Windows:** in the VS Installer, keep only one MSVC toolset if you aren't pinning one (it reduces confusion, not time). More cores means a faster C++ build. UBT parallelizes automatically.
- **Mac:** keep the Mac plugged in and don't let it sleep (in **System Settings > Battery/Energy**).
- Fewer shaders while iterating: **Edit > Editor Preferences** / **Project Settings** offer per-feature toggles, but don't change rendering settings on a shared project without asking the team. Every teammate would have to recompile.

**Is it stuck?** If the shader count in the bottom-right keeps dropping, it's working. If the log hasn't changed for 30+ minutes and CPU usage is near 0%, close the editor and check `Saved/Logs/IronManArena.log` for errors.

---

## 16. Still stuck?

1. Find the **first** error (section 0) and copy the exact line.
2. Note your **engine version** (Help > About Unreal Editor), **VS/MSVC version** or **Xcode version** (`xcodebuild -version`), and **OS version**.
3. Attach `Saved/Logs/IronManArena.log` and, for build failures, the UBT log.
4. Search the exact error text on the Epic Developer Community forums (<https://forums.unrealengine.com>) or Epic's docs (<https://dev.epicgames.com/documentation/en-us/unreal-engine>).

Happy flying! 🚀
