// USettingsWidget: video / controls / audio / accessibility options, built in C++.
// Display options are staged and applied with APPLY (UGameUserSettings::ApplySettings + save);
// sliders for audio, FOV and sensitivity apply live. F11 / Alt+Enter toggle fullscreen anywhere.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsWidget.generated.h"

class UComboBoxString;
class UCheckBox;
class USlider;
class UTextBlock;
class UVerticalBox;
class UButton;

UCLASS()
class IRONMANARENA_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reload every control from the current settings object. */
	void RefreshFromSettings();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildTree();
	UComboBoxString* AddCombo(UVerticalBox* Box, const FText& Label);
	UCheckBox* AddCheck(UVerticalBox* Box, const FText& Label);
	USlider* AddSlider(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, UTextBlock*& OutValue);
	void AddHeader(UVerticalBox* Box, const FText& Label);
	void UpdateSliderLabels();
	void ApplyLiveOptions();

	UFUNCTION()
	void HandleComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleCheckChanged(bool bIsChecked);
	UFUNCTION()
	void HandleSliderChanged(float Value);
	UFUNCTION()
	void HandleApply();
	UFUNCTION()
	void HandleDefaults();
	UFUNCTION()
	void HandleBack();

	UPROPERTY(Transient) TObjectPtr<UComboBoxString> WindowModeCombo;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> ResolutionCombo;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> VSyncCheck;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> FrameCapCombo;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> QualityCombo;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> RayTracingCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> MotionBlurCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> DofCheck;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> ShakeCheck;
	UPROPERTY(Transient) TObjectPtr<USlider> FovSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FovValue;
	UPROPERTY(Transient) TObjectPtr<USlider> SensSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SensValue;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> InvertCheck;
	UPROPERTY(Transient) TObjectPtr<USlider> MasterSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MasterValue;
	UPROPERTY(Transient) TObjectPtr<USlider> SfxSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SfxValue;
	UPROPERTY(Transient) TObjectPtr<USlider> VoiceSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VoiceValue;
	UPROPERTY(Transient) TObjectPtr<USlider> MusicSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MusicValue;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> SubtitlesCheck;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	TArray<FIntPoint> Resolutions;
	bool bRefreshing = false;
};
