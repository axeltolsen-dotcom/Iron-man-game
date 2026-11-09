#include "CareerWidgets.h"

#include "ArenaEnvironment.h"
#include "ArenaAudio.h"
#include "ArenaCombat.h"
#include "SuitSuperComponent.h"
#include "ParkedSuit.h"
#include "SuitUpCinematic.h"
#include "Misc/ConfigCacheIni.h"
#include "ArenaGameMode.h"
#include "ArenaGameUserSettings.h"
#include "ArenaPlayerController.h"
#include "CareerSubsystem.h"
#include "CareerEconomy.h"
#include "CityHouse.h"
#include "CityLife.h"
#include "CityVehicle.h"
#include "DowntownDistrict.h"
#include "EnemyBase.h"
#include "IronManCharacter.h"
#include "JarvisComponent.h"
#include "NpcManifest.h"
#include "PilotAvatar.h"
#include "SuitTypes.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "CareerUI"

namespace
{
	const FLinearColor UiGold(1.f, 0.78f, 0.3f);
	const FLinearColor UiPink(1.f, 0.35f, 0.65f);
	const FLinearColor UiTeal(0.3f, 0.95f, 0.9f);
	const FLinearColor UiDim(0.75f, 0.78f, 0.82f);
	const FLinearColor UiBad(1.f, 0.35f, 0.3f);
	const FLinearColor UiGood(0.5f, 1.f, 0.6f);
	const FLinearColor PanelBg(0.02f, 0.025f, 0.04f, 0.88f);

	FSlateFontInfo Font(int32 Size, bool bBold = true) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); }

	TSharedRef<SWidget> Txt(const FText& T, int32 Size, const FLinearColor& C = FLinearColor::White, bool bBold = true)
	{
		return SNew(STextBlock).Text(T).Font(Font(Size, bBold)).ColorAndOpacity(FSlateColor(C)).ShadowOffset(FVector2D(1.f, 1.f));
	}
	TSharedRef<SWidget> WrapTxt(const FText& T, int32 Size, const FLinearColor& C = UiDim)
	{
		return SNew(STextBlock).Text(T).Font(Font(Size, false)).ColorAndOpacity(FSlateColor(C)).AutoWrapText(true);
	}
	/** World for UI sounds (set when a career screen opens). */
	TWeakObjectPtr<UObject> GUiCtx;
	void UiSfx(FName Event, float Vol = 0.6f) { if (GUiCtx.IsValid()) { ArenaAudio::Play2D(GUiCtx.Get(), Event, Vol); } }

	TSharedRef<SWidget> Btn(const FText& Label, TFunction<void()> OnClick, bool bEnabled = true, const FLinearColor& Tint = FLinearColor(0.12f, 0.14f, 0.2f), int32 Size = 13)
	{
		return SNew(SButton)
			.ButtonColorAndOpacity(Tint)
			.IsEnabled(bEnabled)
			.ContentPadding(FMargin(10.f, 5.f))
			.HAlign(HAlign_Center)
			.OnHovered_Lambda([bEnabled]() { if (bEnabled) { UiSfx(TEXT("ui_hover"), 0.35f); } })
			.OnClicked_Lambda([OnClick]() { UiSfx(TEXT("ui_click")); if (OnClick) { OnClick(); } return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(Font(Size)).ColorAndOpacity(FSlateColor(bEnabled ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f)))
			];
	}
	TSharedRef<SWidget> Swatch(const FLinearColor& C, float W = 22.f, float H = 22.f)
	{
		return SNew(SBox).WidthOverride(W).HeightOverride(H)[SNew(SColorBlock).Color(C)];
	}
	FText Cr(int32 V) { return FText::Format(LOCTEXT("Cr", "{0} CR"), FText::AsNumber(V)); }

	/** One "<  value  >" selector row. */
	TSharedRef<SWidget> Selector(const FText& Label, const FText& Value, TFunction<void(int32)> Step, TSharedPtr<SWidget> Extra = nullptr)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.36f).VAlign(VAlign_Center)[Txt(Label, 12, UiDim)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[Btn(FText::FromString(TEXT("<")), [Step]() { Step(-1); })]
			+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center).HAlign(HAlign_Center)[Txt(Value, 12)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[Btn(FText::FromString(TEXT(">")), [Step]() { Step(1); })];
		if (Extra.IsValid()) { Row->AddSlot().AutoWidth().Padding(6.f, 0.f).VAlign(VAlign_Center)[Extra.ToSharedRef()]; }
		return SNew(SBox).Padding(FMargin(0.f, 2.f))[Row];
	}

	int32 Wrap(int32 V, int32 N) { return N <= 0 ? 0 : ((V % N) + N) % N; }

	AIronManCharacter* Player(const UObject* Ctx) { return Ctx ? Cast<AIronManCharacter>(UGameplayStatics::GetPlayerPawn(Ctx, 0)) : nullptr; }

	TArray<const FNpcVoiceDef*> PlayerVoices()
	{
		TArray<const FNpcVoiceDef*> Out;
		for (const FNpcVoiceDef& V : NpcManifest::Voices()) { if (V.Id != FName(TEXT("police_pa"))) { Out.Add(&V); } }
		return Out;
	}

	const TCHAR* SlotSymbols[] = { TEXT("CHERRY"), TEXT("BELL"), TEXT("BAR"), TEXT("STAR"), TEXT("7"), TEXT("DIAMOND") };
	const int32 SlotWeights[] = { 30, 20, 15, 10, 5, 3 };
	const int32 SlotPays[] = { 6, 12, 20, 40, 100, 250 };
	const float WheelMul[20] = { 0, 1.2f, 0, 2, 0, 1.2f, 0, 3, 0, 1.2f, 0, 2, 0, 1.2f, 0, 4, 0, 1.2f, 0, 2 }; // RTP 0.95
	bool IsRed(int32 N)
	{
		static const int32 Reds[] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };
		for (const int32 R : Reds) { if (R == N) { return true; } }
		return false;
	}
}

// ============================================================================================ preview stage

ACareerPreviewStage::ACareerPreviewStage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(GetRootComponent());
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = true;
	// The avatar / barber / clothing screens pause the game: the capture must keep rendering (it was an empty panel).
	Capture->PrimaryComponentTick.bTickEvenWhenPaused = true;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->FOVAngle = 30.f;
	Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Capture->PostProcessSettings.AutoExposureMinBrightness = 2.f;
	Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Capture->PostProcessSettings.AutoExposureMaxBrightness = 8.f;
	Capture->PostProcessSettings.bOverride_BloomIntensity = true;
	Capture->PostProcessSettings.BloomIntensity = 0.3f;
	Capture->PostProcessSettings.bOverride_VignetteIntensity = true;
	Capture->PostProcessSettings.VignetteIntensity = 0.4f;
	Key = CreateDefaultSubobject<UPointLightComponent>(TEXT("Key"));
	Key->SetupAttachment(GetRootComponent());
	Key->SetRelativeLocation(FVector(220.f, -160.f, 230.f));
	Key->SetIntensityUnits(ELightUnits::Lumens);
	Key->SetIntensity(9000.f);
	Key->SetAttenuationRadius(900.f);
	Key->SetUseTemperature(true);
	Key->SetTemperature(4800.f);
	Rim = CreateDefaultSubobject<UPointLightComponent>(TEXT("Rim"));
	Rim->SetupAttachment(GetRootComponent());
	Rim->SetRelativeLocation(FVector(-200.f, 180.f, 220.f));
	Rim->SetIntensityUnits(ELightUnits::Lumens);
	Rim->SetIntensity(6000.f);
	Rim->SetAttenuationRadius(900.f);
	Rim->SetLightColor(FLinearColor(0.4f, 0.8f, 1.f));
}

void ACareerPreviewStage::ShowAvatar(const FAvatarConfig& Config)
{
	if (!Target)
	{
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
		Target->ClearColor = FLinearColor(0.02f, 0.025f, 0.04f, 1.f);
		Target->InitAutoFormat(480, 640);
		Capture->TextureTarget = Target;
		TArray<FArenaPaintedPart> Dummy;
		Floor = ArenaVisuals::AddRuntimePart(this, GetRootComponent(), EArenaShape::Cylinder, FVector(0, 0, -4.f), FVector(2.2f, 2.2f, 0.08f), FRotator::ZeroRotator,
			FLinearColor(0.1f, 0.11f, 0.13f), 0.f, EArenaSurface::Steel, false, Dummy);
		if (Floor) { Capture->ShowOnlyComponents.Add(Floor); }
	}
	if (!Avatar)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Avatar = GetWorld()->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), GetActorTransform(), SP);
		if (Avatar) { Avatar->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform); Avatar->SetTickableWhenPaused(true); }
	}
	if (Avatar)
	{
		Avatar->ApplyConfig(Config, true);
		UE_LOG(LogTemp, Log, TEXT("Career: avatar preview rebuilt (%s, skin %d, hair %d, face %d)"), Config.Body == EAvatarBody::Feminine ? TEXT("girl") : TEXT("boy"), Config.SkinTone, int32(Config.Hair), Config.Face);
		Capture->ShowOnlyActors.Reset();
		Capture->ShowOnlyActors.Add(Avatar);
	}
	SetFraming(Framing);
}

void ACareerPreviewStage::SetFraming(int32 Mode)
{
	Framing = Mode;
	const float H = Avatar ? Avatar->GetStandingHeight() : 170.f;
	const FVector Look = Mode == 1 ? FVector(0, 0, H * 0.9f) : FVector(0, 0, H * 0.52f);
	const float Dist = Mode == 1 ? 120.f : 520.f;
	const FVector Cam = FVector(Dist, 0.f, Look.Z + (Mode == 1 ? 4.f : 25.f));
	Capture->SetRelativeLocation(Cam);
	Capture->SetRelativeRotation((Look - Cam).Rotation());
}

void ACareerPreviewStage::Spin(float Degrees) { Yaw += Degrees; }

void ACareerPreviewStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Avatar) { Avatar->SetActorRelativeRotation(FRotator(0.f, Yaw, 0.f)); }
	// Paused world: deferred every-frame captures don't run, so capture explicitly.
	if (Capture && Capture->TextureTarget && GetWorld() && GetWorld()->IsPaused()) { Capture->CaptureScene(); }
}

void ACareerPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Avatar) { Avatar->Destroy(); Avatar = nullptr; }
	Super::EndPlay(EndPlayReason);
}

// ============================================================================================ screen

TSharedRef<SWidget> UCareerScreen::RebuildWidget()
{
	return SAssignNew(Body, SBox);
}

void UCareerScreen::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Body.Reset();
}

void UCareerScreen::NativeDestruct()
{
	if (Stage) { Stage->Destroy(); Stage = nullptr; }
	Super::NativeDestruct();
}

void UCareerScreen::Open(ECareerScreen InScreen, int32 InParam)
{
	Screen = InScreen;
	Param = InParam;
	GUiCtx = GetWorld();
	UiSfx(TEXT("ui_open"), 0.5f);
	if (const UCareerSubsystem* C = UCareerSubsystem::Get(this)) { Draft = C->GetAvatar(); }
	const TArray<const FNpcVoiceDef*> Voices = PlayerVoices();
	VoiceIndex = 0;
	for (int32 i = 0; i < Voices.Num(); ++i) { if (Voices[i]->Id == Draft.VoiceId) { VoiceIndex = i; } }
	if (InScreen == ECareerScreen::FirstLaunch && Draft.VoiceId.IsNone() && Voices.Num()) { Draft.VoiceId = Voices[0]->Id; }
	ResultText = FText::GetEmpty();
	bHandActive = false;
	bPendingSettle = false;
	SpinTime = 0.f;
	if (Reels.Num() != 3) { Reels = { 0, 1, 2 }; }
	CasinoRng.GenerateNewSeed();
	if (InScreen == ECareerScreen::Avatar || InScreen == ECareerScreen::FirstLaunch || InScreen == ECareerScreen::Clothing) { EnsurePreview(0); }
	else if (InScreen == ECareerScreen::Barber) { EnsurePreview(1); }
	Refresh();
}

void UCareerScreen::EnsurePreview(int32 Framing)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (!Stage)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Stage = World->SpawnActor<ACareerPreviewStage>(ACareerPreviewStage::StaticClass(), FTransform(FVector(0.f, 0.f, 150000.f)), SP);
		if (Stage) { Stage->SetTickableWhenPaused(true); }
	}
	if (Stage)
	{
		Stage->ShowAvatar(Draft);
		Stage->SetFraming(Framing);
		PreviewBrush = FSlateBrush();
		PreviewBrush.SetResourceObject(Stage->GetTarget());
		PreviewBrush.ImageSize = FVector2D(480.f, 640.f);
	}
}

void UCareerScreen::PushPreview()
{
	if (Stage) { Stage->ShowAvatar(Draft); }
}

void UCareerScreen::CloseScreen()
{
	if (InputOwner.IsValid()) { InputOwner->CloseScreen(); }
}

void UCareerScreen::Refresh()
{
	if (Body.IsValid()) { Body->SetContent(BuildPage()); }
}

FReply UCareerScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (Screen == ECareerScreen::FirstLaunch) { return FReply::Unhandled(); } // must finish setup
	const bool bToggleKey = (K == EKeys::Escape)
		|| (Screen == ECareerScreen::DevBar && (K == EKeys::F1 || K == EKeys::Tilde))
		|| (Screen == ECareerScreen::Shop && K == EKeys::U)
		|| (Screen == ECareerScreen::Phone && K == EKeys::N)
		|| (Screen == ECareerScreen::Avatar && K == EKeys::K);
	if (bToggleKey) { CloseScreen(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UCareerScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SmoothedDt = FMath::Lerp(SmoothedDt, FMath::Max(0.001f, InDeltaTime), 0.05f);
	if (Stage && (Screen == ECareerScreen::Avatar || Screen == ECareerScreen::FirstLaunch || Screen == ECareerScreen::Clothing)) { Stage->Spin(InDeltaTime * 18.f); }
	if (bPendingSettle && Screen == ECareerScreen::Casino)
	{
		SpinTime -= InDeltaTime;
		const ECasinoGame G = ECasinoGame(Param);
		if (SpinTime > 0.f)
		{
			if (G == ECasinoGame::Slots) { for (int32 i = 0; i < 3; ++i) { if (SpinTime > 0.25f * i) { Reels[i] = CasinoRng.RandRange(0, 5); } } }
			else if (G == ECasinoGame::Wheel) { WheelAngle += InDeltaTime * 900.f * FMath::Max(0.1f, SpinTime); }
			else if (G == ECasinoGame::Roulette) { RouletteNumber = CasinoRng.RandRange(0, 36); }
			else if (G == ECasinoGame::CoinFlip) { RouletteNumber = CasinoRng.RandRange(0, 1); }
			else if (G == ECasinoGame::HighLow) { RouletteNumber = CasinoRng.RandRange(1, 13); }
			Refresh();
		}
		else { CasinoPlay(); }
	}
	if (Screen == ECareerScreen::DevBar)
	{
		DevRefreshAcc += InDeltaTime;
		if (DevRefreshAcc > 0.5f) { DevRefreshAcc = 0.f; Refresh(); }
	}
}

TSharedRef<SWidget> UCareerScreen::Frame(const FText& Title, TSharedRef<SWidget> Content, float Width)
{
	const UCareerSubsystem* C = UCareerSubsystem::Get(this);
	TWeakObjectPtr<UCareerScreen> Self(this);
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(Width).MaxDesiredHeight(860.f)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelBg).Padding(18.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[Txt(Title, 24, UiGold)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(C ? Cr(C->GetCredits()) : FText::GetEmpty(), 18, UiGold)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f, 0.f, 0.f)
					[
						Screen == ECareerScreen::FirstLaunch ? SNullWidget::NullWidget : Btn(LOCTEXT("Close", "CLOSE  [Esc]"), [Self]() { if (Self.IsValid()) { Self->CloseScreen(); } })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 10.f)[SNew(SBox).HeightOverride(2.f)[SNew(SColorBlock).Color(UiPink)]]
				+ SVerticalBox::Slot().FillHeight(1.f)[Content]
			]
		]
	];
}

