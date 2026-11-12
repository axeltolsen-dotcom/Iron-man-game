#include "HelmetHUDWidget.h"

#include "IronManArena.h"
#include "ArenaGameMode.h"
#include "ArenaGameUserSettings.h"
#include "BossTitan.h"
#include "CityLayout.h"
#include "DamageArcMath.h"
#include "DamageDirectionWidget.h"
#include "DowntownDistrict.h"
#include "HUDDataSubsystem.h"
#include "EnemyBase.h"
#include "IronManCharacter.h"
#include "JarvisComponent.h"
#include "Pickup.h"
#include "ReactorComponent.h"
#include "StoryAlly.h"
#include "StoryDirector.h"
#include "SuitTypes.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "UObject/UnrealType.h"

#include "ArenaKeyBindings.h"

namespace
{
	/** Vent key shown on the helmet: an explicit VentKeyLabel other than the default "X" wins, else the Settings > CONTROLS binding. */
	FString HelmetVentKeyText(const FText& Explicit)
	{
		const FString E = Explicit.ToString();
		if (!E.IsEmpty() && !E.Equals(TEXT("X"), ESearchCase::CaseSensitive)) { return E; }
		return ArenaKeys::ShortLabel(TEXT("Vent"));
	}
}


#define LOCTEXT_NAMESPACE "HelmetHUDWidget"

namespace HelmetHUD
{
	// Element ids used by code (hud_layout.json ids).
	const FName Id_VisorFrame(TEXT("VisorFrame"));
	const FName Id_WarningVignette(TEXT("WarningVignette"));
	const FName Id_PitchLadder(TEXT("PitchLadder"));
	const FName Id_HeadingTape(TEXT("HeadingTape"));
	const FName Id_SpeedTape(TEXT("SpeedTape"));
	const FName Id_AltitudeTape(TEXT("AltitudeTape"));
	const FName Id_TargetLayer(TEXT("TargetLayer"));
	const FName Id_Reticle(TEXT("Reticle"));
	const FName Id_IntegrityDiagram(TEXT("IntegrityDiagram"));
	const FName Id_HeatGauge(TEXT("HeatGauge"));
	const FName Id_WarningStack(TEXT("WarningStack"));
	const FName Id_SubtitleBar(TEXT("SubtitleBar"));
	const FName Id_CommsPortrait(TEXT("CommsPortrait"));
	const FName Id_VisorNoise(TEXT("VisorNoise"));
	const FName Id_GlitchOverlay(TEXT("GlitchOverlay"));

	// Material assets and parameters (hud_spec.md section 15.2).
	const FName MI_VisorWarp(TEXT("MI_HUD_VisorWarp"));
	const FName MI_Bracket(TEXT("MI_HUD_Bracket"));
	const FName P_Texture(TEXT("Texture"));
	const FName P_Fill(TEXT("Fill"));
	const FName P_FillColor(TEXT("FillColor"));
	const FName P_BackColor(TEXT("BackColor"));
	const FName P_SegmentCount(TEXT("SegmentCount"));
	const FName P_WarnStart(TEXT("WarnStart"));
	const FName P_CritStart(TEXT("CritStart"));
	const FName P_RecoverMarker(TEXT("RecoverMarker"));
	const FName P_Pulse(TEXT("Pulse"));
	const FName P_Jitter(TEXT("Jitter"));
	const FName P_Dropout(TEXT("Dropout"));
	const FName P_GlowColor(TEXT("GlowColor"));
	const FName P_Chevrons(TEXT("Chevrons"));
	const FName P_GlitchAmount(TEXT("GlitchAmount"));
	const FName P_TearCount(TEXT("TearCount"));
	const FName P_TearSeed(TEXT("TearSeed"));
	const FName P_BlockSize(TEXT("BlockSize"));
	const FName P_Desaturate(TEXT("Desaturate"));
	const FName P_Blackout(TEXT("Blackout"));
	const FName P_ScanlineDensity(TEXT("ScanlineDensity"));
	const FName P_ScanlineIntensity(TEXT("ScanlineIntensity"));
	const FName P_NoiseIntensity(TEXT("NoiseIntensity"));
	const FName P_NoiseFPS(TEXT("NoiseFPS"));
	const FName P_ChromaticEdgePx(TEXT("ChromaticEdgePx"));
	const FName P_PersonaTint(TEXT("PersonaTint"));
	const FName P_VignetteColor(TEXT("VignetteColor"));
	const FName P_VignetteIntensity(TEXT("VignetteIntensity"));
	const FName P_Color(TEXT("Color"));
	const FName P_Progress(TEXT("Progress"));
	const FName P_State(TEXT("State"));
	const FName P_LockProgress(TEXT("LockProgress"));
	const FName P_Spread(TEXT("Spread"));
	const FName P_Tint(TEXT("Tint"));
	const FName P_GridAlpha(TEXT("GridAlpha"));
	const FName P_SweepAngle(TEXT("SweepAngle"));
	const FName P_RingCount(TEXT("RingCount"));
	const FName P_GlyphIndex(TEXT("GlyphIndex"));
	const FName P_Speaking(TEXT("Speaking"));
	const FName P_Morph(TEXT("Morph"));
	const FName P_Damage(TEXT("Damage"));
	const FName P_Hatch(TEXT("Hatch"));
	const FName P_Flash(TEXT("Flash"));
	const FName P_PixelSize(TEXT("PixelSize"));
	const FName P_SignalLoss(TEXT("SignalLoss"));
	const FName P_Invulnerable(TEXT("Invulnerable"));
	const FName P_FrameColor(TEXT("FrameColor"));

	// Colour keys (hud_layout.json colors).
	const FName C_Neutral(TEXT("neutralText"));
	const FName C_Shadow(TEXT("shadow"));
	const FName C_PanelBg(TEXT("panelBg"));
	const FName C_TickDim(TEXT("tickDim"));
	const FName C_Caution(TEXT("caution"));
	const FName C_Warning(TEXT("warning"));
	const FName C_Critical(TEXT("critical"));
	const FName C_OverheatWhite(TEXT("overheatWhite"));
	const FName C_MorganComms(TEXT("morganComms"));
	const FName C_Ally(TEXT("ally"));
	const FName C_Objective(TEXT("objective"));
	const FName C_ThreatDrone(TEXT("threatDrone"));
	const FName C_ThreatTank(TEXT("threatTank"));
	const FName C_ThreatBoss(TEXT("threatBoss"));
	const FName C_ThreatProjectile(TEXT("threatProjectile"));
	const FName C_Pickup(TEXT("pickup"));
	const FName C_IntegrityDamaged(TEXT("integrityDamaged"));
	const FName C_IntegrityCritical(TEXT("integrityCritical"));
	const FName C_IntegrityOffline(TEXT("integrityOffline"));
	const FName C_LockAcquire(TEXT("lockAcquire"));
	const FName C_LockTracking(TEXT("lockTracking"));
	const FName C_LockLocked(TEXT("lockLocked"));

	// Warning ids (hud_layout.json warnings).
	const FName W_SuitCritical(TEXT("SuitCritical"));
	const FName W_Overheat(TEXT("Overheat"));
	const FName W_MissileLock(TEXT("MissileLock"));
	const FName W_ArmorCritical(TEXT("ArmorCritical"));
	const FName W_VentNow(TEXT("VentNow"));
	const FName W_CoreDark(TEXT("CoreDark"));
	const FName W_LowPower(TEXT("LowPower"));
	const FName W_PullUp(TEXT("PullUp"));

	// Animation keys (hud_layout.json animations).
	const FName A_Boot(TEXT("bootSequence"));
	const FName A_BracketAcquire(TEXT("bracketAcquire"));
	const FName A_BracketLockSnap(TEXT("bracketLockSnap"));
	const FName A_BracketLockedPulse(TEXT("bracketLockedPulse"));
	const FName A_WarningIn(TEXT("warningIn"));
	const FName A_WarningOut(TEXT("warningOut"));
	const FName A_CommsRing(TEXT("commsIncomingRing"));
	const FName A_CommsOpen(TEXT("commsOpen"));
	const FName A_CommsClose(TEXT("commsClose"));
	const FName A_SubtitleIn(TEXT("subtitleIn"));
	const FName A_SubtitleOut(TEXT("subtitleOut"));
	const FName A_PersonaSwap(TEXT("personaSwap"));
	const FName A_HeatPulseWarning(TEXT("heatPulseWarning"));
	const FName A_HeatPulseCritical(TEXT("heatPulseCritical"));
	const FName A_IntegrityHitFlash(TEXT("integrityHitFlash"));
	const FName A_GlitchTear(TEXT("glitchTear"));
	const FName A_GlitchBlackout(TEXT("glitchBlackout"));
	const FName A_ImpactKick(TEXT("impactKick"));
	const FName A_LowPowerBrownout(TEXT("lowPowerBrownout"));

	constexpr float SpeedOfSoundMs = 343.f;
	constexpr int32 NumRegions = (int32)EHUDBodyRegion::Count;
	constexpr float WarningSlotSpacing = 44.f;

	/** Stable pseudo-random value in [-1, 1] for an integer step (frame-rate independent jitter). */
	float Hash11(int32 Step, int32 Salt)
	{
		uint32 H = uint32(Step) * 747796405u + uint32(Salt) * 2891336453u;
		H = ((H >> ((H >> 28u) + 4u)) ^ H) * 277803737u;
		H = (H >> 22u) ^ H;
		return float(H & 0xFFFFu) / 32767.5f - 1.f;
	}

	FLinearColor WithAlpha(const FLinearColor& InColor, float Alpha)
	{
		FLinearColor Out = InColor;
		Out.A = Alpha;
		return Out;
	}

	FName RobotoTypeface(FName Typeface)
	{
		const FString T = Typeface.ToString();
		if (T.Contains(TEXT("Bold")) || T.Contains(TEXT("Semi")) || T.Contains(TEXT("Medium")))
		{
			return FName(TEXT("Bold"));
		}
		if (T.Contains(TEXT("Light")))
		{
			return FName(TEXT("Light"));
		}
		return FName(TEXT("Regular"));
	}

	FSlateBrush MakeOutlineBrush(float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f), FSlateColor(FLinearColor::White), Width);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}

	EHUDThreatType ThreatFromKind(EEnemyKind Kind)
	{
		switch (Kind)
		{
		case EEnemyKind::Tank: return EHUDThreatType::Tank;
		case EEnemyKind::Boss: return EHUDThreatType::Boss;
		case EEnemyKind::Drone:
		default:               return EHUDThreatType::Drone;
		}
	}

	bool IsHostile(EHUDThreatType Type)
	{
		return Type == EHUDThreatType::Drone || Type == EHUDThreatType::Tank || Type == EHUDThreatType::Boss || Type == EHUDThreatType::Projectile;
	}
}

using namespace HelmetHUD;

// ================================================================================================
// Setup
// ================================================================================================

void UHelmetHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Config.Elements.Num() == 0)
	{
		LoadLayoutConfig();
	}
	if (VentKeyLabel.IsEmpty())
	{
		VentKeyLabel = LOCTEXT("VentKeyDefault", "X");
	}
	for (int32 i = 0; i < NumRegions; ++i)
	{
		RegionFlashStart[i] = -100.f;
		RegionDip[i] = 0.f;
	}
	GlitchRng.Initialize(0x4D4F5247);
	if (const UArenaGameUserSettings* Settings = UArenaGameUserSettings::GetArenaSettings())
	{
		HudState.bSubtitlesEnabled = Settings->bSubtitles;
	}
	CacheBindings();
}

TSharedRef<SWidget> UHelmetHUDWidget::RebuildWidget()
{
	// Same pattern as UArenaHUDWidget: a pure C++ instance has an empty WidgetTree, so build one.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	// HUD scale as a layout scale: children get (viewport / scale) of room and keep their anchors.
	return SNew(SDPIScaler)
		.DPIScale(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(this, &UHelmetHUDWidget::GetHudScale)))
		[
			Super::RebuildWidget()
		];
}

void UHelmetHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CacheBindings();
	PlayBootSequence();
}

void UHelmetHUDWidget::LoadLayoutConfig()
{
	const FString Path = LayoutPathOverride.IsEmpty() ? FHelmetHUDConfig::DefaultPath() : LayoutPathOverride;
	Config = FHelmetHUDConfig::Load(Path);
	for (int32 i = Config.BootStaggerS.Num(); i < 4; ++i)
	{
		Config.BootStaggerS.Add(0.f);
	}

	const FName MonoKey(TEXT("HUD_Mono"));
	if (UObject* Mono = LoadHUDFont(MonoKey))
	{
		PaintFont = FSlateFontInfo(Mono, 9.f);
	}
	else
	{
		PaintFont = FCoreStyle::GetDefaultFontStyle(FName(TEXT("Regular")), 9);
	}
}

void UHelmetHUDWidget::ReloadLayout()
{
	LoadLayoutConfig();
	if (bTreeBuiltInCode)
	{
		for (const FHUDElementDef& Def : Config.Elements)
		{
			UWidget* Widget = FindBoundWidget(Def.Id);
			ApplyElementSlot(Widget, Def);
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				ApplyElementFont(Text, Def, 0);
			}
		}
	}
	FontPersona = -1;
	CacheBindings();
}

UMaterialInterface* UHelmetHUDWidget::LoadHUDMaterial(FName MaterialName) const
{
	if (MaterialName.IsNone() || MaterialDirectory.IsEmpty())
	{
		return nullptr;
	}
	FString Dir = MaterialDirectory;
	if (!Dir.EndsWith(TEXT("/")))
	{
		Dir += TEXT("/");
	}
	const FString PackagePath = Dir + MaterialName.ToString();
	if (!FPackageName::IsValidLongPackageName(PackagePath) || !FPackageName::DoesPackageExist(PackagePath))
	{
		return nullptr;
	}
	return LoadObject<UMaterialInterface>(nullptr, *(PackagePath + TEXT(".") + MaterialName.ToString()));
}

UObject* UHelmetHUDWidget::LoadHUDFont(FName FontKey) const
{
	const FHUDFontDef* Def = Config.Fonts.Find(FontKey);
	if (!Def || Def->Asset.IsEmpty())
	{
		return nullptr;
	}
	if (!FPackageName::IsValidLongPackageName(Def->Asset) || !FPackageName::DoesPackageExist(Def->Asset))
	{
		return nullptr;
	}
	return LoadObject<UFont>(nullptr, *(Def->Asset + TEXT(".") + FPackageName::GetShortName(Def->Asset)));
}

UWidget* UHelmetHUDWidget::FindBoundWidget(FName Id) const
{
	const FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(GetClass(), Id);
	if (!Prop)
	{
		return nullptr;
	}
	return Cast<UWidget>(Prop->GetObjectPropertyValue_InContainer(this));
}

void UHelmetHUDWidget::ApplyElementSlot(UWidget* Widget, const FHUDElementDef& Def) const
{
	UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	if (!CanvasSlot)
	{
		return;
	}
	CanvasSlot->SetAnchors(FAnchors(float(Def.AnchorMin.X), float(Def.AnchorMin.Y), float(Def.AnchorMax.X), float(Def.AnchorMax.Y)));
	CanvasSlot->SetAlignment(Def.Alignment);
	CanvasSlot->SetOffsets(FMargin(float(Def.Position.X), float(Def.Position.Y), float(Def.Extent.X), float(Def.Extent.Y)));
	CanvasSlot->SetZOrder(Def.ZOrder);
}

void UHelmetHUDWidget::ApplyElementFont(UTextBlock* Text, const FHUDElementDef& Def, int32 ExtraSpacing) const
{
	if (!Text)
	{
		return;
	}
	const float FontSize = Def.FontSize > 0 ? float(Def.FontSize) : 14.f;
	FSlateFontInfo Font;
	if (UObject* FontObject = LoadHUDFont(Def.FontKey))
	{
		Font = FSlateFontInfo(FontObject, FontSize, Def.Typeface);
	}
	else
	{
		Font = FCoreStyle::GetDefaultFontStyle(RobotoTypeface(Def.Typeface), FMath::RoundToInt(FontSize));
	}
	Font.LetterSpacing = Def.LetterSpacing + ExtraSpacing;
	Text->SetFont(Font);
}

UWidget* UHelmetHUDWidget::CreateElementWidget(const FHUDElementDef& Def)
{
	if (!WidgetTree)
	{
		return nullptr;
	}
	static const FName TextBlockName(TEXT("UTextBlock"));
	static const FName ImageName(TEXT("UImage"));

	if (Def.BindAs == TextBlockName)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Def.Id);
		ApplyElementFont(Text, Def, 0);
		Text->SetColorAndOpacity(FSlateColor(Col(C_Neutral)));
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(Col(C_Shadow));
		Text->SetJustification(Def.Alignment.X > 0.75 ? ETextJustify::Right : (Def.Alignment.X > 0.25 ? ETextJustify::Center : ETextJustify::Left));
		Text->SetText(FText::GetEmpty());
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}

	if (Def.BindAs == ImageName)
	{
		UMaterialInterface* Material = LoadHUDMaterial(Def.Material);
		const bool bFullScreen = Def.Parent.IsNone();
		if (bFullScreen && !Material)
		{
			// A full-screen image without its material would be a solid quad over the view.
			return nullptr;
		}
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Def.Id);
		if (Material)
		{
			Image->SetBrushFromMaterial(Material);
		}
		else
		{
			const FString IdStr = Def.Id.ToString();
			if (IdStr.Contains(TEXT("Ring")) || Def.Id == FName(TEXT("ReticleImage")) || Def.Id == FName(TEXT("GlyphImage")) || Def.Id == FName(TEXT("RadarSweep")))
			{
				Image->SetBrush(MakeOutlineBrush(2.f));
			}
			if (Def.Id == FName(TEXT("CommsFeed")))
			{
				Image->SetColorAndOpacity(Col(C_PanelBg));
			}
		}
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Def.Id);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Panel;
}

