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
#include "Components/ButtonSlot.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "ArenaAudio.h"
#include "CareerWidgets.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"

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
	// Holo-HUD style (research: Cantina cyan HUD): flat, transparent plate, cyan hairline on hover, left aligned.
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle St = B->GetStyle();
	auto Brush = [](const FLinearColor& C) { FSlateBrush Br; Br.DrawAs = ESlateBrushDrawType::RoundedBox; Br.TintColor = FSlateColor(C);
		Br.OutlineSettings = FSlateBrushOutlineSettings(FVector4(2.f, 2.f, 2.f, 2.f), FSlateColor(FLinearColor(0.35f, 0.9f, 1.f, C.A > 0.1f ? 0.9f : 0.18f)), 1.f); return Br; };
	St.SetNormal(Brush(FLinearColor(0.02f, 0.06f, 0.09f, 0.05f)));
	St.SetHovered(Brush(FLinearColor(0.1f, 0.45f, 0.6f, 0.35f)));
	St.SetPressed(Brush(FLinearColor(0.7f, 0.12f, 0.08f, 0.6f)));
	St.SetNormalPadding(FMargin(18.f, 6.f)); St.SetPressedPadding(FMargin(20.f, 7.f, 16.f, 5.f));
	B->SetStyle(St);
	B->SetBackgroundColor(FLinearColor::White);
	OutLabel = MakeText(WidgetTree, Label, 22, FLinearColor(0.78f, 0.93f, 1.f));
	OutLabel->SetJustification(ETextJustify::Left);
	if (UButtonSlot* BS = Cast<UButtonSlot>(B->SetContent(OutLabel))) { BS->SetHorizontalAlignment(HAlign_Left); BS->SetVerticalAlignment(VAlign_Center); }
	B->OnHovered.AddDynamic(this, &UMainMenuWidget::HandleHoverChanged);
	B->OnUnhovered.AddDynamic(this, &UMainMenuWidget::HandleHoverChanged);
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(B))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetPadding(FMargin(0.f, 3.f));
	}
	Buttons.Add(TPair<TObjectPtr<UButton>, TObjectPtr<UTextBlock>>(B, OutLabel));
	ButtonCaptions.Add(Label.ToString());
	return B;
}

void UMainMenuWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Dim the game behind the menu.
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.01f, 0.03f, 0.28f)); // the city flyover stays visible behind the menu
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// Left holo panel (dark glass strip + cyan edge line), menu column on top of it.
	UBorder* Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Strip"));
	Strip->SetBrushColor(FLinearColor(0.f, 0.03f, 0.06f, 0.62f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Strip)) { S->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f)); S->SetOffsets(FMargin(0.f, 0.f, 560.f, 0.f)); }
	UBorder* Edge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Edge"));
	Edge->SetBrushColor(FLinearColor(0.35f, 0.9f, 1.f, 0.55f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Edge)) { S->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f)); S->SetOffsets(FMargin(560.f, 0.f, 2.f, 0.f)); }

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Box))
	{
		S->SetAnchors(FAnchors(0.f, 0.5f));
		S->SetAlignment(FVector2D(0.f, 0.5f));
		S->SetPosition(FVector2D(70.f, 0.f));
		S->SetSize(FVector2D(440.f, 600.f));
		S->SetAutoSize(true);
	}
	MenuBox = Box;

	auto AddRow = [Box](UWidget* W, float PadBottom)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom));
		}
	};

	TitleText = MakeText(WidgetTree, LOCTEXT("Title", "IRON MAN: ARENA"), 54, FLinearColor(0.69f, 0.07f, 0.15f)); // #B11226 candy red
	TitleText->SetJustification(ETextJustify::Left);
	AddRow(TitleText, 2.f);
	SubtitleText = MakeText(WidgetTree, FText::GetEmpty(), 16, FLinearColor(0.77f, 0.63f, 0.16f), "Regular"); // #C5A028 gold
	SubtitleText->SetJustification(ETextJustify::Left);
	SubtitleText->SetAutoWrapText(true);
	AddRow(SubtitleText, 10.f);
	ScoreText = MakeText(WidgetTree, FText::GetEmpty(), 18, FLinearColor(0.6f, 0.9f, 1.f));
	ScoreText->SetJustification(ETextJustify::Left);
	AddRow(ScoreText, 22.f);

	UTextBlock* Tmp = nullptr;
	ResumeButton = MakeButton(Box, LOCTEXT("Resume", "RESUME"), Tmp); ResumeLabel = Tmp;
	StartButton = MakeButton(Box, LOCTEXT("Arena", "ARENA"), Tmp); StartLabel = Tmp;
	StoryButton = MakeButton(Box, LOCTEXT("Story", "STORY"), Tmp); StoryLabel = Tmp;
	SettingsButton = MakeButton(Box, LOCTEXT("Settings", "SETTINGS"), Tmp); SettingsLabel = Tmp;
	TitleButton = MakeButton(Box, LOCTEXT("ToTitle", "QUIT TO TITLE"), Tmp); TitleLabel = Tmp;
	CreditsButton = MakeButton(Box, LOCTEXT("Credits", "CREDITS"), Tmp); CreditsLabel = Tmp;
	QuitButton = MakeButton(Box, LOCTEXT("Quit", "QUIT GAME"), Tmp); QuitLabel = Tmp;
	CreditsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCreditsClicked);

	ResumeButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleResumeClicked);
	StartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStartClicked);
	StoryButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStoryClicked);
	SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettingsClicked);
	TitleButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleTitleClicked);
	QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);

	ControlsText = MakeText(WidgetTree, LOCTEXT("Controls",
		"WASD move  |  Mouse aim  |  F flight  |  Space / Ctrl up / down  |  Shift boost  |  V first / third person  |  Alt focus aim\n"
		"LMB repulsors (hold in focus = charged shot)  |  RMB special  |  hold RMB / double-click = SUPER  |  X vent  |  Q Hulkbuster\n"
		"O exit suit / call suit  |  H faceplate  |  B web swing (Iron Spider)  |  E interact  |  U shop  |  N phone  |  M map\n"
		"1-5 quick suits  |  hold T suit wheel  |  G summon ally  |  P / Esc pause  |  F11 fullscreen"),
		12, FLinearColor(0.6f, 0.8f, 0.95f, 0.85f), "Regular");
	ControlsText->SetJustification(ETextJustify::Left);
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(ControlsText))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));
	}

	// Credits overlay (CC-BY attributions are required in shipping builds: contrib/audio/LICENSES.md).
	CreditsPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CreditsPanel"));
	CreditsPanel->SetBrushColor(FLinearColor(0.f, 0.02f, 0.04f, 0.92f));
	CreditsPanel->SetPadding(FMargin(40.f, 30.f));
	{
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UTextBlock* T = MakeText(WidgetTree, LOCTEXT("CreditsBody",
			"IRON MAN: ARENA  -  a fan project by Axel\n\n"
			"AUDIO  (CC-BY - attribution required)\n"
			"Facility alert siren - OpenGameArt.org - CC-BY 4.0 - opengameart.org/content/facility-alert-siren\n"
			"Aargh male screams - OpenGameArt.org - CC-BY 3.0 - opengameart.org/content/aargh-male-screams\n"
			"Female screams - OpenGameArt.org - CC-BY 3.0 - opengameart.org/content/female-screams\n\n"
			"AUDIO  (CC0 - thank you)\n"
			"Kenney.nl - Sci-Fi Sounds, UI Audio, Interface Sounds, Casino Audio\n"
			"messersm - Space Sound Effects  |  GGBotNet - Car Sound Effects Pack  |  Fantozzi - Footsteps\n"
			"rubberduck - 60 CC0 Sci-Fi SFX  |  Independent.nu - breaking / falling / hit SFX\n"
			"OpenGameArt.org CC0 packs - metal footsteps, glass break, sirens, battle, inventory, coins, Menu Chill Music, Menu Loop\n"
			"Procedural city ambience, birds and tyre screech - generated for this project (CC0)\n\n"
			"CHARACTERS & ANIMATION\n"
			"Unreal Engine Mannequins (Manny / Quinn) and the Unreal Animation Library - Epic Games\n\n"
			"Built with Unreal Engine. Iron Man and related characters are property of Marvel; this is a non-commercial fan game.\n\n"
			"Press Esc or click CREDITS again to close."),
			15, FLinearColor(0.8f, 0.93f, 1.f), "Regular");
		T->SetJustification(ETextJustify::Left);
		T->SetAutoWrapText(true);
		Scroll->AddChild(T);
		CreditsPanel->SetContent(Scroll);
	}
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(CreditsPanel))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(640.f, 90.f, 80.f, 90.f));
	}
	CreditsPanel->SetVisibility(ESlateVisibility::Collapsed);

	RefreshTexts();
}

