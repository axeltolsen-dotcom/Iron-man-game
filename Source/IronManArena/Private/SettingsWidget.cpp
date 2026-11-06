#include "SettingsWidget.h"

#include "ArenaUI.h"
#include "ArenaGameUserSettings.h"
#include "ArenaPlayerController.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "SettingsWidget"

namespace
{
	const int32 FrameCaps[] = { 30, 60, 90, 120, 144, 165, 240, 0 };
	constexpr int32 NumFrameCaps = UE_ARRAY_COUNT(FrameCaps);
	const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };

	FString ResString(const FIntPoint& R) { return FString::Printf(TEXT("%d x %d"), R.X, R.Y); }
	FString CapString(int32 Cap) { return Cap > 0 ? FString::Printf(TEXT("%d fps"), Cap) : FString(TEXT("Unlimited")); }
}

void USettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

TSharedRef<SWidget> USettingsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildTree(); }
	return Super::RebuildWidget();
}

void USettingsWidget::AddHeader(UVerticalBox* Box, const FText& Label)
{
	UTextBlock* T = ArenaUI::MakeText(WidgetTree, Label, 18, ArenaUI::Red);
	T->SetJustification(ETextJustify::Left);
	if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(T)) { S->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f)); }
}

namespace
{
	UHorizontalBox* MakeRow(UWidgetTree* Tree, UVerticalBox* Box, const FText& Label)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(230.f);
		UTextBlock* T = ArenaUI::MakeText(Tree, Label, 15, ArenaUI::Ice, "Regular");
		T->SetJustification(ETextJustify::Left);
		LabelBox->SetContent(T);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) { S->SetVerticalAlignment(VAlign_Center); }
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row)) { S->SetPadding(FMargin(0.f, 3.f)); }
		return Row;
	}

	void AddToRow(UWidgetTree* Tree, UHorizontalBox* Row, UWidget* W, float Width)
	{
		UWidget* Content = W;
		if (Width > 0.f)
		{
			USizeBox* SB = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			SB->SetWidthOverride(Width);
			SB->SetContent(W);
			Content = SB;
		}
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Content))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(6.f, 0.f));
		}
	}
}

UComboBoxString* USettingsWidget::AddCombo(UVerticalBox* Box, const FText& Label)
{
	UHorizontalBox* Row = MakeRow(WidgetTree, Box, Label);
	UComboBoxString* C = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	C->OnSelectionChanged.AddDynamic(this, &USettingsWidget::HandleComboChanged);
	AddToRow(WidgetTree, Row, C, 260.f);
	return C;
}

UCheckBox* USettingsWidget::AddCheck(UVerticalBox* Box, const FText& Label)
{
	UHorizontalBox* Row = MakeRow(WidgetTree, Box, Label);
	UCheckBox* C = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	C->OnCheckStateChanged.AddDynamic(this, &USettingsWidget::HandleCheckChanged);
	AddToRow(WidgetTree, Row, C, 0.f);
	return C;
}

USlider* USettingsWidget::AddSlider(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, UTextBlock*& OutValue)
{
	UHorizontalBox* Row = MakeRow(WidgetTree, Box, Label);
	USlider* S = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	S->SetMinValue(Min);
	S->SetMaxValue(Max);
	S->SetStepSize(Step);
	S->SetSliderBarColor(FLinearColor(0.3f, 0.3f, 0.35f));
	S->SetSliderHandleColor(ArenaUI::Gold);
	S->OnValueChanged.AddDynamic(this, &USettingsWidget::HandleSliderChanged);
	AddToRow(WidgetTree, Row, S, 260.f);
	OutValue = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 15, ArenaUI::Gold, "Regular");
	AddToRow(WidgetTree, Row, OutValue, 70.f);
	return S;
}

void USettingsWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.02f, 0.85f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Outer"));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Outer))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	UTextBlock* Title = ArenaUI::MakeText(WidgetTree, LOCTEXT("Title", "SETTINGS"), 40, ArenaUI::Red);
	if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(Title)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f)); }

	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ScrollSize->SetHeightOverride(560.f);
	ScrollSize->SetWidthOverride(660.f);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ScrollSize->SetContent(Scroll);
	Outer->AddChildToVerticalBox(ScrollSize);
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Box);

	UTextBlock* Tmp = nullptr;
	AddHeader(Box, LOCTEXT("Video", "VIDEO"));
	WindowModeCombo = AddCombo(Box, LOCTEXT("WindowMode", "Window mode"));
	WindowModeCombo->AddOption(TEXT("Fullscreen"));
	WindowModeCombo->AddOption(TEXT("Windowed Fullscreen"));
	WindowModeCombo->AddOption(TEXT("Windowed"));
	ResolutionCombo = AddCombo(Box, LOCTEXT("Resolution", "Resolution"));
	VSyncCheck = AddCheck(Box, LOCTEXT("VSync", "VSync"));
	FrameCapCombo = AddCombo(Box, LOCTEXT("FrameCap", "Frame rate limit"));
	for (int32 Cap : FrameCaps) { FrameCapCombo->AddOption(CapString(Cap)); }
	QualityCombo = AddCombo(Box, LOCTEXT("Quality", "Graphics quality"));
	for (const TCHAR* Q : QualityNames) { QualityCombo->AddOption(Q); }
	RayTracingCheck = AddCheck(Box, UArenaGameUserSettings::IsRayTracingSupported()
		? LOCTEXT("RT", "Hardware ray tracing (RTX-class GPU)")
		: LOCTEXT("RTNo", "Hardware ray tracing (not supported on this GPU / RHI)"));
	MotionBlurCheck = AddCheck(Box, LOCTEXT("MotionBlur", "Motion blur"));
	DofCheck = AddCheck(Box, LOCTEXT("Dof", "Cinematic depth of field"));

	AddHeader(Box, LOCTEXT("Controls", "CAMERA & CONTROLS"));
	FovSlider = AddSlider(Box, LOCTEXT("Fov", "Field of view"), 70.f, 120.f, 1.f, Tmp); FovValue = Tmp;
	SensSlider = AddSlider(Box, LOCTEXT("Sens", "Mouse sensitivity"), 0.1f, 3.f, 0.05f, Tmp); SensValue = Tmp;
	InvertCheck = AddCheck(Box, LOCTEXT("Invert", "Invert Y"));

	AddHeader(Box, LOCTEXT("Audio", "AUDIO"));
	MasterSlider = AddSlider(Box, LOCTEXT("Master", "Master volume"), 0.f, 1.f, 0.01f, Tmp); MasterValue = Tmp;
	SfxSlider = AddSlider(Box, LOCTEXT("Sfx", "SFX volume"), 0.f, 1.f, 0.01f, Tmp); SfxValue = Tmp;
	VoiceSlider = AddSlider(Box, LOCTEXT("Voice", "Voice volume"), 0.f, 1.f, 0.01f, Tmp); VoiceValue = Tmp;
	MusicSlider = AddSlider(Box, LOCTEXT("Music", "Music volume"), 0.f, 1.f, 0.01f, Tmp); MusicValue = Tmp;

	AddHeader(Box, LOCTEXT("Access", "ACCESSIBILITY"));
	SubtitlesCheck = AddCheck(Box, LOCTEXT("Subtitles", "Subtitles"));
	ShakeCheck = AddCheck(Box, LOCTEXT("Shake", "Camera shake"));
	ShakeSlider = AddSlider(Box, LOCTEXT("ShakeAmt", "Camera shake intensity"), 0.f, 1.5f, 0.05f, Tmp); ShakeValue = Tmp;
	SubtitleScaleSlider = AddSlider(Box, LOCTEXT("SubSize", "Subtitle size"), 0.8f, 1.6f, 0.05f, Tmp); SubtitleScaleValue = Tmp;

	AddHeader(Box, LOCTEXT("HelmetHud", "HELMET HUD"));
	HudColorBlindCheck = AddCheck(Box, LOCTEXT("HudCB", "Colour-blind safe HUD"));
	HudReducedMotionCheck = AddCheck(Box, LOCTEXT("HudRM", "Reduced motion"));
	HudOpacitySlider = AddSlider(Box, LOCTEXT("HudOp", "HUD opacity"), 0.5f, 1.f, 0.05f, Tmp); HudOpacityValue = Tmp;
	HudScaleSlider = AddSlider(Box, LOCTEXT("HudScale", "HUD scale"), 0.75f, 1.25f, 0.05f, Tmp); HudScaleValue = Tmp;

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto AddButton = [this, Buttons](const FText& Label) -> UButton*
	{
		UTextBlock* L = nullptr;
		UButton* B = ArenaUI::MakeButton(WidgetTree, Label, 18, L);
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(B)) { S->SetPadding(FMargin(8.f, 0.f)); }
		return B;
	};
	AddButton(LOCTEXT("Apply", "APPLY"))->OnClicked.AddDynamic(this, &USettingsWidget::HandleApply);
	AddButton(LOCTEXT("Defaults", "DEFAULTS"))->OnClicked.AddDynamic(this, &USettingsWidget::HandleDefaults);
	AddButton(LOCTEXT("Back", "BACK"))->OnClicked.AddDynamic(this, &USettingsWidget::HandleBack);
	if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(Buttons)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 14.f, 0.f, 4.f)); }

	StatusText = ArenaUI::MakeText(WidgetTree, LOCTEXT("Hint", "F11 / Alt+Enter toggles fullscreen anywhere.  Esc = back."), 13, ArenaUI::Ice, "Regular");
	if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(StatusText)) { S->SetHorizontalAlignment(HAlign_Center); }

	RefreshFromSettings();
}

