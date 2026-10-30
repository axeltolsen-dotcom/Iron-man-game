// AArenaPlayerController: owns the HUD, the suit AI (UJarvisComponent) and every menu screen
// (title / pause / game over, suit select, story hub, settings). Handles F11 / Alt+Enter.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuWidget.h"
#include "ArenaPlayerController.generated.h"

class UArenaHUDWidget;
class UHelmetHUDWidget;
class UJarvisComponent;
class USuitSelectWidget;
class USettingsWidget;
class UStoryMenuWidget;
class UUserWidget;
struct FKeyEvent;

class AMenuCameraActor;

UCLASS()
class IRONMANARENA_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AArenaPlayerController();

	/** Shows the main menu in the given mode, pauses the game, switches to UI input. */
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void ShowMenu(EMenuMode Mode);

	/** Hides every menu, unpauses, back to game input. */
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void HideMenu();

	/** P / Esc / Start in game. Opens or closes the pause menu while playing. */
	UFUNCTION(BlueprintCallable, Category = "Arena|UI")
	void TogglePauseMenu();

	// ---- Menu actions (called by the C++ widgets) ----
	/** Title: open suit select for an Arena run.  Paused / Game over: restart the current mode. */
	void MenuStartGame();
	void MenuOpenSuitSelect();
	void MenuConfirmSuit(int32 SuitIndex);
	void MenuOpenStory();
	void MenuStartStory(int32 ChapterIndex, bool bPlayPrologue);
	void MenuStartSideMission(FName MissionId);
	void MenuOpenSettings();
	/** Close the current sub screen and return to the main menu in its previous mode. */
	void MenuBack();
	void MenuRetry();
	void MenuQuitToTitle();
	void MenuResume();
	void MenuQuit();

	/** F11 / Alt+Enter from a focused widget. Returns true when handled. */
	bool HandleGlobalKey(const FKeyEvent& InKeyEvent);

	bool IsMenuOpen() const;

	UJarvisComponent* GetJarvis() const { return Jarvis; }
	UArenaHUDWidget* GetHUDWidget() const { return HUDWidget; }
	UHelmetHUDWidget* GetHelmetHUD() const { return HelmetHUD; }

	/** Override with Blueprint children if you want to restyle them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TSubclassOf<UArenaHUDWidget> HUDWidgetClass;
	/** EDITH's first-person helmet HUD (hud_spec.md), one Z above the arena HUD. Null class = off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TSubclassOf<UHelmetHUDWidget> HelmetHUDClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TSubclassOf<UMainMenuWidget> MenuWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TSubclassOf<USuitSelectWidget> SuitSelectWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TSubclassOf<USettingsWidget> SettingsWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena|UI")
	TSubclassOf<UStoryMenuWidget> StoryMenuWidgetClass;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Puts Widget on screen as the only menu, pauses, UI input focused on it. */
	void PresentScreen(UUserWidget* Widget, int32 ZOrder);
	void RemoveSubScreen();
	void ToggleFullscreen();

	/** The suit AI (voice lines, subtitles, comms). UJarvisComponent::Get finds it here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena|AI")
	TObjectPtr<UJarvisComponent> Jarvis;

	UPROPERTY(Transient) TObjectPtr<UArenaHUDWidget> HUDWidget;
	UPROPERTY(Transient) TObjectPtr<UHelmetHUDWidget> HelmetHUD;
	/** Pushes the HUD accessibility settings to both HUDs when they change. */
	void SyncHUDSettings();
	uint32 LastHUDSettingsHash = 0;
	UPROPERTY(Transient) TObjectPtr<UMainMenuWidget> MenuWidget;
	UPROPERTY(Transient) TObjectPtr<USuitSelectWidget> SuitSelectWidget;
	UPROPERTY(Transient) TObjectPtr<USettingsWidget> SettingsWidget;
	UPROPERTY(Transient) TObjectPtr<UStoryMenuWidget> StoryMenuWidget;
	/** Sub screen currently shown instead of the main menu (null = main menu or none). */
	UPROPERTY(Transient) TObjectPtr<UUserWidget> SubScreen;

	EMenuMode LastMenuMode = EMenuMode::Title;

	/** Title-menu background camera (slow orbit over the city). */
	UPROPERTY(Transient) TObjectPtr<AMenuCameraActor> MenuCamera;
	void StartMenuCamera();
	void StopMenuCamera();
};
