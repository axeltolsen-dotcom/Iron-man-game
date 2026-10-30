#include "StoryMenuWidget.h"

#include "ArenaUI.h"
#include "ArenaPlayerController.h"
#include "ArenaGameMode.h"
#include "ArenaSaveGame.h"
#include "StoryLibrary.h"
#include "StoryTypes.h"
#include "SuitTypes.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "StoryMenuWidget"

void UStoryMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

TSharedRef<SWidget> UStoryMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildTree(); }
	return Super::RebuildWidget();
}

void UStoryMenuWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.02f, 0.82f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); }

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Box))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}
	auto AddRow = [Box](UWidget* W, float PadBottom)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom)); }
	};
	auto AddButtonRow = [this, &AddRow](const TArray<UButton*>& Buttons)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (UButton* B : Buttons)
		{
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(B)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(8.f, 0.f)); }
		}
		AddRow(Row, 10.f);
	};

	AddRow(ArenaUI::MakeText(WidgetTree, LOCTEXT("Mode", "STORY MODE"), 20, ArenaUI::Ice), 0.f);
	StoryTitleText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 44, ArenaUI::Red);
	AddRow(StoryTitleText, 4.f);
	ProgressText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 16, ArenaUI::Gold, "Regular");
	AddRow(ProgressText, 16.f);

	// Name row.
	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* NameLabel = ArenaUI::MakeText(WidgetTree, LOCTEXT("NameLabel", "YOUR NAME"), 16, ArenaUI::Gold);
	USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	NameSize->SetWidthOverride(300.f);
	NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	NameBox->SetHintText(LOCTEXT("NameHint", "Harley"));
	NameSize->SetContent(NameBox);
	if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(NameLabel)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f)); }
	if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(NameSize)) { S->SetVerticalAlignment(VAlign_Center); }
	AddRow(NameRow, 18.f);

	UTextBlock* L = nullptr;
	UTextBlock* Tmp = nullptr;
	ContinueButton = ArenaUI::MakeButton(WidgetTree, LOCTEXT("Continue", "CONTINUE"), 24, Tmp); ContinueLabel = Tmp;
	UButton* NewStory = ArenaUI::MakeButton(WidgetTree, LOCTEXT("NewStory", "NEW STORY"), 24, L);
	ContinueButton->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleContinue);
	NewStory->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleNewStory);
	AddButtonRow({ ContinueButton.Get(), NewStory });

	// Chapter select.
	UHorizontalBox* ChapterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* Prev = ArenaUI::MakeButton(WidgetTree, FText::FromString(TEXT("<")), 22, L);
	UButton* Next = ArenaUI::MakeButton(WidgetTree, FText::FromString(TEXT(">")), 22, L);
	Prev->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandlePrevChapter);
	Next->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleNextChapter);
	USizeBox* ChapterSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChapterSize->SetWidthOverride(520.f);
	ChapterText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 20, ArenaUI::Gold);
	ChapterSize->SetContent(ChapterText);
	for (UWidget* W : TArray<UWidget*>{ Prev, ChapterSize, Next })
	{
		if (UHorizontalBoxSlot* S = ChapterRow->AddChildToHorizontalBox(W)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(8.f, 0.f)); }
	}
	AddRow(ChapterRow, 4.f);
	ChapterInfoText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 14, ArenaUI::Ice, "Regular");
	ChapterInfoText->SetAutoWrapText(true);
	AddRow(ChapterInfoText, 10.f);

	UButton* Play = ArenaUI::MakeButton(WidgetTree, LOCTEXT("PlayChapter", "PLAY CHAPTER"), 20, L);
	Play->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandlePlayChapter);
	AddButtonRow({ Play });

	// Side missions (Friday's side_missions.json).
	if (UStoryLibrary::GetStory().SideMissions.Num() > 0)
	{
		UHorizontalBox* SideRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UButton* SPrev = ArenaUI::MakeButton(WidgetTree, FText::FromString(TEXT("<")), 22, L);
		UButton* SNext = ArenaUI::MakeButton(WidgetTree, FText::FromString(TEXT(">")), 22, L);
		SPrev->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandlePrevSide);
		SNext->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleNextSide);
		USizeBox* SideSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SideSize->SetWidthOverride(520.f);
		SideText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 20, ArenaUI::Gold);
		SideSize->SetContent(SideText);
		for (UWidget* W : TArray<UWidget*>{ SPrev, SideSize, SNext })
		{
			if (UHorizontalBoxSlot* S = SideRow->AddChildToHorizontalBox(W)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(8.f, 0.f)); }
		}
		AddRow(ArenaUI::MakeText(WidgetTree, LOCTEXT("SideHeader", "SIDE MISSIONS"), 14, ArenaUI::Ice), 2.f);
		AddRow(SideRow, 4.f);
		SideInfoText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 14, ArenaUI::Ice, "Regular");
		SideInfoText->SetAutoWrapText(true);
		AddRow(SideInfoText, 6.f);
		SidePlayButton = ArenaUI::MakeButton(WidgetTree, LOCTEXT("PlaySide", "PLAY SIDE MISSION"), 20, L);
		SidePlayButton->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandlePlaySide);
		AddButtonRow({ SidePlayButton.Get() });
	}

	UButton* Back = ArenaUI::MakeButton(WidgetTree, LOCTEXT("Back", "BACK"), 20, L);
	Back->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleBack);
	if (UStoryLibrary::GetStory().Codex.Num() > 0)
	{
		UButton* Codex = ArenaUI::MakeButton(WidgetTree, LOCTEXT("Codex", "CODEX"), 20, L);
		Codex->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleOpenCodex);
		AddButtonRow({ Codex, Back });
	}
	else
	{
		AddButtonRow({ Back });
	}

	// Codex overlay (hidden until CODEX).
	CodexPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CodexPanel"));
	CodexPanel->SetBrushColor(FLinearColor(0.f, 0.005f, 0.02f, 0.96f));
	CodexPanel->SetPadding(FMargin(48.f, 36.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(CodexPanel))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		S->SetZOrder(5);
	}
	{
		UVerticalBox* CBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CodexPanel->SetContent(CBox);
		auto CRow = [CBox](UWidget* W, float PadBottom, bool bCenter)
		{
			if (UVerticalBoxSlot* S = CBox->AddChildToVerticalBox(W)) { S->SetHorizontalAlignment(bCenter ? HAlign_Center : HAlign_Left); S->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom)); }
		};
		CRow(ArenaUI::MakeText(WidgetTree, LOCTEXT("CodexTitle", "CODEX"), 20, ArenaUI::Ice), 4.f, true);
		CodexHeader = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 14, ArenaUI::Gold, "Regular");
		CRow(CodexHeader, 12.f, true);
		CodexTitle = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 28, ArenaUI::Red);
		CRow(CodexTitle, 10.f, false);
		USizeBox* BodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BodySize->SetWidthOverride(760.f);
		BodySize->SetMinDesiredHeight(170.f);
		CodexBody = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 17, ArenaUI::Ice, "Regular");
		CodexBody->SetAutoWrapText(true);
		BodySize->SetContent(CodexBody);
		CRow(BodySize, 14.f, false);
		CodexRewards = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 13, ArenaUI::Gold, "Regular");
		CodexRewards->SetAutoWrapText(true);
		CRow(CodexRewards, 14.f, false);
		UHorizontalBox* CNav = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UButton* CPrev = ArenaUI::MakeButton(WidgetTree, LOCTEXT("CodexPrev", "< PREV"), 18, L);
		UButton* CNext = ArenaUI::MakeButton(WidgetTree, LOCTEXT("CodexNext", "NEXT >"), 18, L);
		UButton* CClose = ArenaUI::MakeButton(WidgetTree, LOCTEXT("CodexClose", "CLOSE"), 18, L);
		CPrev->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandlePrevCodex);
		CNext->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleNextCodex);
		CClose->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleCloseCodex);
		for (UWidget* W : TArray<UWidget*>{ CPrev, CNext, CClose })
		{
			if (UHorizontalBoxSlot* S = CNav->AddChildToHorizontalBox(W)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(8.f, 0.f)); }
		}
		CRow(CNav, 0.f, true);
	}
	CodexPanel->SetVisibility(ESlateVisibility::Collapsed);

	RefreshFromSave();
}

