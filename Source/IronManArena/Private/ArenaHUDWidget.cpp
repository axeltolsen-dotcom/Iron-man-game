#include "ArenaHUDWidget.h"

#include "ArenaGameMode.h"
#include "IronManCharacter.h"
#include "BossTitan.h"
#include "ReactorComponent.h"
#include "JarvisComponent.h"
#include "ArenaGameUserSettings.h"
#include "StoryDirector.h"
#include "SuitTypes.h"
#include "Components/Border.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "ArenaHUDWidget"

namespace
{
	const FLinearColor HudGold(1.f, 0.8f, 0.3f);
	const FLinearColor HudRed(0.95f, 0.2f, 0.15f);
	const FLinearColor HudCyan(0.35f, 0.85f, 1.f);
}

TSharedRef<SWidget> UArenaHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UTextBlock* UArenaHUDWidget::AddText(UCanvasPanel* Root, const FText& Text, int32 Size, const FLinearColor& Color,
	const FVector2D& Anchor, const FVector2D& Alignment, const FVector2D& Position)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(Text);
	T->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetShadowOffset(FVector2D(2.f, 2.f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	if (Alignment.X > 0.75f) { T->SetJustification(ETextJustify::Right); }
	else if (Alignment.X > 0.25f) { T->SetJustification(ETextJustify::Center); }
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(T))
	{
		S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
	}
	return T;
}

UProgressBar* UArenaHUDWidget::AddBar(UCanvasPanel* Root, const FLinearColor& Fill, const FVector2D& Anchor,
	const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size)
{
	UProgressBar* B = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	B->SetFillColorAndOpacity(Fill);
	B->SetPercent(1.f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(B))
	{
		S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetSize(Size);
	}
	return B;
}

void UArenaHUDWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Root;
	RootPanel = Root;
	// The widget itself stays HitTestInvisible (so it keeps ticking); only the panel is collapsed.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Bottom-left: health / energy.
	AddText(Root, LOCTEXT("Armor", "ARMOR"), 14, HudGold, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -102.f));
	HealthBar = AddBar(Root, HudRed, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -78.f), FVector2D(320.f, 22.f));
	HealthText = AddText(Root, FText::GetEmpty(), 14, FLinearColor::White, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(360.f, -78.f));
	AddText(Root, LOCTEXT("Energy", "ARC ENERGY"), 14, HudGold, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -56.f));
	EnergyBar = AddBar(Root, HudCyan, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -32.f), FVector2D(320.f, 22.f));
	EnergyText = AddText(Root, FText::GetEmpty(), 14, FLinearColor::White, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(360.f, -32.f));
	FlightText = AddText(Root, FText::GetEmpty(), 16, HudCyan, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -128.f));

	// Top-left: score / combo.
	ScoreText = AddText(Root, FText::GetEmpty(), 30, FLinearColor::White, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(30.f, 24.f));
	HighScoreText = AddText(Root, FText::GetEmpty(), 14, HudGold, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(30.f, 64.f));
	ComboText = AddText(Root, FText::GetEmpty(), 22, HudGold, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(30.f, 88.f));

	// Top-right: wave / hostiles.
	WaveText = AddText(Root, FText::GetEmpty(), 22, FLinearColor::White, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 24.f));

	// Bottom-right: Hulkbuster.
	HulkbusterText = AddText(Root, FText::GetEmpty(), 18, HudGold, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-30.f, -32.f));

	// Top-centre: boss bar.
	BossText = AddText(Root, FText::GetEmpty(), 18, FLinearColor(0.8f, 0.6f, 1.f), FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));
	BossBar = AddBar(Root, FLinearColor(0.6f, 0.2f, 1.f), FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 48.f), FVector2D(600.f, 18.f));

	// Centre: banner + crosshair.
	BannerTitle = AddText(Root, FText::GetEmpty(), 46, HudGold, FVector2D(0.5f, 0.3f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	BannerSubtitle = AddText(Root, FText::GetEmpty(), 20, FLinearColor::White, FVector2D(0.5f, 0.3f), FVector2D(0.5f, 0.f), FVector2D(0.f, 34.f));
	Crosshair = AddText(Root, FText::FromString(TEXT("+")), 28, FLinearColor(0.6f, 0.95f, 1.f, 0.9f), FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	// Reactor heat (under the energy bar's label row, above armor).
	ReactorText = AddText(Root, FText::GetEmpty(), 13, HudCyan, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -176.f));
	HeatBar = AddBar(Root, FLinearColor(1.f, 0.45f, 0.1f), FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(30.f, -152.f), FVector2D(200.f, 10.f));
	HeatText = AddText(Root, FText::GetEmpty(), 13, FLinearColor::White, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(240.f, -150.f));

	// Current suit (bottom-right, above the Hulkbuster line).
	SuitText = AddText(Root, FText::GetEmpty(), 16, HudGold, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-30.f, -60.f));

	// Subtitles (bottom-centre).
	SubtitleLabel = AddText(Root, FText::GetEmpty(), 16, HudCyan, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -150.f));
	SubtitleLine = AddText(Root, FText::GetEmpty(), 20, FLinearColor::White, FVector2D(0.5f, 1.f), FVector2D(0.5f, 0.f), FVector2D(0.f, -146.f));
	SubtitleLine->SetAutoWrapText(true);
	SubtitleLine->SetWrapTextAt(900.f);

	// Suit wheel: 8 slots on a circle around the crosshair.
	for (int32 i = 0; i < AIronManCharacter::WheelSlots; ++i)
	{
		const float Angle = 2.f * PI * float(i) / float(AIronManCharacter::WheelSlots); // clockwise from the top
		const FVector2D Pos(FMath::Sin(Angle) * 230.f, -FMath::Cos(Angle) * 190.f);
		WheelTexts.Add(AddText(Root, FText::GetEmpty(), 16, FLinearColor::White, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), Pos));
	}
	WheelCenter = AddText(Root, FText::GetEmpty(), 14, HudGold, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 40.f));

	// Story: objective panel under the wave text, ally hint, beacon marker.
	ObjectiveChapter = AddText(Root, FText::GetEmpty(), 15, HudGold, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 24.f));
	ObjectiveText = AddText(Root, FText::GetEmpty(), 20, FLinearColor::White, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 46.f));
	ObjectiveText->SetAutoWrapText(true);
	ObjectiveText->SetWrapTextAt(460.f);
	ObjectiveProgress = AddText(Root, FText::GetEmpty(), 16, HudCyan, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 104.f));
	AllyHint = AddText(Root, FText::GetEmpty(), 16, FLinearColor(0.5f, 1.f, 0.55f), FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-30.f, 130.f));
	BeaconMarker = AddText(Root, FText::GetEmpty(), 16, HudGold, FVector2D(0.f, 0.f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	// Helmet frame: four thin tinted bars + a darker top/bottom band (first person only).
	HelmetFrame.Add(AddFrameBar(Root, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(0.f, 0.f, 0.f, 46.f), FVector2D::ZeroVector));
	HelmetFrame.Add(AddFrameBar(Root, FAnchors(0.f, 1.f, 1.f, 1.f), FMargin(0.f, -46.f, 0.f, 46.f), FVector2D::ZeroVector));
	HelmetFrame.Add(AddFrameBar(Root, FAnchors(0.f, 0.f, 0.f, 1.f), FMargin(0.f, 0.f, 22.f, 0.f), FVector2D::ZeroVector));
	HelmetFrame.Add(AddFrameBar(Root, FAnchors(1.f, 0.f, 1.f, 1.f), FMargin(-22.f, 0.f, 22.f, 0.f), FVector2D::ZeroVector));
}

UBorder* UArenaHUDWidget::AddFrameBar(UCanvasPanel* Root, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
	B->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(B))
	{
		S->SetAnchors(Anchors);
		S->SetOffsets(Offsets);
		S->SetAlignment(Alignment);
		S->SetZOrder(-1);
	}
	return B;
}

void UArenaHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!HealthBar || !RootPanel) { return; }

	Pulse += InDeltaTime;
	UWorld* World = GetWorld();
	const AArenaGameMode* GM = World ? World->GetAuthGameMode<AArenaGameMode>() : nullptr;
	const AIronManCharacter* Player = Cast<AIronManCharacter>(GetOwningPlayerPawn());

	const bool bPlaying = GM && GM->GetArenaState() != EArenaGameState::Menu;
	const bool bCinematic = Player && Player->IsInCinematic(); // suit-up and story cutscenes own the screen
	RootPanel->SetVisibility(bPlaying && !bCinematic ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bPlaying || bCinematic) { return; }
	const bool bStory = GM->GetPlayMode() == EArenaPlayMode::Story;

	if (Player)
	{
		const float HP = Player->GetHealth();
		const float MaxHP = FMath::Max(1.f, Player->GetMaxHealth());
		HealthBar->SetPercent(HP / MaxHP);
		const bool bLow = HP / MaxHP < 0.3f;
		const float Blink = (bLow && FMath::Fmod(Pulse, 0.5f) < 0.25f) ? 1.f : 0.75f;
		HealthBar->SetFillColorAndOpacity(Player->IsHulkbusterActive() ? HudGold : HudRed * Blink);
		HealthText->SetText(FText::AsNumber(FMath::CeilToInt(HP)));

		const float EN = Player->GetEnergy();
		EnergyBar->SetPercent(EN / FMath::Max(1.f, Player->GetMaxEnergy()));
		EnergyBar->SetFillColorAndOpacity(Player->IsFlightLocked() ? FLinearColor(0.4f, 0.4f, 0.45f) : HudCyan);
		EnergyText->SetText(FText::AsNumber(FMath::FloorToInt(EN)));

		if (Player->IsFlightLocked())
		{
			FlightText->SetText(LOCTEXT("Locked", "THRUSTERS OFFLINE - LAND TO RECHARGE"));
			FlightText->SetColorAndOpacity(FSlateColor(HudRed));
		}
		else
		{
			FlightText->SetText(Player->IsFlying() ? LOCTEXT("Flying", "FLIGHT [F]") : LOCTEXT("Ground", "GROUND - RECHARGING [F]"));
			FlightText->SetColorAndOpacity(FSlateColor(HudCyan));
		}

		if (Player->IsHulkbusterActive())
		{
			HulkbusterText->SetText(FText::Format(LOCTEXT("BusterOn", "HULKBUSTER  {0}s"), FText::AsNumber(FMath::CeilToInt(Player->GetHulkbusterTimeLeft()))));
		}
		else if (Player->GetHulkbusterCharges() > 0)
		{
			const bool bFlash = FMath::Fmod(Pulse, 0.8f) < 0.4f;
			HulkbusterText->SetText(FText::Format(LOCTEXT("BusterReady", "HULKBUSTER READY [Q]  x{0}"), FText::AsNumber(Player->GetHulkbusterCharges())));
			HulkbusterText->SetColorAndOpacity(FSlateColor(bFlash ? FLinearColor::White : HudGold));
		}
		else if (bStory)
		{
			HulkbusterText->SetText(FText::GetEmpty()); // story grants charges per objective
		}
		else
		{
			HulkbusterText->SetText(FText::Format(LOCTEXT("BusterIn", "HULKBUSTER IN {0} PTS"), FText::AsNumber(GM->GetPointsToNextHulkbuster())));
			HulkbusterText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
		}
		SuitText->SetText(Player->GetSuit().Name);
		Crosshair->SetVisibility(Player->IsDead() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	ScoreText->SetText(FText::AsNumber(GM->GetScore()));
	HighScoreText->SetText(FText::Format(LOCTEXT("HighHud", "HIGH {0}"), FText::AsNumber(FMath::Max(GM->GetHighScore(), GM->GetScore()))));
	if (GM->GetCombo() > 1)
	{
		ComboText->SetText(FText::Format(LOCTEXT("Combo", "COMBO x{0}"), FText::AsNumber(GM->GetCombo())));
		ComboText->SetRenderOpacity(0.35f + 0.65f * GM->GetComboTimeFraction());
		ComboText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ComboText->SetVisibility(ESlateVisibility::Collapsed);
	}

	WaveText->SetVisibility(bStory ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	WaveText->SetText(FText::Format(LOCTEXT("WaveHud", "WAVE {0}\nHOSTILES {1}"), FText::AsNumber(GM->GetWave()), FText::AsNumber(GM->GetHostilesRemaining())));
	TickStory(bStory ? GM : nullptr, Player);
	TickReactor(Player);
	TickSubtitles(Player);
	TickWheel(Player);
	TickHelmet(Player);

	if (const ABossTitan* Boss = GM->GetActiveBoss())
	{
		BossBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		BossText->SetVisibility(ESlateVisibility::HitTestInvisible);
		BossBar->SetPercent(Boss->GetHealth() / FMath::Max(1.f, Boss->GetMaxHealth()));
		BossBar->SetFillColorAndOpacity(Boss->IsInvulnerable() ? FLinearColor(0.5f, 0.5f, 0.6f) : FLinearColor(0.6f, 0.2f, 1.f));
		BossText->SetText(FText::Format(LOCTEXT("BossHud", "{0}  -  PHASE {1}"), Boss->GetBossName(), FText::AsNumber(Boss->GetPhase())));
	}
	else
	{
		BossBar->SetVisibility(ESlateVisibility::Collapsed);
		BossText->SetVisibility(ESlateVisibility::Collapsed);
	}

	FText Title, Sub;
	float Alpha = 0.f;
	if (GM->GetBanner(Title, Sub, Alpha))
	{
		BannerTitle->SetText(Title);
		BannerSubtitle->SetText(Sub);
		BannerTitle->SetRenderOpacity(Alpha);
		BannerSubtitle->SetRenderOpacity(Alpha);
		BannerTitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		BannerSubtitle->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		BannerTitle->SetVisibility(ESlateVisibility::Collapsed);
		BannerSubtitle->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ------------------------------------------------------------------------------------------------

void UArenaHUDWidget::TickReactor(const AIronManCharacter* Player)
{
	const UReactorComponent* R = Player ? Player->GetReactor() : nullptr;
	if (!R)
	{
		HeatBar->SetVisibility(ESlateVisibility::Collapsed);
		HeatText->SetVisibility(ESlateVisibility::Collapsed);
		ReactorText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	HeatBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	HeatText->SetVisibility(ESlateVisibility::HitTestInvisible);
	ReactorText->SetVisibility(ESlateVisibility::HitTestInvisible);

	const float Heat = FMath::Clamp(R->GetHeatFraction(), 0.f, 1.f);
	HeatBar->SetPercent(Heat);
	const bool bBlink = FMath::Fmod(Pulse, 0.4f) < 0.2f;
	FLinearColor HeatColor = FMath::Lerp(FLinearColor(1.f, 0.75f, 0.2f), FLinearColor(1.f, 0.15f, 0.05f), Heat);
	if (R->IsOverheated() && bBlink) { HeatColor = FLinearColor::White; }
	HeatBar->SetFillColorAndOpacity(HeatColor);

	FText HeatLine;
	if (R->IsCoreDark()) { HeatLine = LOCTEXT("CoreDark", "CORE DARK"); }
	else if (R->IsVenting()) { HeatLine = LOCTEXT("Venting", "VENTING..."); }
	else if (R->IsOverheated()) { HeatLine = LOCTEXT("Overheat", "OVERHEAT - VENT [X]"); }
	else if (R->GetVentCooldownLeft() > 0.f) { HeatLine = FText::Format(LOCTEXT("VentCd", "HEAT {0}%  vent in {1}s"), FText::AsNumber(FMath::RoundToInt(Heat * 100.f)), FText::AsNumber(FMath::CeilToInt(R->GetVentCooldownLeft()))); }
	else { HeatLine = FText::Format(Heat > 0.6f ? LOCTEXT("HeatHigh", "HEAT {0}%  [X] VENT") : LOCTEXT("Heat", "HEAT {0}%"), FText::AsNumber(FMath::RoundToInt(Heat * 100.f))); }
	HeatText->SetText(HeatLine);

	FText Stage = LOCTEXT("ReactorArena", "ARC REACTOR");
	if (R->HasStage())
	{
		const UEnum* E = StaticEnum<EReactorStage>();
		const FText StageName = E ? E->GetDisplayNameTextByValue(int64(R->GetStage())) : FText::GetEmpty();
		Stage = FText::Format(LOCTEXT("ReactorStage", "MAKESHIFT REACTOR - {0}"), StageName);
	}
	ReactorText->SetText(Stage);
	const float Inst = R->GetInstability();
	ReactorText->SetColorAndOpacity(FSlateColor(Inst > 0.01f && bBlink ? FLinearColor(1.f, 0.6f, 0.2f) : HudCyan));
}

void UArenaHUDWidget::TickSubtitles(const AIronManCharacter* Player)
{
	const UArenaGameUserSettings* Settings = UArenaGameUserSettings::GetArenaSettings();
	const bool bWanted = !Settings || Settings->bSubtitles;
	const UJarvisComponent* J = UJarvisComponent::Get(this);
	FText Label, Line;
	FLinearColor Color = HudCyan;
	float Alpha = 0.f;
	if (bWanted && J && J->GetSubtitle(Label, Line, Color, Alpha) && Alpha > 0.01f)
	{
		SubtitleLabel->SetText(Label);
		SubtitleLabel->SetColorAndOpacity(FSlateColor(Color));
		SubtitleLine->SetText(Line);
		SubtitleLabel->SetRenderOpacity(Alpha);
		SubtitleLine->SetRenderOpacity(Alpha);
		SubtitleLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		SubtitleLine->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		SubtitleLabel->SetVisibility(ESlateVisibility::Collapsed);
		SubtitleLine->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UArenaHUDWidget::TickWheel(const AIronManCharacter* Player)
{
	const bool bOpen = Player && Player->IsSuitWheelOpen();
	WheelCenter->SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Crosshair->SetRenderOpacity(bOpen ? 0.2f : 1.f);
	for (int32 i = 0; i < WheelTexts.Num(); ++i)
	{
		UTextBlock* T = WheelTexts[i];
		if (!T) { continue; }
		const int32 Suit = bOpen ? Player->GetWheelSuit(i) : INDEX_NONE;
		if (Suit == INDEX_NONE)
		{
			T->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FSuitDef& Def = USuitLibrary::GetSuit(Suit);
		const bool bSelected = Player->GetWheelSelection() == i;
		const bool bCurrent = Player->GetSuitIndex() == Suit;
		T->SetText(bSelected ? FText::Format(LOCTEXT("WheelSel", "[ {0} ]"), Def.Name) : Def.Name);
		T->SetColorAndOpacity(FSlateColor(bSelected ? HudGold : (bCurrent ? HudCyan : FLinearColor::White)));
		T->SetRenderScale(FVector2D(bSelected ? 1.15f : 1.f));
		T->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (bOpen)
	{
		const int32 Pages = FMath::Max(1, Player->GetWheelPageCount());
		const int32 Sel = Player->GetWheelSelection();
		const int32 SelSuit = Sel != INDEX_NONE ? Player->GetWheelSuit(Sel) : INDEX_NONE;
		const FText Mark = SelSuit != INDEX_NONE ? USuitLibrary::GetSuit(SelSuit).Mark : FText::GetEmpty();
		WheelCenter->SetText(FText::Format(LOCTEXT("WheelPage", "{0}\nPAGE {1}/{2}  (wheel / Tab)"), Mark, FText::AsNumber(Player->GetWheelPage() + 1), FText::AsNumber(Pages)));
	}
}

void UArenaHUDWidget::SetSubtitleScale(float InScale)
{
	SubtitleScale = FMath::Clamp(InScale, 0.8f, 1.6f);
	if (SubtitleLabel)
	{
		FSlateFontInfo F = SubtitleLabel->GetFont();
		F.Size = FMath::RoundToFloat(16.f * SubtitleScale);
		SubtitleLabel->SetFont(F);
	}
	if (SubtitleLine)
	{
		FSlateFontInfo F = SubtitleLine->GetFont();
		F.Size = FMath::RoundToFloat(20.f * SubtitleScale);
		SubtitleLine->SetFont(F);
		SubtitleLine->SetWrapTextAt(900.f * SubtitleScale);
	}
}

void UArenaHUDWidget::TickHelmet(const AIronManCharacter* Player)
{
	const bool bFirstPersonHelmet = Player && Player->IsFirstPerson() && !Player->IsHulkbusterActive();
	// EDITH's helmet HUD draws its own visor, power/heat and integrity readouts in first person.
	bGaugesHandedOff = bFirstPersonHelmet && bHelmetHUDPresent;
	const ESlateVisibility GaugeVis = bGaugesHandedOff ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	for (UWidget* W : TArray<UWidget*>{ HealthBar, HealthText, EnergyBar, EnergyText, FlightText })
	{
		if (W) { W->SetVisibility(GaugeVis); }
	}
	if (bGaugesHandedOff)
	{
		for (UWidget* W : TArray<UWidget*>{ HeatBar, HeatText, ReactorText }) { if (W) { W->SetVisibility(ESlateVisibility::Collapsed); } }
	}
	const bool bShow = bFirstPersonHelmet && !bGaugesHandedOff;
	FLinearColor Tint = USuitLibrary::GetPersona(Player ? Player->GetSuit().AI : EAIPersona::Jarvis).HudTint;
	Tint.A = 0.18f + 0.04f * FMath::Sin(Pulse * 2.f);
	for (UBorder* B : HelmetFrame)
	{
		if (!B) { continue; }
		B->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		B->SetBrushColor(Tint);
	}
	if (bShow)
	{
		Crosshair->SetColorAndOpacity(FSlateColor(FLinearColor(Tint.R, Tint.G, Tint.B, 0.95f)));
	}
	else
	{
		Crosshair->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.95f, 1.f, 0.9f)));
	}
}

void UArenaHUDWidget::TickStory(const AArenaGameMode* GM, const AIronManCharacter* Player)
{
	const AStoryDirector* Director = GM ? GM->GetStoryDirector() : nullptr;
	FText Chapter, Objective, Progress;
	const bool bHas = Director && Director->IsActive() && Director->GetObjectiveInfo(Chapter, Objective, Progress);
	const ESlateVisibility Vis = bHas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	ObjectiveChapter->SetVisibility(Vis);
	ObjectiveText->SetVisibility(Vis);
	ObjectiveProgress->SetVisibility(Vis);
	if (bHas)
	{
		ObjectiveChapter->SetText(Chapter);
		ObjectiveText->SetText(Objective);
		ObjectiveProgress->SetText(Progress);
	}

	const FText Hint = Director ? Director->GetAllyHint() : FText::GetEmpty();
	AllyHint->SetText(Hint);
	AllyHint->SetVisibility(Hint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	FVector Beacon;
	APlayerController* PC = GetOwningPlayer();
	FVector2D Screen;
	if (Director && PC && Player && Director->GetBeaconLocation(Beacon)
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Beacon, Screen, false))
	{
		const float Meters = FVector::Dist(Player->GetActorLocation(), Beacon) / 100.f;
		BeaconMarker->SetText(FText::Format(LOCTEXT("Beacon", "<>\n{0} m"), FText::AsNumber(FMath::RoundToInt(Meters))));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(BeaconMarker->Slot)) { S->SetPosition(Screen); }
		BeaconMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		BeaconMarker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
