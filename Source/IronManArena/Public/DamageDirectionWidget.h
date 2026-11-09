// UDamageDirectionWidget (FRIDAY, contrib/friday/realism): the directional damage arc Jocasta asked
// for in contrib/jocasta/combat_realism.md ("Player hit reactions": "the HUD should show a short
// directional damage arc").
//
// A paint-only UUserWidget (no child widgets, no textures). It draws the arcs held by
// UHUDDataSubsystem around the screen centre: amber normally, red only when the hit is critical or
// health is critical. Each arc fades to 0 in 0.5 s, and its size and opacity scale with damage.
// Close hits (within 30 deg) merge. There is never a full-screen flash, and reduced motion
// (UArenaGameUserSettings::bHudReducedMotion) turns off the small outward drift.
//
// Use: add it to the viewport above WBP_HelmetHUD (Z order +1), or place it as a full-screen child
// of the helmet HUD's root canvas *inside* the visor RetainerBox so it gets the same curvature
// (REALISTIC_HUD_UE5.md). Sizes are in fractions of min(viewport width, height).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageDirectionWidget.generated.h"

namespace DamageArcMath { struct FArcSet; }

UCLASS()
class IRONMANARENA_API UDamageDirectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Arc radius from the screen centre, fraction of min(w, h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageArc")
	float RadiusFraction = 0.36f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageArc")
	FLinearColor CautionColor = FLinearColor(222.f / 255.f, 176.f / 255.f, 96.f / 255.f, 1.f);   // amber

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageArc")
	FLinearColor CriticalColor = FLinearColor(228.f / 255.f, 74.f / 255.f, 58.f / 255.f, 1.f);   // red: critical only

	/** Global cap on top of the per-arc alpha (matches the HUD's 0.85 opacity rule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageArc", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxOpacity = 0.85f;

	/** Line segments per arc (taper resolution). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageArc", meta = (ClampMin = "4", ClampMax = "32"))
	int32 Segments = 10;

	/** Visor barrel warp, same value as the M_HUD_Visor material's Curvature (0 = flat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageArc")
	float Curvature = 0.055f;

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

public:
	/** Paint style shared by this widget and UHelmetHUDWidget (which draws the arcs itself in the live HUD). */
	struct FPaintStyle
	{
		float RadiusFraction = 0.36f;
		float Curvature = 0.055f;
		int32 Segments = 10;
		FLinearColor CautionColor = FLinearColor(222.f / 255.f, 176.f / 255.f, 96.f / 255.f, 1.f);
		FLinearColor CriticalColor = FLinearColor(228.f / 255.f, 74.f / 255.f, 58.f / 255.f, 1.f);
		/** Final alpha multiplier (HUD opacity x widget tint x MaxOpacity). */
		float Opacity = 0.85f;
		bool bReducedMotion = false;
		bool bHealthCritical = false;
	};

	/**
	 * Draws every live arc of Set into Size (local space of Geometry) at Layer. Returns the layer used.
	 * Motion (off in reduced motion): each arc lands 6% inside its radius and settles outward with a
	 * damped spring in ~0.15 s (a physical "thump"), then drifts out 4% while it fades. Alpha follows
	 * DamageArcMath::FadeAlpha exactly, so it is 0 at LifeSeconds (0.5 s) either way.
	 */
	static int32 PaintArcs(const DamageArcMath::FArcSet& Set, const FPaintStyle& Style, const FGeometry& Geometry,
		FSlateWindowElementList& OutDrawElements, int32 Layer);

private:
	static FVector2f Warp(const FVector2f& Centre, float S, float Nx, float Ny, float Curvature);
};