void UStoryMenuWidget::RefreshFromSave()
{
	if (!StoryTitleText) { return; }
	const FStoryBook& Story = UStoryLibrary::GetStory();
	const int32 NumChapters = Story.Chapters.Num();
	DefaultName = Story.PlayerNameDefault.IsEmpty() ? FString(TEXT("Harley")) : Story.PlayerNameDefault;

	const UWorld* World = GetWorld();
	const AArenaGameMode* GM = World ? World->GetAuthGameMode<AArenaGameMode>() : nullptr;
	const UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(GM ? GM->SaveSlotName : FString(TEXT("IronManArena")));
	ChapterReached = Save ? FMath::Clamp(Save->StoryChapterReached, 0, NumChapters) : 0; // == NumChapters: finale next
	bComplete = Save && Save->bStoryComplete;
	bPrologueSeen = Save && Save->bPrologueSeen;
	const FString Name = Save && !Save->PlayerName.IsEmpty() ? Save->PlayerName : DefaultName;
	NameBox->SetText(FText::FromString(Name));
	NameBox->SetHintText(FText::FromString(DefaultName));

	StoryTitleText->SetText(Story.Title.IsEmpty() ? LOCTEXT("DefaultTitle", "IRON LEGACY") : FText::FromString(Story.Title.ToUpper()));
	if (!Story.IsValid())
	{
		ProgressText->SetText(LOCTEXT("NoStory", "No story data found (Content/Story/story.json)."));
	}
	else if (bComplete)
	{
		ProgressText->SetText(FText::Format(LOCTEXT("Complete", "Story complete - all {0} chapters. Replay any chapter or the finale."), FText::AsNumber(NumChapters)));
	}
	else
	{
		ProgressText->SetText(ChapterReached >= NumChapters ? LOCTEXT("FinaleNext", "Finale unlocked") : FText::Format(LOCTEXT("Progress", "Chapter {0} of {1}"), FText::AsNumber(ChapterReached + 1), FText::AsNumber(NumChapters)));
	}
	ContinueLabel->SetText(bComplete ? LOCTEXT("ReplayFinale", "REPLAY FINALE") : (bPrologueSeen ? LOCTEXT("Continue", "CONTINUE") : LOCTEXT("Begin", "BEGIN")));
	SelectedChapter = ChapterReached;
	RefreshChapter();

	SideDone = Save ? Save->SideMissionsDone : TArray<FName>();
	CodexGranted = Save ? Save->CodexGranted : TArray<FName>();
	RewardsEarned.Reset();
	if (Save) { RewardsEarned.Append(Save->Upgrades); RewardsEarned.Append(Save->Cosmetics); }
	// First available, not yet done mission.
	SelectedSide = 0;
	for (int32 i = 0; i < Story.SideMissions.Num(); ++i)
	{
		if (UStoryLibrary::IsSideMissionAvailable(Story, Story.SideMissions[i], ChapterReached, bComplete) && !SideDone.Contains(Story.SideMissions[i].Id)) { SelectedSide = i; break; }
	}
	RefreshSide();

	// Codex: unlocked entries grouped by category (codex.json order inside a category).
	CodexUnlocked.Reset();
	TArray<FName> Categories;
	for (const FStoryCodexEntry& E : Story.Codex) { Categories.AddUnique(E.Category); }
	for (const FName& Cat : Categories)
	{
		for (int32 i = 0; i < Story.Codex.Num(); ++i)
		{
			const FStoryCodexEntry& E = Story.Codex[i];
			if (E.Category == Cat && UStoryLibrary::IsCodexUnlocked(Story, E, ChapterReached, bComplete, bPrologueSeen, CodexGranted)) { CodexUnlocked.Add(i); }
		}
	}
	SelectedCodex = FMath::Clamp(SelectedCodex, 0, FMath::Max(0, CodexUnlocked.Num() - 1));
	RefreshCodex();
}

