#include "ArenaPlayerController.h"

#include "ArenaHUDWidget.h"
#include "MainMenuWidget.h"
#include "SuitSelectWidget.h"
#include "SettingsWidget.h"
#include "StoryMenuWidget.h"
#include "ArenaGameMode.h"
#include "ArenaGameUserSettings.h"
#include "JarvisComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

AArenaPlayerController::AArenaPlayerController()
{
	HUDWidgetClass = UArenaHUDWidget::StaticClass();
	MenuWidgetClass = UMainMenuWidget::StaticClass();
	SuitSelectWidgetClass = USuitSelectWidget::StaticClass();
	SettingsWidgetClass = USettingsWidget::StaticClass();
	StoryMenuWidgetClass = UStoryMenuWidget::StaticClass();
	Jarvis = CreateDefaultSubobject<UJarvisComponent>(TEXT("Jarvis"));
	SetShowMouseCursor(false);
}

void AArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController()) { return; }

	if (*HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UArenaHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget) { HUDWidget->AddToViewport(0); }
	}
	if (*MenuWidgetClass) { MenuWidget = CreateWidget<UMainMenuWidget>(this, MenuWidgetClass); }

	// Volumes, FOV and mouse settings from GameUserSettings.ini. The pawn may be possessed a
	// frame later, so apply again on the next tick (FOV / sensitivity live on the character).
	if (UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings())
	{
		S->ApplyGameplaySettings(this);
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (UArenaGameUserSettings* Settings = UArenaGameUserSettings::GetArenaSettings()) { Settings->ApplyGameplaySettings(this); }
		}));
	}

	ShowMenu(EMenuMode::Title);
}

void AArenaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// In game input the keys never reach a widget, so check them here (the engine's own
	// F11 / Alt+Enter handling is disabled in DefaultInput.ini so it cannot double-toggle).
	if (WasInputKeyJustPressed(EKeys::F11)
		|| (WasInputKeyJustPressed(EKeys::Enter) && (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))))
	{
		ToggleFullscreen();
	}
}

void AArenaPlayerController::ToggleFullscreen()
{
	if (UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings()) { S->ToggleFullscreen(); }
}

bool AArenaPlayerController::HandleGlobalKey(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::F11 || (Key == EKeys::Enter && InKeyEvent.IsAltDown()))
	{
		if (!InKeyEvent.IsRepeat()) { ToggleFullscreen(); }
		return true;
	}
	return false;
}

bool AArenaPlayerController::IsMenuOpen() const
{
	return (MenuWidget && MenuWidget->IsInViewport()) || (SubScreen && SubScreen->IsInViewport());
}

void AArenaPlayerController::PresentScreen(UUserWidget* Widget, int32 ZOrder)
{
	if (!Widget) { return; }
	if (!Widget->IsInViewport()) { Widget->AddToViewport(ZOrder); }

	SetPause(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AArenaPlayerController::RemoveSubScreen()
{
	if (SubScreen && SubScreen->IsInViewport()) { SubScreen->RemoveFromParent(); }
	SubScreen = nullptr;
}

void AArenaPlayerController::ShowMenu(EMenuMode Mode)
{
	if (!MenuWidget) { return; }
	RemoveSubScreen();
	LastMenuMode = Mode;
	const AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
	MenuWidget->SetMode(Mode, GM);
	PresentScreen(MenuWidget, 10);
}

void AArenaPlayerController::HideMenu()
{
	RemoveSubScreen();
	if (MenuWidget && MenuWidget->IsInViewport()) { MenuWidget->RemoveFromParent(); }
	SetPause(false);
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	FlushPressedKeys();
}

void AArenaPlayerController::TogglePauseMenu()
{
	const AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
	if (!GM || GM->GetArenaState() != EArenaGameState::Playing) { return; }

	if (IsMenuOpen()) { HideMenu(); }
	else { ShowMenu(EMenuMode::Paused); }
}

void AArenaPlayerController::MenuStartGame()
{
	if (LastMenuMode == EMenuMode::Title) { MenuOpenSuitSelect(); }
	else { MenuRetry(); }
}

void AArenaPlayerController::MenuOpenSuitSelect()
{
	if (!SuitSelectWidget && *SuitSelectWidgetClass) { SuitSelectWidget = CreateWidget<USuitSelectWidget>(this, SuitSelectWidgetClass); }
	if (!SuitSelectWidget)
	{
		// No suit select available: straight into the Arena with the last suit.
		HideMenu();
		if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr) { GM->StartNewGame(); }
		return;
	}
	const AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
	SuitSelectWidget->OpenAt(GM ? GM->GetSelectedSuitIndex() : 0);
	if (MenuWidget && MenuWidget->IsInViewport()) { MenuWidget->RemoveFromParent(); }
	RemoveSubScreen();
	SubScreen = SuitSelectWidget;
	PresentScreen(SuitSelectWidget, 11);
}

void AArenaPlayerController::MenuConfirmSuit(int32 SuitIndex)
{
	HideMenu();
	if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		GM->SetSelectedSuitIndex(SuitIndex);
		GM->StartNewGame(SuitIndex);
	}
}

void AArenaPlayerController::MenuOpenStory()
{
	if (!StoryMenuWidget && *StoryMenuWidgetClass) { StoryMenuWidget = CreateWidget<UStoryMenuWidget>(this, StoryMenuWidgetClass); }
	if (!StoryMenuWidget) { return; }
	StoryMenuWidget->RefreshFromSave();
	if (MenuWidget && MenuWidget->IsInViewport()) { MenuWidget->RemoveFromParent(); }
	RemoveSubScreen();
	SubScreen = StoryMenuWidget;
	PresentScreen(StoryMenuWidget, 11);
}

void AArenaPlayerController::MenuStartStory(int32 ChapterIndex, bool bPlayPrologue)
{
	HideMenu();
	if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		GM->StartStory(FMath::Max(0, ChapterIndex), bPlayPrologue);
	}
}

void AArenaPlayerController::MenuStartSideMission(FName MissionId)
{
	HideMenu();
	if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		GM->StartSideMission(MissionId);
	}
}

void AArenaPlayerController::MenuOpenSettings()
{
	if (!SettingsWidget && *SettingsWidgetClass) { SettingsWidget = CreateWidget<USettingsWidget>(this, SettingsWidgetClass); }
	if (!SettingsWidget) { return; }
	SettingsWidget->RefreshFromSettings();
	if (MenuWidget && MenuWidget->IsInViewport()) { MenuWidget->RemoveFromParent(); }
	RemoveSubScreen();
	SubScreen = SettingsWidget;
	PresentScreen(SettingsWidget, 12);
}

void AArenaPlayerController::MenuBack()
{
	ShowMenu(LastMenuMode);
}

void AArenaPlayerController::MenuRetry()
{
	HideMenu();
	if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr) { GM->RestartCurrentMode(); }
}

void AArenaPlayerController::MenuQuitToTitle()
{
	if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr)
	{
		GM->ReturnToTitle(); // shows the title menu itself
		return;
	}
	ShowMenu(EMenuMode::Title);
}

void AArenaPlayerController::MenuResume()
{
	HideMenu();
}

void AArenaPlayerController::MenuQuit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