void USettingsWidget::RefreshFromSettings()
{
	UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	if (!S || !WindowModeCombo) { return; }
	bRefreshing = true;

	switch (S->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen: WindowModeCombo->SetSelectedIndex(0); break;
	case EWindowMode::WindowedFullscreen: WindowModeCombo->SetSelectedIndex(1); break;
	default: WindowModeCombo->SetSelectedIndex(2); break;
	}

	Resolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	const FIntPoint Current = S->GetScreenResolution();
	if (!Resolutions.Contains(Current)) { Resolutions.Add(Current); }
	const FIntPoint Desk = S->GetDesktopResolution();
	if (!Resolutions.Contains(Desk)) { Resolutions.Add(Desk); }
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	ResolutionCombo->ClearOptions();
	for (const FIntPoint& R : Resolutions) { ResolutionCombo->AddOption(ResString(R)); }
	ResolutionCombo->SetSelectedOption(ResString(Current));

	VSyncCheck->SetIsChecked(S->IsVSyncEnabled());
	const int32 Cap = FMath::RoundToInt(S->GetFrameRateLimit());
	int32 CapIdx = NumFrameCaps - 1;
	for (int32 i = 0; i < NumFrameCaps; ++i) { if (FrameCaps[i] == Cap) { CapIdx = i; } }
	FrameCapCombo->SetSelectedIndex(CapIdx);
	QualityCombo->SetSelectedIndex(FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 4)); // -1 (custom) -> Low; re-apply fixes it

	FovSlider->SetValue(S->FieldOfView);
	SensSlider->SetValue(S->MouseSensitivity);
	InvertCheck->SetIsChecked(S->bInvertY);
	MasterSlider->SetValue(S->MasterVolume);
	SfxSlider->SetValue(S->SfxVolume);
	VoiceSlider->SetValue(S->VoiceVolume);
	MusicSlider->SetValue(S->MusicVolume);
	SubtitlesCheck->SetIsChecked(S->bSubtitles);
	RayTracingCheck->SetIsChecked(S->bRayTracing);
	MotionBlurCheck->SetIsChecked(S->bMotionBlur);
	DofCheck->SetIsChecked(S->bCinematicDOF);
	ShakeCheck->SetIsChecked(S->bCameraShake);
	ShakeSlider->SetValue(S->CameraShakeIntensity);
	SubtitleScaleSlider->SetValue(S->SubtitleScale);
	HudColorBlindCheck->SetIsChecked(S->bHudColorBlindSafe);
	HudReducedMotionCheck->SetIsChecked(S->bHudReducedMotion);
	HudOpacitySlider->SetValue(S->HudOpacity);
	HudScaleSlider->SetValue(S->HudScale);
	UpdateSliderLabels();
	bRefreshing = false;
}

void USettingsWidget::UpdateSliderLabels()
{
	auto Pct = [](UTextBlock* T, const USlider* Sl) { if (T && Sl) { T->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Sl->GetValue() * 100.f)))); } };
	if (FovValue) { FovValue->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(FovSlider->GetValue())))); }
	if (SensValue) { SensValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), SensSlider->GetValue()))); }
	Pct(MasterValue, MasterSlider);
	Pct(ShakeValue, ShakeSlider);
	Pct(SfxValue, SfxSlider);
	Pct(VoiceValue, VoiceSlider);
	Pct(MusicValue, MusicSlider);
	Pct(HudOpacityValue, HudOpacitySlider);
	Pct(HudScaleValue, HudScaleSlider);
	Pct(SubtitleScaleValue, SubtitleScaleSlider);
}

