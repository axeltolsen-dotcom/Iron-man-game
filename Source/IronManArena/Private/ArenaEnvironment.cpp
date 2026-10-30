#include "ArenaEnvironment.h"
#include "ContribLighting.h"

#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"

AArenaEnvironment::AArenaEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	using namespace ArenaVisuals;
	const FLinearColor Concrete(0.09f, 0.09f, 0.1f);
	const FLinearColor Steel(0.22f, 0.23f, 0.26f);
	const FLinearColor Glow(0.3f, 0.85f, 1.f);

	auto MakeSolid = [this](UStaticMeshComponent* C)
	{
		// AddPart disables collision; arena geometry must block (WorldStatic, BlockAll).
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->SetCanEverAffectNavigation(true);
	};

	// Floor: top surface at the actor's Z.
	const float FloorScale = FloorHalfSize * 2.f / 100.f;
	Floor = AddPart(this, Root, TEXT("Floor"), EArenaShape::Cube, FVector(0.f, 0.f, -50.f),
		FVector(FloorScale, FloorScale, 1.f), FRotator::ZeroRotator, Concrete, 0.f, Parts, EArenaSurface::Concrete);
	MakeSolid(Floor);
	// Textured concrete takes the colour as a tint: keep it close to white.
	Parts.Last().Color = FLinearColor(0.8f, 0.8f, 0.82f);
	const int32 FirstDressing = Parts.Num();

	// Glowing grid lines on the floor (purely visual).
	for (int32 i = -4; i <= 4; ++i)
	{
		const float Off = i * 2000.f;
		AddPart(this, Root, *FString::Printf(TEXT("GridX_%d"), i + 4), EArenaShape::Cube, FVector(Off, 0.f, 1.f),
			FVector(0.08f, FloorScale, 0.02f), FRotator::ZeroRotator, Glow * 0.4f, 2.f, Parts);
		AddPart(this, Root, *FString::Printf(TEXT("GridY_%d"), i + 4), EArenaShape::Cube, FVector(0.f, Off, 1.f),
			FVector(FloorScale, 0.08f, 0.02f), FRotator::ZeroRotator, Glow * 0.4f, 2.f, Parts);
	}

	// Ring of pylons marking the arena edge (~7000 uu, matches AIronManCharacter::ArenaBoundaryRadius).
	const int32 NumPylons = 16;
	for (int32 i = 0; i < NumPylons; ++i)
	{
		const float A = (2.f * PI * i) / NumPylons;
		const FVector P(FMath::Cos(A) * 7200.f, FMath::Sin(A) * 7200.f, 0.f);
		UStaticMeshComponent* Pylon = AddPart(this, Root, *FString::Printf(TEXT("Pylon_%d"), i), EArenaShape::Cylinder,
			P + FVector(0.f, 0.f, 600.f), FVector(2.f, 2.f, 12.f), FRotator::ZeroRotator, Steel, 0.f, Parts);
		MakeSolid(Pylon);
		Props.Add(Pylon);
		AddPart(this, Root, *FString::Printf(TEXT("PylonLight_%d"), i), EArenaShape::Cylinder,
			P + FVector(0.f, 0.f, 1220.f), FVector(2.2f, 2.2f, 0.4f), FRotator::ZeroRotator, ArenaVisuals::SuitGold, 6.f, Parts);
	}

	// Cover blocks (fixed layout so the arena is the same every run).
	const FVector CoverSpots[] = {
		FVector(1800, 600, 0), FVector(-1500, 1900, 0), FVector(-2200, -1200, 0), FVector(900, -2500, 0),
		FVector(3600, 2600, 0), FVector(-3800, 200, 0), FVector(400, 4200, 0), FVector(2800, -3900, 0),
		FVector(-2600, -4200, 0), FVector(4700, -700, 0), FVector(-4600, 3900, 0), FVector(5200, 4300, 0)
	};
	int32 Index = 0;
	for (const FVector& Spot : CoverSpots)
	{
		const float H = 1.5f + (Index % 3) * 1.2f;
		const float Yaw = Index * 37.f;
		UStaticMeshComponent* Block = AddPart(this, Root, *FString::Printf(TEXT("Cover_%d"), Index), EArenaShape::Cube,
			Spot + FVector(0.f, 0.f, H * 50.f), FVector(3.f, 1.5f, H), FRotator(0.f, Yaw, 0.f), Steel, 0.f, Parts);
		MakeSolid(Block);
		Props.Add(Block);
		AddPart(this, Root, *FString::Printf(TEXT("CoverTrim_%d"), Index), EArenaShape::Cube,
			Spot + FVector(0.f, 0.f, H * 100.f + 3.f), FVector(3.05f, 1.55f, 0.06f), FRotator(0.f, Yaw, 0.f), Glow, 4.f, Parts);
		++Index;
	}

	for (int32 i = FirstDressing; i < Parts.Num(); ++i) { DressingParts.Add(i); }

	// --- Lighting (all Movable: nothing to bake) ---
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-35.f, 40.f, 0.f));
	Sun->SetIntensity(120000.f); // lux, physical sun at the top of the atmosphere
	Sun->SetAtmosphereSunLight(true);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true; // captures the sky atmosphere, no cubemap needed
	SkyLight->SetIntensity(1.2f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.01f);
	Fog->SetVolumetricFog(true);

	// Second, dim cool light for night scenes (moon); intensity 0 = off.
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetRelativeRotation(FRotator(-55.f, 220.f, 0.f));
	Moon->SetIntensity(0.f);
	Moon->SetLightColor(FLinearColor(0.6f, 0.7f, 1.f));
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetCastShadows(false);

	// Volumetric clouds (engine default cloud material, cooked via DefaultGame.ini).
	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 0.f; // cinematic cameras / chapter volumes can override with higher priority
}

