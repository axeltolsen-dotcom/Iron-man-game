#include "CinematicOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Misc/App.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "CinematicOverlay"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, UCanvasPanel* Root, int32 Size, const FLinearColor& Color,
		const FVector2D& Anchor, const FVector2D& Alignment, const FVector2D& Position, const TCHAR* Typeface = TEXT("Bold"))
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(2.f, 2.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		T->SetJustification(Alignment.X > 0.75f ? ETextJustify::Right : (Alignment.X > 0.25f ? ETextJustify::Center : ETextJustify::Left));
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(T))
		{
			S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			S->SetAlignment(Alignment);
			S->SetPosition(Position);
			S->SetAutoSize(true);
		}
		return T;
	}
}

TSharedRef<SWidget> UCinematicOverlayWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UCinematicOverlayWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CineRoot"));
	WidgetTree->RootWidget = Root;
	RootPanel = Root;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	auto MakeBar = [this, Root](bool bTop)
	{
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(FLinearColor::Black);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(B))
		{
			// Horizontal stretch; vertical: Bottom offset = height.
			S->SetAnchors(bTop ? FAnchors(0.f, 0.f, 1.f, 0.f) : FAnchors(0.f, 1.f, 1.f, 1.f));
			S->SetAlignment(FVector2D(0.f, bTop ? 0.f : 1.f));
			S->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		}
		return B;
	};
	TopBar = MakeBar(true);
	BottomBar = MakeBar(false);

	TitleText = MakeText(WidgetTree, Root, 52, FLinearColor::White, FVector2D(0.06f, 0.68f), FVector2D(0.f, 0.5f), FVector2D::ZeroVector);
	SubtitleText = MakeText(WidgetTree, Root, 22, FLinearColor(1.f, 0.8f, 0.35f), FVector2D(0.06f, 0.68f), FVector2D(0.f, 0.f), FVector2D(4.f, 36.f));
	AIText = MakeText(WidgetTree, Root, 18, FLinearColor(0.55f, 0.85f, 1.f), FVector2D(0.06f, 0.68f), FVector2D(0.f, 0.f), FVector2D(4.f, 68.f));
	SkipText = MakeText(WidgetTree, Root, 14, FLinearColor(1.f, 1.f, 1.f, 0.6f), FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-28.f, -18.f), TEXT("Regular"));
	SkipText->SetText(LOCTEXT("Skip", "SPACE / CLICK  -  SKIP"));
	CaptionText = MakeText(WidgetTree, Root, 20, FLinearColor::White, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -150.f), TEXT("Regular"));
	CaptionText->SetVisibility(ESlateVisibility::Collapsed);
	SetState(0.f, 0.f, 0.f);
}

void UCinematicOverlayWidget::SetInfo(const FText& Title, const FText& Subtitle, const FText& AILabel, const FLinearColor& AccentColor, bool bCompact)
{
	if (!TitleText) { TakeWidget(); } // build the tree if it hasn't been yet
	if (!TitleText) { return; }
	bCompactCard = bCompact;
	TitleText->SetText(Title);
	SubtitleText->SetText(Subtitle);
	AIText->SetText(AILabel);
	AIText->SetColorAndOpacity(FSlateColor(AccentColor));
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", bCompact ? 34 : 52));
	BarHeight = bCompact ? 70.f : 120.f;
}

void UCinematicOverlayWidget::SetState(float BarAlpha, float TitleAlpha, float GlitchAmount)
{
	if (!TopBar || !BottomBar) { return; }
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(BarAlpha, 0.f, 1.f), 2.f);
	const float H = BarHeight * Eased;
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(TopBar->Slot)) { S->SetOffsets(FMargin(0.f, 0.f, 0.f, H)); }
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(BottomBar->Slot)) { S->SetOffsets(FMargin(0.f, 0.f, 0.f, H)); }

	const float A = FMath::Clamp(TitleAlpha, 0.f, 1.f);
	const FVector2D Jitter = GlitchAmount > 0.f && FMath::FRand() < 0.35f * GlitchAmount
		? FVector2D(FMath::FRandRange(-14.f, 14.f), FMath::FRandRange(-3.f, 3.f)) * GlitchAmount
		: FVector2D::ZeroVector;
	for (UTextBlock* T : { TitleText.Get(), SubtitleText.Get(), AIText.Get() })
	{
		if (!T) { continue; }
		T->SetRenderOpacity(A);
		// Slide in from the left as it fades in.
		T->SetRenderTranslation(FVector2D((1.f - A) * -40.f, 0.f) + Jitter);
	}
	if (SkipText) { SkipText->SetRenderOpacity(FMath::Min(1.f, BarAlpha) * 0.8f); }
}

