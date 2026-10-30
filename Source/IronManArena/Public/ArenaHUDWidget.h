// UArenaHUDWidget: in-game HUD built in C++ (no UMG asset needed).
// Shows health, energy, reactor heat / vent, flight state, score, combo, wave, Hulkbuster,
// boss bar, banners, crosshair, AI subtitles, the suit wheel, a first-person helmet frame and
// (story mode) the objective panel, ally hint and beacon marker. Values are polled in NativeTick;
// everything hides during cinematics.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UCanvasPanel;
class UWidget;
class UBorder;
class AIronManCharacter;
class AArenaGameMode;

UCLASS()
class IRONMANARENA_API UArenaHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildTree();
	UTextBlock* AddText(UCanvasPanel* Root, const FText& Text, int32 Size, const FLinearColor& Color,
		const FVector2D& Anchor, const FVector2D& Alignment, const FVector2D& Position);
	UProgressBar* AddBar(UCanvasPanel* Root, const FLinearColor& Fill, const FVector2D& Anchor,
		const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size);

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> EnergyBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnergyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FlightText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HighScoreText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ComboText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HulkbusterText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BannerTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BannerSubtitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Crosshair;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BossBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootPanel;

	// --- reactor ---
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HeatBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HeatText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReactorText;

	// --- subtitles (suit AI / comms) ---
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleLine;

	// --- suit wheel ---
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> WheelTexts;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WheelCenter;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SuitText;

	// --- helmet frame (first person) ---
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> HelmetFrame;

	// --- story ---
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveChapter;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ObjectiveProgress;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AllyHint;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BeaconMarker;

	UBorder* AddFrameBar(UCanvasPanel* Root, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment);
	void TickSubtitles(const AIronManCharacter* Player);
	void TickWheel(const AIronManCharacter* Player);
	void TickStory(const AArenaGameMode* GM, const AIronManCharacter* Player);
	void TickReactor(const AIronManCharacter* Player);
	void TickHelmet(const AIronManCharacter* Player);

	float Pulse = 0.f;
};