AArenaEnvironment* AArenaEnvironment::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) { return nullptr; }
	TActorIterator<AArenaEnvironment> It(World);
	return It ? *It : nullptr;
}

void AArenaEnvironment::SetArenaDressingVisible(bool bVisible)
{
	for (const int32 Index : DressingParts)
	{
		if (!Parts.IsValidIndex(Index)) { continue; }
		if (UStaticMeshComponent* C = Parts[Index].Component)
		{
			C->SetVisibility(bVisible);
			// Pylons / cover are solid in free play; hidden ones must not block.
			const bool bSolidPart = Props.Contains(C);
			if (bSolidPart) { C->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision); }
		}
	}
}

void AArenaEnvironment::SetFloorSurface(EArenaSurface Surface, const FLinearColor& Tint)
{
	if (!Floor || Parts.Num() == 0 || Parts[0].Component != Floor) { return; }
	FArenaPaintedPart& P = Parts[0];
	P.Surface = Surface;
	P.Color = Tint;
	if (UMaterialInterface* M = ArenaVisuals::GetSurfaceMaterial(ArenaVisuals::ResolveSurface(P)))
	{
		P.MID = UMaterialInstanceDynamic::Create(M, this);
		ArenaVisuals::RepaintPart(P);
		Floor->SetMaterial(0, P.MID);
	}
}

