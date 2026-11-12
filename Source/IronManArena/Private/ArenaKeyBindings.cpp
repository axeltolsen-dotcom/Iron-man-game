#include "ArenaKeyBindings.h"

#include "IronManCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* KeysSection = TEXT("IronManArena.Keys");

	FArenaKeyBinding MakeBinding(const TCHAR* Id, const TCHAR* Group, const TCHAR* Label, const FKey& Default, const TCHAR* Pad, const TCHAR* RefAction)
	{
		FArenaKeyBinding B;
		B.Id = Id; B.Group = Group; B.Label = Label; B.DefaultKey = Default; B.Pad = Pad; B.ReferenceAction = RefAction;
		return B;
	}
}

const TArray<FArenaKeyBinding>& ArenaKeys::All()
{
	static TArray<FArenaKeyBinding> Table;
	if (Table.Num() == 0)
	{
		const TCHAR* Flight = TEXT("FLIGHT & MOVEMENT");
		const TCHAR* Combat = TEXT("COMBAT");
		const TCHAR* Suit = TEXT("SUIT");
		const TCHAR* City = TEXT("CITY");
		const TCHAR* System = TEXT("SYSTEM");
		Table.Add(MakeBinding(TEXT("MoveForward"), Flight, TEXT("Move forward"), EKeys::W, TEXT("Left stick"), TEXT("Move / strafe")));
		Table.Add(MakeBinding(TEXT("MoveBack"), Flight, TEXT("Move back"), EKeys::S, TEXT("Left stick"), TEXT("")));
		Table.Add(MakeBinding(TEXT("MoveLeft"), Flight, TEXT("Strafe left"), EKeys::A, TEXT("Left stick"), TEXT("")));
		Table.Add(MakeBinding(TEXT("MoveRight"), Flight, TEXT("Strafe right"), EKeys::D, TEXT("Left stick"), TEXT("")));
		Table.Add(MakeBinding(TEXT("Ascend"), Flight, TEXT("Fly up / jump"), EKeys::SpaceBar, TEXT("A"), TEXT("Fly up / jump (on ground)")));
		Table.Add(MakeBinding(TEXT("Descend"), Flight, TEXT("Fly down"), EKeys::LeftControl, TEXT("B"), TEXT("Fly down")));
		Table.Add(MakeBinding(TEXT("Boost"), Flight, TEXT("Boost (hold)"), EKeys::LeftShift, TEXT("Left stick click"), TEXT("Boost (hold)")));
		Table.Add(MakeBinding(TEXT("ToggleFlight"), Flight, TEXT("Toggle flight"), EKeys::F, TEXT("Y"), TEXT("Toggle flight")));
		Table.Add(MakeBinding(TEXT("ToggleView"), Flight, TEXT("First / third person"), EKeys::V, TEXT("Right stick click"), TEXT("First / third person")));
		Table.Add(MakeBinding(TEXT("Fire"), Combat, TEXT("Repulsors (hold)"), EKeys::LeftMouseButton, TEXT("RT"), TEXT("Repulsors (hold to keep firing)")));
		Table.Add(MakeBinding(TEXT("Special"), Combat, TEXT("Suit special / hold for ULTRA"), EKeys::RightMouseButton, TEXT("LT"), TEXT("Suit special (tap)")));
		Table.Add(MakeBinding(TEXT("Hulkbuster"), Combat, TEXT("Hulkbuster"), EKeys::Q, TEXT("X"), TEXT("Hulkbuster")));
		Table.Add(MakeBinding(TEXT("Vent"), Combat, TEXT("Vent reactor"), EKeys::X, TEXT("D-pad down"), TEXT("Vent reactor")));
		Table.Add(MakeBinding(TEXT("Ally"), Combat, TEXT("Summon ally"), EKeys::G, TEXT("D-pad up"), TEXT("Summon ally")));
		Table.Add(MakeBinding(TEXT("SuitWheel"), Suit, TEXT("Suit menu / wheel"), EKeys::T, TEXT("LB"), TEXT("Suit menu / wheel (hold, release to pick)")));
		Table.Add(MakeBinding(TEXT("Faceplate"), Suit, TEXT("Faceplate open / close"), EKeys::H, TEXT("D-pad left"), TEXT("Faceplate open / close")));
		Table.Add(MakeBinding(TEXT("SuitExit"), Suit, TEXT("Exit suit / call suit"), EKeys::O, TEXT("-"), TEXT("Exit suit / call suit")));
		Table.Add(MakeBinding(TEXT("WebSwing"), Suit, TEXT("Web swing mode (Iron Spider)"), EKeys::B, TEXT("D-pad right"), TEXT("Web swing mode (Iron Spider)")));
		Table.Add(MakeBinding(TEXT("WebZip"), Suit, TEXT("Web-zip (swing mode)"), EKeys::R, TEXT("X"), TEXT("Web-zip (swing mode)")));
		Table.Add(MakeBinding(TEXT("Interact"), City, TEXT("Interact / get in / get out"), EKeys::E, TEXT("-"), TEXT("Interact (vehicles, shops, houses)")));
		Table.Add(MakeBinding(TEXT("Shop"), City, TEXT("Shop"), EKeys::U, TEXT("-"), TEXT("Shop")));
		Table.Add(MakeBinding(TEXT("Phone"), City, TEXT("Phone"), EKeys::N, TEXT("-"), TEXT("Phone")));
		Table.Add(MakeBinding(TEXT("Markers"), City, TEXT("Map markers on / off"), EKeys::M, TEXT("-"), TEXT("Map markers on / off")));
		Table.Add(MakeBinding(TEXT("Pause"), System, TEXT("Pause (Esc always pauses too)"), EKeys::P, TEXT("Menu (Start)"), TEXT("Pause")));
	}
	return Table;
}

