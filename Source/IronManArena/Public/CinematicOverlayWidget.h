// UCinematicOverlayWidget: letterbox bars, suit title card and skip hint for the suit-up
// cinematics (and story cutscenes). Built in C++, no UMG asset. ASuitUpCinematic drives it.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicOverlayWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;

UCLASS()
class IRONMANARENA_API UCinematicOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Title card contents. */
	void SetInfo(const FText& Title, const FText& Subtitle, const FText& AILabel, const FLinearColor& AccentColor, bool bCompact);

	/**
	 * Per-frame state.
	 * @param BarAlpha    0 = no bars, 1 = full letterbox.
	 * @param TitleAlpha  0..1 title card opacity.
	 * @param GlitchAmount 0..1 random title jitter (Ultron).
	 */
	void SetState(float BarAlpha, float TitleAlpha, float GlitchAmount);

	/** Optional subtitle line under the title card area (story cutscenes). */
	void SetCaption(const FText& Speaker, const FText& Line, const FLinearColor& Color);

	/** Helmet HUD boot (suit-up eyes-on beat): 0 = off, 1 = full. Line = optional status text. */
	void SetHudBoot(float Alpha, const FText& Line, const FLinearColor& Color);

	/** Full letterbox height in Slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	float BarHeight = 120.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	void BuildTree();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootPanel;
	UPROPERTY(Transient) TObjectPtr<UBorder> TopBar;
	UPROPERTY(Transient) TObjectPtr<UBorder> BottomBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AIText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SkipText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CaptionText;

	bool bCompactCard = false;
	float HudAlpha = 0.f;
	float HudAge = 0.f;
	FLinearColor HudColor = FLinearColor(0.35f, 0.9f, 1.f);
	FText HudLine;
};
