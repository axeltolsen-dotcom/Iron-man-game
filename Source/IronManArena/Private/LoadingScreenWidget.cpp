#include "LoadingScreenWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/AutomationTest.h"

#define LOCTEXT_NAMESPACE "LoadingScreenWidget"

namespace
{
	constexpr float LoadMinWait = 0.25f;  // seconds on screen before the work runs
	constexpr float LoadMinTotal = 1.4f;  // never flash: the screen stays at least this long
	constexpr float LoadFill = 0.35f;     // seconds for the bar to run from its frozen value to 100%
	constexpr float LoadFade = 0.5f;      // fade out into the suit-up cinematic

	const FLinearColor LoadCyan(0.35f, 0.9f, 1.f, 1.f);
	const FLinearColor LoadIce(0.78f, 0.93f, 1.f, 1.f);
	const FLinearColor LoadRed(0.75f, 0.1f, 0.06f, 1.f);
	const FLinearColor LoadGold(1.f, 0.78f, 0.3f, 1.f);

	// Status lines cycle while loading. Flavour text only.
	const TCHAR* const LoadStatus[] = {
		TEXT("SPINNING UP ARC REACTOR"),
		TEXT("CALIBRATING REPULSORS"),
		TEXT("STREAMING CITY BLOCKS"),
		TEXT("SYNCING SUIT AI"),
		TEXT("PRESSURISING FLIGHT STABILISERS"),
		TEXT("LOADING ARMOUR TELEMETRY"),
		TEXT("CHECKING THREAT RADAR"),
	};

	// Tips use the real default bindings (Content/UI/controls_bindings.json, read from the input code).
	const TCHAR* const LoadTips[] = {
		TEXT("Press F to take off; Space climbs, Left Ctrl or C descends."),
		TEXT("Hold Left Shift to boost. Watch the reactor: vent it with X when it runs hot."),
		TEXT("Hold Left Alt to focus aim, then hold Left mouse for a charged shot."),
		TEXT("Hold Right mouse for 0.3 s (or double-click Left mouse) to unleash your suit's ULTRA."),
		TEXT("Hold T for the suit wheel, or press 1 to 5 for quick suits."),
		TEXT("Press O to step out of the suit and walk the city, and again to call it back."),
		TEXT("Press E near cars, bikes, shops and houses. U opens the shop, N your phone."),
		TEXT("Press V to switch between first and third person. H opens the faceplate."),
		TEXT("Q calls in the Hulkbuster once you have earned it. G summons an ally."),
		TEXT("Press M to toggle the map markers. The helmet radar shows the streets around you."),
	};

	void LoadCircle(TArray<FVector2D>& Pts, const FVector2D& C, float R, float A0, float A1, int32 Segs)
	{
		Pts.Reset();
		for (int32 k = 0; k <= Segs; ++k)
		{
			const float A = FMath::Lerp(A0, A1, float(k) / Segs);
			Pts.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
	}
}

bool ULoadingScreenWidget::ShouldSkip()
{
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized()) { return true; }
	if (GIsAutomationTesting) { return true; }
	const TCHAR* Cmd = FCommandLine::Get();
	return FParse::Param(Cmd, TEXT("ArenaSmoke")) || FParse::Param(Cmd, TEXT("StoryAutoplay")) || FParse::Param(Cmd, TEXT("NoLoadingScreen"));
}

float ULoadingScreenWidget::FakeProgress(float SecondsWaiting, bool bWorkDone, float SecondsSinceDone)
{
	// Eases towards 90% while waiting (never stalls at a round number), then runs to 100% once the work is done.
	const float Wait = 0.9f * (1.f - FMath::Exp(-FMath::Max(0.f, SecondsWaiting) * 2.2f));
	if (!bWorkDone) { return Wait; }
	const float T = FMath::Clamp(SecondsSinceDone / LoadFill, 0.f, 1.f);
	return FMath::Lerp(Wait, 1.f, 1.f - FMath::Square(1.f - T));
}