void UHelmetHUDWidget::BuildTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (Config.Elements.Num() == 0)
	{
		LoadLayoutConfig();
	}

	UCanvasPanel* OuterCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	OuterCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = OuterCanvas;
	bTreeBuiltInCode = true;

	auto StretchSlot = [](UCanvasPanelSlot* CanvasSlot, int32 Z)
	{
		if (CanvasSlot)
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f));
			CanvasSlot->SetZOrder(Z);
		}
	};

	// Option A (spec 2.4): everything except VisorFrame inside one retainer with the warp material.
	// Without MI_HUD_VisorWarp there is no retainer (no extra render target) and VisorNoise is used.
	if (UMaterialInterface* WarpMaterial = LoadHUDMaterial(MI_VisorWarp))
	{
		HUDRetainer = WidgetTree->ConstructWidget<URetainerBox>(URetainerBox::StaticClass(), TEXT("HUDRetainer"));
		HUDRetainer->SetVisibility(ESlateVisibility::HitTestInvisible);
		HUDRetainer->SetEffectMaterial(WarpMaterial);
		HUDRetainer->SetTextureParameter(P_Texture);
		StretchSlot(OuterCanvas->AddChildToCanvas(HUDRetainer), 1);
	}
	HUDSafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("HUDSafeZone"));
	HUDSafeZone->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (HUDRetainer)
	{
		HUDRetainer->SetContent(HUDSafeZone);
	}
	else
	{
		StretchSlot(OuterCanvas->AddChildToCanvas(HUDSafeZone), 1);
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	HUDSafeZone->SetContent(RootCanvas);

	TMap<FName, UCanvasPanel*> Containers;
	for (const FHUDElementDef& Def : Config.Elements)
	{
		UCanvasPanel* Parent = nullptr;
		if (Def.Parent.IsNone())
		{
			Parent = (Def.Id == Id_VisorFrame) ? OuterCanvas : RootCanvas.Get();
		}
		else
		{
			Parent = Containers.FindRef(Def.Parent);
		}
		if (!Parent)
		{
			continue;
		}
		UWidget* Widget = CreateElementWidget(Def);
		if (!Widget)
		{
			continue;
		}
		Parent->AddChildToCanvas(Widget);
		ApplyElementSlot(Widget, Def);
		if (UCanvasPanel* AsPanel = Cast<UCanvasPanel>(Widget))
		{
			Containers.Add(Def.Id, AsPanel);
			if (Def.Id == Id_SubtitleBar)
			{
				// Subtitle backing plate (spec 13: background opacity 0.55).
				UImage* Plate = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SubtitlePlate"));
				Plate->SetColorAndOpacity(WithAlpha(Col(C_PanelBg), Config.SubtitleBackgroundOpacity));
				Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
				StretchSlot(AsPanel->AddChildToCanvas(Plate), -1);
			}
		}
		if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(GetClass(), Def.Id))
		{
			if (Widget->IsA(Prop->PropertyClass))
			{
				Prop->SetObjectPropertyValue_InContainer(this, Widget);
			}
		}
	}
	CacheBindings();
}

void UHelmetHUDWidget::CacheBindings()
{
	ElementWidgets.Reset();
	TopElementIdx.Reset();
	TopIndexById.Reset();
	for (int32 i = 0; i < Config.Elements.Num(); ++i)
	{
		const FHUDElementDef& Def = Config.Elements[i];
		UWidget* Widget = FindBoundWidget(Def.Id);
		ElementWidgets.Add(Widget);
		if (Widget && Def.Parent.IsNone())
		{
			TopIndexById.Add(Def.Id, TopElementIdx.Num());
			TopElementIdx.Add(i);
		}
	}
	ParallaxOffset.Init(FVector2D::ZeroVector, TopElementIdx.Num());
	ExtraScale.Init(FVector2D(1.f, 1.f), TopElementIdx.Num());
	ExtraOpacity.Init(1.f, TopElementIdx.Num());

	ImageMIDs.Reset();
	ImagesWithoutMID.Reset();
	TextCache.Reset();
	ColorCache.Reset();
	MIDCache.Reset();
	SetupRetainer();
}

void UHelmetHUDWidget::SetupRetainer()
{
	RetainerMID = nullptr;
	if (!HUDRetainer)
	{
		return;
	}
	if (!HUDRetainer->GetEffectMaterialInterface())
	{
		if (UMaterialInterface* WarpMaterial = LoadHUDMaterial(MI_VisorWarp))
		{
			HUDRetainer->SetEffectMaterial(WarpMaterial);
			HUDRetainer->SetTextureParameter(P_Texture);
		}
	}
	RetainerMID = HUDRetainer->GetEffectMaterial();
	if (RetainerMID)
	{
		for (const TPair<FName, float>& Param : Config.VisorWarp)
		{
			SetScalar(RetainerMID, Param.Key, Param.Value);
		}
	}
}

UImage* UHelmetHUDWidget::PoolImage(TArray<TObjectPtr<UImage>>& Pool, int32 Index, UCanvasPanel* Layer, FName MaterialName, bool bOutline)
{
	while (Pool.Num() <= Index)
	{
		if (!WidgetTree || !Layer)
		{
			return nullptr;
		}
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UMaterialInterface* Material = LoadHUDMaterial(MaterialName))
		{
			Image->SetBrushFromMaterial(Material);
		}
		else if (bOutline)
		{
			Image->SetBrush(MakeOutlineBrush(2.f));
		}
		Image->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* CanvasSlot = Layer->AddChildToCanvas(Image))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		Pool.Add(Image);
	}
	return Pool[Index];
}

// ================================================================================================
// Public API
// ================================================================================================

void UHelmetHUDWidget::UpdateFromState(const FHelmetHUDState& State)
{
	PrevHudState = bHasState ? HudState : State;
	HudState = State;
	bHasState = true;
	bStatePushedThisFrame = true;
}

void UHelmetHUDWidget::PushWarning(FName WarningId, const FText& OverrideText, float HoldSeconds)
{
	if (WarningId.IsNone())
	{
		return;
	}
	const FHUDWarningDef* Def = Config.FindWarning(WarningId);
	FActiveWarning* Existing = ActiveWarnings.FindByPredicate([WarningId](const FActiveWarning& W) { return W.Id == WarningId; });
	const float MinShow = Def ? Def->MinShowS : 2.f;
	if (Existing)
	{
		Existing->ExpireTime = FMath::Max(Existing->ExpireTime, Now() + FMath::Max(HoldSeconds, Config.WarningRefreshExtendsS));
		Existing->LastPushTime = Now();
		Existing->bManual = true;
		if (!OverrideText.IsEmpty())
		{
			Existing->Label = OverrideText;
		}
		return;
	}
	RaiseWarning(WarningId, true, OverrideText.IsEmpty() ? nullptr : &OverrideText);
	if (FActiveWarning* Added = ActiveWarnings.FindByPredicate([WarningId](const FActiveWarning& W) { return W.Id == WarningId; }))
	{
		Added->bManual = true;
		Added->ExpireTime = Now() + FMath::Max(MinShow, HoldSeconds);
	}
}

void UHelmetHUDWidget::ClearWarning(FName WarningId)
{
	for (FActiveWarning& W : ActiveWarnings)
	{
		if (W.Id == WarningId)
		{
			const FHUDWarningDef* Def = Config.FindWarning(WarningId);
			W.bManual = false;
			W.ExpireTime = FMath::Max(W.StartTime + (Def ? Def->MinShowS : 0.f), Now());
		}
	}
}

void UHelmetHUDWidget::SetAccessibility(bool bInColorBlindSafe, bool bInReducedMotion, float InHudOpacity, float InSubtitleScale)
{
	if (bColorBlindSafe != bInColorBlindSafe)
	{
		ColorCache.Reset(); // force colour re-push
	}
	bColorBlindSafe = bInColorBlindSafe;
	bReducedMotion = bInReducedMotion;
	HudOpacityPref = FMath::Clamp(InHudOpacity, 0.5f, 1.f);
	SubtitleScalePref = FMath::Clamp(InSubtitleScale, 0.8f, 1.6f);
}

void UHelmetHUDWidget::PlayBootSequence()
{
	BootStartTime = Now();
	Callouts.OnBoot(this, Now());
}

// ================================================================================================
// Small helpers
// ================================================================================================

FLinearColor UHelmetHUDWidget::Col(FName Key) const
{
	return Config.GetColor(Key, bColorBlindSafe, FLinearColor::White);
}

float UHelmetHUDWidget::AnimAlpha(FName AnimKey, float Elapsed) const
{
	const FHUDAnimDef& Anim = Config.GetAnim(AnimKey);
	if (Anim.DurationS <= KINDA_SMALL_NUMBER)
	{
		return Elapsed >= Anim.DelayS ? 1.f : 0.f;
	}
	return HelmetHUDEase(Anim.Ease, (Elapsed - Anim.DelayS) / Anim.DurationS);
}

float UHelmetHUDWidget::FlashOpacity(float Hz) const
{
	const float Rate = FMath::Min(Hz, Config.FlashMaxHz); // photosensitivity cap (<= 3 Hz)
	if (Rate <= 0.f)
	{
		return 1.f;
	}
	if (bReducedMotion)
	{
		return 0.85f + 0.15f * FMath::Sin(2.f * PI * 1.f * Now()); // steady with a 1 Hz brightness pulse
	}
	return FMath::Frac(Now() * Rate) < 0.5f ? 1.f : 0.35f;
}

bool UHelmetHUDWidget::IsEssential(FName Id) const
{
	return Id == Id_Reticle || Id == Id_IntegrityDiagram || Id == Id_HeatGauge || Id == Id_WarningStack || Id == Id_SubtitleBar;
}

int32 UHelmetHUDWidget::TopIndexOf(FName Id) const
{
	const int32* Found = TopIndexById.Find(Id);
	return Found ? *Found : INDEX_NONE;
}

void UHelmetHUDWidget::SetElementScale(FName Id, const FVector2D& Scale)
{
	const int32 Index = TopIndexOf(Id);
	if (ExtraScale.IsValidIndex(Index))
	{
		ExtraScale[Index] = Scale;
	}
}

void UHelmetHUDWidget::MulElementOpacity(FName Id, float Opacity)
{
	const int32 Index = TopIndexOf(Id);
	if (ExtraOpacity.IsValidIndex(Index))
	{
		ExtraOpacity[Index] *= Opacity;
	}
}

void UHelmetHUDWidget::SetShown(UWidget* Widget, bool bShow)
{
	if (!Widget)
	{
		return;
	}
	const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Widget->GetVisibility() != Wanted)
	{
		Widget->SetVisibility(Wanted);
	}
}

void UHelmetHUDWidget::SetOpacityIfChanged(UWidget* Widget, float Opacity)
{
	if (Widget && !FMath::IsNearlyEqual(Widget->GetRenderOpacity(), Opacity, 0.004f))
	{
		Widget->SetRenderOpacity(Opacity);
	}
}

void UHelmetHUDWidget::SetTextIfChanged(UTextBlock* Text, const FString& Value)
{
	if (!Text)
	{
		return;
	}
	FString& Cached = TextCache.FindOrAdd(Text);
	if (!Cached.Equals(Value, ESearchCase::CaseSensitive) || Value.IsEmpty() != Text->GetText().IsEmpty())
	{
		Cached = Value;
		Text->SetText(FText::FromString(Value));
	}
}

void UHelmetHUDWidget::SetTextIfChanged(UTextBlock* Text, const FText& Value)
{
	if (!Text)
	{
		return;
	}
	const FString AsString = Value.ToString();
	FString& Cached = TextCache.FindOrAdd(Text);
	if (!Cached.Equals(AsString, ESearchCase::CaseSensitive) || AsString.IsEmpty() != Text->GetText().IsEmpty())
	{
		Cached = AsString;
		Text->SetText(Value);
	}
}

void UHelmetHUDWidget::SetTextColorIfChanged(UTextBlock* Text, const FLinearColor& Value)
{
	if (!Text)
	{
		return;
	}
	const FLinearColor* Cached = ColorCache.Find(Text);
	if (!Cached || !Cached->Equals(Value, 0.002f))
	{
		ColorCache.Add(Text, Value);
		Text->SetColorAndOpacity(FSlateColor(Value));
	}
}

void UHelmetHUDWidget::SetImageColorIfChanged(UImage* Image, const FLinearColor& Value)
{
	if (!Image)
	{
		return;
	}
	const FLinearColor* Cached = ColorCache.Find(Image);
	if (!Cached || !Cached->Equals(Value, 0.002f))
	{
		ColorCache.Add(Image, Value);
		Image->SetColorAndOpacity(Value);
	}
}

void UHelmetHUDWidget::SetTransformIfChanged(UWidget* Widget, const FVector2D& Translation, const FVector2D& Scale, float AngleDeg)
{
	if (!Widget)
	{
		return;
	}
	const FWidgetTransform& Current = Widget->GetRenderTransform();
	if (!Current.Translation.Equals(Translation, 0.05) || !Current.Scale.Equals(Scale, 0.001) || !FMath::IsNearlyEqual(Current.Angle, AngleDeg, 0.05f))
	{
		FWidgetTransform NewTransform = Current;
		NewTransform.Translation = Translation;
		NewTransform.Scale = Scale;
		NewTransform.Angle = AngleDeg;
		Widget->SetRenderTransform(NewTransform);
	}
}

UMaterialInstanceDynamic* UHelmetHUDWidget::GetMID(UImage* Image)
{
	if (!Image || ImagesWithoutMID.Contains(Image))
	{
		return nullptr;
	}
	if (const TObjectPtr<UMaterialInstanceDynamic>* Found = ImageMIDs.Find(Image))
	{
		return Found->Get();
	}
	// UImage::GetDynamicMaterial() only works when the brush resource is a material.
	UMaterialInstanceDynamic* MID = Cast<UMaterialInterface>(Image->GetBrush().GetResourceObject()) ? Image->GetDynamicMaterial() : nullptr;
	if (MID)
	{
		ImageMIDs.Add(Image, MID);
	}
	else
	{
		ImagesWithoutMID.Add(Image);
	}
	return MID;
}

void UHelmetHUDWidget::SetScalar(UMaterialInstanceDynamic* MID, FName Param, float Value)
{
	if (!MID)
	{
		return;
	}
	FMIDParamCache& Cache = MIDCache.FindOrAdd(MID);
	const float* Cached = Cache.Scalars.Find(Param);
	if (!Cached || !FMath::IsNearlyEqual(*Cached, Value, 0.0005f))
	{
		Cache.Scalars.Add(Param, Value);
		MID->SetScalarParameterValue(Param, Value);
	}
}

void UHelmetHUDWidget::SetVector(UMaterialInstanceDynamic* MID, FName Param, const FLinearColor& Value)
{
	if (!MID)
	{
		return;
	}
	FMIDParamCache& Cache = MIDCache.FindOrAdd(MID);
	const FLinearColor* Cached = Cache.Vectors.Find(Param);
	if (!Cached || !Cached->Equals(Value, 0.002f))
	{
		Cache.Vectors.Add(Param, Value);
		MID->SetVectorParameterValue(Param, Value);
	}
}

void UHelmetHUDWidget::SetFill(UImage* Image, float Fill, const FLinearColor& FillColor)
{
	if (!Image)
	{
		return;
	}
	const float Clamped = FMath::Clamp(Fill, 0.f, 1.f);
	if (UMaterialInstanceDynamic* MID = GetMID(Image))
	{
		SetScalar(MID, P_Fill, Clamped);
		SetVector(MID, P_FillColor, FillColor);
		return;
	}
	if (!Image->GetRenderTransformPivot().Equals(FVector2D(0.f, 0.5f)))
	{
		Image->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	}
	const FWidgetTransform& Current = Image->GetRenderTransform();
	SetTransformIfChanged(Image, Current.Translation, FVector2D(FMath::Max(Clamped, 0.001f), 1.f), Current.Angle);
	SetImageColorIfChanged(Image, FillColor);
}

void UHelmetHUDWidget::RaiseWarning(FName WarningId, bool bCondition, const FText* OverrideText)
{
	if (!bCondition)
	{
		return;
	}
	FActiveWarning* Existing = ActiveWarnings.FindByPredicate([WarningId](const FActiveWarning& W) { return W.Id == WarningId; });
	if (Existing)
	{
		// Held conditions keep it alive; a re-trigger after a gap extends it (warningStacking.refreshExtendsS).
		const bool bRetrigger = (Now() - Existing->LastPushTime) > 0.25f;
		Existing->ExpireTime = FMath::Max(Existing->ExpireTime, Now() + (bRetrigger ? Config.WarningRefreshExtendsS : 0.05f));
		Existing->LastPushTime = Now();
		return;
	}
	FActiveWarning W;
	W.Id = WarningId;
	W.StartTime = Now();
	W.LastPushTime = Now();
	if (const FHUDWarningDef* Def = Config.FindWarning(WarningId))
	{
		W.Priority = Def->Priority;
		W.ColorKey = Def->ColorKey;
		W.FlashHz = Def->FlashHz;
		W.ExpireTime = Now() + Def->MinShowS;
		FString LabelText = Def->Text.IsEmpty() ? WarningId.ToString().ToUpper() : Def->Text;
		LabelText.ReplaceInline(TEXT("[X]"), *FString::Printf(TEXT("[%s]"), *HelmetVentKeyText(VentKeyLabel)), ESearchCase::CaseSensitive);
		W.Label = FText::FromString(LabelText);
	}
	else
	{
		W.Priority = 7;
		W.ColorKey = C_Neutral;
		W.ExpireTime = Now() + 2.f;
		W.Label = FText::FromName(WarningId);
	}
	if (OverrideText)
	{
		W.Label = *OverrideText;
	}
	ActiveWarnings.Add(W);
}