TSharedRef<SWidget> UCareerScreen::BuildPage()
{
	switch (Screen)
	{
	case ECareerScreen::DevBar: return BuildDevBar();
	case ECareerScreen::Shop: return BuildShop();
	case ECareerScreen::Avatar: return BuildAvatar(false);
	case ECareerScreen::FirstLaunch: return BuildAvatar(true);
	case ECareerScreen::Barber: return BuildBarber();
	case ECareerScreen::Clothing: return BuildClothing();
	case ECareerScreen::Phone: return BuildPhone();
	case ECareerScreen::Casino: return BuildCasino();
	default: return SNullWidget::NullWidget;
	}
}

// -------------------------------------------------------------------------------------------- dev bar

TSharedRef<SWidget> UCareerScreen::BuildDevBar()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
	ACityLife* Life = ACityLife::Find(this);
	TWeakObjectPtr<UCareerScreen> Self(this);
	auto Spawn = [GM, Self](int32 What)
	{
		AIronManCharacter* P = Self.IsValid() ? Player(Self.Get()) : nullptr;
		if (!GM || !P) { return; }
		TSubclassOf<AEnemyBase> Cls = What == 0 ? GM->DroneClass : What == 1 ? GM->TankClass : GM->BossClass;
		if (Cls) { GM->SpawnEnemyAt(Cls, P->GetActorLocation() + P->GetActorForwardVector() * 2500.f + FVector(0, 0, What == 1 ? 0.f : 600.f)); }
	};
	TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	auto Add = [&Row](TSharedRef<SWidget> W) { Row->AddSlot()[W]; };
	const FLinearColor Base(0.12f, 0.14f, 0.2f);
	Add(Txt(FText::Format(LOCTEXT("Fps", "DEV  {0} FPS"), FText::AsNumber(FMath::RoundToInt(1.f / SmoothedDt))), 14, UiTeal));
	Add(Btn(C->IsGodMode() ? LOCTEXT("GodOn", "GOD: ON") : LOCTEXT("GodOff", "GOD: OFF"), [C, Self]() { C->SetGodMode(!C->IsGodMode()); if (Self.IsValid()) { Self->Refresh(); } }, true, C->IsGodMode() ? FLinearColor(0.1f, 0.4f, 0.15f) : Base));
	Add(Btn(LOCTEXT("Cr1k", "+1,000 CR"), [C]() { C->AddCredits(1000, LOCTEXT("DevCredits", "dev")); }));
	Add(Btn(LOCTEXT("Cr100k", "+100,000 CR"), [C]() { C->AddCredits(100000, LOCTEXT("DevCredits", "dev")); }));
	Add(Btn(C->IsInfiniteMoney() ? LOCTEXT("InfMoneyOn", "INFINITE MONEY: ON") : LOCTEXT("InfMoneyOff", "INFINITE MONEY: OFF"), [C, Self]() { C->DevSetInfiniteMoney(!C->IsInfiniteMoney()); if (Self.IsValid()) { Self->Refresh(); } }, true,
		C->IsInfiniteMoney() ? FLinearColor(0.35f, 0.25f, 0.05f) : Base));
	Add(Btn(LOCTEXT("MaxCr", "SET 999,999,999 CR"), [C]() { C->DevSetCredits(UCareerSubsystem::InfiniteCredits); }));
	Add(Btn(LOCTEXT("AllSuitsDev", "GIVE ALL SUITS"), [C]() { C->DevGiveAllSuits(); }));
	Add(Btn(LOCTEXT("AllPropsDev", "UNLOCK HOUSES + VEHICLES + CLOTHES"), [C]() { C->DevUnlockAllProperty(); }));
	Add(Btn(LOCTEXT("Drone", "SPAWN DRONE"), [Spawn]() { Spawn(0); }));
	Add(Btn(LOCTEXT("Tank", "SPAWN TANK"), [Spawn]() { Spawn(1); }));
	Add(Btn(LOCTEXT("Boss", "SPAWN BOSS"), [Spawn]() { Spawn(2); }));
	Add(Btn(LOCTEXT("TpCity", "TP CITY"), [C]() { C->TravelToCity(); }));
	Add(Btn(LOCTEXT("TpArena", "TP ARENA"), [C]() { C->TravelToArena(); }));
	Add(Btn(LOCTEXT("TpHome", "TP HOME"), [C]() { C->TravelToHouse(C->GetHomeHouse()); }, !C->GetHomeHouse().IsNone()));
	for (const float H : { 6.5f, 9.f, 13.f, 17.5f, 19.f, 22.f, 1.f })
	{
		const int32 Hh = int32(H), Mm = int32((H - Hh) * 60.f);
		Add(Btn(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Hh, Mm)), [C, H]() { C->SetTimeOfDay(H); }, true,
			FMath::Abs(C->GetTimeOfDay() - H) < 0.1f ? FLinearColor(0.35f, 0.25f, 0.05f) : Base));
	}
	Add(Btn(C->IsPhotoreal() ? LOCTEXT("PhotoOn", "PHOTOREAL: ON") : LOCTEXT("PhotoOff", "PHOTOREAL: OFF"), [C, Self]() { C->SetPhotoreal(!C->IsPhotoreal()); if (Self.IsValid()) { Self->Refresh(); } }));
	Add(Btn(FText::Format(LOCTEXT("Scal", "GFX: {0}"), UCareerSubsystem::ScalabilityName(C->GetScalabilityPreset())), [C, Self]() { C->ApplyScalabilityPreset((C->GetScalabilityPreset() + 1) % 5); if (Self.IsValid()) { Self->Refresh(); } }));
	if (Life)
	{
		Add(Txt(FText::Format(LOCTEXT("WantedDev", "WANTED {0}"), FText::AsNumber(Life->GetStars())), 13, UiBad));
		Add(Btn(LOCTEXT("ClearW", "CLEAR WANTED"), [Life]() { Life->ClearWanted(); }));
		for (int32 s = 1; s <= 5; ++s) { Add(Btn(FText::AsNumber(s), [Life, s]() { Life->SetWanted(s); })); }
		Add(Btn(Life->bLifeEnabled ? LOCTEXT("LifeOn", "CITY LIFE: ON") : LOCTEXT("LifeOff", "CITY LIFE: OFF"), [Life, Self]() { Life->bLifeEnabled = !Life->bLifeEnabled; if (Self.IsValid()) { Self->Refresh(); } }));
	}
	UCareerInput* In = InputOwner.Get();
	Add(Btn(LOCTEXT("EditChar", "EDIT CHARACTER"), [In]() { if (In) { In->OpenScreen(ECareerScreen::Avatar); } }));
	Add(Btn(LOCTEXT("ShopDev", "SHOP"), [In]() { if (In) { In->OpenScreen(ECareerScreen::Shop); } }));
	Add(Btn(LOCTEXT("PhoneDev", "PHONE"), [In]() { if (In) { In->OpenScreen(ECareerScreen::Phone); } }));
	if (In && In->GetHUD())
	{
		UCareerHUD* Hud = In->GetHUD();
		Add(Btn(Hud->bShowFps ? LOCTEXT("FpsOn", "FPS HUD: ON") : LOCTEXT("FpsOff", "FPS HUD: OFF"), [Hud, Self]() { Hud->bShowFps = !Hud->bShowFps; if (Self.IsValid()) { Self->Refresh(); } }));
	}
	Add(Btn(LOCTEXT("CloseDev", "CLOSE [`]"), [Self]() { if (Self.IsValid()) { Self->CloseScreen(); } }, true, FLinearColor(0.3f, 0.08f, 0.1f)));
	return SNew(SBox).VAlign(VAlign_Top).HAlign(HAlign_Fill)
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelBg).Padding(8.f)[Row]
	];
}

// -------------------------------------------------------------------------------------------- shop

TSharedRef<SWidget> UCareerScreen::BuildShop()
{
	TWeakObjectPtr<UCareerScreen> Self(this);
	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	const FText Names[] = { LOCTEXT("TabUp", "SUIT UPGRADES"), LOCTEXT("TabSuits", "SUITS"), LOCTEXT("TabHouses", "PROPERTIES"), LOCTEXT("TabCars", "VEHICLES") };
	for (int32 t = 0; t < 4; ++t)
	{
		Tabs->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
		[
			Btn(Names[t], [Self, t]() { if (Self.IsValid()) { Self->ShopTab = t; Self->Refresh(); } }, true, ShopTab == t ? FLinearColor(0.45f, 0.12f, 0.3f) : FLinearColor(0.12f, 0.14f, 0.2f), 14)
		];
	}
	TSharedRef<SWidget> Page = ShopTab == 0 ? BuildShopUpgrades() : ShopTab == 1 ? BuildShopSuits() : ShopTab == 2 ? BuildShopHouses() : BuildShopVehicles();
	return Frame(LOCTEXT("ShopTitle", "STARK INDUSTRIES  //  SHOP"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[Tabs]
		+ SVerticalBox::Slot().FillHeight(1.f)[Page]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[WrapTxt(LOCTEXT("EarnHint", "Earn credits from kills, wave clears, story chapters and side missions. Property income is paid every minute."), 11)]);
}

TSharedRef<SWidget> UCareerScreen::BuildShopUpgrades()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	if (!C) { return List; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	for (int32 i = 0; i < int32(ECareerUpgrade::Count); ++i)
	{
		const ECareerUpgrade U = ECareerUpgrade(i);
		const int32 L = C->GetUpgradeLevel(U);
		const int32 Cost = Career::UpgradeCost(U, L);
		TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
		for (int32 p = 0; p < Career::MaxUpgradeLevel; ++p) { Pips->AddSlot().AutoWidth().Padding(2.f)[Swatch(p < L ? UiGold : FLinearColor(0.15f, 0.15f, 0.18f), 26.f, 10.f)]; }
		List->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)[Txt(Career::UpgradeName(U), 16)]
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[WrapTxt(Career::UpgradeDescription(U), 11)]
				+ SVerticalBox::Slot().AutoHeight()[Txt(FText::Format(LOCTEXT("UpMul", "x{0}"), FText::AsNumber(Career::UpgradeMultiplier(U, L))), 11, UiTeal)]]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f)[Pips]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				L >= Career::MaxUpgradeLevel ? Txt(LOCTEXT("Maxed", "MAXED"), 14, UiGood)
				: Btn(FText::Format(LOCTEXT("BuyUp", "UPGRADE  {0}"), Cr(Cost)), [C, U, Self]() { C->BuyUpgrade(U); if (Self.IsValid()) { Self->Refresh(); } }, C->CanAfford(Cost), FLinearColor(0.35f, 0.25f, 0.05f))
			]
		];
	}
	return SNew(SScrollBox) + SScrollBox::Slot()[List];
}

TSharedRef<SWidget> UCareerScreen::BuildShopSuits()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	const TArray<FShopSuitDef>& Suits = Career::ShopSuits();
	TArray<int32> Visible;
	for (int32 i = 0; i < Suits.Num(); ++i)
	{
		if (USuitLibrary::FindSuitIndex(Suits[i].SuitId) == INDEX_NONE) { continue; }
		if (bGirlSuitsOnly && !Suits[i].bFeminine) { continue; }
		Visible.Add(i);
	}
	if (!Visible.Contains(SelectedSuit) && Visible.Num()) { SelectedSuit = Visible[0]; }
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		Btn(bGirlSuitsOnly ? LOCTEXT("AllSuits", "SHOW ALL SUITS") : LOCTEXT("GirlSuits", "SHOW GIRL SUITS"), [Self]() { if (Self.IsValid()) { Self->bGirlSuitsOnly = !Self->bGirlSuitsOnly; Self->Refresh(); } })
	];
	int32 LastTier = -1;
	for (const int32 i : Visible)
	{
		const FShopSuitDef& D = Suits[i];
		const FSuitDef& S = USuitLibrary::GetSuit(USuitLibrary::FindSuitIndex(D.SuitId));
		if (D.Tier != LastTier) { LastTier = D.Tier; List->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[Txt(FText::Format(LOCTEXT("Tier", "TIER {0}"), FText::AsNumber(D.Tier)), 12, UiPink)]; }
		const bool bOwned = C->IsSuitOwned(D.SuitId);
		List->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SButton).ButtonColorAndOpacity(SelectedSuit == i ? FLinearColor(0.3f, 0.1f, 0.25f) : FLinearColor(0.08f, 0.09f, 0.13f))
			.OnClicked_Lambda([Self, i]() { if (Self.IsValid()) { Self->SelectedSuit = i; Self->Refresh(); } return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[Swatch(S.PrimaryColor, 14.f, 14.f)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[Swatch(S.SecondaryColor, 14.f, 14.f)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f, 0.f)[Txt(FText::Format(LOCTEXT("SuitRow", "{0}{1}"), S.Name, D.bFeminine ? LOCTEXT("Fem", "  (girl)") : FText::GetEmpty()), 12)]
				+ SHorizontalBox::Slot().AutoWidth()[Txt(bOwned ? LOCTEXT("Owned", "OWNED") : C->IsSuitLockedByBoss(D.SuitId) ? LOCTEXT("BossLocked", "BEAT BOSS") : Cr(D.Price), 12, bOwned ? UiGood : UiGold)]
			]
		];
	}
	TSharedRef<SWidget> Card = SNullWidget::NullWidget;
	if (Suits.IsValidIndex(SelectedSuit) && USuitLibrary::FindSuitIndex(Suits[SelectedSuit].SuitId) != INDEX_NONE)
	{
		const FShopSuitDef& D = Suits[SelectedSuit];
		const int32 Idx = USuitLibrary::FindSuitIndex(D.SuitId);
		const FSuitDef& S = USuitLibrary::GetSuit(Idx);
		const bool bOwned = C->IsSuitOwned(D.SuitId);
		const AIronManCharacter* Me = Player(this);
		const bool bWorn = Me && Me->GetSuitIndex() == Idx;
		const FName Id = D.SuitId;
		auto Bar = [](const FText& L, float V) -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.35f)[Txt(L, 11, UiDim)]
				+ SHorizontalBox::Slot().FillWidth(0.65f).VAlign(VAlign_Center).HAlign(HAlign_Left)
				[
					SNew(SBox).HeightOverride(8.f).WidthOverride(260.f * FMath::Clamp(V / 2.f, 0.05f, 1.f))[SNew(SColorBlock).Color(UiTeal)]
				];
		};
		// Silhouette preview: helmet, torso + reactor, arms, legs in the suit colours (slimmer for girl suits).
		const float W = D.bFeminine ? 0.82f : 1.f;
		TSharedRef<SWidget> Sil = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Swatch(S.PrimaryColor, 46.f * W, 52.f)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[Swatch(S.SecondaryColor, 22.f * W, 92.f)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[Swatch(S.PrimaryColor, 86.f * W, 96.f)]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 18.f)[Swatch(S.EmissiveColor, 16.f, 16.f)]
				]
				+ SHorizontalBox::Slot().AutoWidth()[Swatch(S.SecondaryColor, 22.f * W, 92.f)]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[Swatch(S.PrimaryColor, 30.f * W, 100.f)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[Swatch(S.PrimaryColor, 30.f * W, 100.f)]
			];
		TSharedRef<SWidget> Action = bWorn ? Txt(LOCTEXT("Wearing", "WEARING"), 14, UiGood)
			: bOwned ? Btn(LOCTEXT("Equip", "EQUIP"), [C, Id, Self]() { if (Self.IsValid()) { Self->CloseScreen(); } C->EquipSuit(Id); }, true, FLinearColor(0.1f, 0.35f, 0.15f), 15)
			: Btn(FText::Format(LOCTEXT("BuySuit", "BUY  {0}"), Cr(D.Price)), [C, Id, Self]() { C->BuySuit(Id); if (Self.IsValid()) { Self->Refresh(); } },
				C->CanAfford(D.Price) && !C->IsSuitLockedByBoss(Id), FLinearColor(0.35f, 0.25f, 0.05f), 15);
		Card = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Txt(S.Name, 22, UiGold)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 8.f)[WrapTxt(FText::FromString(D.Perk), 13, UiTeal)]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f)[Sil]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[Bar(LOCTEXT("StArmor", "Armor"), S.Armor * D.ArmorMul)]
			+ SVerticalBox::Slot().AutoHeight()[Bar(LOCTEXT("StDmg", "Firepower"), S.WeaponDamage * D.DamageMul)]
			+ SVerticalBox::Slot().AutoHeight()[Bar(LOCTEXT("StSpeed", "Flight"), S.Speed * D.FlightMul)]
			+ SVerticalBox::Slot().AutoHeight()[Bar(LOCTEXT("StEnergy", "Energy"), S.MaxEnergy / 100.f)]
			+ SVerticalBox::Slot().AutoHeight()[Bar(LOCTEXT("StHeat", "Heat cap"), D.HeatMul)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f).HAlign(HAlign_Left)[Action];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.48f)[SNew(SScrollBox) + SScrollBox::Slot()[List]]
		+ SHorizontalBox::Slot().FillWidth(0.52f).Padding(16.f, 0.f, 0.f, 0.f)[Card];
}

