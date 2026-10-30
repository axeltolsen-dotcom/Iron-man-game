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
	UButton* Back = ArenaUI::MakeButton(WidgetTree, LOCTEXT("Back", "BACK"), 20, L);
	Play->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandlePlayChapter);
	Back->OnClicked.AddDynamic(this, &UStoryMenuWidget::HandleBack);
	AddButtonRow({ Play, Back });

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
}

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
	const FString Slot = GM ? GM->SaveSlotName : FString(TEXT("IronManArena"));
	if (UArenaSaveGame* Save = UArenaSaveGame::LoadOrCreate(Slot))
	{
		if (Save->PlayerName != Name) { Save->PlayerName = Name; Save->Write(Slot); }
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
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) { HandleBack(); return FReply::Handled(); }
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) { HandlePrevChapter(); return FReply::Handled(); }
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) { HandleNextChapter(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