// ================================================================================================
// Tick
// ================================================================================================

void UHelmetHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Dt = FMath::Clamp(InDeltaTime, 0.f, 0.1f);
	HudTime += Dt;

	if (!bStatePushedThisFrame && bAutoPoll)
	{
		const AIronManCharacter* Player = Cast<AIronManCharacter>(GetOwningPlayerPawn());
		if (bHasState)
		{
			// Swap instead of copying: PrevHudState becomes last frame, HudState's buffers are refilled in place.
			Swap(PrevHudState, HudState);
			FillStateFromGame(this, Player, PrevHudState, HudState, Dt);
		}
		else
		{
			FillStateFromGame(this, Player, HudState, HudState, Dt);
			PrevHudState = HudState;
		}
		bHasState = true;
	}
	bStatePushedThisFrame = false;

	if (bHasState)
	{
		TickHUD(Dt);
		const AIronManCharacter* CalloutPlayer = Cast<AIronManCharacter>(GetOwningPlayerPawn());
		const bool bLive = CalloutPlayer && !CalloutPlayer->IsInCinematic();
		Callouts.Tick(this, CalloutPlayer, HudState, PrevHudState, Config, Now(), bLive);
	}
}

void UHelmetHUDWidget::TickHUD(float DeltaSeconds)
{
	UWidget* RootWidget = WidgetTree ? WidgetTree->RootWidget.Get() : nullptr;
	if (!HudState.bVisible)
	{
		SetShown(RootWidget, false);
		bWasVisible = false;
		return;
	}
	SetShown(RootWidget, true);
	if (!bWasVisible)
	{
		bWasVisible = true;
		PlayBootSequence();
	}

	// A hit: an explicit impact direction, a region hit, or a health drop since last frame.
	const bool bWasHit = !HudState.ImpactDir2D.IsNearlyZero() || HudState.LastHitRegion >= 0
		|| (HudState.HealthFraction < PrevHudState.HealthFraction - 0.001f && !HudState.bDead);
	if (const UHUDDataSubsystem* HitData = UHUDDataSubsystem::Get(this))
	{
		const int32 Serial = HitData->GetHitSerial();
		if (Serial != LastSeenHitSerial)
		{
			LastSeenHitSerial = Serial;
			const FHUDHitReport LastReport = HitData->GetLastHit();
			Callouts.OnPlayerHit(this, LastReport.Damage / FMath::Max(LastReport.MaxHealth, 1.f), int32(LastReport.Region), Now());
		}
	}
	if (bWasHit)
	{
		KickStart = Now();
		KickDir = HudState.ImpactDir2D.IsNearlyZero() ? FVector2D(0.f, -1.f) : HudState.ImpactDir2D.GetSafeNormal();
	}

	for (float& Opacity : ExtraOpacity)
	{
		Opacity = 1.f;
	}
	for (FVector2D& Scale : ExtraScale)
	{
		Scale = FVector2D(1.f, 1.f);
	}

	UpdatePalette(DeltaSeconds);
	UpdatePersona();
	UpdateGlitch(bWasHit);
	UpdateFlight();
	UpdateGauges();
	UpdateIntegrity(bWasHit, DeltaSeconds);
	UpdateTargets(DeltaSeconds);
	UpdateRadarAndWeapon();
	UpdateGame();
	UpdateComms();
	UpdateSubtitle();
	UpdateWarnings();
	UpdateWarningSlots();
	UpdateFX();
	UpdateTransforms(DeltaSeconds);
}

// ================================================================================================
// Palette / persona
// ================================================================================================

void UHelmetHUDWidget::UpdatePalette(float DeltaSeconds)
{
	const int32 Wanted = (int32)HudState.Persona;
	if (ShownPersona < 0)
	{
		ShownPersona = Wanted;
		FromPersona = Wanted;
		PersonaSwapStart = -100.f;
	}
	else if (Wanted != ShownPersona)
	{
		FromPersona = ShownPersona;
		ShownPersona = Wanted;
		PersonaSwapStart = Now();
	}

	// personaSwap: linear crossfade of the palette in linear space.
	const float SwapAlpha = bReducedMotion ? 1.f : AnimAlpha(A_PersonaSwap, Now() - PersonaSwapStart);
	const FHUDPersonaPalette& From = Config.GetPersona(FromPersona);
	const FHUDPersonaPalette& To = Config.GetPersona(ShownPersona);
	PalPrimary = FMath::Lerp(From.Primary, To.Primary, SwapAlpha);
	PalSecondary = FMath::Lerp(From.Secondary, To.Secondary, SwapAlpha);
	PalWarning = FMath::Lerp(From.Warning, To.Warning, SwapAlpha);
	PalCritical = FMath::Lerp(From.Critical, To.Critical, SwapAlpha);

	if (bColorBlindSafe)
	{
		// Colour-blind palette overrides the persona warning/critical hues (spec 13).
		PalWarning = Config.GetColor(C_Warning, true, PalWarning);
		PalCritical = Config.GetColor(C_Critical, true, PalCritical);
	}
	if (HudState.bOverheated)
	{
		PalPrimary = FMath::Lerp(PalPrimary, Col(C_OverheatWhite), 0.2f);
	}
	(void)DeltaSeconds;
}

void UHelmetHUDWidget::UpdatePersona()
{
	const FHUDPersonaPalette& To = Config.GetPersona(ShownPersona);
	const FHUDPersonaPalette& From = Config.GetPersona(FromPersona);
	const float Elapsed = Now() - PersonaSwapStart;
	const float SwapDur = FMath::Max(Config.GetAnim(A_PersonaSwap).DurationS, 0.01f);
	const bool bSwapping = !bReducedMotion && Elapsed < SwapDur;

	// Glyph collapses to a point and regrows; the index switches at the midpoint.
	float GlyphScale = 1.f;
	int32 GlyphIdx = To.GlyphIndex;
	if (bSwapping)
	{
		const float Half = Elapsed / SwapDur;
		GlyphScale = Half < 0.5f ? 1.f - Half * 2.f : (Half - 0.5f) * 2.f;
		GlyphIdx = Half < 0.5f ? From.GlyphIndex : To.GlyphIndex;
	}
	const FHUDAnimDef& SpeakPulse = Config.GetAnim(FName(TEXT("personaSpeakingPulse")));
	const float Speaking = FMath::Clamp(HudState.SpeakingEnvelope, 0.f, 1.f);
	const float PulseScale = bReducedMotion ? 1.f : 1.f + Speaking * FMath::Max(SpeakPulse.ToScale - 1.f, 0.f);

	if (UMaterialInstanceDynamic* MID = GetMID(GlyphImage))
	{
		SetScalar(MID, P_GlyphIndex, float(GlyphIdx));
		SetVector(MID, P_Color, PalPrimary);
		SetScalar(MID, P_Speaking, Speaking);
		SetScalar(MID, P_Pulse, bReducedMotion ? 0.f : Speaking);
		SetScalar(MID, P_Morph, bSwapping ? FMath::Clamp(Elapsed / SwapDur, 0.f, 1.f) : 1.f);
	}
	else
	{
		SetImageColorIfChanged(GlyphImage, PalPrimary);
	}
	if (GlyphImage)
	{
		const float S = FMath::Max(GlyphScale * PulseScale, 0.001f);
		SetTransformIfChanged(GlyphImage, FVector2D::ZeroVector, FVector2D(S, S));
	}

	if (PersonaNameText)
	{
		if (FontPersona != ShownPersona)
		{
			// Persona font feel: Rajdhani etc. when the font asset exists, plus the tracking bias.
			FontPersona = ShownPersona;
			if (const FHUDElementDef* Def = Config.FindElement(FName(TEXT("PersonaNameText"))))
			{
				FHUDElementDef PersonaDef = *Def;
				for (const TPair<FName, FHUDFontDef>& FontPair : Config.Fonts)
				{
					if (FontPair.Value.Family.Equals(To.FontFamily, ESearchCase::IgnoreCase))
					{
						PersonaDef.FontKey = FontPair.Key;
						break;
					}
				}
				ApplyElementFont(PersonaNameText, PersonaDef, To.LetterSpacingBias);
			}
		}
		SetTextIfChanged(PersonaNameText, To.Label);
		SetTextColorIfChanged(PersonaNameText, PalPrimary);
	}
	if (SuitNameText)
	{
		SetTextIfChanged(SuitNameText, HudState.SuitName.ToString().ToUpper());
		SetTextColorIfChanged(SuitNameText, WithAlpha(Col(C_Neutral), 0.8f));
	}
}

// ================================================================================================
// Flight readouts (tapes and ladder ticks are painted in NativePaint)
// ================================================================================================

void UHelmetHUDWidget::UpdateFlight()
{
	const bool bFlight = HudState.bFlying;
	SetShown(PitchLadder, bFlight);
	SetShown(SpeedTape, bFlight);
	SetShown(AltitudeTape, bFlight);
	SetShown(HeadingTape, true);

	const float Heading = FMath::Fmod(FMath::Fmod(HudState.HeadingDeg, 360.f) + 360.f, 360.f);
	SetTextIfChanged(HeadingReadout, FString::Printf(TEXT("%03d"), FMath::RoundToInt(Heading) % 360));
	SetTextColorIfChanged(HeadingReadout, PalPrimary);

	SetTextIfChanged(SpeedReadout, FString::Printf(TEXT("%d"), FMath::RoundToInt(HudState.SpeedMs)));
	SetTextColorIfChanged(SpeedReadout, PalPrimary);
	const float Mach = HudState.SpeedMs / SpeedOfSoundMs;
	SetShown(MachReadout, bFlight && Mach >= Config.MachFrom);
	if (Mach >= Config.MachFrom)
	{
		SetTextIfChanged(MachReadout, FString::Printf(TEXT("M %.2f"), Mach));
		SetTextColorIfChanged(MachReadout, PalSecondary);
	}

	SetTextIfChanged(AltitudeReadout, FString::Printf(TEXT("%d"), FMath::RoundToInt(HudState.AltitudeM)));
	SetTextColorIfChanged(AltitudeReadout, PalPrimary);
	const bool bShowAgl = bFlight && HudState.AglM >= 0.f && HudState.AglM < Config.AglBoxBelowM;
	SetShown(AglReadout, bShowAgl);
	if (bShowAgl)
	{
		SetTextIfChanged(AglReadout, FString::Printf(TEXT("AGL %d"), FMath::RoundToInt(HudState.AglM)));
		SetTextColorIfChanged(AglReadout, HudState.AglM < 15.f ? PalWarning : Col(C_Neutral));
	}

	if (VerticalSpeedCaret)
	{
		// Caret slides along the altitude tape: +-20 m/s maps to +-60 px.
		const float Offset = -FMath::Clamp(HudState.VerticalSpeedMs / 20.f, -1.f, 1.f) * 60.f;
		SetShown(VerticalSpeedCaret, bFlight);
		SetTransformIfChanged(VerticalSpeedCaret, FVector2D(0.f, Offset), FVector2D(1.f, 1.f));
		SetImageColorIfChanged(VerticalSpeedCaret, PalPrimary);
	}
}

// ================================================================================================
// Power / heat gauges
// ================================================================================================

void UHelmetHUDWidget::UpdateGauges()
{
	// ---- Power ----
	const float Power = FMath::Clamp(HudState.PowerFraction, 0.f, 1.f);
	FLinearColor PowerColor = PalPrimary;
	if (Power <= Config.PowerCritical)
	{
		PowerColor = PalCritical;
	}
	else if (Power <= Config.PowerLow)
	{
		PowerColor = PalWarning;
	}
	SetFill(PowerFill, Power, PowerColor);
	if (UMaterialInstanceDynamic* MID = GetMID(PowerFill))
	{
		SetVector(MID, P_BackColor, WithAlpha(Col(C_PanelBg), 0.55f));
		SetScalar(MID, P_SegmentCount, 20.f);
		SetScalar(MID, P_WarnStart, Config.PowerLow);
		SetScalar(MID, P_CritStart, Config.PowerCritical);
	}
	SetTextIfChanged(PowerValueText, Power <= 0.001f ? LOCTEXT("NoOutput", "NO OUTPUT").ToString() : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Power * 100.f)));
	SetTextColorIfChanged(PowerValueText, PowerColor);
	SetTextIfChanged(PowerDrawText, FString::Printf(TEXT("%+d/s"), FMath::RoundToInt(HudState.PowerRatePerS)));
	SetTextColorIfChanged(PowerDrawText, WithAlpha(Col(C_Neutral), 0.7f));

	// Brownout at critical power: whole-HUD opacity dip (lowPowerBrownout), applied in UpdateTransforms.
	RaiseWarning(W_LowPower, Power <= Config.PowerLow && !HudState.bDead);

	// ---- Heat ----
	SetShown(HeatGauge, HudState.bHasReactor);
	SetShown(VentPrompt, HudState.bHasReactor);
	if (!HudState.bHasReactor)
	{
		return;
	}
	const float HeatMax = FMath::Max(Config.HeatMax, 1.f);
	const float Heat = FMath::Clamp(HudState.HeatFraction, 0.f, 1.f) * HeatMax;
	const float HeatFrac = Heat / HeatMax;

	FLinearColor HeatColor = PalSecondary;
	float HeatPulse = 0.f;
	if (HudState.bOverheated || Heat >= Config.HeatOverheat)
	{
		HeatColor = Col(C_OverheatWhite);
		HeatPulse = FlashOpacity(Config.GetAnim(A_HeatPulseCritical).LoopHz);
	}
	else if (Heat >= Config.HeatCritical)
	{
		HeatColor = PalCritical;
		HeatPulse = FlashOpacity(Config.GetAnim(A_HeatPulseCritical).LoopHz); // step flash, capped at 3 Hz
	}
	else if (Heat >= Config.HeatWarning)
	{
		HeatColor = PalWarning;
		const FHUDAnimDef& Pulse = Config.GetAnim(A_HeatPulseWarning);
		HeatPulse = bReducedMotion ? 1.f : 0.75f + 0.25f * (0.5f + 0.5f * FMath::Sin(2.f * PI * FMath::Min(Pulse.LoopHz, Config.FlashMaxHz) * Now()));
	}
	else if (Heat >= Config.HeatCaution)
	{
		HeatColor = Col(C_Caution);
	}
	SetFill(HeatFill, HeatFrac, HeatColor);
	if (UMaterialInstanceDynamic* MID = GetMID(HeatFill))
	{
		float Jitter = 0.f;
		float Dropout = 0.f;
		const int32 Step = FMath::FloorToInt(Now() * 12.f);
		switch (HudState.ReactorStage)
		{
		case EReactorStage::Makeshift: Jitter = bReducedMotion ? 0.f : Hash11(Step, 3) * 1.f; break;
		case EReactorStage::Unstable:  Jitter = bReducedMotion ? 0.f : Hash11(Step, 5) * HudState.Instability * 3.f; break;
		case EReactorStage::Failed:    Dropout = 1.f; break;
		default: break;
		}
		SetVector(MID, P_BackColor, WithAlpha(Col(C_PanelBg), 0.55f));
		SetScalar(MID, P_SegmentCount, 20.f);
		SetScalar(MID, P_WarnStart, Config.HeatWarning / HeatMax);
		SetScalar(MID, P_CritStart, Config.HeatCritical / HeatMax);
		SetScalar(MID, P_RecoverMarker, Config.HeatRecoverAt / HeatMax);
		SetScalar(MID, P_Pulse, HeatPulse);
		SetScalar(MID, P_Jitter, Jitter);
		SetScalar(MID, P_Dropout, Dropout);
		SetVector(MID, P_GlowColor, HudState.CoreColor * 0.3f);
		SetScalar(MID, P_Chevrons, HudState.bVenting && !bReducedMotion ? 1.f : 0.f);
	}
	else if (HeatFill && HeatPulse > 0.f)
	{
		SetOpacityIfChanged(HeatFill, HeatPulse);
	}
	else
	{
		SetOpacityIfChanged(HeatFill, HudState.ReactorStage == EReactorStage::Failed ? 0.5f : 1.f);
	}
	if (HeatRecoverMarker)
	{
		SetShown(HeatRecoverMarker, HudState.bOverheated);
		SetImageColorIfChanged(HeatRecoverMarker, Col(C_Neutral));
	}
	SetTextIfChanged(HeatValueText, FString::Printf(TEXT("%d"), FMath::RoundToInt(Heat)));
	SetTextColorIfChanged(HeatValueText, HeatColor);

	// ---- Vent prompt ----
	const FString KeyLabel = HelmetVentKeyText(VentKeyLabel);
	FString Prompt;
	FLinearColor PromptColor = PalPrimary;
	float PromptOpacity = 1.f;
	bool bShowPrompt = true;
	if (HudState.bVenting)
	{
		Prompt = LOCTEXT("Venting", "VENTING").ToString();
		PromptColor = PalSecondary;
	}
	else if (HudState.bOverheated)
	{
		Prompt = LOCTEXT("Overheat", "OVERHEAT").ToString();
		PromptColor = PalCritical;
		PromptOpacity = FlashOpacity(2.f);
	}
	else if (Heat >= Config.HeatWarning)
	{
		Prompt = FString::Printf(TEXT("VENT [%s]"), *KeyLabel);
		PromptColor = PalWarning;
		PromptOpacity = HudState.VentCooldownLeft > 0.f ? 0.4f : FlashOpacity(1.5f);
	}
	else
	{
		bShowPrompt = false;
	}
	SetShown(VentPrompt, bShowPrompt);
	SetTextIfChanged(VentPrompt, Prompt);
	SetTextColorIfChanged(VentPrompt, PromptColor);
	SetOpacityIfChanged(VentPrompt, PromptOpacity);

	if (VentCooldownRing)
	{
		const float Cooldown = FMath::Max(Config.VentCooldownS, 0.01f);
		const bool bCooling = HudState.VentCooldownLeft > 0.f;
		SetShown(VentCooldownRing, bCooling);
		if (UMaterialInstanceDynamic* MID = GetMID(VentCooldownRing))
		{
			SetScalar(MID, P_Fill, 1.f - FMath::Clamp(HudState.VentCooldownLeft / Cooldown, 0.f, 1.f));
			SetVector(MID, P_FillColor, PalSecondary);
		}
		else
		{
			SetImageColorIfChanged(VentCooldownRing, WithAlpha(PalSecondary, 0.5f));
		}
	}

	RaiseWarning(W_Overheat, HudState.bOverheated);
	RaiseWarning(W_VentNow, !HudState.bOverheated && !HudState.bVenting && Heat >= Config.HeatCritical && HudState.VentCooldownLeft <= 0.f);
	RaiseWarning(W_CoreDark, HudState.bCoreDark);
}

