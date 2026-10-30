#include "MainMenuWidget.h"

#include "ArenaPlayerController.h"
#include "ArenaGameMode.h"
#include "StoryDirector.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "MainMenuWidget"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color, const char* Weight = "Bold")
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(Text);
		T->SetFont(FCoreStyle::GetDefaultFontStyle(Weight, Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		T->SetShadowOffset(FVector2D(2.f, 2.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		return T;
	}
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UButton* UMainMenuWidget::MakeButton(UVerticalBox* Box, const FText& Label, UTextBlock*& OutLabel)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	B->SetBackgroundColor(FLinearColor(0.55f, 0.06f, 0.05f, 1.f));
	OutLabel = MakeText(WidgetTree, Label, 22, FLinearColor(1.f, 0.85f, 0.4f));
	B->SetContent(OutLabel);
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(B))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 6.f));
	}
	return B;
}

void UMainMenuWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Dim the game behind the menu.
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.02f, 0.72f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Box))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	auto AddRow = [Box](UWidget* W, float PadBottom)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom));
		}
	};

	TitleText = MakeText(WidgetTree, LOCTEXT("Title", "IRON MAN: ARENA"), 56, FLinearColor(0.95f, 0.15f, 0.1f));
	AddRow(TitleText, 4.f);
	SubtitleText = MakeText(WidgetTree, FText::GetEmpty(), 20, FLinearColor(1.f, 0.8f, 0.35f), "Regular");
	AddRow(SubtitleText, 14.f);
	ScoreText = MakeText(WidgetTree, FText::GetEmpty(), 22, FLinearColor::White);
	AddRow(ScoreText, 18.f);

	UTextBlock* Tmp = nullptr;
	ResumeButton = MakeButton(Box, LOCTEXT("Resume", "RESUME"), Tmp); ResumeLabel = Tmp;
	StartButton = MakeButton(Box, LOCTEXT("Arena", "ARENA"), Tmp); StartLabel = Tmp;
	StoryButton = MakeButton(Box, LOCTEXT("Story", "STORY"), Tmp); StoryLabel = Tmp;
	SettingsButton = MakeButton(Box, LOCTEXT("Settings", "SETTINGS"), Tmp); SettingsLabel = Tmp;
	TitleButton = MakeButton(Box, LOCTEXT("ToTitle", "QUIT TO TITLE"), Tmp); TitleLabel = Tmp;
	QuitButton = MakeButton(Box, LOCTEXT("Quit", "QUIT GAME"), Tmp); QuitLabel = Tmp;

	ResumeButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleResumeClicked);
	StartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStartClicked);
	StoryButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStoryClicked);
	SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettingsClicked);
	TitleButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleTitleClicked);
	QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);

	ControlsText = MakeText(WidgetTree, LOCTEXT("Controls",
		"WASD move  |  Mouse aim  |  F flight  |  Space / Ctrl up / down  |  Shift boost  |  V first / third person\n"
		"LMB repulsors  |  RMB special  |  X vent reactor heat  |  Q Hulkbuster  |  1-5 quick suits  |  hold T suit wheel (wheel / Tab = page)\n"
		"Story: G summon ally when the HUD offers it  |  Space / click skip a shot, Esc skips the cutscene  |  P / Esc pause  |  F11 fullscreen"),
		13, FLinearColor(0.75f, 0.85f, 1.f), "Regular");
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(ControlsText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));
	}

	RefreshTexts();
}

void UMainMenuWidget::SetMode(EMenuMode InMode, const AArenaGameMode* GM)
{
	Mode = InMode;
	if (GM)
	{
		LastScore = GM->GetScore();
		HighScore = GM->GetHighScore();
		LastWave = GM->GetWave();
		bNewHigh = GM->IsNewHighScore();
		bStoryMode = GM->GetPlayMode() == EArenaPlayMode::Story && Mode != EMenuMode::Title;
		StoryChapterTitle = FText::GetEmpty();
		if (bStoryMode)
		{
			FText Chapter, Objective, Progress;
			if (const AStoryDirector* Director = GM->GetStoryDirector())
			{
				Director->GetObjectiveInfo(Chapter, Objective, Progress);
				StoryChapterTitle = Chapter;
			}
		}
	}
	else
	{
		bStoryMode = false;
	}
	RefreshTexts();
}