TSharedRef<SWidget> UCareerScreen::BuildShopHouses()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	if (!C) { return List; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	for (const FCityHouseDef& H : Career::Houses())
	{
		const FName Id = H.Id;
		const bool bOwned = C->OwnsHouse(Id);
		const bool bHome = C->GetHomeHouse() == Id;
		TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
		if (bOwned)
		{
			Actions->AddSlot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("Travel", "FAST TRAVEL"), [C, Id, Self]() { if (Self.IsValid()) { Self->CloseScreen(); } C->TravelToHouse(Id); })];
			Actions->AddSlot().AutoWidth().Padding(3.f)[bHome ? Txt(LOCTEXT("Home", "HOME"), 13, UiGood) : Btn(LOCTEXT("SetHome", "SET AS HOME"), [C, Id, Self]() { C->SetHomeHouse(Id); if (Self.IsValid()) { Self->Refresh(); } })];
		}
		else
		{
			Actions->AddSlot().AutoWidth().Padding(3.f)[Btn(FText::Format(LOCTEXT("BuyHouse", "BUY  {0}"), Cr(H.Price)), [C, Id, Self]() { C->BuyHouse(Id); if (Self.IsValid()) { Self->Refresh(); } }, C->CanAfford(H.Price), FLinearColor(0.35f, 0.25f, 0.05f))];
		}
		FString Extra;
		if (H.IncomePerMinute > 0) { Extra += FString::Printf(TEXT("+%d CR / min   "), H.IncomePerMinute); }
		if (H.bArmory) { Extra += TEXT("Suit armory (repair + recharge)   "); }
		Extra += TEXT("Spawn + fast-travel point, vehicle garage");
		List->AddSlot().AutoHeight().Padding(0.f, 8.f)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.06f, 0.07f, 0.1f, 1.f)).Padding(10.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Txt(FText::FromString(H.Name), 17, bOwned ? UiGood : FLinearColor::White)]
					+ SVerticalBox::Slot().AutoHeight()[WrapTxt(FText::FromString(H.Blurb), 11)]
					+ SVerticalBox::Slot().AutoHeight()[Txt(FText::FromString(Extra), 11, UiTeal, false)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Actions]
			]
		];
	}
	List->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("HouseHint", "Houses are marked on the HUD (M toggles markers). You can also buy a house by walking up to it and pressing E."), 11)];
	return SNew(SScrollBox) + SScrollBox::Slot()[List];
}

TSharedRef<SWidget> UCareerScreen::BuildShopVehicles()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	if (!C) { return List; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	TWeakObjectPtr<UCareerInput> In = InputOwner;
	if (C->GetHousesOwned().Num() == 0) { List->AddSlot().AutoHeight()[Txt(LOCTEXT("NeedHouse", "Buy a property first - vehicles live in your garage."), 13, UiBad)]; }
	const TCHAR* KindNames[] = { TEXT("CAR"), TEXT("MOTORBIKE"), TEXT("PLANE"), TEXT("HELICOPTER") };
	for (const FVehicleDef& V : Career::Vehicles())
	{
		const FName Id = V.Id;
		const bool bOwned = C->OwnsVehicle(Id);
		List->AddSlot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)[Swatch(V.Paint, 34.f, 18.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Txt(FText::FromString(FString::Printf(TEXT("%s   (%s, %d km/h)"), V.Name, KindNames[int32(V.Kind)], int32(V.TopSpeedKmh))), 14)]
				+ SVerticalBox::Slot().AutoHeight()[WrapTxt(FText::FromString(V.Blurb), 11)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				bOwned ? Btn(LOCTEXT("Deliver", "DELIVER"), [In, Id, Self]() { if (Self.IsValid()) { Self->CloseScreen(); } if (In.IsValid()) { In->DeliverVehicle(Id); } }, true, FLinearColor(0.1f, 0.35f, 0.15f))
				: Btn(FText::Format(LOCTEXT("BuyCar", "BUY  {0}"), Cr(V.Price)), [C, Id, Self]() { C->BuyVehicle(Id); if (Self.IsValid()) { Self->Refresh(); } }, C->CanAfford(V.Price), FLinearColor(0.35f, 0.25f, 0.05f))
			]
		];
	}
	return SNew(SScrollBox) + SScrollBox::Slot()[List];
}

// -------------------------------------------------------------------------------------------- avatar

TSharedRef<SWidget> UCareerScreen::BuildPreview(float Width, float Height)
{
	TWeakObjectPtr<UCareerScreen> Self(this);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).WidthOverride(Width).HeightOverride(Height)[SNew(SImage).Image(&PreviewBrush)]]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[Btn(LOCTEXT("RotL", "< ROTATE"), [Self]() { if (Self.IsValid() && Self->Stage) { Self->Stage->Spin(-30.f); } })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[Btn(LOCTEXT("RotR", "ROTATE >"), [Self]() { if (Self.IsValid() && Self->Stage) { Self->Stage->Spin(30.f); } })]
		];
}

TSharedRef<SWidget> UCareerScreen::BuildAvatar(bool bFirstLaunch)
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	const FAvatarConfig& A = Draft;
	const bool bFem = A.Body == EAvatarBody::Feminine;
	auto Change = [Self](TFunction<void(FAvatarConfig&)> Fn, bool bRepreview)
	{
		if (!Self.IsValid()) { return; }
		Fn(Self->Draft);
		if (bRepreview) { Self->PushPreview(); }
		Self->Refresh();
	};
	auto StyleStep = [C](int32 Kind, int32 Cur, int32 Dir, bool bOwnedOnly)
	{
		const int32 N = Career::ClothingStyleCount(Kind);
		int32 V = Cur;
		for (int32 t = 0; t < N; ++t)
		{
			V = Wrap(V + Dir, N);
			if (!bOwnedOnly || C->OwnsClothing(Kind, V)) { return V; }
		}
		return Cur;
	};
	const int32 NSkin = Career::SkinTones().Num(), NHair = Career::HairColors().Num(), NCol = Career::OutfitColors().Num(), NFace = Career::FaceNames().Num();
	const bool bOwnedOnly = !bFirstLaunch;
	TSharedRef<SVerticalBox> Opts = SNew(SVerticalBox);
	auto Row = [&Opts](TSharedRef<SWidget> W) { Opts->AddSlot().AutoHeight()[W]; };
	auto Head = [&Opts](const FText& T) { Opts->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 2.f)[Txt(T, 13, UiPink)]; };
	auto ColorOf = [](const TArray<FLinearColor>& L, int32 I) { return L.IsValidIndex(I) ? L[I] : FLinearColor::Gray; };

	Head(LOCTEXT("HPilot", "PILOT"));
	Row(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.36f).VAlign(VAlign_Center)[Txt(LOCTEXT("Name", "Name"), 12, UiDim)]
		+ SHorizontalBox::Slot().FillWidth(0.64f)
		[
			SNew(SEditableTextBox).Text(FText::FromString(A.PilotName)).Font(Font(13)).HintText(LOCTEXT("NameHint", "Your name"))
			.OnTextChanged_Lambda([Self](const FText& T) { if (Self.IsValid()) { Self->Draft.PilotName = T.ToString().Left(20); } })
		]);
	Row(Selector(LOCTEXT("Body", "Boy / Girl"), bFem ? LOCTEXT("Girl", "GIRL") : LOCTEXT("Boy", "BOY"), [Change](int32) { Change([](FAvatarConfig& X)
	{
		X.Body = X.Body == EAvatarBody::Feminine ? EAvatarBody::Masculine : EAvatarBody::Feminine;
		X.BodyModel = -1;
		if (X.Body == EAvatarBody::Feminine) { X.FacialHair = EAvatarFacialHair::None; if (X.Hair == EAvatarHair::Short || X.Hair == EAvatarHair::Buzz) { X.Hair = EAvatarHair::Long; } X.bSlimArmor = true; }
		else { X.bSlimArmor = false; if (X.Hair == EAvatarHair::Long || X.Hair == EAvatarHair::Bun) { X.Hair = EAvatarHair::Short; } }
	}, true); }));
	// Real character body (MetaHuman / Character Creator: Kevin / CityKit people) - the model the preview and the pawn use.
	{
		const TArray<int32> Bodies = AvatarCatalog::ForBody(bFem);
		const int32 Cur = AvatarCatalog::Resolve(A);
		if (Bodies.Num() > 0 && AvatarCatalog::All().IsValidIndex(Cur))
		{
			const FAvatarBodyEntry& E = AvatarCatalog::All()[Cur];
			const int32 NB = Bodies.Num();
			Row(Selector(LOCTEXT("BodyModel", "Character"), FText::FromString(FString::Printf(TEXT("%s  (%d/%d)"), *E.Label, Bodies.IndexOfByKey(Cur) + 1, NB)), [Change, NB, Bodies, Cur](int32 D)
			{
				const int32 Pos = FMath::Max(0, Bodies.IndexOfByKey(Cur));
				Change([D, NB, Pos](FAvatarConfig& X) { X.BodyModel = Wrap(Pos + D, NB); }, true);
			}));
		}
	}
	Row(Selector(LOCTEXT("Skin", "Skin tone"), FText::AsNumber(A.SkinTone + 1), [Change, NSkin](int32 D) { Change([D, NSkin](FAvatarConfig& X) { X.SkinTone = Wrap(X.SkinTone + D, NSkin); }, true); }, Swatch(ColorOf(Career::SkinTones(), A.SkinTone))));
	Row(Selector(LOCTEXT("Face", "Face"), Career::FaceNames().IsValidIndex(A.Face) ? Career::FaceNames()[A.Face] : FText::GetEmpty(), [Change, NFace](int32 D) { Change([D, NFace](FAvatarConfig& X) { X.Face = Wrap(X.Face + D, NFace); }, true); }));
	Row(Selector(LOCTEXT("Hair", "Hair style"), Career::HairName(A.Hair), [Change](int32 D) { Change([D](FAvatarConfig& X) { X.Hair = EAvatarHair(Wrap(int32(X.Hair) + D, int32(EAvatarHair::Count))); }, true); }));
	Row(Selector(LOCTEXT("HairC", "Hair colour"), FText::AsNumber(A.HairColor + 1), [Change, NHair](int32 D) { Change([D, NHair](FAvatarConfig& X) { X.HairColor = Wrap(X.HairColor + D, NHair); }, true); }, Swatch(ColorOf(Career::HairColors(), A.HairColor))));
	if (!bFem) { Row(Selector(LOCTEXT("Facial", "Facial hair"), Career::ClothingStyleName(7, int32(A.FacialHair)), [Change](int32 D) { Change([D](FAvatarConfig& X) { X.FacialHair = EAvatarFacialHair(Wrap(int32(X.FacialHair) + D, int32(EAvatarFacialHair::Count))); }, true); })); }
	Row(Selector(LOCTEXT("Height", "Height"), FText::AsPercent(A.Height), [Change](int32 D) { Change([D](FAvatarConfig& X) { X.Height = FMath::Clamp(X.Height + D * 0.03f, 0.85f, 1.1f); }, true); }));

	Head(LOCTEXT("HOutfit", "OUTFIT"));
	const FAvatarOutfit& O = A.Outfit;
	auto ClothRow = [&](const FText& L, int32 Kind, int32 Style, int32 ColorIdx)
	{
		Row(Selector(L, Career::ClothingStyleName(Kind, Style), [Change, StyleStep, Kind, Style, bOwnedOnly](int32 D)
		{
			const int32 V = StyleStep(Kind, Style, D, bOwnedOnly);
			Change([Kind, V](FAvatarConfig& X)
			{
				FAvatarOutfit& Q = X.Outfit;
				switch (Kind)
				{
				case 0: Q.Top = EOutfitTop(V); break;
				case 1: Q.Jacket = EOutfitJacket(V); break;
				case 2: Q.Pants = EOutfitPants(V); break;
				case 3: Q.Shoes = EOutfitShoes(V); break;
				case 4: Q.Hat = EOutfitHat(V); break;
				default: Q.Glasses = EOutfitGlasses(V); break;
				}
			}, true);
		}));
		if (ColorIdx >= 0)
		{
			Row(Selector(FText::Format(LOCTEXT("ColourOf", "  {0} colour"), L), FText::AsNumber(ColorIdx + 1), [Change, Kind, NCol](int32 D)
			{
				Change([Kind, D, NCol](FAvatarConfig& X)
				{
					FAvatarOutfit& Q = X.Outfit;
					int32& R = Kind == 0 ? Q.TopColor : Kind == 1 ? Q.JacketColor : Kind == 2 ? Q.PantsColor : Kind == 3 ? Q.ShoesColor : Q.HatColor;
					R = Wrap(R + D, NCol);
				}, true);
			}, Swatch(ColorOf(Career::OutfitColors(), ColorIdx))));
		}
	};
	ClothRow(LOCTEXT("Top", "Top"), 0, int32(O.Top), O.TopColor);
	ClothRow(LOCTEXT("Jacket", "Jacket"), 1, int32(O.Jacket), O.Jacket != EOutfitJacket::None ? O.JacketColor : -1);
	ClothRow(LOCTEXT("Pants", "Pants"), 2, int32(O.Pants), O.PantsColor);
	ClothRow(LOCTEXT("Shoes", "Shoes"), 3, int32(O.Shoes), O.ShoesColor);
	ClothRow(LOCTEXT("Hat", "Hat"), 4, int32(O.Hat), O.Hat != EOutfitHat::None ? O.HatColor : -1);
	ClothRow(LOCTEXT("Glasses", "Glasses"), 5, int32(O.Glasses), -1);

	Head(LOCTEXT("HSuit", "SUIT"));
	Row(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.36f).VAlign(VAlign_Center)[Txt(LOCTEXT("CustomPaint", "Custom paint (free play)"), 12, UiDim)]
		+ SHorizontalBox::Slot().FillWidth(0.64f)[SNew(SCheckBox).IsChecked(A.bCustomPaint ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Change](ECheckBoxState S) { Change([S](FAvatarConfig& X) { X.bCustomPaint = S == ECheckBoxState::Checked; }, false); })]);
	const TArray<FSuitPaintPreset>& Presets = Career::PaintPresets();
	if (Presets.Num())
	{
		const FSuitPaintPreset& P = Presets[Wrap(PaintPreset, Presets.Num())];
		Row(Selector(LOCTEXT("Preset", "Paint preset"), FText::FromString(P.Name), [Self, Change](int32 D)
		{
			if (!Self.IsValid()) { return; }
			const TArray<FSuitPaintPreset>& L = Career::PaintPresets();
			Self->PaintPreset = Wrap(Self->PaintPreset + D, L.Num());
			const FSuitPaintPreset Q = L[Self->PaintPreset];
			Change([Q](FAvatarConfig& X) { X.bCustomPaint = true; X.SuitPrimary = Q.Primary; X.SuitSecondary = Q.Secondary; X.ReactorGlow = Q.Glow; }, false);
		}, SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[Swatch(A.SuitPrimary)] + SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f)[Swatch(A.SuitSecondary)] + SHorizontalBox::Slot().AutoWidth()[Swatch(A.ReactorGlow)]));
		Row(Selector(LOCTEXT("Glow", "Reactor glow"), LOCTEXT("GlowCycle", "cycle"), [Change](int32 D) { Change([D](FAvatarConfig& X)
		{
			static const FLinearColor G[] = { FLinearColor(0.35f, 0.9f, 1.f), FLinearColor(1.f, 0.3f, 0.2f), FLinearColor(0.4f, 1.f, 0.5f), FLinearColor(1.f, 0.85f, 0.4f), FLinearColor(0.8f, 0.45f, 1.f), FLinearColor::White };
			int32 Cur = 0; float Best = 1e9f;
			for (int32 i = 0; i < 6; ++i) { const float Dd = FLinearColor::Dist(G[i], X.ReactorGlow); if (Dd < Best) { Best = Dd; Cur = i; } }
			X.ReactorGlow = G[Wrap(Cur + D, 6)]; X.bCustomPaint = true;
		}, false); }));
	}
	Row(Selector(LOCTEXT("Helmet", "Helmet"), Career::HelmetName(A.HelmetStyle), [Change](int32 D) { Change([D](FAvatarConfig& X) { X.HelmetStyle = Wrap(X.HelmetStyle + D, Career::NumHelmetStyles()); }, false); }));
	Row(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.36f).VAlign(VAlign_Center)[Txt(LOCTEXT("Slim", "Slim armour (girl fit)"), 12, UiDim)]
		+ SHorizontalBox::Slot().FillWidth(0.64f)[SNew(SCheckBox).IsChecked(A.bSlimArmor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Change](ECheckBoxState S) { Change([S](FAvatarConfig& X) { X.bSlimArmor = S == ECheckBoxState::Checked; }, false); })]);

	Head(LOCTEXT("HVoice", "VOICE"));
	const TArray<const FNpcVoiceDef*> Voices = PlayerVoices();
	if (Voices.Num())
	{
		const FNpcVoiceDef* V = Voices[Wrap(VoiceIndex, Voices.Num())];
		const FName VId = V->Id;
		Row(Selector(LOCTEXT("Voice", "Voice"), FText::FromString(V->Name), [Self](int32 D)
		{
			if (!Self.IsValid()) { return; }
			const TArray<const FNpcVoiceDef*> L = PlayerVoices();
			if (L.Num() == 0) { return; }
			Self->VoiceIndex = Wrap(Self->VoiceIndex + D, L.Num());
			Self->Draft.VoiceId = L[Self->VoiceIndex]->Id;
			Self->Refresh();
		}, Btn(LOCTEXT("PreviewVoice", "PREVIEW"), [Self, VId]()
		{
			if (!Self.IsValid()) { return; }
			if (!UArenaGameUserSettings::AreVoicesEnabled()) { if (UCareerSubsystem* CC = UCareerSubsystem::Get(Self.Get())) { CC->Toast(LOCTEXT("VoicesOff", "Voices are off - turn them on in Settings > Audio > Voices"), UiBad); } return; }
			if (USoundBase* S = NpcManifest::PreviewLine(VId)) { UGameplayStatics::PlaySound2D(Self.Get(), S); }
			else if (UCareerSubsystem* CC = UCareerSubsystem::Get(Self.Get())) { CC->Toast(LOCTEXT("NoSample", "No sample for this voice yet (placeholder / not imported)"), UiBad); }
		})));
	}

	TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
	Actions->AddSlot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("Random", "RANDOMIZE"), [Self]()
	{
		if (!Self.IsValid()) { return; }
		FRandomStream R; R.GenerateNewSeed();
		FAvatarConfig& X = Self->Draft;
		X.SkinTone = R.RandRange(0, FMath::Max(0, Career::SkinTones().Num() - 1));
		X.HairColor = R.RandRange(0, FMath::Max(0, Career::HairColors().Num() - 1));
		X.Hair = EAvatarHair(R.RandRange(0, int32(EAvatarHair::Count) - 1));
		X.Face = R.RandRange(0, FMath::Max(0, Career::FaceNames().Num() - 1));
		X.BodyModel = R.RandRange(0, FMath::Max(0, AvatarCatalog::ForBody(X.Body == EAvatarBody::Feminine).Num() - 1));
		X.Outfit.TopColor = R.RandRange(0, FMath::Max(0, Career::OutfitColors().Num() - 1));
		X.Outfit.PantsColor = R.RandRange(0, FMath::Max(0, Career::OutfitColors().Num() - 1));
		X.Outfit.ShoesColor = R.RandRange(0, FMath::Max(0, Career::OutfitColors().Num() - 1));
		Self->PushPreview();
		Self->Refresh();
	})];
	Actions->AddSlot().AutoWidth().Padding(3.f)[Btn(bFirstLaunch ? LOCTEXT("StartGame", "START  >") : LOCTEXT("SaveChar", "SAVE CHARACTER"), [C, Self, bFirstLaunch]()
	{
		if (!Self.IsValid()) { return; }
		FAvatarConfig X = Self->Draft;
		if (X.PilotName.TrimStartAndEnd().IsEmpty()) { X.PilotName = TEXT("Harley"); }
		C->SetAvatar(X, true);
		if (bFirstLaunch) { C->MarkCharacterCreated(); }
		C->Toast(FText::Format(LOCTEXT("Saved", "Character saved. Welcome, {0}."), FText::FromString(X.PilotName)), UiGood);
		Self->CloseScreen();
	}, true, FLinearColor(0.1f, 0.35f, 0.15f), 15)];

	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[BuildPreview(390.f, 520.f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(390.f)[WrapTxt(FText::FromString(AvatarCatalog::SourceNote()), 10, AvatarCatalog::HasPremium() ? UiTeal : UiGold)]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Opts]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[Actions]
		];
	return Frame(bFirstLaunch ? LOCTEXT("FirstTitle", "CREATE YOUR CHARACTER") : LOCTEXT("AvatarTitle", "EDIT CHARACTER  //  AVATAR"), Content, 1220.f);
}

