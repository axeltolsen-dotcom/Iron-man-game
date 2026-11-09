#include "DamageDirectionWidget.h"

#include "DamageArcMath.h"
#include "HUDDataSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateTypes.h"

void UDamageDirectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);   // never eats input
}

FVector2f UDamageDirectionWidget::Warp(const FVector2f& Centre, float S, float Nx, float Ny, float Curvature)
{
	// Same barrel warp as realistic_hud.js P(): edges pulled in a touch, lines bow with the visor.
	const float R2 = (Nx * Nx + Ny * Ny) * 0.25f;
	const float F = 1.f - Curvature * R2;
	return Centre + FVector2f(Nx * F * S * 0.5f, Ny * F * S * 0.5f);
}

int32 UDamageDirectionWidget::PaintArcs(const DamageArcMath::FArcSet& Set, const FPaintStyle& Style, const FGeometry& Geometry,
	FSlateWindowElementList& OutDrawElements, int32 Layer)
{
	if (Set.Arcs.Num() == 0 || Style.Opacity <= 0.003f) { return Layer; }

	const FVector2f Size = FVector2f(Geometry.GetLocalSize());
	const float S = FMath::Min(Size.X, Size.Y);
	if (S < 2.f) { return Layer; }
	const FVector2f Centre = Size * 0.5f;
	const float Life = FMath::Max(Set.Config.LifeSeconds, 0.05f);
	const int32 Segs = FMath::Clamp(Style.Segments, 4, 32);
	++Layer;

	for (const DamageArcMath::FArc& Arc : Set.Arcs)
	{
		const float Alpha = Arc.CurrentAlpha(Set.Config) * Style.Opacity;
		if (Alpha <= 0.003f) { continue; }

		// Radius motion: land 6% inside, damped spring out (zeta ~0.45, settles by ~0.15 s), slow drift while fading.
		float Grow = 1.f;
		if (!Style.bReducedMotion)
		{
			const float T = Arc.Age;
			const float Spring = -0.06f * FMath::Exp(-T * 22.f) * FMath::Cos(T * 38.f);
			const float Drift = 0.04f * DamageArcMath::Smooth(T / Life);
			Grow = 1.f + Spring + Drift;
		}
		const float R = Style.RadiusFraction * 2.f * Grow;                    // normalised (1 = S / 2)
		const float ThickPx = FMath::Max(1.f, Arc.Look.Thickness * S);
		FLinearColor Col = (Arc.bCritical || Style.bHealthCritical) ? Style.CriticalColor : Style.CautionColor;

		// Tapered arc: N short polylines, thicker and brighter in the middle.
		const float A0 = Arc.AngleRad - Arc.Look.HalfWidthRad;
		const float Span = 2.f * Arc.Look.HalfWidthRad;
		for (int32 i = 0; i < Segs; ++i)
		{
			const float U0 = float(i) / Segs, U1 = float(i + 1) / Segs, Um = 0.5f * (U0 + U1);
			const float Taper = FMath::Sin(Um * PI);
			TArray<FVector2f> Pts;
			for (int32 k = 0; k <= 3; ++k)
			{
				const float A = A0 + Span * FMath::Lerp(U0, U1, k / 3.f);
				Pts.Add(Warp(Centre, S, FMath::Sin(A) * R, -FMath::Cos(A) * R, Style.Curvature));   // 0 = up, clockwise
			}
			Col.A = Alpha * (0.35f + 0.65f * Taper);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Pts,
				ESlateDrawEffect::None, Col, true, ThickPx * (0.35f + 0.65f * Taper));
		}

		// Inward notch at the centre so the direction reads at a glance.
		const float Th = Arc.Look.Thickness * 2.f;
		const float R0 = R - Th * 1.6f, R1 = R - Th * 0.6f, Nd = FMath::DegreesToRadians(2.5f);
		TArray<FVector2f> Notch;
		Notch.Add(Warp(Centre, S, FMath::Sin(Arc.AngleRad - Nd) * R1, -FMath::Cos(Arc.AngleRad - Nd) * R1, Style.Curvature));
		Notch.Add(Warp(Centre, S, FMath::Sin(Arc.AngleRad) * R0, -FMath::Cos(Arc.AngleRad) * R0, Style.Curvature));
		Notch.Add(Warp(Centre, S, FMath::Sin(Arc.AngleRad + Nd) * R1, -FMath::Cos(Arc.AngleRad + Nd) * R1, Style.Curvature));
		Col.A = Alpha;
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Notch, ESlateDrawEffect::None, Col, true, 1.5f);
	}
	return Layer;
}

int32 UDamageDirectionWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const UHUDDataSubsystem* Data = UHUDDataSubsystem::Get(this);
	if (!Data) { return Layer; }

	FPaintStyle Style;
	Style.RadiusFraction = RadiusFraction;
	Style.Curvature = Curvature;
	Style.Segments = Segments;
	Style.CautionColor = CautionColor;
	Style.CriticalColor = CriticalColor;
	Style.Opacity = MaxOpacity * InWidgetStyle.GetColorAndOpacityTint().A;
	Style.bReducedMotion = Data->IsReducedMotion();
	Style.bHealthCritical = Data->GetHealthFraction() <= 0.25f;
	return PaintArcs(Data->GetArcs(), Style, AllottedGeometry, OutDrawElements, Layer);
}