void UStoryMenuWidget::RefreshSide()
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	if (!SideText || Story.SideMissions.Num() == 0) { return; }
	SelectedSide = (SelectedSide % Story.SideMissions.Num() + Story.SideMissions.Num()) % Story.SideMissions.Num();
	const FStoryChapter& M = Story.SideMissions[SelectedSide];
	const bool bAvail = UStoryLibrary::IsSideMissionAvailable(Story, M, ChapterReached, bComplete);
	const bool bDone = SideDone.Contains(M.Id);
	SideText->SetText(FText::Format(LOCTEXT("SideFmt", "{0}{1}"), FText::FromString(M.Title), bDone ? LOCTEXT("SideDoneTag", "  [DONE]") : FText::GetEmpty()));

	auto ChapterLabel = [&Story](FName Id) -> FString
	{
		const int32 I = Story.FindChapter(Id);
		return Story.Chapters.IsValidIndex(I) ? FString::Printf(TEXT("chapter %d"), I + 1) : Id.ToString();
	};
	FString Reward;
	if (!M.RewardUpgrade.IsNone()) { Reward = FString::Printf(TEXT("upgrade %s"), *M.RewardUpgrade.ToString()); }
	if (!M.RewardCosmetic.IsNone()) { Reward = FString::Printf(TEXT("cosmetic %s"), *M.RewardCosmetic.ToString()); }
	if (!M.RewardCodex.IsNone()) { Reward += TEXT(" + codex entry"); }
	FString Window;
	if (!bAvail)
	{
		const int32 From = UStoryLibrary::ChapterOrder(Story, M.AvailableFrom);
		Window = ChapterReached < From ? FString::Printf(TEXT("LOCKED - reach %s"), *ChapterLabel(M.AvailableFrom))
			: FString::Printf(TEXT("CLOSED after %s - finish the story to replay"), *ChapterLabel(M.AvailableUntil));
	}
	else
	{
		Window = FString::Printf(TEXT("%s  |  %d objectives"), *M.Location.ToString(), M.Objectives.Num());
	}
	SideInfoText->SetText(FText::FromString(FString::Printf(TEXT("%s\nReward: %s"), *Window, *Reward)));
	if (SidePlayButton) { SidePlayButton->SetIsEnabled(bAvail); }
}