// -------------------------------------------------------------------------------------------- barber / clothing

TSharedRef<SWidget> UCareerScreen::BuildBarber()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	const FAvatarConfig& Cur = C->GetAvatar();
	const bool bFem = Draft.Body == EAvatarBody::Feminine;
	int32 Price = 0;
	if (Draft.Hair != Cur.Hair || (!bFem && Draft.FacialHair != Cur.FacialHair))
	{
		Price += Career::BarberCutPrice;
		if (!C->OwnsClothing(6, int32(Draft.Hair)) && Career::FindClothing(6, int32(Draft.Hair))) { Price += Career::FindClothing(6, int32(Draft.Hair))->Price; }
		if (!bFem && !C->OwnsClothing(7, int32(Draft.FacialHair)) && Career::FindClothing(7, int32(Draft.FacialHair))) { Price += Career::FindClothing(7, int32(Draft.FacialHair))->Price; }
	}
	if (Draft.HairColor != Cur.HairColor) { Price += Career::BarberColorPrice; }
	auto Change = [Self](TFunction<void(FAvatarConfig&)> Fn) { if (Self.IsValid()) { Fn(Self->Draft); Self->PushPreview(); Self->Refresh(); } };
	const int32 NHair = Career::HairColors().Num();
	TSharedRef<SVerticalBox> Opts = SNew(SVerticalBox);
	Opts->AddSlot().AutoHeight()[Selector(LOCTEXT("BHair", "Cut"), Career::HairName(Draft.Hair), [Change](int32 D) { Change([D](FAvatarConfig& X) { X.Hair = EAvatarHair(Wrap(int32(X.Hair) + D, int32(EAvatarHair::Count))); }); })];
	Opts->AddSlot().AutoHeight()[Selector(LOCTEXT("BColour", "Colour"), FText::AsNumber(Draft.HairColor + 1), [Change, NHair](int32 D) { Change([D, NHair](FAvatarConfig& X) { X.HairColor = Wrap(X.HairColor + D, NHair); }); },
		Swatch(Career::HairColors().IsValidIndex(Draft.HairColor) ? Career::HairColors()[Draft.HairColor] : FLinearColor::Gray))];
	if (!bFem) { Opts->AddSlot().AutoHeight()[Selector(LOCTEXT("BBeard", "Facial hair"), Career::ClothingStyleName(7, int32(Draft.FacialHair)), [Change](int32 D) { Change([D](FAvatarConfig& X) { X.FacialHair = EAvatarFacialHair(Wrap(int32(X.FacialHair) + D, int32(EAvatarFacialHair::Count))); }); })]; }
	Opts->AddSlot().AutoHeight().Padding(0.f, 14.f)[Txt(FText::Format(LOCTEXT("BPrice", "Total: {0}"), Cr(Price)), 16, UiGold)];
	Opts->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("BPay", "PAY & APPLY"), [C, Self, Price]()
		{
			if (!Self.IsValid()) { return; }
			if (Price > 0 && !C->Spend(Price, LOCTEXT("Haircut", "haircut"))) { return; }
			FAvatarConfig X = C->GetAvatar();
			X.Hair = Self->Draft.Hair; X.HairColor = Self->Draft.HairColor; X.FacialHair = Self->Draft.FacialHair;
			C->SetAvatar(X, true);
			if (Self->InputOwner.IsValid()) { Self->InputOwner->RefreshPilotLook(); }
			Self->Refresh();
		}, Price == 0 || C->CanAfford(Price), FLinearColor(0.1f, 0.35f, 0.15f), 15)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("BReset", "RESET"), [Self, C]() { if (Self.IsValid()) { Self->Draft = C->GetAvatar(); Self->PushPreview(); Self->Refresh(); } })]
	];
	Opts->AddSlot().AutoHeight().Padding(0.f, 10.f)[WrapTxt(FText::Format(LOCTEXT("BarberHint", "Cut {0} CR (+ the style price the first time), colour {1} CR. The mirror shows the new look before you pay."), FText::AsNumber(Career::BarberCutPrice), FText::AsNumber(Career::BarberColorPrice)), 11)];
	return Frame(LOCTEXT("BarberTitle", "SHARP CUTS BARBER"), SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[BuildPreview(390.f, 520.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f, 0.f, 0.f, 0.f)[Opts], 1000.f);
}

TSharedRef<SWidget> UCareerScreen::BuildClothing()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	const FAvatarOutfit& O = Draft.Outfit;
	const int32 Styles[6] = { int32(O.Top), int32(O.Jacket), int32(O.Pants), int32(O.Shoes), int32(O.Hat), int32(O.Glasses) };
	const int32 Colors[6] = { O.TopColor, O.JacketColor, O.PantsColor, O.ShoesColor, O.HatColor, -1 };
	int32 Price = 0;
	for (int32 k = 0; k < 6; ++k) { if (!C->OwnsClothing(k, Styles[k])) { if (const FClothingItem* I = Career::FindClothing(k, Styles[k])) { Price += I->Price; } } }
	auto Change = [Self](TFunction<void(FAvatarConfig&)> Fn) { if (Self.IsValid()) { Fn(Self->Draft); Self->PushPreview(); Self->Refresh(); } };
	const int32 NCol = Career::OutfitColors().Num();
	TSharedRef<SVerticalBox> Opts = SNew(SVerticalBox);
	const FText Labels[6] = { LOCTEXT("CTop", "Top"), LOCTEXT("CJacket", "Jacket"), LOCTEXT("CPants", "Pants"), LOCTEXT("CShoes", "Shoes"), LOCTEXT("CHat", "Hat"), LOCTEXT("CGlasses", "Glasses") };
	for (int32 k = 0; k < 6; ++k)
	{
		const int32 St = Styles[k];
		const FClothingItem* I = Career::FindClothing(k, St);
		FText Val = Career::ClothingStyleName(k, St);
		if (!C->OwnsClothing(k, St) && I) { Val = FText::Format(LOCTEXT("StylePrice", "{0}  ({1})"), Val, Cr(I->Price)); }
		Opts->AddSlot().AutoHeight()[Selector(Labels[k], Val, [Change, k](int32 D) { Change([k, D](FAvatarConfig& X)
		{
			const int32 N = Career::ClothingStyleCount(k);
			FAvatarOutfit& Q = X.Outfit;
			switch (k)
			{
			case 0: Q.Top = EOutfitTop(Wrap(int32(Q.Top) + D, N)); break;
			case 1: Q.Jacket = EOutfitJacket(Wrap(int32(Q.Jacket) + D, N)); break;
			case 2: Q.Pants = EOutfitPants(Wrap(int32(Q.Pants) + D, N)); break;
			case 3: Q.Shoes = EOutfitShoes(Wrap(int32(Q.Shoes) + D, N)); break;
			case 4: Q.Hat = EOutfitHat(Wrap(int32(Q.Hat) + D, N)); break;
			default: Q.Glasses = EOutfitGlasses(Wrap(int32(Q.Glasses) + D, N)); break;
			}
		}); })];
		if (Colors[k] >= 0)
		{
			const int32 Ci = Colors[k];
			Opts->AddSlot().AutoHeight()[Selector(FText::Format(LOCTEXT("CColour", "  {0} colour"), Labels[k]), FText::AsNumber(Ci + 1), [Change, k, NCol](int32 D) { Change([k, D, NCol](FAvatarConfig& X)
			{
				FAvatarOutfit& Q = X.Outfit;
				int32& R = k == 0 ? Q.TopColor : k == 1 ? Q.JacketColor : k == 2 ? Q.PantsColor : k == 3 ? Q.ShoesColor : Q.HatColor;
				R = Wrap(R + D, NCol);
			}); }, Swatch(Career::OutfitColors().IsValidIndex(Ci) ? Career::OutfitColors()[Ci] : FLinearColor::Gray))];
		}
	}
	Opts->AddSlot().AutoHeight().Padding(0.f, 10.f)[Txt(FText::Format(LOCTEXT("CPrice", "New items: {0}"), Cr(Price)), 16, UiGold)];
	Opts->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(Price > 0 ? LOCTEXT("CBuy", "BUY & WEAR") : LOCTEXT("CWear", "WEAR"), [C, Self]()
		{
			if (!Self.IsValid()) { return; }
			const FAvatarOutfit Q = Self->Draft.Outfit;
			const int32 S[6] = { int32(Q.Top), int32(Q.Jacket), int32(Q.Pants), int32(Q.Shoes), int32(Q.Hat), int32(Q.Glasses) };
			for (int32 k = 0; k < 6; ++k) { if (!C->BuyClothing(k, S[k])) { return; } }
			FAvatarConfig X = C->GetAvatar();
			X.Outfit = Q;
			C->SetAvatar(X, true);
			if (Self->InputOwner.IsValid()) { Self->InputOwner->RefreshPilotLook(); }
			Self->Refresh();
		}, Price == 0 || C->CanAfford(Price), FLinearColor(0.1f, 0.35f, 0.15f), 15)]
		+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("CReset", "RESET"), [Self, C]() { if (Self.IsValid()) { Self->Draft = C->GetAvatar(); Self->PushPreview(); Self->Refresh(); } })]
	];
	TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
	for (int32 s = 0; s < Career::OutfitSlots; ++s)
	{
		Slots->AddSlot().AutoWidth().Padding(3.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Btn(FText::Format(LOCTEXT("SaveSlot", "SAVE {0}"), FText::AsNumber(s + 1)), [C, s]() { C->SaveOutfitSlot(s); })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Btn(FText::Format(LOCTEXT("WearSlot", "WEAR {0}"), FText::AsNumber(s + 1)), [C, s, Self]()
			{
				C->LoadOutfitSlot(s);
				if (Self.IsValid()) { Self->Draft = C->GetAvatar(); Self->PushPreview(); Self->Refresh(); if (Self->InputOwner.IsValid()) { Self->InputOwner->RefreshPilotLook(); } }
			})]
		];
	}
	Opts->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 2.f)[Txt(LOCTEXT("Slots", "OUTFIT SLOTS"), 13, UiPink)];
	Opts->AddSlot().AutoHeight()[Slots];
	return Frame(LOCTEXT("ClothesTitle", "STARK STREET CLOTHING"), SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[BuildPreview(390.f, 520.f)]
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f, 0.f, 0.f, 0.f)[SNew(SScrollBox) + SScrollBox::Slot()[Opts]], 1120.f);
}

