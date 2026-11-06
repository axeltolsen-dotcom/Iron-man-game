// UMainMenuWidget: title / pause / game-over screen, built entirely in C++ (no UMG asset needed).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;
class AArenaGameMode;

UENUM(BlueprintType)
enum class EMenuMode : uint8
{
	Title,
	Paused,
	GameOver
};

UCLASS()
class IRONMANARENA_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates texts and which buttons are shown. GM may be null. */
	void SetMode(EMenuMode InMode, const AArenaGameMode* GM);

	EMenuMode GetMode() const { return Mode; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION() void HandleStartClicked();
	UFUNCTION() void HandleStoryClicked();
	UFUNCTION() void HandleResumeClicked();
	UFUNCTION() void HandleSettingsClicked();
	UFUNCTION() void HandleTitleClicked();
	UFUNCTION() void HandleQuitClicked();
	UFUNCTION() void HandleCreditsClicked();
	UFUNCTION() void HandleHoverChanged();
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void Click();
	void UpdateMusic();

	void BuildTree();
	UButton* MakeButton(UVerticalBox* Box, const FText& Label, UTextBlock*& OutLabel);
	void RefreshTexts();

	EMenuMode Mode = EMenuMode::Title;
	int32 LastScore = 0;
	int32 HighScore = 0;
	int32 LastWave = 0;
	bool bNewHigh = false;
	bool bStoryMode = false;
	FText StoryChapterTitle;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ControlsText;
	UPROPERTY(Transient) TObjectPtr<UButton> ResumeButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResumeLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> StartButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StartLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> StoryButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StoryLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> SettingsButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SettingsLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> TitleButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> QuitButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QuitLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> CreditsButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CreditsLabel;
	UPROPERTY(Transient) TObjectPtr<class UBorder> CreditsPanel;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Music;
	TArray<TPair<TObjectPtr<UButton>, TObjectPtr<UTextBlock>>> Buttons;
	TArray<FString> ButtonCaptions;
	TWeakObjectPtr<UButton> LastHovered;
	float IntroT = 0.f;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MenuBox;
};