void AArenaEnvironment::ApplyPreset(const FEnvironmentPreset& In)
{
	Preset = In;
	if (Sun)
	{
		Sun->SetRelativeRotation(FRotator(In.SunPitch, In.SunYaw, 0.f));
		Sun->SetIntensity(In.SunLux);
		Sun->SetUseTemperature(true);
		Sun->SetTemperature(In.SunKelvin);
		Sun->SetLightColor(FLinearColor::White);
	}
	if (Moon)
	{
		Moon->SetIntensity(In.MoonLux);
		Moon->SetVisibility(In.MoonLux > 0.f);
	}
	if (SkyLight)
	{
		UTextureCube* Cube = nullptr;
		if (In.bHdriSkyLight)
		{
			static const TCHAR* HdriPackage = TEXT("/Game/Realism/Textures/hdri/T_industrial_sunset_02_puresky");
			if (FPackageName::DoesPackageExist(HdriPackage))
			{
				Cube = LoadObject<UTextureCube>(nullptr, TEXT("/Game/Realism/Textures/hdri/T_industrial_sunset_02_puresky.T_industrial_sunset_02_puresky"));
			}
		}
		if (Cube)
		{
			SkyLight->bRealTimeCapture = false;
			SkyLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
			SkyLight->SetCubemap(Cube);
		}
		else
		{
			SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
			SkyLight->bRealTimeCapture = true;
		}
		SkyLight->SetIntensity(In.SkyLightIntensity);
		SkyLight->RecaptureSky();
	}
	if (Clouds)
	{
		Clouds->SetVisibility(In.bClouds);
		Clouds->SetLayerBottomAltitude(In.CloudBottomKm);
		Clouds->SetLayerHeight(In.CloudHeightKm);
	}
	if (Fog)
	{
		Fog->SetFogDensity(In.FogDensity);
		Fog->SetFogHeightFalloff(In.FogHeightFalloff);
		Fog->SetFogInscatteringColor(In.FogColor);
		Fog->SetVolumetricFog(In.bVolumetricFog);
		Fog->SetVolumetricFogExtinctionScale(In.VolumetricFogExtinction);
		Fog->SetVolumetricFogAlbedo(In.VolumetricFogAlbedo.ToFColor(true));
	}
	if (PostProcess)
	{
		FPostProcessSettings& S = PostProcess->Settings;
		S = FPostProcessSettings();
		S.bOverride_AutoExposureMethod = true;      S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		S.bOverride_AutoExposureBias = true;        S.AutoExposureBias = In.ExposureBias;
		S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = In.ExposureMinEV100; // EV100 (extended range)
		S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = In.ExposureMaxEV100;
		S.bOverride_AutoExposureSpeedUp = true;     S.AutoExposureSpeedUp = 2.5f;
		S.bOverride_AutoExposureSpeedDown = true;   S.AutoExposureSpeedDown = 1.2f;
		S.bOverride_BloomIntensity = true;          S.BloomIntensity = In.BloomIntensity;
		S.bOverride_BloomThreshold = true;          S.BloomThreshold = -1.f; // physically based (energy conserving)
		S.bOverride_LensFlareIntensity = true;      S.LensFlareIntensity = In.LensFlareIntensity;
		S.bOverride_LensFlareBokehSize = true;      S.LensFlareBokehSize = 3.f;
		S.bOverride_LensFlareThreshold = true;      S.LensFlareThreshold = 6.f; // repulsors / reactor / sun
		S.bOverride_WhiteTemp = true;               S.WhiteTemp = In.WhiteBalanceKelvin;
		S.bOverride_ColorSaturation = true;         S.ColorSaturation = FVector4(1.f, 1.f, 1.f, In.Saturation);
		S.bOverride_ColorContrast = true;           S.ColorContrast = FVector4(1.f, 1.f, 1.f, In.Contrast);
		S.bOverride_ColorGain = true;               S.ColorGain = FVector4(In.Gain.R, In.Gain.G, In.Gain.B, 1.f);
		S.bOverride_VignetteIntensity = true;       S.VignetteIntensity = In.Vignette;
		S.bOverride_FilmGrainIntensity = true;      S.FilmGrainIntensity = In.FilmGrain;
		S.bOverride_SceneFringeIntensity = true;    S.SceneFringeIntensity = In.ChromaticAberration;
		S.bOverride_MotionBlurAmount = true;        S.MotionBlurAmount = 0.5f;
		S.bOverride_MotionBlurMax = true;           S.MotionBlurMax = 5.f;
		S.bOverride_DynamicGlobalIlluminationMethod = true; S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		S.bOverride_ReflectionMethod = true;        S.ReflectionMethod = EReflectionMethod::Lumen;
		S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
		S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.f;
		S.bOverride_LumenReflectionQuality = true;  S.LumenReflectionQuality = 1.f;
		S.bOverride_LumenMaxTraceDistance = true;   S.LumenMaxTraceDistance = 20000.f;
	}
}

void AArenaEnvironment::BeginPlay()
{
	Super::BeginPlay();
	ArenaVisuals::PaintAll(this, Parts);
	ApplyArenaLook();
}

void AArenaEnvironment::ApplyArenaLook()
{
	ApplyPreset(EnvironmentPresets::Arena());
	// Free play has no story look; Vison recommends reusing "city" (clear 13:00 daylight).
	if (ContribLighting::HasPreset(TEXT("city"))) { ContribLighting::Apply(this, TEXT("city")); }
}

void AArenaEnvironment::RemoveLighting()
{
	for (UActorComponent* C : TArray<UActorComponent*>{ Sun.Get(), SkyLight.Get(), Atmosphere.Get(), Fog.Get(), Moon.Get(), Clouds.Get() })
	{
		if (C) { C->DestroyComponent(); }
	}
	Sun = nullptr; SkyLight = nullptr; Atmosphere = nullptr; Fog = nullptr; Moon = nullptr; Clouds = nullptr;
}

void AArenaEnvironment::RemoveGeometry()
{
	for (FArenaPaintedPart& P : Parts)
	{
		if (P.Component) { P.Component->DestroyComponent(); }
	}
	Parts.Empty();
	Props.Empty();
	DressingParts.Empty();
	Floor = nullptr;
}