// -------------------------------------------------------------------------------------------- phone

TSharedRef<SWidget> UCareerScreen::BuildPhone()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	TWeakObjectPtr<UCareerInput> In = InputOwner;
	TSharedRef<SVerticalBox> L = SNew(SVerticalBox);
	auto Line = [&L](const FText& Label, TSharedRef<SWidget> Action)
	{
		L->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Txt(Label, 13)]
			+ SHorizontalBox::Slot().AutoWidth()[Action]
		];
	};
	L->AddSlot().AutoHeight()[Txt(LOCTEXT("PhGarage", "GARAGE"), 13, UiPink)];
	if (C->GetVehiclesOwned().Num() == 0) { L->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("PhNoCars", "No vehicles yet - the dealership in Midtown (or the shop, U) sells them."), 11)]; }
	for (const FName& Id : C->GetVehiclesOwned())
	{
		const FVehicleDef* V = Career::FindVehicle(Id);
		if (!V) { continue; }
		const FName VId = Id;
		Line(FText::FromString(V->Name), Btn(LOCTEXT("PhDeliver", "DELIVER"), [In, VId, Self]() { if (Self.IsValid()) { Self->CloseScreen(); } if (In.IsValid()) { In->DeliverVehicle(VId); } }));
	}
	L->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Txt(LOCTEXT("PhHomes", "PROPERTIES"), 13, UiPink)];
	for (const FName& Id : C->GetHousesOwned())
	{
		const FCityHouseDef* H = Career::FindHouse(Id);
		if (!H) { continue; }
		const FName HId = Id;
		Line(FText::FromString(H->Name), Btn(LOCTEXT("PhTravel", "TRAVEL"), [C, HId, Self]() { if (Self.IsValid()) { Self->CloseScreen(); } C->TravelToHouse(HId); }));
	}
	L->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Txt(LOCTEXT("PhContacts", "CONTACTS"), 13, UiPink)];
	if (ACityLife* Life = ACityLife::Find(this))
	{
		const int32 Stars = Life->GetStars();
		const int32 Fee = 400 * Stars;
		Line(LOCTEXT("PhLawyer", "Lawyer (clear wanted level)"), Btn(FText::Format(LOCTEXT("PhFee", "CALL  {0}"), Cr(Fee)), [C, Life, Fee, Self]()
		{
			if (C->Spend(Fee, LOCTEXT("Lawyer", "lawyer"))) { Life->ClearWanted(); }
			if (Self.IsValid()) { Self->Refresh(); }
		}, Stars > 0 && C->CanAfford(Fee)));
	}
	Line(LOCTEXT("PhTaxi", "Taxi to Midtown"), Btn(LOCTEXT("PhGo", "GO  50 CR"), [C, Self]() { if (C->Spend(50, LOCTEXT("Taxi", "taxi"))) { if (Self.IsValid()) { Self->CloseScreen(); } C->TravelToCity(); } }));
	L->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)[Txt(LOCTEXT("PhJobs", "SIDE JOBS"), 13, UiPink)];
	L->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("PhJobsHint", "Quiet Midtown contracts. Credits only; each job has its own cooldown."), 11)];
	for (const FCareerJobDef& Job : CareerEconomy::Jobs())
	{
		const FName JId = Job.Id;
		const int32 Left = C->JobCooldownLeft(JId);
		const FText Label = Left > 0
			? FText::Format(LOCTEXT("PhJobCd", "{0}  (+{1} CR)  — ready in {2}m"), FText::FromString(Job.Name), FText::AsNumber(Job.Payout), FText::AsNumber((Left + 59) / 60))
			: FText::Format(LOCTEXT("PhJobReady", "{0}  (+{1} CR)"), FText::FromString(Job.Name), FText::AsNumber(Job.Payout));
		Line(Label, Btn(LOCTEXT("PhClaim", "CLAIM"), [C, JId, Self]()
		{
			if (C->ClaimJob(JId) && Self.IsValid()) { Self->Refresh(); }
		}, Left == 0));
	}
	return Frame(LOCTEXT("PhoneTitle", "PHONE"), SNew(SScrollBox) + SScrollBox::Slot()[L], 560.f);
}

// -------------------------------------------------------------------------------------------- casino

int32 UCareerScreen::MaxBet() const
{
	const UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return 0; }
	return FMath::Min3(CareerEconomy::CasinoMaxBet(), C->GetCredits(), C->GetCasinoLossAllowance());
}

int32 UCareerScreen::HandValue(const TArray<int32>& Cards) const
{
	int32 Sum = 0, Aces = 0;
	for (const int32 Cd : Cards) { const int32 V = FMath::Min(Cd, 10); Sum += V == 1 ? 11 : V; Aces += V == 1 ? 1 : 0; }
	while (Sum > 21 && Aces > 0) { Sum -= 10; --Aces; }
	return Sum;
}

void UCareerScreen::BlackjackDeal()
{
	if (bHandActive || Bet > MaxBet() || Bet < CareerEconomy::CasinoMinBet()) { return; }
	UiSfx(TEXT("casino_card_shuffle"), 0.8f);
	UiSfx(TEXT("casino_chip_lay"), 0.7f);
	PlayerCards = { CasinoRng.RandRange(1, 13), CasinoRng.RandRange(1, 13) };
	DealerCards = { CasinoRng.RandRange(1, 13) };
	bHandActive = true;
	bDoubled = false;
	ResultText = FText::GetEmpty();
	if (HandValue(PlayerCards) == 21) { BlackjackStand(false); return; }
	Refresh();
}

void UCareerScreen::BlackjackHit()
{
	if (!bHandActive) { return; }
	PlayerCards.Add(CasinoRng.RandRange(1, 13));
	if (HandValue(PlayerCards) > 21)
	{
		bHandActive = false;
		ResultText = LOCTEXT("Bust", "BUST - dealer wins");
		if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->SettleCasino(-(bDoubled ? Bet * 2 : Bet), ECasinoGame::Blackjack); }
	}
	Refresh();
}

void UCareerScreen::BlackjackStand(bool bDouble)
{
	if (!bHandActive) { return; }
	if (bDouble)
	{
		if (Bet * 2 > MaxBet()) { return; }
		bDoubled = true;
		PlayerCards.Add(CasinoRng.RandRange(1, 13));
	}
	const int32 Stake = bDoubled ? Bet * 2 : Bet;
	const int32 P = HandValue(PlayerCards);
	bHandActive = false;
	int32 Net = 0;
	if (P > 21) { Net = -Stake; ResultText = LOCTEXT("Bust2", "BUST - dealer wins"); }
	else
	{
		while (HandValue(DealerCards) < 17) { DealerCards.Add(CasinoRng.RandRange(1, 13)); } // dealer stands on all 17s
		const int32 D = HandValue(DealerCards);
		const bool bNatural = P == 21 && PlayerCards.Num() == 2;
		if (bNatural && !(D == 21 && DealerCards.Num() == 2)) { Net = Stake * 3 / 2; ResultText = LOCTEXT("BJ", "BLACKJACK! pays 3:2"); }
		else if (D > 21 || P > D) { Net = Stake; ResultText = LOCTEXT("Win", "YOU WIN"); }
		else if (P == D) { Net = 0; ResultText = LOCTEXT("Push", "PUSH"); }
		else { Net = -Stake; ResultText = LOCTEXT("Lose", "DEALER WINS"); }
	}
	if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->SettleCasino(Net, ECasinoGame::Blackjack); }
	UiSfx(Net > 0 ? FName(TEXT("coin")) : FName(TEXT("ui_back")), Net > 0 ? 0.9f : 0.4f);
	Refresh();
}