// ================================================================================================
// Integrity
// ================================================================================================

void UHelmetHUDWidget::UpdateIntegrity(bool bWasHit, float DeltaSeconds)
{
	UImage* Regions[NumRegions] = { Region_Helmet, Region_Torso, Region_SpineMount, Region_ArmL, Region_ArmR, Region_LegL, Region_LegR };

	if (bWasHit)
	{
		int32 HitRegion = HudState.LastHitRegion;
		if (HitRegion < 0 || HitRegion >= NumRegions)
		{
			// Guess from the impact direction: above = helmet, sides = arms, else torso.
			const FVector2D Dir = HudState.ImpactDir2D;
			if (Dir.IsNearlyZero())
			{
				HitRegion = (int32)EHUDBodyRegion::Torso;
			}
			else if (Dir.Y < -0.7f)
			{
				HitRegion = (int32)EHUDBodyRegion::Helmet;
			}
			else if (FMath::Abs(Dir.X) > 0.7f)
			{
				HitRegion = Dir.X < 0.f ? (int32)EHUDBodyRegion::ArmL : (int32)EHUDBodyRegion::ArmR;
			}
			else if (Dir.Y > 0.7f)
			{
				HitRegion = (int32)EHUDBodyRegion::LegL;
			}
			else
			{
				HitRegion = (int32)EHUDBodyRegion::Torso;
			}
		}
		RegionFlashStart[HitRegion] = Now();
		RegionDip[HitRegion] = 0.15f; // cosmetic dip, heals over 3 s
	}

	const FHUDAnimDef& HitFlash = Config.GetAnim(A_IntegrityHitFlash);
	const float FlashDur = HitFlash.DurationS > 0.f ? HitFlash.DurationS : 0.25f;
	const FLinearColor Nominal = WithAlpha(PalPrimary, 0.6f);

	for (int32 i = 0; i < NumRegions; ++i)
	{
		RegionDip[i] = FMath::Max(0.f, RegionDip[i] - DeltaSeconds * (0.15f / 3.f));
		float Value = HudState.RegionIntegrity.IsValidIndex(i) ? HudState.RegionIntegrity[i] : HudState.HealthFraction;
		if (i == (int32)EHUDBodyRegion::SpineMount && HudState.bCoreDark)
		{
			Value = 0.f;
		}
		Value = FMath::Clamp(Value - RegionDip[i], 0.f, 1.f);

		FLinearColor RegionColor = Nominal;
		float Hatch = 0.f;
		float Pulse = 0.f;
		if (Value <= Config.IntegrityOffline)
		{
			RegionColor = Col(C_IntegrityOffline);
			Hatch = 1.f;
		}
		else if (Value <= Config.IntegrityCritical)
		{
			RegionColor = Col(C_IntegrityCritical);
			Pulse = bReducedMotion ? 0.f : 0.5f + 0.5f * FMath::Sin(2.f * PI * 1.5f * Now());
		}
		else if (Value <= Config.IntegrityDamaged)
		{
			RegionColor = Col(C_IntegrityDamaged);
		}
		const float Flash = FMath::Clamp(1.f - (Now() - RegionFlashStart[i]) / FlashDur, 0.f, 1.f);
		if (Flash > 0.f)
		{
			RegionColor = FMath::Lerp(RegionColor, FLinearColor::White, Flash);
		}

		UImage* Region = Regions[i];
		if (!Region)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = GetMID(Region))
		{
			SetVector(MID, P_Color, RegionColor);
			SetScalar(MID, P_Damage, 1.f - Value);
			SetScalar(MID, P_Hatch, Hatch);
			SetScalar(MID, P_Flash, Flash);
			SetScalar(MID, P_Pulse, Pulse);
		}
		else
		{
			FLinearColor Tinted = RegionColor;
			Tinted.A *= (Hatch > 0.f ? 0.4f : 1.f) * (1.f - 0.35f * Pulse);
			SetImageColorIfChanged(Region, Tinted);
		}
	}

	const float Health = FMath::Clamp(HudState.HealthFraction, 0.f, 1.f);
	SetTextIfChanged(IntegrityPercentText, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Health * 100.f)));
	SetTextColorIfChanged(IntegrityPercentText, Health <= Config.IntegrityCritical ? Col(C_IntegrityCritical)
		: (Health <= Config.IntegrityDamaged ? Col(C_IntegrityDamaged) : PalPrimary));

	RaiseWarning(W_ArmorCritical, Health <= Config.IntegrityCritical && !HudState.bDead);
}

// ================================================================================================
// Targets / lock
// ================================================================================================

void UHelmetHUDWidget::UpdateTargets(float DeltaSeconds)
{
	UCanvasPanel* Layer = Cast<UCanvasPanel>(TargetLayer.Get());
	const FGeometry& MyGeo = GetCachedGeometry();
	const FGeometry LayerGeo = Layer ? Layer->GetCachedGeometry() : MyGeo;
	const bool bGeoValid = MyGeo.GetLocalSize().X > 1.f && LayerGeo.GetLocalSize().X > 1.f;
	const FVector2D LayerSize = bGeoValid ? FVector2D(LayerGeo.GetLocalSize()) : Config.ReferenceResolution;

	const FHUDAnimDef& Acquire = Config.GetAnim(A_BracketAcquire);
	const FHUDAnimDef& Snap = Config.GetAnim(A_BracketLockSnap);
	const FHUDAnimDef& LockedPulse = Config.GetAnim(A_BracketLockedPulse);
	const float AcquireDur = FMath::Max(Acquire.DurationS, 0.18f);

	int32 NumBrackets = 0;
	int32 NumOffscreen = 0;
	float BestLock = 0.f;
	FHUDLockFrame LockFrame;

	for (const FHUDTarget& Target : HudState.Targets)
	{
		FLockTrack& Track = LockTracks.FindOrAdd(Target.TargetId);
		if (Track.LastSeen <= 0.f || Now() - Track.LastSeen > 1.f)
		{
			Track = FLockTrack();
			Track.FirstSeen = Now();
		}
		Track.LastSeen = Now();

		// ---- Lock progress: producer-driven, or the widget's own timer (cone + range) ----
		const bool bHostile = IsHostile(Target.Type);
		EHUDLockState LockState = Target.Lock;
		if (LockState == EHUDLockState::None)
		{
			Track.bInCone = bHostile && Target.bOnScreen && Target.OffAxisDeg <= Config.LockConeDeg && Target.DistanceM <= Config.LockAcquireRangeM;
			if (Track.bInCone)
			{
				if (Track.Progress <= 0.f)
				{
					Track.AcquireStart = Now();
				}
				Track.LastInCone = Now();
				Track.Progress = FMath::Min(1.f, Track.Progress + DeltaSeconds / FMath::Max(Config.LockTimeRepulsorS, 0.05f));
			}
			else if (Now() - Track.LastInCone > Config.LockLoseGraceS)
			{
				Track.Progress = FMath::Max(0.f, Track.Progress - DeltaSeconds / FMath::Max(Config.LockTimeRepulsorS, 0.05f));
			}
			if (Track.Progress >= 1.f)
			{
				LockState = EHUDLockState::Locked;
			}
			else if (Track.Progress > 0.f)
			{
				LockState = (Now() - Track.AcquireStart) < AcquireDur ? EHUDLockState::Acquire : EHUDLockState::Tracking;
			}
		}
		else
		{
			Track.Progress = LockState == EHUDLockState::Locked ? 1.f : FMath::Clamp(Target.LockProgress, 0.f, 1.f);
		}
		if (bHostile && Track.AcquireStart >= Now() - KINDA_SMALL_NUMBER)
		{
			++LockFrame.AcquireStarts;
		}
		if (LockState == EHUDLockState::Locked)
		{
			if (Track.LockedAt < 0.f)
			{
				Track.LockedAt = Now();
			}
			if (bHostile)
			{
				++LockFrame.NumLocked;
				if (!Track.bWasLocked)
				{
					++LockFrame.NewLocks;
					LockFrame.bNewBossLock |= Target.Type == EHUDThreatType::Boss;
				}
				LockFrame.bBossLocked |= Target.Type == EHUDThreatType::Boss;
			}
			Track.bWasLocked = true;
		}
		else
		{
			Track.LockedAt = -1.f;
			LockFrame.bLostLock |= Track.bWasLocked && bHostile;
			Track.bWasLocked = false;
		}
		if (bHostile && Target.bOnScreen)
		{
			BestLock = FMath::Max(BestLock, Track.Progress);
		}

		// ---- Colour ----
		FLinearColor BracketColor;
		switch (LockState)
		{
		case EHUDLockState::Acquire:  BracketColor = Col(C_LockAcquire); break;
		case EHUDLockState::Tracking: BracketColor = Col(C_LockTracking); break;
		case EHUDLockState::Locked:   BracketColor = Col(C_LockLocked); break;
		case EHUDLockState::Lost:     BracketColor = WithAlpha(Col(C_TickDim), 0.6f); break;
		default:
			switch (Target.Type)
			{
			case EHUDThreatType::Tank:       BracketColor = Col(C_ThreatTank); break;
			case EHUDThreatType::Boss:       BracketColor = Col(C_ThreatBoss); break;
			case EHUDThreatType::Projectile: BracketColor = Col(C_ThreatProjectile); break;
			case EHUDThreatType::Ally:       BracketColor = Col(C_Ally); break;
			case EHUDThreatType::Pickup:     BracketColor = Col(C_Pickup); break;
			case EHUDThreatType::Beacon:     BracketColor = Col(C_Objective); break;
			default:                         BracketColor = Col(C_ThreatDrone); break;
			}
			break;
		}

		if (!Layer)
		{
			continue;
		}

		if (Target.bOnScreen)
		{
			if (NumBrackets >= Config.MaxBrackets)
			{
				continue;
			}
			UImage* Bracket = PoolImage(BracketPool, NumBrackets++, Layer, MI_Bracket, true);
			if (!Bracket)
			{
				continue;
			}
			FVector2D Pos = Target.ScreenPos;
			if (bGeoValid)
			{
				const FVector2D Absolute = FVector2D(MyGeo.LocalToAbsolute(Target.ScreenPos));
				Pos = FVector2D(LayerGeo.AbsoluteToLocal(Absolute));
			}
			// ScreenSize is in viewport units; the layer is HUD-scaled, so undo it to keep brackets on the target.
			const FVector2D BracketSize(FMath::Max<double>(Target.ScreenSize.X / HudScalePref, 24.0), FMath::Max<double>(Target.ScreenSize.Y / HudScalePref, 24.0));
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Bracket->Slot))
			{
				if (!CanvasSlot->GetPosition().Equals(Pos, 0.25))
				{
					CanvasSlot->SetPosition(Pos);
				}
				if (!CanvasSlot->GetSize().Equals(BracketSize, 0.25))
				{
					CanvasSlot->SetSize(BracketSize);
				}
			}

			// bracketAcquire (scale in), bracketLockSnap, bracketLockedPulse.
			float BracketScale = 1.f;
			if (!bReducedMotion)
			{
				const float SinceSeen = Now() - Track.FirstSeen;
				if (SinceSeen < Acquire.DurationS)
				{
					BracketScale = FMath::Lerp(Acquire.FromScale, Acquire.ToScale, AnimAlpha(A_BracketAcquire, SinceSeen));
				}
				if (Track.LockedAt >= 0.f)
				{
					const float SinceLock = Now() - Track.LockedAt;
					if (SinceLock < Snap.DurationS)
					{
						BracketScale = FMath::Lerp(Snap.FromScale, Snap.ToScale, AnimAlpha(A_BracketLockSnap, SinceLock));
					}
					else
					{
						BracketScale = Snap.ToScale * (1.f + LockedPulse.Amplitude * FMath::Sin(2.f * PI * FMath::Min(LockedPulse.LoopHz, Config.FlashMaxHz) * (SinceLock - Snap.DurationS)));
					}
				}
			}
			else if (Track.LockedAt >= 0.f)
			{
				BracketScale = Snap.ToScale;
			}
			SetTransformIfChanged(Bracket, FVector2D::ZeroVector, FVector2D(BracketScale, BracketScale), LockState == EHUDLockState::Locked ? 45.f : 0.f);
			if (UMaterialInstanceDynamic* MID = GetMID(Bracket))
			{
				SetVector(MID, P_Color, BracketColor);
				SetScalar(MID, P_Progress, Track.Progress);
				SetScalar(MID, P_State, float((uint8)LockState));
			}
			else
			{
				SetImageColorIfChanged(Bracket, BracketColor);
			}
			SetShown(Bracket, true);
		}
		else if (bHostile || Target.Type == EHUDThreatType::Beacon || Target.Type == EHUDThreatType::Ally)
		{
			if (NumOffscreen >= Config.MaxOffscreen)
			{
				continue;
			}
			UImage* Arrow = PoolImage(OffscreenPool, NumOffscreen++, Layer, NAME_None, false);
			if (!Arrow)
			{
				continue;
			}
			const float AngleRad = FMath::DegreesToRadians(Target.EdgeAngleDeg);
			const FVector2D Centre = LayerSize * 0.5;
			const FVector2D Radius = Centre * (1.f - 2.f * Config.OffscreenEllipseInset);
			const FVector2D Pos(Centre.X + FMath::Sin(AngleRad) * Radius.X, Centre.Y - FMath::Cos(AngleRad) * Radius.Y);
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Arrow->Slot))
			{
				if (!CanvasSlot->GetPosition().Equals(Pos, 0.25))
				{
					CanvasSlot->SetPosition(Pos);
				}
				if (!CanvasSlot->GetSize().Equals(FVector2D(14.f, 14.f), 0.25))
				{
					CanvasSlot->SetSize(FVector2D(14.f, 14.f));
				}
			}
			SetTransformIfChanged(Arrow, FVector2D::ZeroVector, FVector2D(1.f, 1.f), 45.f); // diamond
			SetImageColorIfChanged(Arrow, BracketColor);
			SetShown(Arrow, true);
		}
	}

	for (int32 i = NumBrackets; i < BracketPool.Num(); ++i)
	{
		SetShown(BracketPool[i], false);
	}
	for (int32 i = NumOffscreen; i < OffscreenPool.Num(); ++i)
	{
		SetShown(OffscreenPool[i], false);
	}

	// Drop tracks not seen for a second.
	for (auto It = LockTracks.CreateIterator(); It; ++It)
	{
		if (Now() - It.Value().LastSeen > 1.f)
		{
			It.RemoveCurrent();
		}
	}

	// ---- Reticle ----
	ReticleLock = FMath::FInterpTo(ReticleLock, BestLock, DeltaSeconds, 20.f);
	const FLinearColor ReticleColor = BestLock >= 1.f ? Col(C_LockLocked) : PalPrimary;
	if (UMaterialInstanceDynamic* MID = GetMID(ReticleImage))
	{
		SetVector(MID, P_Color, ReticleColor);
		SetScalar(MID, P_LockProgress, ReticleLock);
		SetScalar(MID, P_Spread, HudState.bBoosting ? 1.f : 0.f);
	}
	else
	{
		SetImageColorIfChanged(ReticleImage, ReticleColor);
	}
	if (UMaterialInstanceDynamic* MID = GetMID(LockProgressRing))
	{
		SetVector(MID, P_Color, ReticleColor);
		SetScalar(MID, P_LockProgress, ReticleLock);
	}
	else if (LockProgressRing)
	{
		SetShown(LockProgressRing, ReticleLock > 0.01f);
		SetImageColorIfChanged(LockProgressRing, WithAlpha(ReticleColor, ReticleLock));
		const float RingScale = 1.4f - 0.4f * ReticleLock;
		SetTransformIfChanged(LockProgressRing, FVector2D::ZeroVector, FVector2D(RingScale, RingScale));
	}

	RaiseWarning(W_MissileLock, HudState.bIncomingMissile);
	Callouts.OnLocks(this, LockFrame, HudState, PrevHudState, Now());
}