void UStoryMenuWidget::RefreshCodex()
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	if (!CodexPanel) { return; }
	CodexPanel->SetVisibility(bCodexOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!CodexTitle) { return; }
	if (CodexUnlocked.Num() == 0)
	{
		CodexHeader->SetText(FText::Format(LOCTEXT("CodexNone", "0 of {0} entries unlocked"), FText::AsNumber(Story.Codex.Num())));
		CodexTitle->SetText(LOCTEXT("CodexEmpty", "NOTHING YET"));
		CodexBody->SetText(LOCTEXT("CodexEmptyBody", "Finish chapters and side missions to unlock archive entries."));
	}
	else
	{
		SelectedCodex = (SelectedCodex % CodexUnlocked.Num() + CodexUnlocked.Num()) % CodexUnlocked.Num();
		const FStoryCodexEntry& E = Story.Codex[CodexUnlocked[SelectedCodex]];
		CodexHeader->SetText(FText::Format(LOCTEXT("CodexCount", "{0}  |  entry {1} of {2} unlocked  ({3} total)"),
			FText::FromString(E.Category.ToString().ToUpper()), FText::AsNumber(SelectedCodex + 1), FText::AsNumber(CodexUnlocked.Num()), FText::AsNumber(Story.Codex.Num())));
		CodexTitle->SetText(FText::FromString(E.Title.ToUpper()));
		FString Body = UStoryLibrary::ResolveTokens(E.Body, NameBox ? NameBox->GetText().ToString() : DefaultName, FString());
		if (E.Suits.Num() > 0)
		{
			TArray<FString> Names;
			for (const FName& Id : E.Suits)
			{
				const int32 Idx = USuitLibrary::FindSuitIndex(Id);
				Names.Add(Idx != INDEX_NONE ? USuitLibrary::GetSuit(Idx).Name.ToString() : Id.ToString());
			}
			Body += TEXT("\n\nSuits: ") + FString::Join(Names, TEXT(", "));
		}
		CodexBody->SetText(FText::FromString(Body));
	}
	if (CodexRewards)
	{
		TArray<FString> R;
		for (const FName& Id : RewardsEarned) { R.Add(Id.ToString()); }
		CodexRewards->SetText(R.Num() > 0 ? FText::FromString(TEXT("Rewards earned: ") + FString::Join(R, TEXT(", "))) : FText::GetEmpty());
	}
}

void UStoryMenuWidget::HandlePrevSide() { --SelectedSide; RefreshSide(); }
void UStoryMenuWidget::HandleNextSide() { ++SelectedSide; RefreshSide(); }

void UStoryMenuWidget::HandlePlaySide()
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	if (!Story.SideMissions.IsValidIndex(SelectedSide)) { return; }
	CommitName();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuStartSideMission(Story.SideMissions[SelectedSide].Id); }
}

void UStoryMenuWidget::HandleOpenCodex() { bCodexOpen = true; RefreshCodex(); }
void UStoryMenuWidget::HandleCloseCodex() { bCodexOpen = false; RefreshCodex(); }
void UStoryMenuWidget::HandlePrevCodex() { --SelectedCodex; RefreshCodex(); }
void UStoryMenuWidget::HandleNextCodex() { ++SelectedCodex; RefreshCodex(); }