void UMainMenuWidget::RefreshTexts()
{
	if (!TitleText) { return; } // tree not built yet; RebuildWidget will call us

	const ESlateVisibility Shown = ESlateVisibility::Visible;
	const ESlateVisibility Hidden = ESlateVisibility::Collapsed;
	switch (Mode)
	{
	case EMenuMode::Title:
		TitleText->SetText(LOCTEXT("Title", "IRON MAN: ARENA"));
		SubtitleText->SetText(LOCTEXT("TitleSub", "Story: Harley's legacy.  Arena: survive the waves - every 4th brings a Titan."));
		ScoreText->SetText(FText::Format(LOCTEXT("High", "HIGH SCORE  {0}"), FText::AsNumber(HighScore)));
		StartLabel->SetText(LOCTEXT("Arena", "ARENA"));
		break;
	case EMenuMode::Paused:
		TitleText->SetText(LOCTEXT("Paused", "PAUSED"));
		SubtitleText->SetText(bStoryMode ? StoryChapterTitle : FText::Format(LOCTEXT("PausedSub", "Wave {0}"), FText::AsNumber(LastWave)));
		ScoreText->SetText(FText::Format(LOCTEXT("ScoreNow", "SCORE  {0}     HIGH  {1}"), FText::AsNumber(LastScore), FText::AsNumber(HighScore)));
		StartLabel->SetText(bStoryMode ? LOCTEXT("RestartChapter", "RESTART CHAPTER") : LOCTEXT("Restart", "RESTART"));
		break;
	case EMenuMode::GameOver:
		TitleText->SetText(LOCTEXT("GameOver", "SUIT OFFLINE"));
		if (bStoryMode)
		{
			SubtitleText->SetText(StoryChapterTitle);
			ScoreText->SetText(LOCTEXT("StoryOver", "Morgan has the suit rebooted. Ready when you are."));
			StartLabel->SetText(LOCTEXT("RetryChapter", "RETRY CHAPTER"));
		}
		else
		{
			SubtitleText->SetText(bNewHigh
				? LOCTEXT("NewHighSub", "NEW HIGH SCORE!")
				: FText::Format(LOCTEXT("GameOverSub", "You reached wave {0}"), FText::AsNumber(LastWave)));
			ScoreText->SetText(FText::Format(LOCTEXT("ScoreFinal", "SCORE  {0}     HIGH  {1}"), FText::AsNumber(LastScore), FText::AsNumber(HighScore)));
			StartLabel->SetText(LOCTEXT("Again", "PLAY AGAIN"));
		}
		break;
	}
	ResumeButton->SetVisibility(Mode == EMenuMode::Paused ? Shown : Hidden);
	StoryButton->SetVisibility(Mode == EMenuMode::Title ? Shown : Hidden);
	SettingsButton->SetVisibility(Mode != EMenuMode::GameOver ? Shown : Hidden);
	TitleButton->SetVisibility(Mode != EMenuMode::Title ? Shown : Hidden);
}

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer());
	if (PC && PC->HandleGlobalKey(InKeyEvent)) { return FReply::Handled(); }

	const FKey Key = InKeyEvent.GetKey();
	if (Mode == EMenuMode::Paused && (Key == EKeys::P || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right))
	{
		HandleResumeClicked();
		return FReply::Handled();
	}
	if (Mode == EMenuMode::GameOver && (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_Special_Right))
	{
		HandleStartClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMainMenuWidget::HandleStartClicked()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuStartGame(); }
}

void UMainMenuWidget::HandleStoryClicked()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuOpenStory(); }
}

void UMainMenuWidget::HandleResumeClicked()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuResume(); }
}

void UMainMenuWidget::HandleSettingsClicked()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuOpenSettings(); }
}

void UMainMenuWidget::HandleTitleClicked()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuQuitToTitle(); }
}

void UMainMenuWidget::HandleQuitClicked()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuQuit(); }
}

#undef LOCTEXT_NAMESPACE