// ================================================================================================
// Radar, weapon, Hulkbuster
// ================================================================================================

namespace
{
	/** Clips segment A-B (radar metres) to the circle of radius R round the origin. False when nothing is left. */
	bool MinimapClipToCircle(FVector2f& InOutA, FVector2f& InOutB, float R)
	{
		const FVector2f Dir = InOutB - InOutA;
		const float QA = Dir.SizeSquared();
		if (QA < 1e-4f) { return false; }
		const float QB = 2.f * FVector2f::DotProduct(InOutA, Dir);
		const float QC = InOutA.SizeSquared() - R * R;
		const float Disc = QB * QB - 4.f * QA * QC;
		if (Disc <= 0.f) { return false; }
		const float Root = FMath::Sqrt(Disc);
		const float T0 = FMath::Max((-QB - Root) / (2.f * QA), 0.f);
		const float T1 = FMath::Min((-QB + Root) / (2.f * QA), 1.f);
		if (T0 >= T1) { return false; }
		const FVector2f Start = InOutA;
		InOutA = Start + Dir * T0;
		InOutB = Start + Dir * T1;
		return true;
	}
}

void UHelmetHUDWidget::UpdateMinimapRoads(float RangeM)
{
	MinimapLocalSegs.Reset();
	MinimapLocalWidthM.Reset();
	bMinimapHasNorth = false;
	const APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn || !GetWorld())
	{
		return;
	}
	bMinimapHasNorth = true;
	const FRotator ControlRot = Pawn->GetControlRotation();
	MinimapNorthDeg = -float(ControlRot.Yaw);

	// Find the live city (the JSON city layout, else the Downtown grid); rescan every 2 s while there is none.
	AActor* City = MinimapCity.Get();
	if ((!City || City->IsHidden()) && HudTime >= MinimapNextScanTime)
	{
		MinimapNextScanTime = HudTime + 2.f;
		MinimapWorldSegs.Reset();
		MinimapWorldWidthCm.Reset();
		City = nullptr;
		ACityLayout* Layout = ACityLayout::Find(this);
		if (Layout && !Layout->IsHidden())
		{
			Layout->GetMinimapRoads(MinimapWorldSegs, MinimapWorldWidthCm);
			City = Layout;
		}
		else if (ADowntownDistrict* Dist = ADowntownDistrict::Find(this))
		{
			if (!Dist->IsHidden())
			{
				const FVector DistLoc = Dist->GetActorLocation();
				const double Lo = -double(ADowntownDistrict::HalfExtent);
				const double Hi = double(ADowntownDistrict::HalfExtent);
				const float RoadW = ADowntownDistrict::RoadWidth + 2.f * ADowntownDistrict::WalkWidth;
				for (int32 Line = 0; Line <= ADowntownDistrict::NumBlocks; ++Line)
				{
					const double GX = Dist->RoadLineWorldX(Line);
					const double GY = Dist->RoadLineWorldY(Line);
					MinimapWorldSegs.Add(FVector4d(GX, DistLoc.Y + Lo, GX, DistLoc.Y + Hi));
					MinimapWorldWidthCm.Add(RoadW);
					MinimapWorldSegs.Add(FVector4d(DistLoc.X + Lo, GY, DistLoc.X + Hi, GY));
					MinimapWorldWidthCm.Add(RoadW);
				}
				City = Dist;
			}
		}
		MinimapCity = City;
		if (City)
		{
			UE_LOG(LogTemp, Log, TEXT("HUD minimap: %d road segments from %s"), MinimapWorldSegs.Num(), *City->GetName());
		}
	}
	if (!City || City->IsHidden() || MinimapWorldSegs.Num() == 0)
	{
		return;
	}

	const FVector Location = Pawn->GetActorLocation();
	const FRotator YawOnly(0.0, ControlRot.Yaw, 0.0);
	const FVector YawFwd = YawOnly.Vector();
	const FVector YawRight = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	auto ToLocal = [&Location, &YawFwd, &YawRight](double WX, double WY) -> FVector2f
	{
		const FVector Delta(WX - Location.X, WY - Location.Y, 0.0);
		return FVector2f(float(FVector::DotProduct(Delta, YawRight)), float(FVector::DotProduct(Delta, YawFwd))) / 100.f;
	};
	const float ClipR = FMath::Max(RangeM, 1.f);
	for (int32 i = 0; i < MinimapWorldSegs.Num(); ++i)
	{
		const FVector4d& Seg = MinimapWorldSegs[i];
		FVector2f SegA = ToLocal(Seg.X, Seg.Y);
		FVector2f SegB = ToLocal(Seg.Z, Seg.W);
		if (MinimapClipToCircle(SegA, SegB, ClipR))
		{
			MinimapLocalSegs.Add(FVector4f(SegA.X, SegA.Y, SegB.X, SegB.Y));
			MinimapLocalWidthM.Add(MinimapWorldWidthCm.IsValidIndex(i) ? MinimapWorldWidthCm[i] / 100.f : 15.f);
		}
	}
}

void UHelmetHUDWidget::UpdateRadarAndWeapon()
{
	float Range = HudState.RadarRangeM;
	if (Range <= 0.f)
	{
		Range = HudState.bBossActive ? Config.RadarRangeBossM : (HudState.bFlying ? Config.RadarRangeFlightM : Config.RadarRangeGroundM);
	}
	UpdateMinimapRoads(Range);
	SetTextIfChanged(RadarRangeText, FString::Printf(TEXT("%dm"), FMath::RoundToInt(Range)));
	SetTextColorIfChanged(RadarRangeText, WithAlpha(PalPrimary, 0.8f));
	if (UMaterialInstanceDynamic* MID = GetMID(RadarSweep))
	{
		const float Period = FMath::Max(Config.RadarSweepPeriodS, 0.1f);
		SetScalar(MID, P_SweepAngle, bReducedMotion ? 0.f : FMath::Frac(Now() / Period) * 360.f);
		SetVector(MID, P_Color, PalPrimary);
		SetScalar(MID, P_GridAlpha, 0.25f);
		SetScalar(MID, P_RingCount, 3.f);
	}
	else
	{
		SetImageColorIfChanged(RadarSweep, WithAlpha(PalPrimary, 0.35f));
	}

	// Weapon
	const bool bOffline = HudState.bOverheated || (HudState.bHasReactor && HudState.ReactorStage == EReactorStage::Failed);
	SetTextIfChanged(WeaponNameText, bOffline ? LOCTEXT("WeaponOffline", "OFFLINE").ToString() : HudState.WeaponName.ToString().ToUpper());
	SetTextColorIfChanged(WeaponNameText, bOffline ? PalCritical : PalPrimary);
	const bool bShowSpecial = !HudState.SpecialName.IsEmpty();
	SetShown(SpecialCostText, bShowSpecial);
	if (bShowSpecial)
	{
		const bool bAffordable = HudState.PowerFraction * 100.f >= HudState.SpecialCost;
		SetTextIfChanged(SpecialCostText, FString::Printf(TEXT("%s  %d"), *HudState.SpecialName.ToString().ToUpper(), FMath::RoundToInt(HudState.SpecialCost)));
		SetTextColorIfChanged(SpecialCostText, bAffordable ? PalSecondary : WithAlpha(Col(C_TickDim), 0.8f));
	}
	const bool bShowTimer = HudState.ActiveEffectTimeLeft > 0.f || HudState.bBeamActive;
	SetShown(BeamTimerText, bShowTimer);
	if (bShowTimer)
	{
		const FString EffectName = HudState.ActiveEffectName.IsEmpty() ? FString(TEXT("BEAM")) : HudState.ActiveEffectName.ToString().ToUpper();
		SetTextIfChanged(BeamTimerText, HudState.ActiveEffectTimeLeft > 0.f
			? FString::Printf(TEXT("%s %.1fs"), *EffectName, HudState.ActiveEffectTimeLeft) : EffectName);
		SetTextColorIfChanged(BeamTimerText, PalWarning);
	}

	// Hulkbuster
	const bool bShowHulk = HudState.bHulkbusterActive || HudState.HulkbusterCharges > 0;
	SetShown(HulkbusterStatus, bShowHulk);
	if (bShowHulk)
	{
		SetTextIfChanged(HulkbusterText, HudState.bHulkbusterActive
			? FString::Printf(TEXT("HULKBUSTER %ds"), FMath::CeilToInt(HudState.HulkbusterTimeLeft))
			: FString::Printf(TEXT("HULKBUSTER x%d"), HudState.HulkbusterCharges));
		SetTextColorIfChanged(HulkbusterText, HudState.bHulkbusterActive ? PalWarning : PalSecondary);
	}
}

// ================================================================================================
// Boss, objective, score / combo
// ================================================================================================

void UHelmetHUDWidget::UpdateGame()
{
	SetShown(BossBar, HudState.bBossActive);
	if (HudState.bBossActive)
	{
		const FString BossLabel = HudState.BossPhase > 0
			? FString::Printf(TEXT("%s \u00B7 PHASE %d"), *HudState.BossName.ToString().ToUpper(), HudState.BossPhase)
			: HudState.BossName.ToString().ToUpper();
		SetTextIfChanged(BossNameText, BossLabel);
		SetTextColorIfChanged(BossNameText, Col(C_Neutral));
		const FLinearColor BossColor = HudState.bBossInvulnerable ? Col(C_TickDim) : Col(C_ThreatBoss);
		SetFill(BossFill, HudState.BossFraction, BossColor);
		if (UMaterialInstanceDynamic* MID = GetMID(BossFill))
		{
			SetScalar(MID, P_Invulnerable, HudState.bBossInvulnerable ? 1.f : 0.f);
			SetVector(MID, P_BackColor, WithAlpha(Col(C_PanelBg), 0.55f));
		}
	}

	const bool bShowObjective = HudState.bStoryMode && !HudState.ObjectiveText.IsEmpty();
	SetShown(ObjectivePanel, bShowObjective);
	if (bShowObjective)
	{
		SetTextIfChanged(ObjectiveChapterText, HudState.ObjectiveChapter.ToString().ToUpper());
		SetTextColorIfChanged(ObjectiveChapterText, WithAlpha(PalSecondary, 0.9f));
		SetTextIfChanged(ObjectiveText, HudState.ObjectiveText);
		SetTextColorIfChanged(ObjectiveText, Col(C_Objective));
		SetTextIfChanged(ObjectiveProgressText, HudState.ObjectiveProgress);
		SetTextColorIfChanged(ObjectiveProgressText, Col(C_Neutral));
	}

	const bool bShowScore = !HudState.bStoryMode || HudState.bShowScoreInStory;
	SetShown(ScoreCombo, bShowScore);
	if (bShowScore)
	{
		SetTextIfChanged(ScoreText, FString::Printf(TEXT("%d"), HudState.Score));
		SetTextColorIfChanged(ScoreText, PalPrimary);
		const bool bCombo = HudState.Combo > 1;
		SetShown(ComboText, bCombo);
		if (bCombo)
		{
			SetTextIfChanged(ComboText, FString::Printf(TEXT("x%d"), HudState.Combo));
			SetTextColorIfChanged(ComboText, PalWarning);
			SetOpacityIfChanged(ComboText, 0.35f + 0.65f * FMath::Clamp(HudState.ComboTimeFraction, 0.f, 1.f));
		}
		SetShown(WaveText, HudState.Wave > 0);
		SetTextIfChanged(WaveText, FString::Printf(TEXT("WAVE %d"), HudState.Wave));
		SetTextColorIfChanged(WaveText, WithAlpha(Col(C_Neutral), 0.8f));
	}
}

// ================================================================================================
// Comms / subtitles
// ================================================================================================

void UHelmetHUDWidget::UpdateComms()
{
	const bool bActive = !HudState.CommsSpeaker.IsNone() && (HudState.CommsEnvelope > 0.01f || !HudState.SubtitleLine.IsEmpty());
	const float Elapsed = Now() - CommsPhaseStart;
	const float RingDur = FMath::Max(Config.GetAnim(A_CommsRing).DurationS, 0.01f);
	const float OpenDur = FMath::Max(Config.GetAnim(A_CommsOpen).DurationS, 0.01f);
	const float CloseDur = FMath::Max(Config.GetAnim(A_CommsClose).DurationS, 0.01f);
	auto SetPhase = [this](int32 NewPhase)
	{
		CommsPhase = NewPhase;
		CommsPhaseStart = Now();
	};

	if (bActive)
	{
		CommsLastActive = Now();
		CommsShownSpeaker = HudState.CommsSpeaker;
	}
	switch (CommsPhase)
	{
	case 0: if (bActive) { SetPhase(bReducedMotion ? 3 : 1); } break;
	case 1: if (Elapsed >= RingDur) { SetPhase(2); } break;
	case 2: if (Elapsed >= OpenDur) { SetPhase(3); } break;
	case 3: if (!bActive && Now() - CommsLastActive > 1.5f) { SetPhase(bReducedMotion ? 0 : 4); } break;
	case 4:
		if (bActive) { SetPhase(2); }
		else if (Elapsed >= CloseDur) { SetPhase(0); }
		break;
	default: SetPhase(0); break;
	}

	SetShown(CommsPortrait, CommsPhase != 0);
	if (CommsPhase == 0)
	{
		return;
	}
	const float PhaseElapsed = Now() - CommsPhaseStart;

	float FeedScaleY = 1.f;
	switch (CommsPhase)
	{
	case 1: FeedScaleY = 0.f; break;
	case 2: FeedScaleY = AnimAlpha(A_CommsOpen, PhaseElapsed); break;
	case 4: FeedScaleY = 1.f - AnimAlpha(A_CommsClose, PhaseElapsed); break;
	default: break;
	}
	if (CommsFeed)
	{
		SetTransformIfChanged(CommsFeed, FVector2D::ZeroVector, FVector2D(1.f, FMath::Max(FeedScaleY, 0.001f)));
	}

	const FString SpeakerUpper = HudState.CommsSpeakerLabel.IsEmpty() ? CommsShownSpeaker.ToString().ToUpper() : HudState.CommsSpeakerLabel.ToString().ToUpper();
	const bool bMorgan = SpeakerUpper.Contains(TEXT("MORGAN"));
	const FLinearColor SpeakerColor = bMorgan ? Col(C_MorganComms) : HudState.SubtitleColor;

	// Incoming ring: N pulses over the ring duration.
	if (CommsRing)
	{
		SetShown(CommsRing, CommsPhase == 1);
		if (CommsPhase == 1)
		{
			const FHUDAnimDef& Ring = Config.GetAnim(A_CommsRing);
			const float PulseLen = RingDur / float(FMath::Max(Ring.Loops, 1));
			const float T = FMath::Frac(PhaseElapsed / PulseLen);
			const float RingScale = 1.f + 0.6f * T;
			SetTransformIfChanged(CommsRing, FVector2D::ZeroVector, FVector2D(RingScale, RingScale));
			SetImageColorIfChanged(CommsRing, WithAlpha(SpeakerColor, 1.f - T));
		}
	}

	SetTextIfChanged(CommsNameText, CommsPhase == 1 ? FString::Printf(TEXT("INCOMING \u00B7 %s"), *SpeakerUpper) : SpeakerUpper);
	SetTextColorIfChanged(CommsNameText, SpeakerColor);

	const float SignalLoss = FMath::Clamp(HudState.CommsSignalLoss, 0.f, 1.f);
	if (UMaterialInstanceDynamic* MID = GetMID(CommsFeed))
	{
		const FHUDAnimDef& FeedJitter = Config.GetAnim(FName(TEXT("commsFeedJitter")));
		SetScalar(MID, P_PixelSize, 4.f);
		SetScalar(MID, P_Desaturate, 0.3f);
		SetScalar(MID, P_ScanlineIntensity, 0.12f);
		SetScalar(MID, P_SignalLoss, SignalLoss);
		SetScalar(MID, P_Jitter, bReducedMotion ? 0.f : FMath::Max(FeedJitter.Amplitude, 1.f) * SignalLoss);
		SetVector(MID, P_Tint, WithAlpha(SpeakerColor, 0.15f));
		SetScalar(MID, P_Speaking, FMath::Clamp(HudState.CommsEnvelope, 0.f, 1.f));
	}
	SetFill(CommsSignalBars, 1.f - SignalLoss, SignalLoss > 0.5f ? PalWarning : PalPrimary);
}

