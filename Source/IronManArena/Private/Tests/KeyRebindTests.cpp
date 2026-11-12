// IronManArena.UI.KeyRebind: Settings > CONTROLS key table, save / swap / reset, and reserved keys.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ArenaKeyBindings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKeyRebindTest, "IronManArena.UI.KeyRebind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FKeyRebindTest::RunTest(const FString& Parameters)
{
	// Every binding has a unique id and a valid, non-reserved default; defaults do not collide.
	TSet<FString> Ids;
	TSet<FName> DefaultKeys;
	for (const FArenaKeyBinding& B : ArenaKeys::All())
	{
		TestFalse(FString::Printf(TEXT("unique id %s"), B.Id), Ids.Contains(B.Id));
		Ids.Add(B.Id);
		TestTrue(FString::Printf(TEXT("%s default valid"), B.Id), B.DefaultKey.IsValid() && !ArenaKeys::IsReserved(B.DefaultKey));
		TestFalse(FString::Printf(TEXT("%s default unique"), B.Id), DefaultKeys.Contains(B.DefaultKey.GetFName()));
		DefaultKeys.Add(B.DefaultKey.GetFName());
	}

	// ReferenceAction names must match a row of Vison's generated table, or the old row would show twice.
	TSet<FString> RefActions;
#define CONTROLS_GROUP(G)
#define CONTROLS_ROW(A, K, P, N, RN, RD) RefActions.Add(A);
#define CONTROLS_RESERVED_KEY(K)
#include "../Generated/ControlsReference.inl"
#undef CONTROLS_GROUP
#undef CONTROLS_ROW
#undef CONTROLS_RESERVED_KEY
	for (const FArenaKeyBinding& B : ArenaKeys::All())
	{
		if (B.ReferenceAction && B.ReferenceAction[0]) { TestTrue(FString::Printf(TEXT("reference row '%s'"), B.ReferenceAction), RefActions.Contains(B.ReferenceAction)); }
	}

	TestTrue(TEXT("Esc reserved"), ArenaKeys::IsReserved(EKeys::Escape));
	TestTrue(TEXT("gamepad reserved"), ArenaKeys::IsReserved(EKeys::Gamepad_FaceButton_Bottom));
	TestFalse(TEXT("K bindable"), ArenaKeys::IsReserved(EKeys::K));

	ArenaKeys::ResetAll();
	TestEqual(TEXT("default flight key"), ArenaKeys::Get(TEXT("ToggleFlight")), EKeys::F);
	ArenaKeys::Set(TEXT("ToggleFlight"), EKeys::K);
	TestEqual(TEXT("saved"), ArenaKeys::Get(TEXT("ToggleFlight")), EKeys::K);
	TestEqual(TEXT("who uses K"), ArenaKeys::IdUsingKey(EKeys::K, TEXT("")), FString(TEXT("ToggleFlight")));
	TestTrue(TEXT("nobody else uses F"), ArenaKeys::IdUsingKey(EKeys::F, TEXT("")).IsEmpty());
	ArenaKeys::ResetAll();
	TestEqual(TEXT("reset"), ArenaKeys::Get(TEXT("ToggleFlight")), EKeys::F);
	TestEqual(TEXT("unknown id"), ArenaKeys::Get(TEXT("NoSuchAction")), EKeys::Invalid);

	// On-screen prompts follow the rebinds (and a swap doesn't chain).
	TestEqual(TEXT("short space"), ArenaKeys::ShortName(EKeys::SpaceBar), FString(TEXT("Space")));
	TestEqual(TEXT("short lmb"), ArenaKeys::ShortName(EKeys::LeftMouseButton), FString(TEXT("LMB")));
	TestEqual(TEXT("prompt unchanged at defaults"), ArenaKeys::RebindPrompt(FString(TEXT("[E] get in"))), FString(TEXT("[E] get in")));
	ArenaKeys::Set(TEXT("Interact"), EKeys::F);
	ArenaKeys::Set(TEXT("ToggleFlight"), EKeys::E);
	TestEqual(TEXT("prompt swap"), ArenaKeys::RebindPrompt(FString(TEXT("[E] get in  FLIGHT [F]"))), FString(TEXT("[F] get in  FLIGHT [E]")));
	ArenaKeys::Set(TEXT("Ascend"), EKeys::K);
	TestEqual(TEXT("prompt space"), ArenaKeys::RebindPrompt(FString(TEXT("[Space] web swing"))), FString(TEXT("[K] web swing")));
	TestEqual(TEXT("ftext prompt"), ArenaKeys::RebindPrompt(FText::FromString(TEXT("[E] read"))).ToString(), FString(TEXT("[F] read")));
	ArenaKeys::ResetAll();
	return true;
}

#endif
