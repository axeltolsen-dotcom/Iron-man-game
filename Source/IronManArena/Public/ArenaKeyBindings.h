// ArenaKeyBindings: the rebindable keyboard / mouse keys shown in Settings > CONTROLS.
// Saved per user in GameUserSettings.ini [IronManArena.Keys] (Id=KeyName); DefaultGame.ini may seed it.
// Gamepad buttons stay fixed (shown next to each row). ApplyToPlayer rebuilds the Enhanced Input mapping live.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

struct FArenaKeyBinding
{
	const TCHAR* Id = TEXT("");
	const TCHAR* Group = TEXT("");
	const TCHAR* Label = TEXT("");
	FKey DefaultKey;
	const TCHAR* Pad = TEXT("-");
	/** Action text of the ControlsReference.inl row this binding replaces in the settings list ("" = none). */
	const TCHAR* ReferenceAction = TEXT("");
};

namespace ArenaKeys
{
	IRONMANARENA_API const TArray<FArenaKeyBinding>& All();
	IRONMANARENA_API const FArenaKeyBinding* Find(const TCHAR* Id);
	/** Saved key for Id, else its default (EKeys::Invalid for an unknown Id). */
	IRONMANARENA_API FKey Get(const TCHAR* Id);
	/** Save Id=NewKey to GameUserSettings.ini and flush it. */
	IRONMANARENA_API void Set(const TCHAR* Id, const FKey& NewKey);
	/** Drop every saved override (back to the defaults). */
	IRONMANARENA_API void ResetAll();
	/** Keys that cannot be bound: Esc, F1, `~, F11, Enter, gamepad buttons, mouse axes and the wheel. */
	IRONMANARENA_API bool IsReserved(const FKey& K);
	/** Id of the other binding currently using K ("" if none). */
	IRONMANARENA_API FString IdUsingKey(const FKey& K, const TCHAR* ExceptId);
	/** Rebuild the player's Enhanced Input mapping context so new keys work immediately. */
	IRONMANARENA_API void ApplyToPlayer(APlayerController* PC);
}