void UHelmetHUDWidget::UpdateSubtitle()
{
	const bool bWant = HudState.bSubtitlesEnabled && !HudState.SubtitleLine.IsEmpty() && CommsPhase != 1;
	if (bWant != bSubtitleShowing)
	{
		bSubtitleShowing = bWant;
		SubtitleFadeStart = Now();
	}
	const float Elapsed = Now() - SubtitleFadeStart;
	float Opacity = bSubtitleShowing ? AnimAlpha(A_SubtitleIn, Elapsed) : 1.f - AnimAlpha(A_SubtitleOut, Elapsed);
	if (bReducedMotion)
	{
		Opacity = bSubtitleShowing ? 1.f : 0.f;
	}
	Opacity *= FMath::Clamp(HudState.SubtitleAlpha, 0.f, 1.f);

	SetShown(SubtitleBar, Opacity > 0.001f);
	MulElementOpacity(Id_SubtitleBar, Opacity);
	SetElementScale(Id_SubtitleBar, FVector2D(SubtitleScalePref, SubtitleScalePref));

	if (bSubtitleShowing)
	{
		// Keep the last line while fading out.
		SetTextIfChanged(SpeakerText, HudState.SubtitleSpeaker.ToString().ToUpper());
		SetTextColorIfChanged(SpeakerText, HudState.SubtitleColor);
		SetTextIfChanged(LineText, HudState.SubtitleLine);
		SetTextColorIfChanged(LineText, Col(C_Neutral));
	}
}

// ================================================================================================
// Warnings (spec section 9)
// ================================================================================================

void UHelmetHUDWidget::UpdateWarnings()
{
	RaiseWarning(W_SuitCritical, GlitchTier >= 4 && !HudState.bDead);
	const bool bTerrain = HudState.bFlying && HudState.VerticalSpeedMs < -15.f && HudState.AglM >= 0.f && HudState.AglM < 30.f;
	RaiseWarning(W_PullUp, HudState.bPullUp || bTerrain);

	ActiveWarnings.RemoveAll([this](const FActiveWarning& W) { return W.ExpireTime < Now(); });
	ActiveWarnings.Sort([](const FActiveWarning& A, const FActiveWarning& B)
	{
		return A.Priority != B.Priority ? A.Priority < B.Priority : A.StartTime > B.StartTime;
	});

	const FName NewTop = ActiveWarnings.Num() > 0 ? ActiveWarnings[0].Id : NAME_None;
	if (NewTop != TopWarningId)
	{
		TopWarningId = NewTop;
		// Audio cue: only the first time an id reaches slot 0 while it stays active.
		if (!NewTop.IsNone() && !AnnouncedWarnings.Contains(NewTop))
		{
			AnnouncedWarnings.Add(NewTop);
			OnTopWarningChanged.Broadcast(NewTop);
			Callouts.OnTopWarning(this, NewTop, Now());
		}
	}
	for (auto It = AnnouncedWarnings.CreateIterator(); It; ++It)
	{
		const FName Announced = *It;
		if (!ActiveWarnings.ContainsByPredicate([Announced](const FActiveWarning& W) { return W.Id == Announced; }))
		{
			It.RemoveCurrent();
		}
	}

	// Edge vignette in the top warning's colour.
	if (WarningVignette)
	{
		const bool bAny = ActiveWarnings.Num() > 0;
		SetShown(WarningVignette, bAny);
		if (bAny)
		{
			const FActiveWarning& Top = ActiveWarnings[0];
			const bool bCritical = Top.ColorKey == C_Critical;
			const FLinearColor VignetteColor = bCritical ? PalCritical : (Top.ColorKey == C_Warning ? PalWarning : Col(Top.ColorKey));
			const float Intensity = (bCritical ? 0.35f : 0.2f) * (bReducedMotion ? 1.f : FlashOpacity(Top.FlashHz));
			if (UMaterialInstanceDynamic* MID = GetMID(WarningVignette))
			{
				SetVector(MID, P_VignetteColor, VignetteColor);
				SetScalar(MID, P_VignetteIntensity, Intensity);
			}
			else
			{
				SetImageColorIfChanged(WarningVignette, WithAlpha(VignetteColor, Intensity));
			}
		}
	}
}

void UHelmetHUDWidget::UpdateWarningSlots()
{
	UTextBlock* Slots[3] = { WarningSlot0, WarningSlot1, WarningSlot2 };
	const int32 MaxVisible = FMath::Clamp(Config.WarningMaxVisible, 1, 3);

	FName PrevIds[3];
	for (int32 i = 0; i < 3; ++i)
	{
		PrevIds[i] = SlotAnims[i].ShownId;
	}

	for (int32 i = 0; i < 3; ++i)
	{
		FWarningSlotAnim& Anim = SlotAnims[i];
		const FActiveWarning* Wanted = (i < MaxVisible && ActiveWarnings.IsValidIndex(i)) ? &ActiveWarnings[i] : nullptr;
		const FName WantedId = Wanted ? Wanted->Id : NAME_None;
		if (WantedId == Anim.ShownId)
		{
			if (Wanted)
			{
				Anim.ShownLabel = Wanted->Label; // label may change (PushWarning override)
			}
			continue;
		}
		if (!Wanted)
		{
			if (Anim.AnimKind != 3)
			{
				Anim.AnimKind = 3;
				Anim.AnimStart = Now();
			}
			if (Now() - Anim.AnimStart >= Config.GetAnim(A_WarningOut).DurationS)
			{
				Anim.ShownId = NAME_None;
			}
			continue;
		}
		int32 FromSlot = INDEX_NONE;
		for (int32 j = 0; j < 3; ++j)
		{
			if (PrevIds[j] == WantedId)
			{
				FromSlot = j;
			}
		}
		Anim.AnimKind = FromSlot == INDEX_NONE ? 0 : (FromSlot > i ? 2 : 1);
		Anim.AnimStart = Now();
		Anim.ShownId = WantedId;
		Anim.ShownLabel = Wanted->Label;
		Anim.ShownFlashHz = Wanted->FlashHz;
		Anim.ShownColor = Wanted->ColorKey == C_Critical ? PalCritical : (Wanted->ColorKey == C_Warning ? PalWarning : Col(Wanted->ColorKey));
	}

	const FHUDAnimDef& InAnim = Config.GetAnim(A_WarningIn);
	const float SlideDur = FMath::Max(Config.WarningDemoteSlideMs / 1000.f, 0.01f);
	for (int32 i = 0; i < 3; ++i)
	{
		UTextBlock* SlotText = Slots[i];
		const FWarningSlotAnim& Anim = SlotAnims[i];
		if (!SlotText)
		{
			continue;
		}
		const bool bFadingOut = Anim.AnimKind == 3;
		if (Anim.ShownId.IsNone())
		{
			SetShown(SlotText, false);
			continue;
		}
		SetShown(SlotText, true);
		SetTextIfChanged(SlotText, Anim.ShownLabel);
		SetTextColorIfChanged(SlotText, Anim.ShownColor);

		const float Elapsed = Now() - Anim.AnimStart;
		float Opacity = (i == 0 ? 1.f : Config.WarningLowerDim) * FlashOpacity(Anim.ShownFlashHz);
		FVector2D Offset = FVector2D::ZeroVector;
		float Scale = 1.f;
		if (!bReducedMotion)
		{
			switch (Anim.AnimKind)
			{
			case 0:
				Scale = FMath::Lerp(InAnim.FromScale, InAnim.ToScale, AnimAlpha(A_WarningIn, Elapsed));
				Opacity *= FMath::Clamp(Elapsed / FMath::Max(InAnim.DurationS, 0.01f), 0.f, 1.f);
				break;
			case 1: // demoted: slides down from the slot above
				Offset.Y = -WarningSlotSpacing * (1.f - HelmetHUDEase(EHUDEase::EaseOut, Elapsed / SlideDur));
				break;
			case 2: // promoted: slides up from the slot below
				Offset.Y = WarningSlotSpacing * (1.f - HelmetHUDEase(EHUDEase::EaseOut, Elapsed / SlideDur));
				break;
			case 3:
				Opacity *= 1.f - AnimAlpha(A_WarningOut, Elapsed);
				break;
			default: break;
			}
		}
		else if (bFadingOut)
		{
			Opacity = 0.f;
		}
		SetOpacityIfChanged(SlotText, Opacity);
		SetTransformIfChanged(SlotText, Offset, FVector2D(Scale, Scale));
	}
}

// ================================================================================================
// Glitch / visor FX (spec section 11, material params section 15.2)
// ================================================================================================

void UHelmetHUDWidget::UpdateGlitch(bool bWasHit)
{
	int32 HealthTier = 0;
	const float FromHealth = HudState.bDead ? 0.f : Config.GlitchForHealth(FMath::Clamp(HudState.HealthFraction, 0.f, 1.f), &HealthTier);
	GlitchValue = FMath::Max(FromHealth, FMath::Clamp(HudState.GlitchExternal, 0.f, 1.f));
	GlitchTier = HealthTier;
	for (const FHUDGlitchTier& Tier : Config.GlitchTiers)
	{
		if (Tier.Glitch > 0.f && GlitchValue >= Tier.Glitch)
		{
			GlitchTier = FMath::Max(GlitchTier, Tier.Tier);
		}
	}

	if (bReducedMotion)
	{
		// No tears, blackouts, dropouts or jitter; a static desaturation shows damage instead.
		bBlackoutActive = false;
		bDropoutActive = false;
		JitterPx = 0.f;
		TearCountValue = 0;
		return;
	}

	auto StartTear = [this]()
	{
		TearStart = Now();
		TearSeedValue = GlitchRng.RandRange(1, 9999);
		TearCountValue = GlitchRng.RandRange(1, 3);
	};
	if (bWasHit && GlitchTier >= 1)
	{
		StartTear();
	}
	if (GlitchTier >= 2)
	{
		if (Now() >= NextTearTime)
		{
			if (NextTearTime > 0.f)
			{
				StartTear();
			}
			NextTearTime = Now() + (GlitchTier >= 3 ? GlitchRng.FRandRange(0.6f, 1.5f) : GlitchRng.FRandRange(2.f, 4.f));
		}
	}
	else
	{
		NextTearTime = Now() + 2.f;
	}
	if (Now() - TearStart >= FMath::Max(Config.GetAnim(A_GlitchTear).DurationS, 0.01f))
	{
		TearCountValue = 0;
	}

	// Tier 4: short blackouts of everything except the essentials.
	if (GlitchTier >= 4)
	{
		if (Now() >= NextBlackoutTime)
		{
			if (NextBlackoutTime > 0.f)
			{
				BlackoutStart = Now();
			}
			NextBlackoutTime = Now() + GlitchRng.FRandRange(3.f, 6.f);
		}
	}
	else
	{
		NextBlackoutTime = 0.f;
	}
	bBlackoutActive = (Now() - BlackoutStart) < FMath::Max(Config.GetAnim(A_GlitchBlackout).DurationS, 0.01f);

	// Tier 3+: tape/ladder dropouts and jitter.
	if (GlitchTier >= 3)
	{
		if (Now() >= NextDropoutTime)
		{
			if (NextDropoutTime > 0.f)
			{
				DropoutStart = Now();
			}
			NextDropoutTime = Now() + GlitchRng.FRandRange(0.8f, 2.f);
		}
	}
	else
	{
		NextDropoutTime = 0.f;
	}
	bDropoutActive = (Now() - DropoutStart) < 0.1f;
	JitterPx = GlitchTier >= 3 ? 3.f : 0.f;
}

void UHelmetHUDWidget::UpdateFX()
{
	const float Desaturate = bReducedMotion ? (GlitchTier > 0 ? 0.1f : 0.f) : (GlitchTier >= 3 ? 0.3f : 0.f);

	if (GlitchOverlay)
	{
		SetShown(GlitchOverlay, GlitchValue > 0.f);
		if (UMaterialInstanceDynamic* MID = GetMID(GlitchOverlay))
		{
			SetScalar(MID, P_GlitchAmount, bReducedMotion ? 0.f : GlitchValue);
			SetScalar(MID, P_TearCount, float(TearCountValue));
			SetScalar(MID, P_TearSeed, float(TearSeedValue));
			SetScalar(MID, P_BlockSize, 4.f + 4.f * float(GlitchTier));
			SetScalar(MID, P_Desaturate, Desaturate);
			SetScalar(MID, P_Blackout, 0.f); // blackouts are done with element opacity (essentials stay)
		}
	}

	if (RetainerMID)
	{
		const float* BaseChroma = Config.VisorWarp.Find(P_ChromaticEdgePx);
		const float* BaseNoise = Config.VisorWarp.Find(P_NoiseIntensity);
		SetScalar(RetainerMID, P_GlitchAmount, bReducedMotion ? 0.f : GlitchValue);
		SetScalar(RetainerMID, P_TearCount, float(TearCountValue));
		SetScalar(RetainerMID, P_TearSeed, float(TearSeedValue));
		SetScalar(RetainerMID, P_ChromaticEdgePx, (BaseChroma ? *BaseChroma : 1.5f) * (GlitchTier >= 2 && !bReducedMotion ? 2.f : 1.f));
		SetScalar(RetainerMID, P_NoiseIntensity, GlitchTier >= 3 && !bReducedMotion ? 0.15f : (BaseNoise ? *BaseNoise : 0.03f));
		SetScalar(RetainerMID, P_Desaturate, Desaturate);
		SetVector(RetainerMID, P_PersonaTint, PalPrimary);
	}

	// VisorNoise is the no-retainer fallback for scanlines/noise.
	if (VisorNoise)
	{
		SetShown(VisorNoise, !RetainerMID);
		if (UMaterialInstanceDynamic* MID = RetainerMID ? nullptr : GetMID(VisorNoise))
		{
			for (const FName& Param : { P_ScanlineDensity, P_ScanlineIntensity, P_NoiseIntensity, P_NoiseFPS })
			{
				if (const float* Value = Config.VisorWarp.Find(Param))
				{
					SetScalar(MID, Param, *Value);
				}
			}
			if (GlitchTier >= 3 && !bReducedMotion)
			{
				SetScalar(MID, P_NoiseIntensity, 0.15f);
			}
			SetVector(MID, P_PersonaTint, PalPrimary);
		}
	}

	if (UMaterialInstanceDynamic* MID = GetMID(VisorFrame))
	{
		SetVector(MID, P_FrameColor, WithAlpha(PalPrimary, 0.5f));
	}
	else
	{
		SetImageColorIfChanged(VisorFrame, WithAlpha(PalPrimary, 0.5f));
	}
}

// ================================================================================================
// Transforms: boot, parallax, head bob, impact kick, glitch opacity (spec sections 4 and 12)
// ================================================================================================

void UHelmetHUDWidget::UpdateTransforms(float DeltaSeconds)
{
	// Camera angular velocity: from the state, or derived from heading/pitch deltas.
	FVector2D Omega = HudState.CameraAngularVelDeg;
	if (Omega.IsNearlyZero() && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const float DYaw = FMath::FindDeltaAngleDegrees(PrevHudState.HeadingDeg, HudState.HeadingDeg);
		const float DPitch = HudState.PitchDeg - PrevHudState.PitchDeg;
		Omega = FVector2D(DYaw, DPitch) / DeltaSeconds;
	}

	const FHUDAnimDef& Boot = Config.GetAnim(A_Boot);
	const float BootElapsed = Now() - BootStartTime;
	const float LastStagger = Config.BootStaggerS.Num() >= 4 ? FMath::Max(FMath::Max(Config.BootStaggerS[0], Config.BootStaggerS[1]), FMath::Max(Config.BootStaggerS[2], Config.BootStaggerS[3])) : 0.f;
	const float BootElemDur = FMath::Max(Boot.DurationS - LastStagger, 0.1f);
	const float KickDur = FMath::Max(Config.ImpactRecoverMs / 1000.f, 0.01f);
	const float KickT = (Now() - KickStart) / KickDur;
	const float KickAmount = KickT < 1.f ? 1.f - HelmetHUDEase(Config.GetAnim(A_ImpactKick).Ease, KickT) : 0.f;
	const int32 JitterStep = FMath::FloorToInt(Now() * 20.f);

	for (int32 k = 0; k < TopElementIdx.Num(); ++k)
	{
		const FHUDElementDef& Def = Config.Elements[TopElementIdx[k]];
		UWidget* Widget = ElementWidgets.IsValidIndex(TopElementIdx[k]) ? ElementWidgets[TopElementIdx[k]].Get() : nullptr;
		if (!Widget)
		{
			continue;
		}
		const int32 LayerIdx = FHelmetHUDConfig::LayerForZ(Def.ZOrder);
		const float Depth = FMath::Clamp(Def.Depth, 0.f, 1.f);

		// Boot
		float BootAlpha = 1.f;
		if (BootElapsed < Boot.DurationS + Boot.DelayS)
		{
			const float Delay = Boot.DelayS + (Config.BootStaggerS.IsValidIndex(LayerIdx) ? Config.BootStaggerS[LayerIdx] : 0.f);
			BootAlpha = HelmetHUDEase(Boot.Ease, (BootElapsed - Delay) / BootElemDur);
		}
		const float BootScaleY = bReducedMotion ? 1.f : FMath::Lerp(0.6f, 1.f, BootAlpha);

		// Parallax (lags behind camera rotation, deeper elements move more)
		FVector2D Target = FVector2D::ZeroVector;
		if (!bReducedMotion && Depth > 0.f)
		{
			Target.X = FMath::Clamp(-Omega.X * Config.ParallaxPxPerDegPerSec * Depth, -Config.ParallaxMaxOffsetPx.X, Config.ParallaxMaxOffsetPx.X);
			Target.Y = FMath::Clamp(Omega.Y * Config.ParallaxPxPerDegPerSec * Depth, -Config.ParallaxMaxOffsetPx.Y, Config.ParallaxMaxOffsetPx.Y);
		}
		const float HalfLifeMs = LayerIdx == 1 ? Config.LagHalfLifeNearMs : (LayerIdx == 2 ? Config.LagHalfLifeMidMs : (LayerIdx == 3 ? Config.LagHalfLifeFarMs : 0.f));
		FVector2D& Offset = ParallaxOffset[k];
		if (HalfLifeMs <= 0.f)
		{
			Offset = Target;
		}
		else
		{
			const float Blend = 1.f - FMath::Pow(2.f, -DeltaSeconds * 1000.f / HalfLifeMs);
			Offset = FMath::Lerp(Offset, Target, Blend);
		}

		FVector2D Translation = Offset;
		if (!bReducedMotion)
		{
			Translation += HudState.HeadBobPx * Depth;
			Translation += KickDir * Config.ImpactKickPx * KickAmount * FMath::Max(Depth, 0.3f);
			if (JitterPx > 0.f && !IsEssential(Def.Id))
			{
				Translation += FVector2D(Hash11(JitterStep, k * 2), Hash11(JitterStep, k * 2 + 1)) * JitterPx;
			}
		}

		// Opacity: subtitle fades etc. (ExtraOpacity), boot, blackout, dropout
		float Opacity = ExtraOpacity[k] * BootAlpha;
		if (bBlackoutActive && !IsEssential(Def.Id))
		{
			Opacity = 0.f;
		}
		if (bDropoutActive && (Def.Id == Id_PitchLadder || Def.Id == Id_SpeedTape || Def.Id == Id_AltitudeTape || Def.Id == Id_HeadingTape))
		{
			Opacity = 0.f;
		}
		SetOpacityIfChanged(Widget, Opacity);
		const FVector2D Scale(ExtraScale[k].X, ExtraScale[k].Y * BootScaleY);
		SetTransformIfChanged(Widget, Translation, Scale, Widget->GetRenderTransform().Angle);
	}

	// Global opacity: HUD opacity setting x low-power brownout.
	float Global = HudOpacityPref;
	if (HudState.PowerFraction <= Config.PowerCritical && !bReducedMotion)
	{
		const FHUDAnimDef& Brownout = Config.GetAnim(A_LowPowerBrownout);
		Global *= 1.f - Brownout.Amplitude * (0.5f + 0.5f * FMath::Sin(2.f * PI * Brownout.LoopHz * Now()));
	}
	if (HUDRetainer)
	{
		SetOpacityIfChanged(HUDRetainer, Global);
	}
	else
	{
		SetOpacityIfChanged(RootCanvas, Global);
	}
}