void UMainMenuWidget::HandleAvatarClicked()
{
	// Character creator (CUSTOMIZE AVATAR / K) is disabled.
}

void UMainMenuWidget::HandleCreditsClicked()
{
	Click();
	if (CreditsPanel) { CreditsPanel->SetVisibility(CreditsPanel->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
}

void UMainMenuWidget::HandleHoverChanged()
{
	UButton* Now = nullptr;
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		UButton* B = Buttons[i].Key;
		UTextBlock* L = Buttons[i].Value;
		const bool bH = B && B->IsHovered();
		if (bH) { Now = B; }
		if (L) { L->SetColorAndOpacity(FSlateColor(bH ? FLinearColor(1.f, 0.9f, 0.55f) : FLinearColor(0.78f, 0.93f, 1.f))); }
	}
	if (Now && Now != LastHovered.Get()) { ArenaAudio::Play2D(this, TEXT("ui_hover"), 0.5f); }
	LastHovered = Now;
}

void UMainMenuWidget::Click() { ArenaAudio::Play2D(this, TEXT("ui_click"), 0.8f); }

void UMainMenuWidget::UpdateMusic()
{
	const bool bWant = Mode == EMenuMode::Title && IsInViewport();
	if (bWant && !Music) { Music = ArenaAudio::Loop2D(this, TEXT("menu_music_loop"), 0.55f); if (Music) { Music->FadeIn(1.5f, 0.55f); } }
	else if (!bWant && Music) { Music->FadeOut(0.8f, 0.f); Music = nullptr; }
}

void UMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Intro: the column slides in from the left with a fade (first 0.5 s each time the menu shows).
	IntroT = FMath::Min(1.f, IntroT + InDeltaTime / 0.5f);
	const float E = FMath::InterpEaseOut(0.f, 1.f, IntroT, 3.f);
	if (MenuBox) { MenuBox->SetRenderTranslation(FVector2D(-60.f * (1.f - E), 0.f)); MenuBox->SetRenderOpacity(E); }
	// Hovered button nudges right (and back) smoothly.
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		if (UButton* B = Buttons[i].Key)
		{
			const float Want = B->IsHovered() ? 14.f : 0.f;
			const FVector2D Cur = B->GetRenderTransform().Translation;
			B->SetRenderTranslation(FVector2D(FMath::FInterpTo(float(Cur.X), Want, InDeltaTime, 14.f), 0.f));
		}
	}
	UpdateMusic();
}

void UMainMenuWidget::NativeDestruct()
{
	if (Music) { Music->FadeOut(0.5f, 0.f); Music = nullptr; }
	Super::NativeDestruct();
}

void UMainMenuWidget::SetMode(EMenuMode InMode, const AArenaGameMode* GM)
{
	if (Mode != InMode) { IntroT = 0.f; }
	Mode = InMode;
	if (CreditsPanel) { CreditsPanel->SetVisibility(ESlateVisibility::Collapsed); }
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
	if (AvatarButton) { AvatarButton->SetVisibility(Hidden); }
	StoryButton->SetVisibility(Mode == EMenuMode::Title ? Shown : Hidden);
	SettingsButton->SetVisibility(Mode != EMenuMode::GameOver ? Shown : Hidden);
	TitleButton->SetVisibility(Mode != EMenuMode::Title ? Shown : Hidden);
}

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer());
	if (PC && PC->HandleGlobalKey(InKeyEvent)) { return FReply::Handled(); }

	const FKey Key = InKeyEvent.GetKey();
	if (CreditsPanel && CreditsPanel->GetVisibility() != ESlateVisibility::Collapsed && (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::BackSpace))
	{
		CreditsPanel->SetVisibility(ESlateVisibility::Collapsed);
		ArenaAudio::Play2D(this, TEXT("ui_back"), 0.7f);
		return FReply::Handled();
	}
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
	Click();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuStartGame(); }
}

void UMainMenuWidget::HandleStoryClicked()
{
	Click();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuOpenStory(); }
}

void UMainMenuWidget::HandleResumeClicked()
{
	Click();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuResume(); }
}

void UMainMenuWidget::HandleSettingsClicked()
{
	Click();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuOpenSettings(); }
}

void UMainMenuWidget::HandleTitleClicked()
{
	Click();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuQuitToTitle(); }
}

void UMainMenuWidget::HandleQuitClicked()
{
	Click();
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuQuit(); }
}

#undef LOCTEXT_NAMESPACE