void UStoryMenuWidget::RefreshChapter()
{
	const FStoryBook& Story = UStoryLibrary::GetStory();
	const int32 MaxSel = bComplete ? Story.Chapters.Num() : FMath::Min(ChapterReached, Story.Chapters.Num()); // index Num = finale
	SelectedChapter = FMath::Clamp(SelectedChapter, 0, FMath::Max(0, MaxSel));
	if (!ChapterText) { return; }
	if (Story.Chapters.IsValidIndex(SelectedChapter))
	{
		const FStoryChapter& C = Story.Chapters[SelectedChapter];
		ChapterText->SetText(FText::Format(LOCTEXT("ChapterFmt", "{0}. {1}"), FText::AsNumber(SelectedChapter + 1), FText::FromString(C.Title)));
		const int32 SuitIdx = C.UnlockSuit.IsNone() ? INDEX_NONE : USuitLibrary::FindSuitIndex(C.UnlockSuit);
		const FText Info = SuitIdx != INDEX_NONE
			? FText::Format(LOCTEXT("ChapterSuit", "{0} objectives  |  suit: {1}"), FText::AsNumber(C.Objectives.Num()), USuitLibrary::GetSuit(SuitIdx).Name)
			: FText::Format(LOCTEXT("ChapterNoSuit", "{0} objectives"), FText::AsNumber(C.Objectives.Num()));
		ChapterInfoText->SetText(C.Lore.IsEmpty() ? Info
			: FText::Format(LOCTEXT("ChapterLore", "{0}\n{1}"), Info, FText::FromString(C.Lore)));
	}
	else
	{
		ChapterText->SetText(LOCTEXT("Finale", "FINALE"));
		ChapterInfoText->SetText(LOCTEXT("FinaleInfo", "The last stand - with Morgan in Rescue."));
	}
}

void UStoryMenuWidget::CommitName()
{
	const FString Name = ArenaUI::SanitizeName(NameBox ? NameBox->GetText().ToString() : DefaultName, DefaultName, 20);
	if (NameBox) { NameBox->SetText(FText::FromString(Name)); }
	const UWorld* World = GetWorld();
	const AArenaGameMode* GM = World ? World->GetAuthGameMode<AArenaGameMode>() : nullptr;
	const FString SaveSlot = GM ? GM->SaveSlotName : FString(TEXT("IronManArena"));
	if (UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(SaveSlot))
	{
		if (Save->PlayerName != Name) { Save->PlayerName = Name; Save->Write(SaveSlot); }
	}
}

void UStoryMenuWidget::Launch(int32 Chapter, bool bPrologue)
{
	CommitName();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuStartStory(Chapter, bPrologue); }
}

void UStoryMenuWidget::HandleContinue()
{
	const int32 NumChapters = UStoryLibrary::GetStory().Chapters.Num();
	if (bComplete || ChapterReached >= NumChapters) { Launch(NumChapters, false); return; }
	Launch(ChapterReached, !bPrologueSeen && ChapterReached == 0);
}

void UStoryMenuWidget::HandleNewStory() { Launch(0, true); }
void UStoryMenuWidget::HandlePrevChapter() { --SelectedChapter; RefreshChapter(); }
void UStoryMenuWidget::HandleNextChapter() { ++SelectedChapter; RefreshChapter(); }
void UStoryMenuWidget::HandlePlayChapter() { Launch(SelectedChapter, false); }

void UStoryMenuWidget::HandleBack()
{
	CommitName();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuBack(); }
}

FReply UStoryMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer()))
	{
		if (PC->HandleGlobalKey(InKeyEvent)) { return FReply::Handled(); }
	}
	const FKey Key = InKeyEvent.GetKey();
	if (bCodexOpen)
	{
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) { HandleCloseCodex(); return FReply::Handled(); }
		if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) { HandlePrevCodex(); return FReply::Handled(); }
		if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) { HandleNextCodex(); return FReply::Handled(); }
		return FReply::Handled();
	}
	if (Key == EKeys::C || Key == EKeys::Gamepad_Special_Left) { HandleOpenCodex(); return FReply::Handled(); }
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) { HandlePrevSide(); return FReply::Handled(); }
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) { HandleNextSide(); return FReply::Handled(); }
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) { HandleBack(); return FReply::Handled(); }
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) { HandlePrevChapter(); return FReply::Handled(); }
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) { HandleNextChapter(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