void UCinematicOverlayWidget::SetCaption(const FText& Speaker, const FText& Line, const FLinearColor& Color)
{
	if (!CaptionText) { return; }
	if (Line.IsEmpty())
	{
		CaptionText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	CaptionText->SetText(Speaker.IsEmpty() ? Line : FText::Format(LOCTEXT("Caption", "{0}: {1}"), Speaker, Line));
	CaptionText->SetColorAndOpacity(FSlateColor(Color));
	CaptionText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCinematicOverlayWidget::SetHudBoot(float Alpha, const FText& Line, const FLinearColor& Color)
{
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	if (A > 0.f && HudAlpha <= 0.f) { HudAge = 0.f; }
	HudAge += A > 0.f ? FApp::GetDeltaTime() : 0.f;
	HudAlpha = A;
	HudColor = Color;
	if (!Line.IsEmpty()) { HudLine = Line; }
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UCinematicOverlayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, G, MyCullingRect, Out, LayerId, InWidgetStyle, bParentEnabled);
	if (HudAlpha <= 0.01f) { return LayerId; }
	// Inside-the-helmet boot: curved frame brackets, reticle rings that spin up, status lines typing in.
	const FVector2D Size = G.GetLocalSize();
	const FVector2D C = Size * 0.5f;
	const float A = HudAlpha;
	const FLinearColor Col = FLinearColor(HudColor.R, HudColor.G, HudColor.B, 0.85f * A);
	const FLinearColor Dim = FLinearColor(HudColor.R, HudColor.G, HudColor.B, 0.35f * A);
	const int32 L = LayerId + 1;
	auto Lines = [&](const TArray<FVector2D>& P, const FLinearColor& K, float W) { FSlateDrawElement::MakeLines(Out, L, G.ToPaintGeometry(), P, ESlateDrawEffect::None, K, true, W); };
	auto Arc = [&](float R, float From, float To, const FLinearColor& K, float W)
	{
		TArray<FVector2D> P;
		for (int32 i = 0; i <= 32; ++i) { const float T = FMath::DegreesToRadians(FMath::Lerp(From, To, i / 32.f)); P.Add(C + FVector2D(FMath::Cos(T), FMath::Sin(T)) * R); }
		Lines(P, K, W);
	};
	const float Spin = HudAge * 220.f;
	const float R0 = Size.Y * 0.11f * (0.6f + 0.4f * FMath::Min(1.f, HudAge * 3.f));
	Arc(R0, Spin, Spin + 250.f, Col, 2.f);
	Arc(R0 * 1.5f, -Spin * 0.6f, -Spin * 0.6f + 140.f, Dim, 1.5f);
	Arc(R0 * 1.5f, -Spin * 0.6f + 180.f, -Spin * 0.6f + 320.f, Dim, 1.5f);
	Arc(Size.Y * 0.42f, 200.f, 340.f, Dim, 1.f);
	Arc(Size.Y * 0.42f, 20.f, 160.f, Dim, 1.f);
	// Corner brackets (helmet visor edge).
	const float M = 60.f, Bk = 70.f;
	Lines({ FVector2D(M, M + Bk), FVector2D(M, M), FVector2D(M + Bk, M) }, Col, 2.f);
	Lines({ FVector2D(Size.X - M - Bk, M), FVector2D(Size.X - M, M), FVector2D(Size.X - M, M + Bk) }, Col, 2.f);
	Lines({ FVector2D(M, Size.Y - M - Bk), FVector2D(M, Size.Y - M), FVector2D(M + Bk, Size.Y - M) }, Col, 2.f);
	Lines({ FVector2D(Size.X - M - Bk, Size.Y - M), FVector2D(Size.X - M, Size.Y - M), FVector2D(Size.X - M, Size.Y - M - Bk) }, Col, 2.f);
	// Status lines typing in.
	static const TCHAR* Status[] = { TEXT("POWER .......... 100%"), TEXT("ARMOR SEAL ...... LOCKED"), TEXT("REPULSORS ....... ARMED"), TEXT("FLIGHT CTRL ..... ONLINE"), TEXT("HUD ............. ONLINE") };
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Mono", 13);
	const int32 Chars = int32(HudAge * 90.f);
	int32 Used = 0;
	for (int32 i = 0; i < 5; ++i)
	{
		const FString S(Status[i]);
		const int32 Take = FMath::Clamp(Chars - Used, 0, S.Len());
		Used += S.Len();
		if (Take <= 0) { break; }
		FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(FVector2D(420.f, 20.f), FSlateLayoutTransform(FVector2D(M + 20.f, Size.Y * 0.36f + i * 22.f))), S.Left(Take), Font, ESlateDrawEffect::None, Col);
	}
	if (!HudLine.IsEmpty())
	{
		FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(FVector2D(420.f, 24.f), FSlateLayoutTransform(FVector2D(Size.X - M - 300.f, Size.Y * 0.36f))), HudLine, FCoreStyle::GetDefaultFontStyle("Bold", 16), ESlateDrawEffect::None, Col);
	}
	return L;
}

#undef LOCTEXT_NAMESPACE