// ================================================================================================
// NativePaint: tape ticks, pitch ladder, radar rings and blips.
// Painted after the children, so these vectors are not warped by the retainer material.
// ================================================================================================

int32 UHelmetHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 ChildLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bHasState || !HudState.bVisible)
	{
		return ChildLayer;
	}
	const int32 PaintLayer = ChildLayer + 1;
	const UWidget* GlobalHost = HUDRetainer ? static_cast<const UWidget*>(HUDRetainer.Get()) : static_cast<const UWidget*>(RootCanvas.Get());
	const float GlobalAlpha = InWidgetStyle.GetColorAndOpacityTint().A * (GlobalHost ? GlobalHost->GetRenderOpacity() : HudOpacityPref);

	// Rect of a bound element in this widget's local space; false if it is hidden or not laid out.
	auto ElementRect = [&AllottedGeometry](const UWidget* Element, FVector2f& OutPos, FVector2f& OutSize, float& OutAlpha) -> bool
	{
		if (!Element || !Element->IsVisible() || Element->GetRenderOpacity() <= 0.01f)
		{
			return false;
		}
		const FGeometry& ElementGeo = Element->GetCachedGeometry();
		const FVector2f AbsSize = FVector2f(ElementGeo.GetAbsoluteSize());
		if (AbsSize.X < 1.f || AllottedGeometry.Scale <= 0.f)
		{
			return false;
		}
		OutPos = FVector2f(AllottedGeometry.AbsoluteToLocal(FVector2D(ElementGeo.GetAbsolutePosition())));
		OutSize = AbsSize / AllottedGeometry.Scale;
		OutAlpha = Element->GetRenderOpacity();
		return true;
	};
	auto Line = [&](const FVector2f& A, const FVector2f& B, const FLinearColor& Tint, float Thickness)
	{
		TArray<FVector2f> Points;
		Points.Add(A);
		Points.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, PaintLayer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Thickness);
	};
	auto Poly = [&](const TArray<FVector2f>& Points, const FLinearColor& Tint, float Thickness)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, PaintLayer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Thickness);
	};
	auto Label = [&](const FString& Str, const FVector2f& Pos, float Align, const FLinearColor& Tint, int32 FontSize)
	{
		FSlateFontInfo LabelFont = PaintFont;
		LabelFont.Size = FontSize;
		const float ApproxW = 0.62f * float(FontSize) * float(Str.Len());
		const FVector2f TopLeft(Pos.X - ApproxW * Align, Pos.Y - float(FontSize) * 0.65f);
		FSlateDrawElement::MakeText(OutDrawElements, PaintLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(ApproxW + 4.f, float(FontSize) * 1.4f), FSlateLayoutTransform(TopLeft)),
			Str, LabelFont, ESlateDrawEffect::None, Tint);
	};

	const FLinearColor Ink = PalPrimary;
	const FLinearColor Dim = Config.GetColor(C_TickDim, bColorBlindSafe, FLinearColor(0.3f, 0.3f, 0.3f, 1.f));
	FVector2f Pos;
	FVector2f Size2;
	float ElemAlpha = 1.f;

	// ---- Heading tape (horizontal, value grows to the right) ----
	if (ElementRect(HeadingTape.Get(), Pos, Size2, ElemAlpha))
	{
		const float A = GlobalAlpha * ElemAlpha;
		const float Heading = HudState.HeadingDeg;
		const float Px = Config.HeadingPxPerUnit;
		const float Cx = Pos.X + Size2.X * 0.5f;
		const float Bottom = Pos.Y + Size2.Y;
		const float Minor = FMath::Max(Config.HeadingMinor, 1.f);
		const float Start = FMath::CeilToFloat((Heading - Config.HeadingWindowHalf) / Minor) * Minor;
		for (float D = Start; D <= Heading + Config.HeadingWindowHalf; D += Minor)
		{
			const float X = Cx + (D - Heading) * Px;
			if (X < Pos.X || X > Pos.X + Size2.X)
			{
				continue;
			}
			const int32 Deg = ((FMath::RoundToInt(D) % 360) + 360) % 360;
			const bool bMajor = (Deg % FMath::Max(FMath::RoundToInt(Config.HeadingMajor), 1)) == 0;
			const float Len = bMajor ? 10.f : 5.f;
			Line(FVector2f(X, Bottom), FVector2f(X, Bottom - Len), WithAlpha(bMajor ? Ink : Dim, A), 1.f);
			if (Deg % 30 == 0)
			{
				FString Text;
				switch (Deg)
				{
				case 0:   Text = TEXT("N"); break;
				case 90:  Text = TEXT("E"); break;
				case 180: Text = TEXT("S"); break;
				case 270: Text = TEXT("W"); break;
				default:  Text = FString::Printf(TEXT("%02d"), Deg / 10); break;
				}
				Label(Text, FVector2f(X, Bottom - 18.f), 0.5f, WithAlpha(Ink, A), 9);
			}
		}
		for (const float Beacon : HudState.BeaconHeadingsDeg)
		{
			const float Delta = FMath::FindDeltaAngleDegrees(Heading, Beacon);
			const float X = FMath::Clamp(Cx + Delta * Px, Pos.X + 4.f, Pos.X + Size2.X - 4.f);
			TArray<FVector2f> Caret;
			Caret.Add(FVector2f(X - 5.f, Pos.Y));
			Caret.Add(FVector2f(X, Pos.Y + 6.f));
			Caret.Add(FVector2f(X + 5.f, Pos.Y));
			Poly(Caret, WithAlpha(Config.GetColor(C_Objective, bColorBlindSafe), A), 2.f);
		}
	}

	// ---- Speed / altitude tapes (vertical, value grows upward) ----
	auto PaintTape = [&](const UWidget* Tape, float Value, float WindowHalf, float Minor, float Major, float PxPerUnit, bool bTicksOnRight)
	{
		FVector2f TapePos;
		FVector2f TapeSize;
		float TapeAlpha = 1.f;
		if (!ElementRect(Tape, TapePos, TapeSize, TapeAlpha))
		{
			return;
		}
		const float A = GlobalAlpha * TapeAlpha;
		const float Cy = TapePos.Y + TapeSize.Y * 0.5f;
		const float Edge = bTicksOnRight ? TapePos.X + TapeSize.X : TapePos.X;
		const float Dir = bTicksOnRight ? -1.f : 1.f;
		const float Step = FMath::Max(Minor, 0.5f);
		const float Start = FMath::Max(0.f, FMath::CeilToFloat((Value - WindowHalf) / Step) * Step);
		const int32 MajorStep = FMath::Max(FMath::RoundToInt(Major), 1);
		for (float V = Start; V <= Value + WindowHalf; V += Step)
		{
			const float Y = Cy - (V - Value) * PxPerUnit;
			if (Y < TapePos.Y || Y > TapePos.Y + TapeSize.Y || FMath::Abs(Y - Cy) < 12.f)
			{
				continue; // keep the centre readout box clear
			}
			const bool bMajor = (FMath::RoundToInt(V) % MajorStep) == 0;
			const float Len = bMajor ? 10.f : 5.f;
			Line(FVector2f(Edge, Y), FVector2f(Edge + Dir * Len, Y), WithAlpha(bMajor ? Ink : Dim, A), 1.f);
			if (bMajor)
			{
				Label(FString::Printf(TEXT("%d"), FMath::RoundToInt(V)), FVector2f(Edge + Dir * 14.f, Y), bTicksOnRight ? 1.f : 0.f, WithAlpha(Ink, A * 0.85f), 8);
			}
		}
	};
	PaintTape(SpeedTape.Get(), HudState.SpeedMs, Config.SpeedWindowHalf, Config.SpeedMinor, Config.SpeedMajor, Config.SpeedPxPerUnit, true);
	PaintTape(AltitudeTape.Get(), HudState.AltitudeM, Config.AltWindowHalf, Config.AltMinor, Config.AltMajor, Config.AltPxPerUnit, false);

	// ---- Pitch ladder (rotated by roll, negative rungs dashed) ----
	if (ElementRect(PitchLadder.Get(), Pos, Size2, ElemAlpha))
	{
		const float A = GlobalAlpha * ElemAlpha;
		const FVector2f Centre = Pos + Size2 * 0.5f;
		const float RollRad = FMath::DegreesToRadians(-HudState.RollDeg);
		const float CosR = FMath::Cos(RollRad);
		const float SinR = FMath::Sin(RollRad);
		auto Rot = [&](float X, float Y) { return Centre + FVector2f(X * CosR - Y * SinR, X * SinR + Y * CosR); };
		const float StepDeg = FMath::Max(Config.LadderStepDeg, 1.f);
		const float Pitch = HudState.PitchDeg;
		const float Start = FMath::CeilToFloat((Pitch - Config.LadderVisibleDeg) / StepDeg) * StepDeg;
		const float HalfW = Size2.X * 0.5f;
		for (float P = FMath::Max(Start, -90.f); P <= FMath::Min(Pitch + Config.LadderVisibleDeg, 90.f); P += StepDeg)
		{
			const float Y = (Pitch - P) * Config.LadderPxPerDeg;
			if (FMath::Abs(Y) > Size2.Y * 0.5f)
			{
				continue;
			}
			const bool bHorizon = FMath::IsNearlyZero(P);
			const float Gap = bHorizon ? HalfW * 0.15f : HalfW * 0.25f;
			const float RungHalfLen = bHorizon ? HalfW : HalfW * 0.6f;
			const FLinearColor Tint = WithAlpha(Ink, A * (bHorizon ? 0.9f : 0.7f));
			for (const float Side : { -1.f, 1.f })
			{
				if (P < 0.f)
				{
					const float DashLen = 6.f;
					for (float X = Gap; X < RungHalfLen; X += DashLen * 2.f)
					{
						Line(Rot(Side * X, Y), Rot(Side * FMath::Min(X + DashLen, RungHalfLen), Y), Tint, 1.f);
					}
				}
				else
				{
					Line(Rot(Side * Gap, Y), Rot(Side * RungHalfLen, Y), Tint, bHorizon ? 1.5f : 1.f);
				}
				if (!bHorizon)
				{
					// End tick points toward the horizon.
					const float TickY = P > 0.f ? 5.f : -5.f;
					Line(Rot(Side * RungHalfLen, Y), Rot(Side * RungHalfLen, Y + TickY), Tint, 1.f);
					Label(FString::Printf(TEXT("%d"), FMath::RoundToInt(P)), Rot(Side * (RungHalfLen + 14.f), Y), 0.5f, Tint, 8);
				}
			}
		}
	}

	// ---- Threat radar ----
	if (ElementRect(ThreatRadar.Get(), Pos, Size2, ElemAlpha))
	{
		const float A = GlobalAlpha * ElemAlpha;
		const FVector2f Centre = Pos + Size2 * 0.5f;
		const float Radius = FMath::Max(FMath::Min(Size2.X, Size2.Y) * 0.5f - 2.f, 4.f);
		// Street map: the city's roads round the player, heading-up, under the rings and blips.
		{
			float MapRange = HudState.RadarRangeM;
			if (MapRange <= 0.f)
			{
				MapRange = HudState.bBossActive ? Config.RadarRangeBossM : (HudState.bFlying ? Config.RadarRangeFlightM : Config.RadarRangeGroundM);
			}
			MapRange = FMath::Max(MapRange, 1.f);
			const FLinearColor RoadInk = WithAlpha(Ink, A * 0.16f);
			for (int32 i = 0; i < MinimapLocalSegs.Num(); ++i)
			{
				const FVector4f& Seg = MinimapLocalSegs[i];
				const float WidthPx = FMath::Clamp(MinimapLocalWidthM[i] / MapRange * Radius, 1.f, 7.f);
				Line(Centre + FVector2f(Seg.X, -Seg.Y) / MapRange * Radius, Centre + FVector2f(Seg.Z, -Seg.W) / MapRange * Radius, RoadInk, WidthPx);
			}
			if (bMinimapHasNorth)
			{
				// "N" drawn with lines just outside the outer ring (world +X is north, as on the heading tape).
				const float NorthRad = FMath::DegreesToRadians(MinimapNorthDeg);
				const FVector2f NDir(FMath::Sin(NorthRad), -FMath::Cos(NorthRad));
				const FVector2f NC = Centre + NDir * (Radius + 8.f);
				const FLinearColor NInk = WithAlpha(Ink, A * 0.85f);
				Line(Centre + NDir * (Radius - 3.f), Centre + NDir * (Radius + 2.f), NInk, 1.5f);
				Poly({ NC + FVector2f(-2.5f, 3.5f), NC + FVector2f(-2.5f, -3.5f), NC + FVector2f(2.5f, 3.5f), NC + FVector2f(2.5f, -3.5f) }, NInk, 1.2f);
			}
		}
		for (int32 Ring = 1; Ring <= 3; ++Ring)
		{
			const float R = Radius * float(Ring) / 3.f;
			TArray<FVector2f> Circle;
			for (int32 i = 0; i <= 32; ++i)
			{
				const float Ang = 2.f * PI * float(i) / 32.f;
				Circle.Add(Centre + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * R);
			}
			Poly(Circle, WithAlpha(Ink, A * (Ring == 3 ? 0.5f : 0.25f)), 1.f);
		}
		if (!ImageMIDs.Contains(RadarSweep) && !bReducedMotion)
		{
			const float Ang = 2.f * PI * FMath::Frac(HudTime / FMath::Max(Config.RadarSweepPeriodS, 0.1f)) - HALF_PI;
			Line(Centre, Centre + FVector2f(FMath::Cos(Ang), FMath::Sin(Ang)) * Radius, WithAlpha(Ink, A * 0.5f), 1.f);
		}
		float Range = HudState.RadarRangeM;
		if (Range <= 0.f)
		{
			Range = HudState.bBossActive ? Config.RadarRangeBossM : (HudState.bFlying ? Config.RadarRangeFlightM : Config.RadarRangeGroundM);
		}
		for (const FHUDRadarBlip& Blip : HudState.Blips)
		{
			FVector2f P(Blip.LocalXY.X / Range, -Blip.LocalXY.Y / Range);
			const float Len = P.Size();
			const bool bEdge = Len > 1.f;
			if (bEdge)
			{
				P /= Len;
			}
			const FVector2f C = Centre + P * Radius;
			FLinearColor BlipColor;
			float S = 2.5f;
			switch (Blip.Type)
			{
			case EHUDThreatType::Tank:       BlipColor = Config.GetColor(C_ThreatTank, bColorBlindSafe); S = 3.5f; break;
			case EHUDThreatType::Boss:       BlipColor = Config.GetColor(C_ThreatBoss, bColorBlindSafe); S = 5.f; break;
			case EHUDThreatType::Projectile: BlipColor = Config.GetColor(C_ThreatProjectile, bColorBlindSafe); S = 1.5f; break;
			case EHUDThreatType::Ally:       BlipColor = Config.GetColor(C_Ally, bColorBlindSafe); break;
			case EHUDThreatType::Pickup:     BlipColor = Config.GetColor(C_Pickup, bColorBlindSafe); break;
			case EHUDThreatType::Beacon:     BlipColor = Config.GetColor(C_Objective, bColorBlindSafe); S = 4.f; break;
			default:                         BlipColor = Config.GetColor(C_ThreatDrone, bColorBlindSafe); break;
			}
			BlipColor.A *= A * (bEdge ? 0.6f : 1.f);
			TArray<FVector2f> Shape;
			if (Blip.Type == EHUDThreatType::Boss || Blip.Type == EHUDThreatType::Beacon)
			{
				Shape = { C + FVector2f(0.f, -S), C + FVector2f(S, 0.f), C + FVector2f(0.f, S), C + FVector2f(-S, 0.f), C + FVector2f(0.f, -S) };
			}
			else if (Blip.Type == EHUDThreatType::Ally)
			{
				Shape = { C + FVector2f(0.f, -S), C + FVector2f(S, S), C + FVector2f(-S, S), C + FVector2f(0.f, -S) };
			}
			else if (Blip.Type == EHUDThreatType::Pickup)
			{
				Line(C + FVector2f(-S, 0.f), C + FVector2f(S, 0.f), BlipColor, 1.5f);
				Line(C + FVector2f(0.f, -S), C + FVector2f(0.f, S), BlipColor, 1.5f);
			}
			else
			{
				Shape = { C + FVector2f(-S, -S), C + FVector2f(S, -S), C + FVector2f(S, S), C + FVector2f(-S, S), C + FVector2f(-S, -S) };
			}
			if (Shape.Num() > 0)
			{
				Poly(Shape, BlipColor, 1.5f);
			}
			if (FMath::Abs(Blip.HeightDeltaM) > Config.RadarElevationMarkerM)
			{
				const float Up = Blip.HeightDeltaM > 0.f ? -1.f : 1.f;
				Line(C + FVector2f(0.f, Up * (S + 1.f)), C + FVector2f(0.f, Up * (S + 5.f)), BlipColor, 1.f);
			}
		}
	}

	// FRIDAY directional damage arcs (HUDDataSubsystem), painted on top of the HUD in the visor space.
	int32 TopLayer = PaintLayer;
	if (const UHUDDataSubsystem* ArcData = UHUDDataSubsystem::Get(this))
	{
		UDamageDirectionWidget::FPaintStyle ArcStyle;
		ArcStyle.Opacity = 0.85f * InWidgetStyle.GetColorAndOpacityTint().A * HudOpacityPref;
		ArcStyle.bReducedMotion = bReducedMotion;
		ArcStyle.bHealthCritical = ArcData->GetHealthFraction() <= 0.25f;
		TopLayer = UDamageDirectionWidget::PaintArcs(ArcData->GetArcs(), ArcStyle, AllottedGeometry, OutDrawElements, PaintLayer);
	}
	return TopLayer;
}