void ULoadingScreenWidget::Begin(const FText& InTitle, const FText& InSubtitle, TFunction<void()> InWork)
{
	if (PendingWork)
	{
		// Already waiting: run the previous work first so nothing is dropped.
		TFunction<void()> Prev = MoveTemp(PendingWork);
		PendingWork = nullptr;
		Prev();
	}
	TitleText = InTitle;
	SubtitleText = InSubtitle;
	TipText = LoadTips[FMath::RandRange(0, int32(UE_ARRAY_COUNT(LoadTips)) - 1)];
	PendingWork = MoveTemp(InWork);
	Phase = EPhase::Waiting;
	PhaseTime = 0.f;
	TotalTime = 0.f;
	DoneAt = -1.f;
	PaintedFrames = 0;
	Opacity = 1.f;
	SetRenderOpacity(1.f);
	SetVisibility(ESlateVisibility::Visible); // swallow clicks while loading
}

TSharedRef<SWidget> ULoadingScreenWidget::RebuildWidget()
{
	return SNew(SBox);
}

void ULoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Phase == EPhase::Idle) { return; }
	// Clamp: the frame that runs the work is long and must not skip the hold / fade.
	const float Dt = FMath::Min(InDeltaTime, 0.05f);
	PhaseTime += Dt;
	TotalTime += Dt;

	if (Phase == EPhase::Waiting && PaintedFrames >= 2 && PhaseTime >= LoadMinWait)
	{
		Phase = EPhase::Holding;
		PhaseTime = 0.f;
		TFunction<void()> Work = MoveTemp(PendingWork);
		PendingWork = nullptr;
		if (Work) { Work(); }
		DoneAt = TotalTime;
	}
	else if (Phase == EPhase::Holding && PhaseTime >= LoadFill && TotalTime >= LoadMinTotal)
	{
		Phase = EPhase::Fading;
		PhaseTime = 0.f;
		SetVisibility(ESlateVisibility::HitTestInvisible); // the cinematic / game takes input again
	}
	else if (Phase == EPhase::Fading)
	{
		Opacity = 1.f - FMath::Clamp(PhaseTime / LoadFade, 0.f, 1.f);
		SetRenderOpacity(Opacity);
		if (Opacity <= 0.f)
		{
			Phase = EPhase::Idle;
			RemoveFromParent();
		}
	}
}

