// ULoadingScreenWidget: full-screen Stark boot screen shown while a mode starts (city free play, story chapters,
// side missions). The city / chapter setup runs synchronously on the game thread and used to freeze the last menu
// frame for a moment; now the screen is painted first (spinning arc reactor, progress bar, status lines, a
// controls tip), the setup runs behind it, then it fades into the suit-up cinematic. All drawing is NativePaint
// (no assets). Skipped under -nullrhi / smoke / story-autoplay / automation so headless tests keep their timing.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

UCLASS()
class IRONMANARENA_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the screen and runs Work once it has been on screen for a couple of painted frames. */
	void Begin(const FText& InTitle, const FText& InSubtitle, TFunction<void()> InWork);

	/** True when the screen must not defer anything (no renderer, smoke / autoplay / automation runs). */
	static bool ShouldSkip();

	bool IsBusy() const { return Phase != EPhase::Idle; }

	/** Progress-bar fill for a given phase time (0..1); pure, used by the paint and the unit test. */
	static float FakeProgress(float SecondsWaiting, bool bWorkDone, float SecondsSinceDone);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	enum class EPhase : uint8 { Idle, Waiting, Holding, Fading };

	EPhase Phase = EPhase::Idle;
	FText TitleText;
	FText SubtitleText;
	FString TipText;
	TFunction<void()> PendingWork;
	float PhaseTime = 0.f;   // seconds in the current phase (real time)
	float TotalTime = 0.f;   // seconds since Begin
	float DoneAt = -1.f;     // TotalTime when the work finished
	mutable int32 PaintedFrames = 0; // frames painted since Begin (the work waits for 2 so the screen is really visible)
	float Opacity = 1.f;
};