const FArenaKeyBinding* ArenaKeys::Find(const TCHAR* Id)
{
	for (const FArenaKeyBinding& B : All())
	{
		if (FCString::Stricmp(B.Id, Id) == 0) { return &B; }
	}
	return nullptr;
}

FKey ArenaKeys::Get(const TCHAR* Id)
{
	FString S;
	if (GConfig && (GConfig->GetString(KeysSection, Id, S, GGameUserSettingsIni) || GConfig->GetString(KeysSection, Id, S, GGameIni)))
	{
		const FKey K(*S);
		if (K.IsValid() && !IsReserved(K)) { return K; }
	}
	const FArenaKeyBinding* B = Find(Id);
	return B ? B->DefaultKey : EKeys::Invalid;
}

void ArenaKeys::Set(const TCHAR* Id, const FKey& NewKey)
{
	if (!GConfig || !NewKey.IsValid()) { return; }
	GConfig->SetString(KeysSection, Id, *NewKey.GetFName().ToString(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ArenaKeys::ResetAll()
{
	if (!GConfig) { return; }
	// Write the defaults explicitly so a DefaultGame.ini seed cannot win over "Defaults".
	for (const FArenaKeyBinding& B : All())
	{
		GConfig->SetString(KeysSection, B.Id, *B.DefaultKey.GetFName().ToString(), GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

bool ArenaKeys::IsReserved(const FKey& K)
{
	if (!K.IsValid() || K.IsGamepadKey() || K.IsAxis1D() || K.IsAxis2D() || K.IsAxis3D() || K.IsTouch()) { return true; }
	return K == EKeys::Escape || K == EKeys::F1 || K == EKeys::Tilde || K == EKeys::F11 || K == EKeys::Enter
		|| K == EKeys::MouseScrollUp || K == EKeys::MouseScrollDown || K == EKeys::MouseWheelAxis
		|| K == EKeys::Tab || K == EKeys::One || K == EKeys::Two || K == EKeys::Three || K == EKeys::Four || K == EKeys::Five;
}

FString ArenaKeys::IdUsingKey(const FKey& K, const TCHAR* ExceptId)
{
	for (const FArenaKeyBinding& B : All())
	{
		if (FCString::Stricmp(B.Id, ExceptId) != 0 && Get(B.Id) == K) { return B.Id; }
	}
	return FString();
}

void ArenaKeys::ApplyToPlayer(APlayerController* PC)
{
	if (!PC) { return; }
	if (AIronManCharacter* Char = Cast<AIronManCharacter>(PC->GetPawn())) { Char->RebuildInputMapping(); }
}

FString ArenaKeys::ShortName(const FKey& K)
{
	if (!K.IsValid()) { return TEXT("?"); }
	if (K == EKeys::SpaceBar) { return TEXT("Space"); }
	if (K == EKeys::LeftMouseButton) { return TEXT("LMB"); }
	if (K == EKeys::RightMouseButton) { return TEXT("RMB"); }
	if (K == EKeys::MiddleMouseButton) { return TEXT("MMB"); }
	if (K == EKeys::ThumbMouseButton) { return TEXT("Mouse4"); }
	if (K == EKeys::ThumbMouseButton2) { return TEXT("Mouse5"); }
	if (K == EKeys::LeftShift || K == EKeys::RightShift) { return TEXT("Shift"); }
	if (K == EKeys::LeftControl || K == EKeys::RightControl) { return TEXT("Ctrl"); }
	if (K == EKeys::LeftAlt || K == EKeys::RightAlt) { return TEXT("Alt"); }
	if (K == EKeys::BackSpace) { return TEXT("Backspace"); }
	if (K == EKeys::CapsLock) { return TEXT("Caps"); }
	FString S = K.GetDisplayName(false).ToString();
	if (S.IsEmpty()) { S = K.GetFName().ToString(); }
	return S;
}

FString ArenaKeys::ShortLabel(const TCHAR* Id)
{
	return ShortName(Get(Id));
}

FString ArenaKeys::RebindPrompt(const FString& In)
{
	if (!In.Contains(TEXT("["))) { return In; }
	const TArray<FArenaKeyBinding>& Table = All();
	FString Out = In;
	TArray<int32> Changed;
	// Pass 1: each changed default "[Def]" -> a private marker, so swapped keys can't chain.
	for (int32 i = 0; i < Table.Num(); ++i)
	{
		const FKey Cur = Get(Table[i].Id);
		if (Cur == Table[i].DefaultKey) { continue; }
		const FString Def = FString::Printf(TEXT("[%s]"), *ShortName(Table[i].DefaultKey));
		if (!Out.Contains(Def, ESearchCase::CaseSensitive)) { continue; }
		Out.ReplaceInline(*Def, *FString::Printf(TEXT("[\x01%d\x02]"), i), ESearchCase::CaseSensitive);
		Changed.Add(i);
	}
	// Pass 2: markers -> the current key names.
	for (int32 i : Changed)
	{
		Out.ReplaceInline(*FString::Printf(TEXT("[\x01%d\x02]"), i), *FString::Printf(TEXT("[%s]"), *ShortLabel(Table[i].Id)), ESearchCase::CaseSensitive);
	}
	return Out;
}

FText ArenaKeys::RebindPrompt(const FText& In)
{
	const FString S = In.ToString();
	const FString R = RebindPrompt(S);
	return R.Equals(S, ESearchCase::CaseSensitive) ? In : FText::FromString(R);
}