void UCareerScreen::CasinoPlay()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return; }
	const ECasinoGame G = ECasinoGame(Param);
	if (!bPendingSettle)
	{
		if (Bet < CareerEconomy::CasinoMinBet() || Bet > MaxBet()) { ResultText = LOCTEXT("BetLimit", "Bet over today's limit or your credits"); Refresh(); return; }
		bPendingSettle = true;
		UiSfx(G == ECasinoGame::Slots ? FName(TEXT("casino_chips_handle")) : FName(TEXT("casino_chips_stack")), 0.8f);
		SpinTime = G == ECasinoGame::Slots ? 1.4f : G == ECasinoGame::Wheel ? 2.6f : (G == ECasinoGame::CoinFlip || G == ECasinoGame::HighLow) ? 0.9f : 2.0f;
		ResultText = LOCTEXT("Spinning", "...");
		Refresh();
		return;
	}
	bPendingSettle = false;
	int32 Net = -Bet;
	if (G == ECasinoGame::Slots)
	{
		auto Pick = [this]()
		{
			int32 R = CasinoRng.RandRange(0, 82);
			for (int32 i = 0; i < 6; ++i) { if (R < SlotWeights[i]) { return i; } R -= SlotWeights[i]; }
			return 0;
		};
		Reels = { Pick(), Pick(), Pick() };
		int32 Cherries = 0;
		for (const int32 R : Reels) { Cherries += R == 0 ? 1 : 0; }
		if (Reels[0] == Reels[1] && Reels[1] == Reels[2]) { Net = Bet * SlotPays[Reels[0]] - Bet; }
		else if (Cherries == 2) { Net = 0; }
		ResultText = Net > 0 ? FText::Format(LOCTEXT("SlotWin", "WIN {0}"), Cr(Net + Bet)) : Net == 0 ? LOCTEXT("SlotPush", "Two cherries - bet back") : LOCTEXT("SlotLose", "No win");
	}
	else if (G == ECasinoGame::Roulette)
	{
		RouletteNumber = CasinoRng.RandRange(0, 36); // European single zero: house edge 2.7 %
		const int32 N = RouletteNumber;
		bool bWin = false;
		int32 Mul = 2;
		switch (RouletteBet)
		{
		case 0: bWin = N > 0 && IsRed(N); break;
		case 1: bWin = N > 0 && !IsRed(N); break;
		case 2: bWin = N > 0 && N % 2 == 1; break;
		case 3: bWin = N > 0 && N % 2 == 0; break;
		case 4: bWin = N >= 1 && N <= 18; break;
		case 5: bWin = N >= 19; break;
		case 6: case 7: case 8: bWin = N > 0 && (N - 1) / 12 == RouletteBet - 6; Mul = 3; break;
		default: bWin = N == 17; Mul = 36; break;
		}
		Net = bWin ? Bet * (Mul - 1) : -Bet;
		ResultText = FText::Format(LOCTEXT("RouletteRes", "{0} {1}  -  {2}"), FText::AsNumber(N), N == 0 ? LOCTEXT("Green", "GREEN") : IsRed(N) ? LOCTEXT("Red", "RED") : LOCTEXT("Black", "BLACK"),
			bWin ? FText::Format(LOCTEXT("RWin", "WIN {0}"), Cr(Net + Bet)) : LOCTEXT("RLose", "no win"));
	}
	else if (G == ECasinoGame::Wheel)
	{
		WheelSegment = CasinoRng.RandRange(0, 19);
		const float M = WheelMul[WheelSegment];
		Net = FMath::RoundToInt(Bet * M) - Bet;
		ResultText = M > 0.f ? FText::Format(LOCTEXT("WheelWin", "x{0}  -  {1}"), FText::AsNumber(M), Cr(Net + Bet)) : LOCTEXT("WheelLose", "LOSE");
	}
	else if (G == ECasinoGame::CoinFlip)
	{
		const bool bHeads = CasinoRng.FRand() < CareerEconomy::CoinFlipWinChance();
		RouletteNumber = bHeads ? 1 : 0;
		Net = bHeads ? Bet : -Bet; // even money; win chance < 0.5 gives the house its edge
		ResultText = bHeads
			? FText::Format(LOCTEXT("FlipWin", "HEADS  -  WIN {0}"), Cr(Net + Bet))
			: LOCTEXT("FlipLose", "TAILS  -  no win");
	}
	else if (G == ECasinoGame::HighLow)
	{
		// RouletteBet: 0 = Low, 1 = High. Shown card was RouletteNumber; draw next.
		const int32 Shown = FMath::Clamp(RouletteNumber, 1, 13);
		int32 Next = CasinoRng.RandRange(1, 13);
		// Soft house edge: with probability (1 - winChance) force a miss when not a push.
		const bool bForceMiss = CasinoRng.FRand() >= CareerEconomy::HighLowWinChance();
		if (Next == Shown) { Next = FMath::Clamp(Shown + (CasinoRng.FRand() < 0.5f ? -1 : 1), 1, 13); }
		if (bForceMiss)
		{
			if (RouletteBet == 1) { Next = CasinoRng.RandRange(1, FMath::Max(1, Shown - 1)); }
			else { Next = CasinoRng.RandRange(FMath::Min(13, Shown + 1), 13); }
		}
		else
		{
			if (RouletteBet == 1) { Next = CasinoRng.RandRange(FMath::Min(13, Shown + 1), 13); }
			else { Next = CasinoRng.RandRange(1, FMath::Max(1, Shown - 1)); }
		}
		const bool bWin = (RouletteBet == 1) ? (Next > Shown) : (Next < Shown);
		Net = bWin ? Bet : -Bet;
		static const TCHAR* Rank[] = { TEXT("?"), TEXT("A"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K") };
		ResultText = FText::Format(LOCTEXT("HLRes", "{0} → {1}  -  {2}"),
			FText::FromString(Rank[Shown]), FText::FromString(Rank[Next]),
			bWin ? FText::Format(LOCTEXT("HLWin", "WIN {0}"), Cr(Net + Bet)) : LOCTEXT("HLLose", "no win"));
		RouletteNumber = Next;
	}
	C->SettleCasino(Net, G);
	UiSfx(Net > 0 ? (G == ECasinoGame::Slots ? FName(TEXT("slot_win")) : FName(TEXT("coin"))) : FName(TEXT("ui_back")), Net > 0 ? 0.9f : 0.4f);
	Refresh();
}

TSharedRef<SWidget> UCareerScreen::BuildCasino()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return SNullWidget::NullWidget; }
	TWeakObjectPtr<UCareerScreen> Self(this);
	const ECasinoGame G = ECasinoGame(FMath::Clamp(Param, 0, int32(ECasinoGame::Count) - 1));
	const int32 Allow = C->GetCasinoLossAllowance();
	const FLinearColor Base(0.12f, 0.14f, 0.2f), Hot(0.45f, 0.1f, 0.2f), Sel(0.35f, 0.25f, 0.05f);
	TSharedRef<SHorizontalBox> Bets = SNew(SHorizontalBox);
	for (const int32 B : { 10, 50, 100, 250, 500, 1000 })
	{
		Bets->AddSlot().AutoWidth().Padding(2.f)[Btn(FText::AsNumber(B), [Self, B]() { if (Self.IsValid() && !Self->bHandActive && !Self->bPendingSettle) { Self->Bet = B; Self->Refresh(); } }, B <= MaxBet(), Bet == B ? Sel : Base)];
	}
	TSharedRef<SVerticalBox> Game = SNew(SVerticalBox);
	const bool bBusy = bPendingSettle || bHandActive;
	const TSharedRef<SWidget> PlayBtn = Btn(G == ECasinoGame::Roulette ? LOCTEXT("RSpin", "SPIN THE WHEEL") : LOCTEXT("Spin", "SPIN"), [Self]() { if (Self.IsValid()) { Self->CasinoPlay(); } }, !bBusy && Allow > 0, Hot, 20);
	if (G == ECasinoGame::Slots)
	{
		TSharedRef<SHorizontalBox> R = SNew(SHorizontalBox);
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 Sym = Reels.IsValidIndex(i) ? Reels[i] : 0;
			R->AddSlot().AutoWidth().Padding(6.f)
			[
				SNew(SBox).WidthOverride(170.f).HeightOverride(120.f)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.9f, 0.88f, 0.82f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[Txt(FText::FromString(SlotSymbols[Sym]), Sym == 4 ? 40 : 20, Sym >= 4 ? FLinearColor(0.8f, 0.05f, 0.1f) : FLinearColor(0.05f, 0.05f, 0.08f))]
				]
			];
		}
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)[R];
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(8.f)[PlayBtn];
		Game->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("Paytable", "3 x CHERRY 6x  |  BELL 12x  |  BAR 20x  |  STAR 40x  |  7 100x  |  DIAMOND 250x  |  any 2 cherries: bet back.  (return ~92 %)"), 11)];
	}
	else if (G == ECasinoGame::Blackjack)
	{
		auto CardsText = [](const TArray<int32>& Cards, bool bHole)
		{
			static const TCHAR* Names[] = { TEXT("?"), TEXT("A"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K") };
			FString S;
			for (const int32 Cd : Cards) { S += FString::Printf(TEXT("[%s] "), Names[FMath::Clamp(Cd, 0, 13)]); }
			if (bHole) { S += TEXT("[#]"); }
			return FText::FromString(S);
		};
		Game->AddSlot().AutoHeight()[Txt(FText::Format(LOCTEXT("Dealer", "DEALER  {0}   {1}"), CardsText(DealerCards, bHandActive), DealerCards.Num() ? FText::AsNumber(HandValue(DealerCards)) : FText::GetEmpty()), 18)];
		Game->AddSlot().AutoHeight().Padding(0.f, 10.f)[Txt(FText::Format(LOCTEXT("You", "YOU     {0}   {1}"), CardsText(PlayerCards, false), PlayerCards.Num() ? FText::AsNumber(HandValue(PlayerCards)) : FText::GetEmpty()), 18, UiGold)];
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("Deal", "DEAL"), [Self]() { if (Self.IsValid()) { Self->BlackjackDeal(); } }, !bHandActive && Allow > 0, Hot, 16)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("Hit", "HIT"), [Self]() { if (Self.IsValid()) { Self->BlackjackHit(); } }, bHandActive, Base, 16)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("Stand", "STAND"), [Self]() { if (Self.IsValid()) { Self->BlackjackStand(false); } }, bHandActive, Base, 16)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)[Btn(LOCTEXT("Double", "DOUBLE"), [Self]() { if (Self.IsValid()) { Self->BlackjackStand(true); } }, bHandActive && PlayerCards.Num() == 2, Base, 16)]
		];
		Game->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("BJRules", "Dealer stands on 17. Blackjack pays 3:2. Double on your first two cards."), 11)];
	}
	else if (G == ECasinoGame::Roulette)
	{
		const FText BetNames[] = { LOCTEXT("RBRed", "RED"), LOCTEXT("RBBlack", "BLACK"), LOCTEXT("RBOdd", "ODD"), LOCTEXT("RBEven", "EVEN"), LOCTEXT("RBLow", "1-18"), LOCTEXT("RBHigh", "19-36"),
			LOCTEXT("RBD1", "1st 12"), LOCTEXT("RBD2", "2nd 12"), LOCTEXT("RBD3", "3rd 12"), LOCTEXT("RB17", "17 (35:1)") };
		TSharedRef<SWrapBox> W = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
		for (int32 b = 0; b < 10; ++b)
		{
			W->AddSlot()[Btn(BetNames[b], [Self, b]() { if (Self.IsValid() && !Self->bPendingSettle) { Self->RouletteBet = b; Self->Refresh(); } }, true, RouletteBet == b ? Sel : (b == 0 ? FLinearColor(0.45f, 0.05f, 0.05f) : Base))];
		}
		const int32 N = RouletteNumber;
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(140.f).HeightOverride(140.f)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.BorderBackgroundColor(N == 0 ? FLinearColor(0.05f, 0.4f, 0.1f) : IsRed(N) ? FLinearColor(0.6f, 0.04f, 0.04f) : FLinearColor(0.03f, 0.03f, 0.03f))
				[Txt(FText::AsNumber(N), 40)]
			]
		];
		Game->AddSlot().AutoHeight().Padding(0.f, 8.f)[W];
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)[PlayBtn];
		Game->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("RRules", "European single-zero wheel. Even bets pay 1:1, dozens 2:1, a single number 35:1. Zero loses every outside bet."), 11)];
	}
	else if (G == ECasinoGame::Wheel)
	{
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(2.f);
		const int32 Lit = bPendingSettle ? (int32(WheelAngle / 18.f) % 20) : WheelSegment;
		for (int32 s = 0; s < 20; ++s)
		{
			const float M = WheelMul[s];
			Grid->AddSlot(s % 10, s / 10)
			[
				SNew(SBox).WidthOverride(64.f).HeightOverride(44.f)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.BorderBackgroundColor(s == Lit ? UiGold : (M > 0.f ? FLinearColor(0.15f, 0.08f, 0.3f) : FLinearColor(0.08f, 0.08f, 0.1f)))
					[Txt(M > 0.f ? FText::Format(LOCTEXT("WMul", "x{0}"), FText::AsNumber(M)) : LOCTEXT("WLose", "-"), 13, s == Lit ? FLinearColor::Black : FLinearColor::White)]
				]
			];
		}
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)[Grid];
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(8.f)[PlayBtn];
		Game->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("WRules", "20 segments: 10 lose, 5 x1.2, 3 x2, 1 x3, 1 x4 (return 95 %)."), 11)];
	}
	else if (G == ECasinoGame::CoinFlip)
	{
		const bool bHeads = RouletteNumber != 0;
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(160.f).HeightOverride(160.f)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.BorderBackgroundColor(bPendingSettle ? FLinearColor(0.35f, 0.3f, 0.1f) : (bHeads ? UiGold : FLinearColor(0.25f, 0.25f, 0.28f)))
				[Txt(bPendingSettle ? LOCTEXT("FlipSpin", "...") : (bHeads ? LOCTEXT("Heads", "HEADS") : LOCTEXT("Tails", "TAILS")), 28, FLinearColor::Black)]
			]
		];
		const TSharedRef<SWidget> FlipBtn = Btn(LOCTEXT("Flip", "FLIP"), [Self]() { if (Self.IsValid()) { Self->CasinoPlay(); } }, !bBusy && Allow > 0, Hot, 20);
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(8.f)[FlipBtn];
		Game->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("FlipRules", "Call heads. Even money; house edge from a slightly unfair coin. Credits only."), 11)];
	}
	else // HighLow
	{
		if (RouletteNumber < 1 || RouletteNumber > 13) { RouletteNumber = 7; }
		static const TCHAR* Rank[] = { TEXT("?"), TEXT("A"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("J"), TEXT("Q"), TEXT("K") };
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(120.f).HeightOverride(160.f)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.BorderBackgroundColor(FLinearColor(0.92f, 0.92f, 0.9f))
				[Txt(FText::FromString(Rank[FMath::Clamp(RouletteNumber, 1, 13)]), 40, FLinearColor(0.05f, 0.05f, 0.08f))]
			]
		];
		Game->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(8.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)[Btn(LOCTEXT("HLLow", "LOW"), [Self]() { if (Self.IsValid()) { Self->RouletteBet = 0; Self->CasinoPlay(); } }, !bBusy && Allow > 0, RouletteBet == 0 ? Sel : Hot, 18)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)[Btn(LOCTEXT("HLHigh", "HIGH"), [Self]() { if (Self.IsValid()) { Self->RouletteBet = 1; Self->CasinoPlay(); } }, !bBusy && Allow > 0, RouletteBet == 1 ? Sel : Hot, 18)]
		];
		Game->AddSlot().AutoHeight()[WrapTxt(LOCTEXT("HLRules", "Guess whether the next card is higher or lower. Aces low, kings high. Even money with a house edge. Credits only."), 11)];
	}
	return Frame(FText::Format(LOCTEXT("CasinoTitle", "ARC ROYALE  //  {0}"), Career::CasinoGameName(G)),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Txt(LOCTEXT("Bet", "BET  "), 14, UiDim)]
			+ SHorizontalBox::Slot().AutoWidth()[Bets]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)[Txt(FText::Format(LOCTEXT("Allow", "Can still lose today: {0}"), Cr(Allow)), 12, Allow > 0 ? UiDim : UiBad)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)[Game]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Txt(ResultText, 22, UiGold)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f)[WrapTxt(FText::Format(LOCTEXT("CasinoNote", "In-game credits only - no real money, nothing to buy. Daily loss limit {0} CR; bets {1} - {2} CR."), Cr(CareerEconomy::CasinoDailyLossLimit()), Cr(CareerEconomy::CasinoMinBet()), Cr(CareerEconomy::CasinoMaxBet())), 11, UiDim)],
		900.f);
}

// ============================================================================================ HUD

TSharedRef<SWidget> UCareerHUD::RebuildWidget()
{
	return SNew(SBox).Visibility(EVisibility::HitTestInvisible);
}

void UCareerHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Fps = FMath::Lerp(Fps, 1.f / FMath::Max(0.001f, InDeltaTime), 0.05f);
}

