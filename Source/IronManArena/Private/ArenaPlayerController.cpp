#include "ArenaPlayerController.h"
#include "CityLayout.h"
#include "ArenaEnvironment.h"
#include "MenuCameraActor.h"
#include "ArenaFeel.h"
#include "Camera/PlayerCameraManager.h"

#include "ArenaHUDWidget.h"
#include "HelmetHUDWidget.h"
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
	HelmetHUDClass = UHelmetHUDWidget::StaticClass();
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

	// Trauma camera shake (hits, blasts, boss phases); scaled to 0 by the Camera Shake / Reduced Motion settings.
	if (PlayerCameraManager) { PlayerCameraManager->AddNewCameraModifier(UArenaShakeModifier::StaticClass()); }

	if (*HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UArenaHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget) { HUDWidget->AddToViewport(0); }
	}
	if (*HelmetHUDClass)
	{
		HelmetHUD = CreateWidget<UHelmetHUDWidget>(this, HelmetHUDClass);
		if (HelmetHUD) { HelmetHUD->AddToViewport(1); } // Z+1: over the arena HUD
	}
	SyncHUDSettings();
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
	SyncHUDSettings();
	// In game input the keys never reach a widget, so check them here (the engine's own
	// F11 / Alt+Enter handling is disabled in DefaultInput.ini so it cannot double-toggle).
	if (WasInputKeyJustPressed(EKeys::F11)
		|| (WasInputKeyJustPressed(EKeys::Enter) && (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))))
	{
		ToggleFullscreen();
	}
}

void AArenaPlayerController::SyncHUDSettings()
{
	const UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	if (!S) { return; }
	const float Opacity = FMath::Clamp(S->HudOpacity, 0.5f, 1.f);
	const float HudScaleValue = FMath::Clamp(S->HudScale, 0.75f, 1.25f);
	const float SubScale = FMath::Clamp(S->SubtitleScale, 0.8f, 1.6f);
	uint32 Hash = GetTypeHash(S->bHudColorBlindSafe);
	Hash = HashCombine(Hash, GetTypeHash(S->bHudReducedMotion));
	Hash = HashCombine(Hash, GetTypeHash(Opacity));
	Hash = HashCombine(Hash, GetTypeHash(HudScaleValue));
	Hash = HashCombine(Hash, GetTypeHash(SubScale));
	Hash = HashCombine(Hash, GetTypeHash(HelmetHUD.Get()));
	Hash = HashCombine(Hash, GetTypeHash(HUDWidget.Get()));
	if (Hash == LastHUDSettingsHash) { return; }
	LastHUDSettingsHash = Hash;
	if (HelmetHUD)
	{
		HelmetHUD->SetAccessibility(S->bHudColorBlindSafe, S->bHudReducedMotion, Opacity, SubScale);
		HelmetHUD->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		HelmetHUD->SetRenderScale(FVector2D(HudScaleValue, HudScaleValue));
	}
	if (HUDWidget)
	{
		HUDWidget->SetHelmetHUDPresent(HelmetHUD != nullptr);
		HUDWidget->SetSubtitleScale(SubScale);
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
	if (Mode == EMenuMode::Title) { StartMenuCamera(); }
}

void AArenaPlayerController::StartMenuCamera()
{
	UWorld* World = GetWorld();
	if (!World || !IsLocalController()) { return; }
	if (!MenuCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MenuCamera = World->SpawnActor<AMenuCameraActor>(AMenuCameraActor::StaticClass(), FTransform::Identity, Params);
	}
	if (!MenuCamera) { return; }
	if (const AArenaEnvironment* Env = AArenaEnvironment::Find(this))
	{
		MenuCamera->Focus = Env->GetActorLocation();
		MenuCamera->bCityScale = false;
		if (const ACityLayout* City = Env->GetCityLayout())
		{
			// Slow crane around the Legacy tower, looking across the skyline.
			MenuCamera->Focus = City->GetLandmarkLocation() + FVector(0.f, 0.f, 15000.f);
			MenuCamera->bCityScale = true;
		}
	}
	MenuCamera->UpdatePose();
	MenuCamera->HoldFor = this;
	SetViewTarget(MenuCamera);
}

void AArenaPlayerController::StopMenuCamera()
{
	if (!MenuCamera) { return; }
	MenuCamera->HoldFor = nullptr;
	if (GetViewTarget() == MenuCamera && GetPawn()) { SetViewTarget(GetPawn()); }
}

void AArenaPlayerController::HideMenu()
{
	RemoveSubScreen();
	if (MenuWidget && MenuWidget->IsInViewport()) { MenuWidget->RemoveFromParent(); }
	StopMenuCamera(); // before StartNewGame: the suit-up cinematic sets its own view target
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