void USettingsWidget::ApplyLiveOptions()
{
	UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	if (!S) { return; }
	S->FieldOfView = FovSlider->GetValue();
	S->MouseSensitivity = SensSlider->GetValue();
	S->bInvertY = InvertCheck->IsChecked();
	S->MasterVolume = MasterSlider->GetValue();
	S->SfxVolume = SfxSlider->GetValue();
	S->VoiceVolume = VoiceSlider->GetValue();
	S->MusicVolume = MusicSlider->GetValue();
	S->bSubtitles = SubtitlesCheck->IsChecked();
	S->bRayTracing = RayTracingCheck->IsChecked();
	S->bMotionBlur = MotionBlurCheck->IsChecked();
	S->bCinematicDOF = DofCheck->IsChecked();
	S->bCameraShake = ShakeCheck->IsChecked();
	S->CameraShakeIntensity = FMath::Clamp(ShakeSlider->GetValue(), 0.f, 1.5f);
	S->SubtitleScale = SubtitleScaleSlider->GetValue();
	S->bHudColorBlindSafe = HudColorBlindCheck->IsChecked();
	S->bHudReducedMotion = HudReducedMotionCheck->IsChecked();
	S->HudOpacity = HudOpacitySlider->GetValue();
	S->HudScale = HudScaleSlider->GetValue();
	S->ApplyGameplaySettings(GetOwningPlayer());
}

void USettingsWidget::HandleComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRefreshing || !StatusText) { return; }
	StatusText->SetText(LOCTEXT("Pending", "Press APPLY to use the new display settings."));
}

void USettingsWidget::HandleCheckChanged(bool bIsChecked)
{
	if (bRefreshing) { return; }
	ApplyLiveOptions(); // invert Y / subtitles live; VSync waits for APPLY
}

void USettingsWidget::HandleSliderChanged(float Value)
{
	if (bRefreshing) { return; }
	UpdateSliderLabels();
	ApplyLiveOptions();
}

void USettingsWidget::HandleApply()
{
	UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings();
	if (!S) { return; }
	ApplyLiveOptions();

	const int32 ModeIdx = WindowModeCombo->GetSelectedIndex();
	const EWindowMode::Type Mode = ModeIdx == 0 ? EWindowMode::Fullscreen : (ModeIdx == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
	S->SetFullscreenMode(Mode);
	if (Mode != EWindowMode::Windowed) { S->ToggleFullscreenMode = Mode; }

	const int32 ResIdx = ResolutionCombo->GetSelectedIndex();
	if (Mode == EWindowMode::WindowedFullscreen) { S->SetScreenResolution(S->GetDesktopResolution()); }
	else if (Resolutions.IsValidIndex(ResIdx)) { S->SetScreenResolution(Resolutions[ResIdx]); }

	S->SetVSyncEnabled(VSyncCheck->IsChecked());
	const int32 CapIdx = FrameCapCombo->GetSelectedIndex();
	S->SetFrameRateLimit(CapIdx >= 0 && CapIdx < NumFrameCaps ? float(FrameCaps[CapIdx]) : 0.f);
	S->SetOverallScalabilityLevel(FMath::Clamp(QualityCombo->GetSelectedIndex(), 0, 4));

	S->ApplySettings(false);
	S->SaveSettings();
	S->ApplyGameplaySettings(GetOwningPlayer());
	RefreshFromSettings();
	if (StatusText) { StatusText->SetText(LOCTEXT("Applied", "Settings applied and saved.")); }
}

void USettingsWidget::HandleDefaults()
{
	if (UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings())
	{
		// Ultron's perf budget: High (2) is the 60 fps tier on the RTX 2070 SUPER target; Epic is the
		// 30 fps console tier. Textures cost VRAM, not GPU time, so they stay Epic.
		S->SetToDefaults();
		S->SetOverallScalabilityLevel(2);
		S->SetTextureQuality(3);
		S->SetFrameRateLimit(60.f);
		S->ApplySettings(false);
		S->SaveSettings();
		S->ApplyGameplaySettings(GetOwningPlayer());
	}
	RefreshFromSettings();
	if (StatusText) { StatusText->SetText(LOCTEXT("DefaultsDone", "Defaults restored (windowed fullscreen).")); }
}

void USettingsWidget::HandleBack()
{
	if (UArenaGameUserSettings* S = UArenaGameUserSettings::GetArenaSettings()) { S->SaveSettings(); }
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuBack(); }
}

FReply USettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer()))
	{
		if (PC->HandleGlobalKey(InKeyEvent)) { RefreshFromSettings(); return FReply::Handled(); }
	}
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		HandleBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