int32 UCareerHUD::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, G, MyCullingRect, Out, LayerId, InWidgetStyle, bParentEnabled);
	const UCareerSubsystem* C = UCareerSubsystem::Get(this);
	APlayerController* PCtl = GetOwningPlayer();
	if (!C || !PCtl) { return LayerId; }
	// Title / pause menus own the screen: no credits, crosshair or super ring behind them.
	if (const AArenaPlayerController* MenuPC = Cast<AArenaPlayerController>(PCtl)) { if (MenuPC->IsMenuOpen()) { return LayerId; } }
	const FVector2D Size = G.GetLocalSize();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto Text = [&](const FString& S, const FVector2D& Pos, int32 FontSize, const FLinearColor& Col, bool bRightAlign)
	{
		const FSlateFontInfo F = Font(FontSize);
		FVector2D P = Pos;
		if (bRightAlign) { P.X -= Measure->Measure(S, F).X; }
		FSlateDrawElement::MakeText(Out, LayerId + 1, G.ToPaintGeometry(FVector2D(1.f, 1.f), FSlateLayoutTransform(P + FVector2D(2.f, 2.f))), S, F, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.7f * Col.A));
		FSlateDrawElement::MakeText(Out, LayerId + 2, G.ToPaintGeometry(FVector2D(1.f, 1.f), FSlateLayoutTransform(P)), S, F, ESlateDrawEffect::None, Col);
	};
	float Y = Size.Y * 0.16f;
	Text(FString::Printf(TEXT("$ %s CR"), *FText::AsNumber(C->GetCredits()).ToString()), FVector2D(Size.X - 30.f, Y), 20, UiGold, true);
	Y += 30.f;
	if (const ACityLife* Life = ACityLife::Find(this))
	{
		if (Life->GetStars() > 0)
		{
			const bool bFlash = !Life->IsPlayerInSearchArea() && FMath::Fmod(FPlatformTime::Seconds(), 0.6) < 0.3;
			for (int32 s = 0; s < 5; ++s)
			{
				const FVector2D Ctr(Size.X - 44.f - (4 - s) * 34.f, Y + 14.f);
				TArray<FVector2D> Pts;
				for (int32 k = 0; k <= 10; ++k)
				{
					const float A = -PI * 0.5f + k * PI / 5.f;
					const float R = (k % 2 == 0) ? 14.f : 6.f;
					Pts.Add(Ctr + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
				}
				const bool bOn = s < Life->GetStars();
				const FLinearColor Col = bOn ? (bFlash ? FLinearColor(1.f, 1.f, 1.f, 0.5f) : FLinearColor(1.f, 0.85f, 0.2f)) : FLinearColor(0.3f, 0.3f, 0.3f, 0.6f);
				FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, bOn ? 3.f : 1.5f);
			}
			Y += 34.f;
			if (!Life->IsPlayerInSearchArea())
			{
				Text(FString::Printf(TEXT("LOSING THEM  %d%%"), FMath::RoundToInt(Life->GetEscapeFraction() * 100.f)), FVector2D(Size.X - 30.f, Y), 12, UiTeal, true);
				Y += 20.f;
			}
		}
	}
	const double Now = FPlatformTime::Seconds();
	for (const FCareerToast& T : C->GetToasts())
	{
		FLinearColor Col = T.Color;
		Col.A = FMath::Clamp(4.5f - float(Now - T.Time), 0.f, 1.f);
		Text(T.Text.ToString(), FVector2D(Size.X - 30.f, Y), 14, Col, true);
		Y += 22.f;
	}
	const ADowntownDistrict* Dist = ADowntownDistrict::Find(this);
	APawn* Pawn = PCtl->GetPawn();
	if (bShowMarkers && Dist && Pawn && Dist->IsDistrictVisible())
	{
		int32 VX = 0, VY = 0;
		PCtl->GetViewportSize(VX, VY);
		const float Scale = VX > 0 ? Size.X / float(VX) : 1.f;
		const FVector Eye = Pawn->GetActorLocation();
		auto Marker = [&](const FVector& W, const FString& Label, const FLinearColor& Col)
		{
			FVector2D Scr;
			if (!PCtl->ProjectWorldLocationToScreen(W, Scr, true)) { return; }
			Scr *= Scale;
			if (Scr.X < 0.f || Scr.Y < 0.f || Scr.X > Size.X || Scr.Y > Size.Y) { return; }
			const float Dm = FVector::Dist(W, Eye) / 100.f;
			TArray<FVector2D> Diamond = { Scr + FVector2D(0, -8), Scr + FVector2D(8, 0), Scr + FVector2D(0, 8), Scr + FVector2D(-8, 0), Scr + FVector2D(0, -8) };
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, Col, true, 2.5f);
			Text(FString::Printf(TEXT("%s  %dm"), *Label, FMath::RoundToInt(Dm)), Scr + FVector2D(12.f, -9.f), 11, Col, false);
		};
		for (const ACityHouse* H : Dist->GetHouses())
		{
			const FCityHouseDef* D = H ? Career::FindHouse(H->GetHouseId()) : nullptr;
			if (!D) { continue; }
			const bool bOwned = C->OwnsHouse(D->Id);
			Marker(H->GetMarkerLocation(), bOwned ? FString::Printf(TEXT("HOME: %s"), D->Name) : FString::Printf(TEXT("FOR SALE: %s  %d CR"), D->Name, D->Price),
				bOwned ? FLinearColor(0.4f, 0.75f, 1.f) : FLinearColor(0.4f, 1.f, 0.5f));
		}
		for (const FCityPoi& P : Dist->GetPois())
		{
			if (P.Type == FName(TEXT("casino_game"))) { continue; }
			Marker(Dist->PoiWorld(P) + FVector(0, 0, 400.f), P.Name, UiPink);
		}
	}
	if (!Prompt.IsEmpty())
	{
		const FString S = Prompt.ToString();
		Text(S, FVector2D(Size.X * 0.5f - Measure->Measure(S, Font(16)).X * 0.5f, Size.Y * 0.72f), 16, FLinearColor::White, false);
	}
	if (InputOwner.IsValid())
	{
		if (const ACityVehicle* V = InputOwner->GetDrivenVehicle())
		{
			FString S = FString::Printf(TEXT("%d km/h"), FMath::RoundToInt(V->GetSpeedKmh()));
			if (V->GetKind() == ECityVehicleKind::Plane || V->GetKind() == ECityVehicleKind::Helicopter) { S += FString::Printf(TEXT("   ALT %d m"), FMath::RoundToInt(V->GetAltitudeM())); }
			if (V->GetKind() == ECityVehicleKind::Plane) { S += FString::Printf(TEXT("   THR %d%%"), FMath::RoundToInt(V->GetThrottle() * 100.f)); }
			Text(S, FVector2D(Size.X - 30.f, Size.Y - 80.f), 22, UiTeal, true);
			Text(TEXT("[E] get out"), FVector2D(Size.X - 30.f, Size.Y - 50.f), 12, UiDim, true);
		}
		else if (InputOwner->IsSuitOff())
		{
			Text(TEXT("ON FOOT (suit off)  -  O: suit up (near the suit) / call the suit"), FVector2D(30.f, Size.Y - 60.f), 13, UiTeal, false);
		}
	}
	if (bShowFps) { Text(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Fps)), FVector2D(20.f, 12.f), 14, UiTeal, false); }

	// ---- combat crosshair: bloom ticks, focus ring, charge arc, hit markers, kill confirms (ArenaCombat) ----
	const AIronManCharacter* Me = Cast<AIronManCharacter>(Pawn);
	const bool bCombat = Me && !Me->IsDead() && !Me->IsInCinematic() && !(InputOwner.IsValid() && (InputOwner->IsSuitOff() || InputOwner->GetDrivenVehicle() || InputOwner->IsScreenOpen()));
	if (bCombat)
	{
		const FVector2D Ctr = Size * 0.5f;
		const float Bloom = ArenaCombat::GetBloom();
		const bool bFocus = ArenaCombat::IsFocus();
		const float Gap = 9.f + Bloom * 16.f - (bFocus ? 4.f : 0.f);
		const FLinearColor Cyan(0.45f, 0.95f, 1.f, 0.9f);
		auto Line = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Col, float W)
		{
			TArray<FVector2D> P = { A, B };
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, W);
		};
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2D Dd = k == 0 ? FVector2D(1, 0) : k == 1 ? FVector2D(-1, 0) : k == 2 ? FVector2D(0, 1) : FVector2D(0, -1);
			Line(Ctr + Dd * Gap, Ctr + Dd * (Gap + 8.f), Cyan, 2.f);
		}
		auto Arc = [&](float R, float From01, float To01, const FLinearColor& Col, float W)
		{
			TArray<FVector2D> P;
			const int32 N = FMath::Max(2, int32(48 * (To01 - From01)));
			for (int32 i = 0; i <= N; ++i)
			{
				const float A = -PI * 0.5f + 2.f * PI * FMath::Lerp(From01, To01, float(i) / N);
				P.Add(Ctr + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, W);
		};
		if (bFocus) { Arc(30.f, 0.f, 1.f, FLinearColor(0.45f, 0.95f, 1.f, 0.35f), 1.f); }
		const float Charge = ArenaCombat::GetCharge();
		if (Charge > 0.f) { Arc(30.f, 0.f, Charge, Charge >= 1.f ? FLinearColor(1.f, 0.9f, 0.4f) : FLinearColor(0.6f, 1.f, 1.f), 3.f); }
		const float HitAge = ArenaCombat::HitMarkerAge();
		const float KillAge = ArenaCombat::KillMarkerAge();
		if (HitAge < 0.25f)
		{
			const bool bKill = KillAge < 0.25f;
			const float A = 1.f - HitAge / 0.25f;
			const float In = 7.f + (bKill ? 4.f : 0.f), OutR = In + (bKill ? 12.f : 8.f) + 4.f * (1.f - A);
			const FLinearColor Col = bKill ? FLinearColor(1.f, 0.25f, 0.2f, A) : FLinearColor(1.f, 1.f, 1.f, A);
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector2D Dd = FVector2D(k < 2 ? 1.f : -1.f, (k % 2) ? 1.f : -1.f) * 0.7071f;
				Line(Ctr + Dd * In, Ctr + Dd * OutR, Col, bKill ? 3.f : 2.f);
			}
		}
		if (KillAge < 1.2f)
		{
			const int32 Streak = ArenaCombat::KillStreak();
			const float A = FMath::Clamp(1.2f - KillAge, 0.f, 1.f);
			const FString S = Streak > 1 ? FString::Printf(TEXT("KILL  x%d"), Streak) : FString(TEXT("KILL"));
			const FSlateFontInfo F = Font(15);
			Text(S, FVector2D(Ctr.X - Measure->Measure(S, F).X * 0.5f, Ctr.Y + 34.f), 15, FLinearColor(1.f, 0.35f, 0.25f, A), false);
		}

		// ---- super move cooldown ring (bottom centre) + key hints ----
		if (const USuitSuperComponent* Sup = USuitSuperComponent::Get(Me))
		{
			const FSuperDef D = Sup->CurrentDef();
			const FVector2D RC(Size.X * 0.5f, Size.Y - 92.f);
			const float Frac = Sup->GetCooldownFraction();
			const bool bReady = Sup->IsReady();
			const float Pulse = bReady ? 0.75f + 0.25f * FMath::Sin(float(FPlatformTime::Seconds()) * 5.f) : 1.f;
			auto Ring = [&](float R, float To01, const FLinearColor& Col, float W)
			{
				TArray<FVector2D> P;
				const int32 N = FMath::Max(2, int32(48 * To01));
				for (int32 i = 0; i <= N; ++i) { const float An = -PI * 0.5f + 2.f * PI * To01 * float(i) / N; P.Add(RC + FVector2D(FMath::Cos(An), FMath::Sin(An)) * R); }
				FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, W);
			};
			Ring(24.f, 1.f, FLinearColor(0.45f, 0.95f, 1.f, 0.25f), 2.f);
			if (Sup->IsActive()) { Ring(24.f, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.9f), 4.f); }
			else { Ring(24.f, Frac, bReady ? FLinearColor(D.Tint.R, D.Tint.G, D.Tint.B, Pulse) : FLinearColor(0.45f, 0.95f, 1.f, 0.7f), bReady ? 4.f : 3.f); }
			const FString Nm = FString(D.Name).ToUpper();
			const FSlateFontInfo F12 = Font(12), F10 = Font(10);
			Text(Nm, FVector2D(RC.X - Measure->Measure(Nm, F12).X * 0.5f, RC.Y + 30.f), 12, bReady ? FLinearColor(1.f, 0.92f, 0.6f) : UiDim, false);
			const FString Hint = bReady ? FString(TEXT("HOLD RMB  /  DOUBLE-CLICK")) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Frac * 100.f));
			Text(Hint, FVector2D(RC.X - Measure->Measure(Hint, F10).X * 0.5f, RC.Y - 8.f + (bReady ? 54.f : 0.f)), 10, bReady ? UiTeal : UiDim, false);
			FString Keys = TEXT("[O] exit suit   [H] faceplate   [Alt] focus");
			if (Sup->CanSwing()) { Keys += Sup->IsSwingMode() ? TEXT("   [B] fly   [Space] web swing   [R] web zip") : TEXT("   [B] web swing"); }
			if (Sup->IsMaskOpen()) { Keys += TEXT("   - faceplate open, HUD offline"); }
			Text(Keys, FVector2D(30.f, Size.Y - 34.f), 11, UiDim, false);
		}

		// ---- speed lines at high flight speed (boost) ----
		const float Spd = Me->GetVelocity().Size();
		if (Me->IsFlying() && Spd > 2800.f)
		{
			const float A = FMath::Clamp((Spd - 2800.f) / 3500.f, 0.f, 1.f);
			const float Tm = float(FPlatformTime::Seconds());
			for (int32 i = 0; i < 28; ++i)
			{
				const float Seed = FMath::Frac(FMath::Sin(i * 12.9898f) * 43758.5453f);
				const float An = Seed * 2.f * PI;
				const float Ph0 = FMath::Frac(Tm * (1.6f + Seed) + Seed * 7.f);
				const FVector2D Dir2(FMath::Cos(An), FMath::Sin(An));
				const float R0 = FMath::Lerp(0.32f, 0.62f, Ph0) * Size.Y, R1 = R0 + (40.f + 90.f * A) ;
				Line(Ctr + Dir2 * R0, Ctr + Dir2 * R1, FLinearColor(0.85f, 0.95f, 1.f, 0.22f * A * (1.f - Ph0)), 1.5f);
			}
		}
	}
	return LayerId + 3;
}

// ============================================================================================ input component

UCareerInput::UCareerInput()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarDevMenu(TEXT("ima.DevMenu"), 1, TEXT("Dev bar (` / F1) in non-shipping builds (0 = hide)."));
#endif

bool UCareerInput::IsDevMenuAllowed()
{
#if UE_BUILD_SHIPPING
	static const bool bFile = FPaths::FileExists(FPaths::ProjectSavedDir() / TEXT("AllowDevMenu.txt"));
	return bFile;
#else
	return CVarDevMenu.GetValueOnGameThread() != 0;
#endif
}

UCareerInput* UCareerInput::Get(const UObject* WorldContext)
{
	APlayerController* P = WorldContext ? UGameplayStatics::GetPlayerController(WorldContext, 0) : nullptr;
	return P ? P->FindComponentByClass<UCareerInput>() : nullptr;
}

APlayerController* UCareerInput::PC() const { return Cast<APlayerController>(GetOwner()); }

void UCareerInput::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* P = PC();
	if (!P || !P->IsLocalController()) { return; }
	HUD = CreateWidget<UCareerHUD>(P, UCareerHUD::StaticClass());
	if (HUD) { HUD->InputOwner = this; HUD->AddToViewport(40); }
}

void UCareerInput::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnFootPilot) { OnFootPilot->Destroy(); OnFootPilot = nullptr; }
	if (Delivered.IsValid()) { Delivered->Destroy(); }
	Super::EndPlay(EndPlayReason);
}

bool UCareerInput::IsScreenOpen() const { return ScreenWidget && ScreenWidget->IsInViewport() && ScreenWidget->GetScreen() != ECareerScreen::None; }
ECareerScreen UCareerInput::GetOpenScreen() const { return IsScreenOpen() ? ScreenWidget->GetScreen() : ECareerScreen::None; }

void UCareerInput::OpenScreen(ECareerScreen Screen, int32 Param)
{
	APlayerController* P = PC();
	if (!P) { return; }
	if (Screen == ECareerScreen::DevBar && !IsDevMenuAllowed()) { return; }
	UE_LOG(LogTemp, Log, TEXT("Career: open screen %d (avatar=3, first-launch=4)"), int32(Screen));
	if (!ScreenWidget)
	{
		ScreenWidget = CreateWidget<UCareerScreen>(P, UCareerScreen::StaticClass());
		if (!ScreenWidget) { return; }
		ScreenWidget->InputOwner = this;
	}
	if (!ScreenWidget->IsInViewport()) { ScreenWidget->AddToViewport(60); }
	ScreenWidget->SetIsFocusable(true);
	ScreenWidget->Open(Screen, Param);
	P->bShowMouseCursor = true;
	if (Screen == ECareerScreen::DevBar)
	{
		FInputModeGameAndUI M;
		M.SetHideCursorDuringCapture(false);
		M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		P->SetInputMode(M);
	}
	else
	{
		FInputModeUIOnly M;
		M.SetWidgetToFocus(ScreenWidget->TakeWidget());
		P->SetInputMode(M);
		UGameplayStatics::SetGamePaused(this, true);
	}
}

void UCareerInput::CloseScreen()
{
	APlayerController* P = PC();
	const bool bWasFirst = ScreenWidget && ScreenWidget->GetScreen() == ECareerScreen::FirstLaunch;
	if (ScreenWidget)
	{
		ScreenWidget->RemoveFromParent();
		ScreenWidget = nullptr; // fresh widget (and preview stage) next time
	}
	if (!P) { return; }
	AArenaPlayerController* APC = Cast<AArenaPlayerController>(P);
	if (APC && APC->IsMenuOpen())
	{
		// Back to the title / pause menu (keeps UI input).
		APC->ShowMenu(APC->GetLastMenuMode());
		return;
	}
	P->bShowMouseCursor = false;
	P->SetInputMode(FInputModeGameOnly());
	UGameplayStatics::SetGamePaused(this, false);
	if (bWasFirst) { Greet(); }
	RefreshPilotLook();
}

void UCareerInput::Greet()
{
	if (bGreeted) { return; }
	const UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C || C->NeedsCharacterSetup()) { return; }
	bGreeted = true;
	const bool bFem = C->GetAvatar().Body == EAvatarBody::Feminine;
	if (UJarvisComponent* J = UJarvisComponent::Get(this))
	{
		J->SayScripted(bFem ? FName(TEXT("FRIDAY")) : FName(TEXT("JARVIS")), bFem ? LOCTEXT("Friday", "F.R.I.D.A.Y.") : LOCTEXT("Jarvis", "J.A.R.V.I.S."),
			FText::Format(LOCTEXT("Welcome", "Welcome, {0}. All systems online."), FText::FromString(C->GetPilotName())), UiTeal, nullptr, 60, false);
	}
	if (UCareerSubsystem* CM = UCareerSubsystem::Get(this)) { CM->Toast(FText::Format(LOCTEXT("WelcomeToast", "Welcome, {0}"), FText::FromString(C->GetPilotName())), UiTeal); }
}

// -ArenaSmoke runs headless with a fresh Saved/: the first-launch creator would pause the world and the
// smoke never starts a wave. Skip the creator there only (the player still gets it on a real first launch).
static bool IsHeadlessArenaSmoke()
{
	static const bool bSmoke = FParse::Param(FCommandLine::Get(), TEXT("ArenaSmoke"));
	return bSmoke;
}

void UCareerInput::OnFreePlayStarted()
{
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!C) { return; }
	C->ApplySuitLock(Player(this));
	C->ReapplyLook();
	if (C->NeedsCharacterSetup() && !IsHeadlessArenaSmoke())
	{
		bFirstLaunchShown = true;
		UE_LOG(LogTemp, Log, TEXT("Career: free play started without a character - opening the creator"));
		OpenScreen(ECareerScreen::FirstLaunch);
		return;
	}
	if (!C->GetHomeHouse().IsNone()) { C->TravelToHouse(C->GetHomeHouse()); }
	bGreeted = false;
	Greet();
}