int32 ULoadingScreenWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, G, MyCullingRect, Out, LayerId, InWidgetStyle, bParentEnabled);
	if (Phase == EPhase::Idle) { return LayerId; }
	++PaintedFrames;

	const FVector2D Size = G.GetLocalSize();
	const float Ui = FMath::Max(0.5f, Size.Y / 1080.f);
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const float Time = float(FPlatformTime::Seconds());
	auto Box = [&](const FVector2D& Pos, const FVector2D& Sz, const FLinearColor& Col, int32 L)
	{
		FSlateDrawElement::MakeBox(Out, LayerId + L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(Pos)), White, ESlateDrawEffect::None, Col);
	};
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto Text = [&](const FString& S, FVector2D Pos, float FontSize, const char* Weight, const FLinearColor& Col, float AlignX)
	{
		const FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Weight, FMath::RoundToInt(FontSize * Ui));
		Pos.X -= Measure->Measure(S, F).X * AlignX;
		FSlateDrawElement::MakeText(Out, LayerId + 4, G.ToPaintGeometry(FVector2D(1.f, 1.f), FSlateLayoutTransform(Pos + FVector2D(2.f, 2.f))), S, F,
			ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.8f * Col.A));
		FSlateDrawElement::MakeText(Out, LayerId + 5, G.ToPaintGeometry(FVector2D(1.f, 1.f), FSlateLayoutTransform(Pos)), S, F, ESlateDrawEffect::None, Col);
	};

	// Backdrop: near-black with a cool centre glow (stacked translucent bands) and faint scanlines.
	Box(FVector2D::ZeroVector, Size, FLinearColor(0.004f, 0.008f, 0.012f, 1.f), 0);
	for (int32 k = 1; k <= 8; ++k)
	{
		const float F = k / 8.f;
		const FVector2D Sz(Size.X * (1.1f - F * 0.75f), Size.Y * (1.1f - F * 0.8f));
		Box((Size - Sz) * 0.5f - FVector2D(0.f, Size.Y * 0.06f * F), Sz, FLinearColor(0.02f, 0.09f, 0.13f, 0.06f), 1);
	}
	const float LineStep = FMath::Max(3.f, 4.f * Ui);
	for (float Y = 0.f; Y < Size.Y; Y += LineStep)
	{
		Box(FVector2D(0.f, Y), FVector2D(Size.X, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.22f), 2);
	}

	// Arc reactor: core glow, triangle housing, ten coils, two counter-rotating broken rings.
	const FVector2D Core(Size.X * 0.5f, Size.Y * 0.36f);
	const float R = 92.f * Ui;
	const float Pulse = 0.82f + 0.18f * FMath::Sin(Time * 3.1f);
	TArray<FVector2D> Pts;
	for (int32 k = 6; k >= 1; --k)
	{
		const float Rr = R * 0.18f * k;
		LoadCircle(Pts, Core, Rr, 0.f, 2.f * PI, 48);
		FSlateDrawElement::MakeLines(Out, LayerId + 3, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None,
			FLinearColor(0.55f, 0.95f, 1.f, 0.07f * Pulse * (7 - k)), true, Rr * 0.22f);
	}
	LoadCircle(Pts, Core, R * 0.28f, 0.f, 2.f * PI, 40);
	FSlateDrawElement::MakeLines(Out, LayerId + 4, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.9f, 1.f, 1.f, Pulse), true, 3.f * Ui);
	{
		TArray<FVector2D> Tri;
		for (int32 k = 0; k <= 3; ++k)
		{
			const float A = -PI * 0.5f + k * 2.f * PI / 3.f;
			Tri.Add(Core + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * 0.5f);
		}
		FSlateDrawElement::MakeLines(Out, LayerId + 4, G.ToPaintGeometry(), Tri, ESlateDrawEffect::None, LoadCyan * FLinearColor(1.f, 1.f, 1.f, 0.8f), true, 2.f * Ui);
	}
	for (int32 k = 0; k < 10; ++k)
	{
		const float A0 = k * 2.f * PI / 10.f + 0.06f;
		LoadCircle(Pts, Core, R * 0.74f, A0, A0 + 2.f * PI / 10.f - 0.12f, 8);
		FSlateDrawElement::MakeLines(Out, LayerId + 4, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None,
			FLinearColor(0.6f, 0.92f, 1.f, 0.55f + 0.35f * Pulse), false, 10.f * Ui);
	}
	for (int32 k = 0; k < 3; ++k)
	{
		const float A0 = Time * 1.6f + k * 2.f * PI / 3.f;
		LoadCircle(Pts, Core, R * 1.02f, A0, A0 + 1.3f, 20);
		FSlateDrawElement::MakeLines(Out, LayerId + 4, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, LoadCyan, false, 3.f * Ui);
		const float B0 = -Time * 0.9f + k * 2.f * PI / 3.f;
		LoadCircle(Pts, Core, R * 1.16f, B0, B0 + 0.55f, 10);
		FSlateDrawElement::MakeLines(Out, LayerId + 4, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.35f, 0.9f, 1.f, 0.45f), false, 1.5f * Ui);
	}

	// Title block.
	Text(TEXT("STARK INDUSTRIES"), FVector2D(Size.X * 0.5f, Core.Y + R * 1.45f), 14.f, "Bold", LoadRed, 0.5f);
	Text(TitleText.ToString().ToUpper(), FVector2D(Size.X * 0.5f, Core.Y + R * 1.45f + 24.f * Ui), 44.f, "Bold", LoadIce, 0.5f);
	if (!SubtitleText.IsEmpty())
	{
		Text(SubtitleText.ToString(), FVector2D(Size.X * 0.5f, Core.Y + R * 1.45f + 84.f * Ui), 18.f, "Regular", LoadCyan * FLinearColor(1.f, 1.f, 1.f, 0.85f), 0.5f);
	}

	// Progress bar + status line.
	const bool bDone = DoneAt >= 0.f;
	const float WaitSeconds = bDone ? DoneAt : TotalTime;
	const float Pct = FakeProgress(WaitSeconds, bDone, bDone ? TotalTime - DoneAt : 0.f);
	const FVector2D BarPos(Size.X * 0.3f, Size.Y * 0.8f);
	const FVector2D BarSize(Size.X * 0.4f, FMath::Max(2.f, 3.f * Ui));
	Box(BarPos, BarSize, FLinearColor(0.35f, 0.9f, 1.f, 0.15f), 3);
	Box(BarPos, FVector2D(BarSize.X * Pct, BarSize.Y), LoadCyan, 4);
	Box(BarPos + FVector2D(BarSize.X * Pct - 6.f * Ui, -3.f * Ui), FVector2D(6.f * Ui, BarSize.Y + 6.f * Ui), FLinearColor(0.9f, 1.f, 1.f, 1.f), 5);
	const int32 StatusIndex = bDone && Pct > 0.99f ? -1 : int32(TotalTime / 0.55f) % int32(UE_ARRAY_COUNT(LoadStatus));
	const FString Status = StatusIndex < 0 ? FString(TEXT("ALL SYSTEMS GO")) : FString(LoadStatus[StatusIndex]) + FString::ChrN(1 + int32(TotalTime * 6.f) % 3, TEXT('.'));
	Text(Status, BarPos + FVector2D(0.f, 12.f * Ui), 13.f, "Bold", StatusIndex < 0 ? LoadGold : LoadIce * FLinearColor(1.f, 1.f, 1.f, 0.8f), 0.f);
	Text(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Pct * 100.f)), BarPos + FVector2D(BarSize.X, 12.f * Ui), 13.f, "Bold", LoadCyan, 1.f);

	// Tip + corner tags.
	Text(FString(TEXT("TIP   ")) + TipText, FVector2D(Size.X * 0.5f, Size.Y * 0.9f), 15.f, "Regular", FLinearColor(0.85f, 0.9f, 0.95f, 0.75f), 0.5f);
	Text(TEXT("SECURE BOOT  //  MARK SERIES ARMOUR OS"), FVector2D(36.f * Ui, 30.f * Ui), 11.f, "Bold", FLinearColor(0.35f, 0.9f, 1.f, 0.5f), 0.f);
	Text(FString::Printf(TEXT("T+%05.2f"), TotalTime), FVector2D(Size.X - 36.f * Ui, 30.f * Ui), 11.f, "Bold", FLinearColor(0.35f, 0.9f, 1.f, 0.5f), 1.f);
	// Thin corner brackets.
	const float Cb = 28.f * Ui, M = 22.f * Ui;
	const FLinearColor BrCol(0.35f, 0.9f, 1.f, 0.35f);
	const FVector2D Corners[4] = { FVector2D(M, M), FVector2D(Size.X - M, M), FVector2D(M, Size.Y - M), FVector2D(Size.X - M, Size.Y - M) };
	for (int32 k = 0; k < 4; ++k)
	{
		const float Sx = (k % 2 == 0) ? 1.f : -1.f, Sy = (k < 2) ? 1.f : -1.f;
		TArray<FVector2D> Br = { Corners[k] + FVector2D(Cb * Sx, 0.f), Corners[k], Corners[k] + FVector2D(0.f, Cb * Sy) };
		FSlateDrawElement::MakeLines(Out, LayerId + 3, G.ToPaintGeometry(), Br, ESlateDrawEffect::None, BrCol, false, 1.5f * Ui);
	}
	return LayerId + 6;
}

#undef LOCTEXT_NAMESPACE