// ================================================================================================
// FillStateFromGame: builds the state from the project's real getters.
// ================================================================================================

void UHelmetHUDWidget::FillStateFromGame(const UObject* WorldContext, const AIronManCharacter* Player, FHelmetHUDState& InOutState, float DeltaSeconds)
{
	FillStateFromGame(WorldContext, Player, InOutState, InOutState, DeltaSeconds);
}

void UHelmetHUDWidget::FillStateFromGame(const UObject* WorldContext, const AIronManCharacter* Player, const FHelmetHUDState& Prev, FHelmetHUDState& Out, float DeltaSeconds)
{
	// Read Prev before Out is reset (they may be the same object).
	const float PrevPower = Prev.PowerFraction;
	const float PrevRate = Prev.PowerRatePerS;
	const bool bHadPrev = Prev.bVisible;

	// Back to defaults, but keep the array allocations (Targets, Blips, beacons, zones) for reuse.
	{
		TArray<FHUDTarget> Targets = MoveTemp(Out.Targets);
		TArray<FHUDRadarBlip> Blips = MoveTemp(Out.Blips);
		TArray<float> Beacons = MoveTemp(Out.BeaconHeadingsDeg);
		TArray<float> Regions = MoveTemp(Out.RegionIntegrity);
		Out = FHelmetHUDState();
		Targets.Reset();
		Blips.Reset();
		Beacons.Reset();
		Regions.Reset();
		Out.Targets = MoveTemp(Targets);
		Out.Blips = MoveTemp(Blips);
		Out.BeaconHeadingsDeg = MoveTemp(Beacons);
		Out.RegionIntegrity = MoveTemp(Regions);
	}

	UWorld* GameWorld = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!Player || !GameWorld)
	{
		Out.bVisible = false;
		return;
	}
	// Cached enemy / pickup lists and the throttled AGL trace live here (null outside Game/PIE worlds).
	UHUDDataSubsystem* HUDData = UHUDDataSubsystem::Get(WorldContext);

	// Same lookup as UArenaHUDWidget (AIronManCharacter::GetArenaGameMode is protected upstream).
	const AArenaGameMode* GM = GameWorld->GetAuthGameMode<AArenaGameMode>();
	const FSuitDef& Suit = Player->GetSuit();

	// ---- Mode ----
	const bool bPlaying = !GM || GM->GetArenaState() == EArenaGameState::Playing;
	Out.bVisible = bPlaying && Player->IsFirstPerson() && !Player->IsInCinematic() && !Player->IsHulkbusterActive();
	Out.bFlying = Player->IsFlying();
	Out.bBoosting = Player->IsBoosting();
	Out.bDead = Player->IsDead();
	Out.bStoryMode = GM && GM->GetPlayMode() == EArenaPlayMode::Story;
	const UJarvisComponent* Jarvis = UJarvisComponent::Get(Player);
	Out.Persona = Jarvis ? Jarvis->GetPersona() : Suit.AI;
	Out.SuitName = Suit.Name;

	// ---- Flight ----
	const FVector Location = Player->GetActorLocation();
	const FVector Velocity = Player->GetVelocity();
	Out.SpeedMs = float(Velocity.Size()) / 100.f;
	Out.VerticalSpeedMs = float(Velocity.Z) / 100.f;
	Out.AltitudeM = float(Location.Z - (GM ? GM->GetArenaFloorZ() : 0.f)) / 100.f;
	if (HUDData)
	{
		Out.AglM = HUDData->GetAglM(Player); // ~10 Hz trace, extrapolated between
	}
	else
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HelmetHUDAgl), false, Player);
		if (GameWorld->LineTraceSingleByChannel(Hit, Location, Location - FVector(0.0, 0.0, 20000.0), ECC_Visibility, Params))
		{
			Out.AglM = FMath::Max(0.f, (Hit.Distance - Player->GetSimpleCollisionHalfHeight()) / 100.f);
		}
	}
	const FRotator ControlRot = Player->GetControlRotation();
	Out.HeadingDeg = float(FMath::Fmod(FRotator::ClampAxis(ControlRot.Yaw), 360.0));
	Out.PitchDeg = float(FRotator::NormalizeAxis(ControlRot.Pitch));
	Out.RollDeg = float(FRotator::NormalizeAxis(ControlRot.Roll));

	// ---- Reactor / power ----
	const float MaxEnergy = Player->GetMaxEnergy();
	Out.PowerFraction = MaxEnergy > 0.f ? FMath::Clamp(Player->GetEnergy() / MaxEnergy, 0.f, 1.f) : 0.f;
	if (bHadPrev && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const float Raw = (Out.PowerFraction - PrevPower) * MaxEnergy / DeltaSeconds;
		Out.PowerRatePerS = FMath::Lerp(PrevRate, Raw, 1.f - FMath::Exp(-DeltaSeconds / 0.25f));
	}
	if (const UReactorComponent* Reactor = Player->GetReactor())
	{
		Out.bHasReactor = true;
		Out.HeatFraction = Reactor->GetHeatFraction();
		Out.bOverheated = Reactor->IsOverheated();
		Out.bVenting = Reactor->IsVenting();
		Out.VentCooldownLeft = Reactor->GetVentCooldownLeft();
		Out.Instability = Reactor->GetInstability();
		Out.bCoreDark = Reactor->IsCoreDark();
		Out.ReactorStage = Reactor->HasStage() ? Reactor->GetStage() : EReactorStage::Upgraded;
		Out.CoreColor = Reactor->GetCoreColor(USuitLibrary::GetPersona(Out.Persona).HudTint);
		Out.CommsSignalLoss = (Reactor->HasStage() && Reactor->GetStage() == EReactorStage::Failed) ? 0.4f : 0.f;
	}

	// ---- Integrity ----
	const float MaxHealth = Player->GetMaxHealth();
	Out.HealthFraction = MaxHealth > 0.f ? FMath::Clamp(Player->GetHealth() / MaxHealth, 0.f, 1.f) : 0.f;

	// ---- Combat ----
	Out.WeaponName = Player->GetPrimaryName();
	Out.SpecialName = Player->GetSpecialName();
	Out.SpecialCost = Player->GetSpecialCost();
	Out.bBeamActive = Player->IsBeamActive();
	{
		FText EffectName;
		Out.ActiveEffectTimeLeft = Player->GetActiveEffectTime(EffectName);
		Out.ActiveEffectName = EffectName;
	}
	Out.bHulkbusterActive = Player->IsHulkbusterActive();
	Out.HulkbusterTimeLeft = Player->GetHulkbusterTimeLeft();
	Out.HulkbusterCharges = Player->GetHulkbusterCharges();
	Out.RadarRangeM = 0.f; // auto (ground / flight / boss) from hud_layout.json thresholds.radar

	// ---- Targets and radar ----
	APlayerController* PC = Cast<APlayerController>(Player->GetController()); // non-const: ProjectWorldLocationToWidgetPosition takes APlayerController*
	FVector CamLoc = Location;
	FRotator CamRot = ControlRot;
	float FovDeg = 90.f;
	if (PC && PC->PlayerCameraManager)
	{
		CamLoc = PC->PlayerCameraManager->GetCameraLocation();
		CamRot = PC->PlayerCameraManager->GetCameraRotation();
		FovDeg = PC->PlayerCameraManager->GetFOVAngle();
	}
	FVector2D ViewSize(1920.0, 1080.0);
	if (PC)
	{
		int32 ViewX = 0;
		int32 ViewY = 0;
		PC->GetViewportSize(ViewX, ViewY);
		const float DpiScale = UWidgetLayoutLibrary::GetViewportScale(PC);
		if (ViewX > 0 && ViewY > 0 && DpiScale > 0.f)
		{
			ViewSize = FVector2D(double(ViewX), double(ViewY)) / double(DpiScale);
		}
	}
	const FVector CamFwd = CamRot.Vector();
	const FRotator YawOnly(0.0, ControlRot.Yaw, 0.0);
	const FVector YawFwd = YawOnly.Vector();
	const FVector YawRight = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	const float TanHalfFov = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FovDeg, 10.f, 170.f) * 0.5f));

	auto AddBlip = [&Out, &Location, &YawFwd, &YawRight](const FVector& WorldPos, EHUDThreatType Type)
	{
		const FVector Delta = WorldPos - Location;
		FHUDRadarBlip Blip;
		Blip.LocalXY = FVector2D(FVector::DotProduct(Delta, YawRight), FVector::DotProduct(Delta, YawFwd)) / 100.0;
		Blip.HeightDeltaM = float(Delta.Z) / 100.f;
		Blip.Type = Type;
		Out.Blips.Add(Blip);
	};
	auto AddTarget = [&](const AActor* Actor, const FVector& Aim, float HitRadiusCm, EHUDThreatType Type)
	{
		const FVector ToTarget = Aim - CamLoc;
		const float Dist = float(ToTarget.Size());
		if (Dist < 1.f)
		{
			return;
		}
		FHUDTarget Target;
		Target.TargetId = int32(Actor->GetUniqueID());
		Target.Type = Type;
		Target.DistanceM = Dist / 100.f;
		Target.HeightDeltaM = float(Aim.Z - Location.Z) / 100.f;
		const FVector Dir = ToTarget / Dist;
		Target.OffAxisDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(CamFwd, Dir)), -1.f, 1.f)));

		FVector2D WidgetPos = FVector2D::ZeroVector;
		const bool bInFront = PC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Aim, WidgetPos, false);
		Target.bOnScreen = bInFront && WidgetPos.X >= 0.0 && WidgetPos.Y >= 0.0 && WidgetPos.X <= ViewSize.X && WidgetPos.Y <= ViewSize.Y;
		Target.ScreenPos = WidgetPos;
		const float SizePx = (2.f * HitRadiusCm / Dist) / (2.f * TanHalfFov) * float(ViewSize.X);
		Target.ScreenSize = FVector2D(FMath::Clamp(SizePx, 24.f, 400.f), FMath::Clamp(SizePx, 24.f, 400.f));
		const FVector LocalDir = CamRot.UnrotateVector(Dir); // X fwd, Y right, Z up
		Target.EdgeAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(float(LocalDir.Y), float(LocalDir.Z)));
		Out.Targets.Add(Target);
	};

	auto AddEnemy = [&](const AEnemyBase* Enemy)
	{
		if (!Enemy || Enemy->IsDead())
		{
			return;
		}
		const EHUDThreatType Type = ThreatFromKind(Enemy->GetKind());
		AddTarget(Enemy, Enemy->GetAimPoint(), Enemy->GetHitRadius(), Type);
		AddBlip(Enemy->GetActorLocation(), Type);
	};
	if (HUDData)
	{
		// Event-driven lists (OnActorSpawned + 1 Hz rescan): no world iteration per frame.
		for (const TWeakObjectPtr<AEnemyBase>& Enemy : HUDData->GetTrackedEnemies())
		{
			AddEnemy(Enemy.Get());
		}
		for (const TWeakObjectPtr<APickup>& Pickup : HUDData->GetTrackedPickups())
		{
			if (const APickup* P = Pickup.Get())
			{
				AddBlip(P->GetActorLocation(), EHUDThreatType::Pickup);
			}
		}
	}
	else
	{
		for (TActorIterator<AEnemyBase> It(GameWorld); It; ++It)
		{
			AddEnemy(*It);
		}
		for (TActorIterator<APickup> It(GameWorld); It; ++It)
		{
			if (const APickup* Pickup = *It)
			{
				AddBlip(Pickup->GetActorLocation(), EHUDThreatType::Pickup);
			}
		}
	}

	// ---- Game ----
	if (GM)
	{
		if (const ABossTitan* Boss = GM->GetActiveBoss())
		{
			Out.bBossActive = !Boss->IsDead();
			Out.BossName = Boss->GetBossName();
			Out.BossFraction = Boss->GetMaxHealth() > 0.f ? FMath::Clamp(Boss->GetHealth() / Boss->GetMaxHealth(), 0.f, 1.f) : 0.f;
			Out.BossPhase = Boss->GetPhase();
			Out.bBossInvulnerable = Boss->IsInvulnerable();
		}
		Out.Score = GM->GetScore();
		Out.Combo = GM->GetCombo();
		Out.ComboTimeFraction = GM->GetComboTimeFraction();
		Out.Wave = GM->GetWave();

		const AStoryDirector* Story = GM->GetStoryDirector();
		if (Story && Story->IsActive())
		{
			Story->GetObjectiveInfo(Out.ObjectiveChapter, Out.ObjectiveText, Out.ObjectiveProgress);
			FVector BeaconLoc;
			if (Story->GetBeaconLocation(BeaconLoc))
			{
				AddBlip(BeaconLoc, EHUDThreatType::Beacon);
				const FVector ToBeacon = BeaconLoc - Location;
				Out.BeaconHeadingsDeg.Add(float(FRotator::ClampAxis(ToBeacon.Rotation().Yaw)));
			}
			if (const AStoryAlly* Ally = Story->GetActiveAlly())
			{
				AddBlip(Ally->GetActorLocation(), EHUDThreatType::Ally);
			}
		}
	}

	// ---- Comms / subtitles ----
	if (Jarvis)
	{
		FText SpeakerLabel;
		FText Line;
		FLinearColor LineColor = FLinearColor::White;
		float LineAlpha = 0.f;
		if (Jarvis->GetSubtitle(SpeakerLabel, Line, LineColor, LineAlpha))
		{
			Out.SubtitleSpeaker = SpeakerLabel;
			Out.SubtitleLine = Line;
			Out.SubtitleColor = LineColor;
			Out.SubtitleAlpha = LineAlpha;
			const FText& PersonaLabel = USuitLibrary::GetPersona(Out.Persona).Label;
			if (!SpeakerLabel.IsEmpty() && !SpeakerLabel.EqualToCaseIgnored(PersonaLabel))
			{
				// Someone other than the suit AI is talking: open the comms portrait.
				Out.CommsSpeaker = FName(*SpeakerLabel.ToString());
				Out.CommsSpeakerLabel = SpeakerLabel;
				Out.CommsEnvelope = LineAlpha;
			}
			else
			{
				Out.SpeakingEnvelope = LineAlpha;
			}
		}
	}
	if (const UArenaGameUserSettings* Settings = UArenaGameUserSettings::GetArenaSettings())
	{
		Out.bSubtitlesEnabled = Settings->bSubtitles;
	}

	// Hit direction, struck zone and per-zone wear from UHUDDataSubsystem (fed by AIronManCharacter::TakeDamage).
	if (HUDData)
	{
		Out.ImpactDir2D = HUDData->GetImpactDir2D();
		Out.LastHitRegion = HUDData->GetLastHitRegion();
		Out.RegionIntegrity.Append(HUDData->GetRegionIntegrityRef()); // into the kept buffer, no temp copy
	}
	// Not wired (no getter in the project yet): bIncomingMissile, bPullUp (derived from AGL).
}

#undef LOCTEXT_NAMESPACE