void UCareerInput::SetSuitOff(bool bOff)
{
	AIronManCharacter* Char = Player(this);
	if (!Char || bOff == bSuitOff) { return; }
	if (bOff && Char->IsFlying()) { Char->SimulateFlightKey(false); }
	const bool bAirborne = Char->GetCharacterMovement() && !Char->GetCharacterMovement()->IsMovingOnGround();
	if (bOff)
	{
		// Leave the empty suit standing (or hover-idling) where it is, the pilot steps out beside it.
		if (Parked) { Parked->Destroy(); }
		Parked = AParkedSuit::SpawnFrom(Char, bAirborne);
		Char->SetActorLocation(Char->GetActorLocation() - Char->GetActorRightVector() * 120.f + Char->GetActorForwardVector() * 30.f, true);
		ArenaAudio::PlayAt(Char, TEXT("suitup_servo"), Char->GetActorLocation(), 0.9f);
	}
	else if (Parked) { Parked->Destroy(); Parked = nullptr; }
	bSuitOff = bOff;
	Char->bCareerSuitOff = bOff;
	Char->SetActorHiddenInGame(bOff);
	if (UCharacterMovementComponent* M = Char->GetCharacterMovement())
	{
		if (bOff) { SavedWalkSpeed = M->MaxWalkSpeed; M->MaxWalkSpeed = 420.f; }
		else { M->MaxWalkSpeed = SavedWalkSpeed; }
	}
	if (bOff)
	{
		if (!OnFootPilot)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			OnFootPilot = GetWorld()->SpawnActor<APilotAvatar>(APilotAvatar::StaticClass(), Char->GetActorTransform(), SP);
		}
		if (OnFootPilot)
		{
			OnFootPilot->AttachToActor(Char, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			OnFootPilot->SetActorRelativeLocation(FVector(0, 0, -Char->GetSimpleCollisionHalfHeight()));
			OnFootPilot->SetActorRelativeRotation(FRotator::ZeroRotator);
			RefreshPilotLook();
		}
	}
	else if (OnFootPilot) { OnFootPilot->Destroy(); OnFootPilot = nullptr; }
	if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->Toast(bOff ? LOCTEXT("SuitOff", "Suit off - on foot") : LOCTEXT("SuitOn", "Suit up!"), UiTeal); }
}

FKey UCareerInput::Key(const TCHAR* Name, const FKey& Default)
{
	FString S;
	if ((GConfig && GConfig->GetString(TEXT("IronManArena.Keys"), Name, S, GGameUserSettingsIni)) || (GConfig && GConfig->GetString(TEXT("IronManArena.Keys"), Name, S, GGameIni)))
	{
		const FKey K(*S);
		if (K.IsValid()) { return K; }
	}
	return Default;
}

void UCareerInput::RequestSuitUp()
{
	AIronManCharacter* Char = Player(this);
	if (!Char || !bSuitOff) { return; }
	const float D = Parked ? float(FVector::Dist(Parked->GetActorLocation(), Char->GetActorLocation())) : 0.f;
	if (Parked && D > 350.f && !Parked->IsCalled())
	{
		Parked->Call(Char);
		if (UCareerSubsystem* C = UCareerSubsystem::Get(this)) { C->Toast(LOCTEXT("SuitCalled", "Suit inbound..."), UiTeal); }
		return;
	}
	if (Parked && Parked->IsCalled() && !Parked->HasArrived()) { return; }
	SetSuitOff(false);
	ASuitUpCinematic::Play(Char, Char->GetSuitIndex(), true); // short suit-up: plates fly on, faceplate, eyes, HUD boot
}

void UCareerInput::RefreshPilotLook()
{
	if (OnFootPilot) { if (const UCareerSubsystem* C = UCareerSubsystem::Get(this)) { OnFootPilot->ApplyConfig(C->GetAvatar(), true); } }
	if (AIronManCharacter* Char = Player(this)) { Char->RefreshCareer(); }
}

bool UCareerInput::DeliverVehicle(FName VehicleId)
{
	AIronManCharacter* Char = Player(this);
	const FVehicleDef* D = Career::FindVehicle(VehicleId);
	ADowntownDistrict* Dist = ADowntownDistrict::Find(this);
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!Char || !D || !C || !C->OwnsVehicle(VehicleId)) { return false; }
	if (Driving.IsValid()) { Driving->Exit(); Driving = nullptr; }
	if (Delivered.IsValid()) { Delivered->Destroy(); }
	FTransform Xf;
	if (D->Kind == ECityVehicleKind::Plane)
	{
		if (!Dist) { C->Toast(LOCTEXT("NoStrip", "Planes need the city airstrip"), UiBad); return false; }
		Xf = Dist->GetAirstripStart();
		Xf.AddToTranslation(FVector(0, 0, 150.f));
		C->TravelTo(Xf.GetLocation() + Xf.GetRotation().RotateVector(FVector(-200.f, 600.f, 0.f)), Xf.Rotator().Yaw);
		C->Toast(LOCTEXT("PlaneReady", "Your plane is fuelled on the airstrip - E to board"), UiTeal);
	}
	else
	{
		const FVector Near = Char->GetActorLocation() + Char->GetActorForwardVector() * 600.f;
		Xf = Dist ? Dist->GetRoadsideSpotNear(Near) : FTransform(FRotator(0.f, Char->GetActorRotation().Yaw, 0.f), Near);
		if (D->Kind == ECityVehicleKind::Helicopter || !Dist)
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(Deliver), false, Char);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Near + FVector(0, 0, 3000.f), Near - FVector(0, 0, 20000.f), ECC_Visibility, Q)) { Xf.SetLocation(Hit.ImpactPoint + FVector(0, 0, 200.f)); }
		}
		else { Xf.AddToTranslation(FVector(0, 0, 100.f)); }
		C->Toast(FText::Format(LOCTEXT("Delivered", "{0} delivered - E to get in"), FText::FromString(D->Name)), UiTeal);
	}
	Delivered = ACityVehicle::SpawnVehicle(GetWorld(), VehicleId, Xf);
	return Delivered.IsValid();
}

const FCityPoi* UCareerInput::NearestPoi(float& OutDist) const
{
	const ADowntownDistrict* Dist = ADowntownDistrict::Find(this);
	const AIronManCharacter* Char = Player(this);
	OutDist = 1e12f;
	if (!Dist || !Char || !Dist->IsDistrictVisible()) { return nullptr; }
	const FCityPoi* Best = nullptr;
	for (const FCityPoi& P : Dist->GetPois())
	{
		if (P.Radius <= 0.f) { continue; }
		const float D = FVector::Dist(Dist->PoiWorld(P), Char->GetActorLocation());
		if (D < P.Radius && D < OutDist) { OutDist = D; Best = &P; }
	}
	return Best;
}

void UCareerInput::TickInteract()
{
	InteractKind = 0;
	FText Prompt;
	AIronManCharacter* Char = Player(this);
	UCareerSubsystem* C = UCareerSubsystem::Get(this);
	if (!Char || !C) { if (HUD) { HUD->Prompt = FText::GetEmpty(); } return; }
	if (Delivered.IsValid() && FVector::Dist(Delivered->GetActorLocation(), Char->GetActorLocation()) < 650.f)
	{
		InteractKind = 1;
		Prompt = LOCTEXT("PEnter", "[E] get in");
	}
	float Dd = 0.f;
	if (InteractKind == 0)
	{
		if (const FCityPoi* P = NearestPoi(Dd))
		{
			InteractKind = 2;
			InteractId = P->Type;
			InteractParam = P->Game;
			const bool bShop = P->Type == FName(TEXT("barber")) || P->Type == FName(TEXT("clothing")) || P->Type == FName(TEXT("dealership"));
			const bool bHours = bShop && !CareerEconomy::IsShopOpenAt(C->GetTimeOfDay());
			Prompt = Char->IsFlying() ? FText::Format(LOCTEXT("PLand", "Land to use {0}"), FText::FromString(P->Name))
				: bHours ? FText::Format(LOCTEXT("PClosed", "{0} closed (opens {1}:00)"), FText::FromString(P->Name), FText::AsNumber(int32(CareerEconomy::ShopOpenHour())))
				: (bShop && P->Type != FName(TEXT("dealership")) && !bSuitOff) ? FText::Format(LOCTEXT("PSuitOff", "[O] take the suit off to enter {0}"), FText::FromString(P->Name))
				: FText::Format(LOCTEXT("PUse", "[E] {0}"), FText::FromString(P->Name));
		}
	}
	if (InteractKind == 0)
	{
		if (const ADowntownDistrict* Dist = ADowntownDistrict::Find(this))
		{
			for (const ACityHouse* H : Dist->GetHouses())
			{
				if (!H || FVector::Dist(H->GetSpawnTransform().GetLocation(), Char->GetActorLocation()) > 1500.f) { continue; }
				const FCityHouseDef* D = Career::FindHouse(H->GetHouseId());
				if (!D) { continue; }
				InteractKind = 3;
				InteractId = D->Id;
				Prompt = C->OwnsHouse(D->Id) ? (C->GetHomeHouse() == D->Id ? FText::Format(LOCTEXT("PAtHome", "{0} (home)"), FText::FromString(D->Name)) : FText::Format(LOCTEXT("PHome", "[E] set {0} as home"), FText::FromString(D->Name)))
					: (D->IncomePerMinute < 0
						? FText::Format(LOCTEXT("PLease", "[E] lease {0} for {1} (rent {2}/min)"), FText::FromString(D->Name), Cr(D->Price), Cr(-D->IncomePerMinute))
						: FText::Format(LOCTEXT("PBuyHouse", "[E] buy {0} for {1}"), FText::FromString(D->Name), Cr(D->Price)));
				break;
			}
		}
	}
	if (HUD) { HUD->Prompt = Prompt; }
	APlayerController* P = PC();
	if (!P || InteractKind == 0 || !P->WasInputKeyJustPressed(EKeys::E)) { return; }
	if (InteractKind == 1 && Delivered.IsValid())
	{
		if (bSuitOff) { SetSuitOff(false); }
		if (Delivered->Enter(Char)) { Driving = Delivered; if (HUD) { HUD->Prompt = FText::GetEmpty(); } }
	}
	else if (InteractKind == 2 && !Char->IsFlying())
	{
		const bool bShopPoi = InteractId == FName(TEXT("barber")) || InteractId == FName(TEXT("clothing")) || InteractId == FName(TEXT("dealership"));
		if (bShopPoi && !CareerEconomy::IsShopOpenAt(C->GetTimeOfDay()))
		{
			C->Toast(FText::Format(LOCTEXT("ShopClosedToast", "Closed until {0}:00"), FText::AsNumber(int32(CareerEconomy::ShopOpenHour()))), FLinearColor(1.f, 0.55f, 0.35f));
		}
		else if (InteractId == FName(TEXT("casino_game"))) { OpenScreen(ECareerScreen::Casino, InteractParam); }
		else if (InteractId == FName(TEXT("barber")) && bSuitOff) { OpenScreen(ECareerScreen::Barber); }
		else if (InteractId == FName(TEXT("clothing")) && bSuitOff) { OpenScreen(ECareerScreen::Clothing); }
		else if (InteractId == FName(TEXT("dealership"))) { OpenScreen(ECareerScreen::Shop, 3); }
	}
	else if (InteractKind == 3)
	{
		if (C->OwnsHouse(InteractId)) { C->SetHomeHouse(InteractId); } else { C->BuyHouse(InteractId); }
	}
}

void UCareerInput::TickVehicle(float DeltaTime)
{
	ACityVehicle* V = Driving.Get();
	APlayerController* P = PC();
	if (!V || !P) { return; }
	auto Ax = [P](const FKey& Pos, const FKey& Neg) { return (P->IsInputKeyDown(Pos) ? 1.f : 0.f) - (P->IsInputKeyDown(Neg) ? 1.f : 0.f); };
	float Mx = 0.f, My = 0.f;
	P->GetInputMouseDelta(Mx, My);
	const bool bAir = V->GetKind() == ECityVehicleKind::Plane || V->GetKind() == ECityVehicleKind::Helicopter;
	// Q/E are yaw in the air; E alone (tap) gets out - use X / Q for rudder so E stays the exit key.
	V->SetInput(Ax(EKeys::W, EKeys::S), Ax(EKeys::D, EKeys::A), Ax(EKeys::SpaceBar, EKeys::LeftControl), Ax(EKeys::X, EKeys::Q),
		!bAir && P->IsInputKeyDown(EKeys::SpaceBar), Ax(EKeys::LeftShift, EKeys::LeftControl), Mx);
	if (P->WasInputKeyJustPressed(EKeys::E)) { V->Exit(); Driving = nullptr; }
	else if (P->WasInputKeyJustPressed(EKeys::Escape) || P->WasInputKeyJustPressed(EKeys::P))
	{
		if (AArenaPlayerController* APC = Cast<AArenaPlayerController>(P)) { APC->TogglePauseMenu(); }
	}
}

void UCareerInput::TickKeys()
{
	APlayerController* P = PC();
	if (!P) { return; }
	AArenaPlayerController* APC = Cast<AArenaPlayerController>(P);
	if (APC && APC->IsMenuOpen()) { return; }
	if (IsScreenOpen() && GetOpenScreen() != ECareerScreen::DevBar) { return; }
	if (IsDevMenuAllowed() && (P->WasInputKeyJustPressed(EKeys::F1) || P->WasInputKeyJustPressed(EKeys::Tilde)))
	{
		if (GetOpenScreen() == ECareerScreen::DevBar) { CloseScreen(); } else { OpenScreen(ECareerScreen::DevBar); }
		return;
	}
	if (GetOpenScreen() == ECareerScreen::DevBar) { return; }
	if (P->WasInputKeyJustPressed(EKeys::U)) { OpenScreen(ECareerScreen::Shop); return; }
	if (P->WasInputKeyJustPressed(EKeys::K)) { OpenScreen(ECareerScreen::Avatar); return; }
	if (P->WasInputKeyJustPressed(EKeys::N)) { OpenScreen(ECareerScreen::Phone); return; }
	if (P->WasInputKeyJustPressed(EKeys::M) && HUD) { HUD->bShowMarkers = !HUD->bShowMarkers; }
	if (P->WasInputKeyJustPressed(Key(TEXT("SuitExit"), EKeys::O)) && !Driving.IsValid()) { if (bSuitOff) { RequestSuitUp(); } else { SetSuitOff(true); } }
	if (bSuitOff && Parked && Parked->HasArrived()) { RequestSuitUp(); }
}

void UCareerInput::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* P = PC();
	if (!P || !P->IsLocalController()) { return; }
	// First launch: the character creator opens straight over the title screen (before any mode is picked).
	if (!bFirstLaunchShown && !IsScreenOpen())
	{
		const AArenaPlayerController* TPC = Cast<AArenaPlayerController>(P);
		const UCareerSubsystem* CS = UCareerSubsystem::Get(this);
		if (TPC && TPC->IsMenuOpen() && TPC->GetLastMenuMode() == EMenuMode::Title && CS && CS->NeedsCharacterSetup() && !IsHeadlessArenaSmoke())
		{
			bFirstLaunchShown = true;
			UE_LOG(LogTemp, Log, TEXT("Career: no character yet - opening the first-launch creator on the title screen"));
			OpenScreen(ECareerScreen::FirstLaunch);
		}
	}
	{
		const AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr;
		const AArenaPlayerController* APC = Cast<AArenaPlayerController>(P);
		const bool bPlaying = GM && GM->GetArenaState() == EArenaGameState::Playing && !(APC && APC->IsMenuOpen());
		if (bPlaying && !bRunActive) { bRunActive = true; OnFreePlayStarted(); }
		else if (GM && GM->GetArenaState() != EArenaGameState::Playing) { bRunActive = false; }
	}
	if (AIronManCharacter* SupChar = const_cast<AIronManCharacter*>(Player(this)))
	{
		if (!USuitSuperComponent::Get(SupChar))
		{
			USuitSuperComponent* Sup = NewObject<USuitSuperComponent>(SupChar, TEXT("SuitSuper"));
			Sup->RegisterComponent();
		}
	}
	if (UGameplayStatics::IsGamePaused(this)) { return; }
	if (OnFootPilot)
	{
		if (const AIronManCharacter* Char = Player(this)) { OnFootPilot->SetMoveSpeed(Char->GetVelocity().Size2D()); }
	}
	if (Driving.IsValid()) { TickVehicle(DeltaTime); }
	else { TickKeys(); TickInteract(); }
}

#undef LOCTEXT_NAMESPACE
