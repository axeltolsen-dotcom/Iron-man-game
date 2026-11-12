#include "WorkshopEasterEggs.h"
#include "SuitReactiveState.h"

#include "IronManArena.h"
#include "IronManCharacter.h"
#include "ArenaAudio.h"
#include "ArenaGameMode.h"
#include "CareerSubsystem.h"
#include "StoryAutopilot.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define LOCTEXT_NAMESPACE "WorkshopEasterEggs"

using namespace ArenaVisuals;

namespace
{
	// Local layout (cm). +X faces the open side of the nook (it is spawned facing the player), the back wall is at -X.
	constexpr float WorkshopDockX = -215.f;
	float WorkshopDockY(int32 Index) { return -420.f + Index * 55.f; }
	const FVector WorkshopBayCentre(-150.f, -230.f, 0.f);
	const FVector WorkshopPinboardPoint(-150.f, 110.f, 0.f);
	const FVector WorkshopTrayPoint(-130.f, 275.f, 0.f);
	const FVector WorkshopCarCentre(210.f, -240.f, 0.f);
	const FVector WorkshopExtHome(30.f, 440.f, 0.f);
	const FVector WorkshopDuckHome(-178.f, 245.f, 93.f);
	// Footprint of the floor plate: X -290..370, Y -480..480.
	const FVector WorkshopFloorCentre(40.f, 0.f, 0.f);
	const FVector WorkshopHalfExtent(330.f, 480.f, 150.f);

	const FLinearColor WorkshopUltronRed(1.f, 0.07f, 0.04f);
	const FLinearColor WorkshopToastTeal(0.3f, 0.95f, 0.9f);
	const FLinearColor WorkshopToastGold(1.f, 0.78f, 0.3f);
	const FLinearColor WorkshopToastDim(0.75f, 0.78f, 0.82f);
	const FLinearColor WorkshopPlateDark(0.32f, 0.33f, 0.35f);

	struct FWorkshopBotDef
	{
		const TCHAR* Plate;
		FLinearColor Color;
	};
	const FWorkshopBotDef WorkshopBots[WorkshopEggs::NumDocks] = {
		{ TEXT("DUM-E"), FLinearColor(1.f, 0.72f, 0.3f) },
		{ TEXT("U"), FLinearColor(0.95f, 0.55f, 0.22f) },
		{ TEXT("JARVIS"), FLinearColor(0.35f, 0.7f, 1.f) },
		{ TEXT("FRIDAY"), FLinearColor(0.2f, 0.95f, 0.85f) },
		{ TEXT("EDITH"), FLinearColor(0.75f, 0.45f, 1.f) },
		{ TEXT("VISON"), FLinearColor(1.f, 0.85f, 0.3f) },
		{ TEXT("ULTRON"), FLinearColor(1.f, 0.55f, 0.08f) }, // settles amber after the red flicker
		{ TEXT("JOCASTA - offline"), FLinearColor(0.7f, 0.85f, 1.f) },
	};

	const TCHAR* const WorkshopNotes[WorkshopEggs::NumNotes] = {
		TEXT("Don't let Dum-E near the extinguisher. -J"),
		TEXT("Thruster recalibration: still 'spicy'. -F"),
		TEXT("Mirror Lock solutions <= 3 moves. Trust Jocasta."),
		TEXT("PhotorealGrade=1. No lens tricks. -V"),
		TEXT("Efficiency is not optional. Also, please tidy the bolts. -U"),
	};

	UCareerSubsystem* WorkshopCareer(const UObject* Ctx) { return UCareerSubsystem::Get(Ctx); }

	void WorkshopToast(const UObject* Ctx, const FText& Text, const FLinearColor& Col)
	{
		if (UCareerSubsystem* C = WorkshopCareer(Ctx)) { C->Toast(Text, Col); }
	}
}

// ============================================================================ construction

AWorkshopEasterEggs::AWorkshopEasterEggs()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	NookRoot = CreateDefaultSubobject<USceneComponent>(TEXT("NookRoot"));
	SetRootComponent(NookRoot);

	const FLinearColor FloorCol(0.09f, 0.09f, 0.1f);
	const FLinearColor WallCol(0.17f, 0.17f, 0.18f);
	const FLinearColor SteelCol(0.32f, 0.32f, 0.34f);
	const FLinearColor DarkCol(0.05f, 0.05f, 0.06f);
	const FLinearColor WoodCol(0.42f, 0.28f, 0.16f);

	// ---- shell: floor plate, back wall, side wall with a door frame
	AddPart(this, NookRoot, TEXT("Floor"), EArenaShape::Cube, WorkshopFloorCentre + FVector(0, 0, 2), FVector(6.6f, 9.6f, 0.04f), FRotator::ZeroRotator,
		FloorCol, 0.f, Parts, EArenaSurface::HangarFloor);
	AddPart(this, NookRoot, TEXT("BackWall"), EArenaShape::Cube, FVector(-280, 0, 140), FVector(0.2f, 9.6f, 2.8f), FRotator::ZeroRotator,
		WallCol, 0.f, Parts, EArenaSurface::BlockWall);
	AddPart(this, NookRoot, TEXT("SideWall"), EArenaShape::Cube, FVector(-120, 475, 140), FVector(3.4f, 0.15f, 2.8f), FRotator::ZeroRotator,
		WallCol, 0.f, Parts, EArenaSurface::BlockWall);
	AddPart(this, NookRoot, TEXT("DoorPostA"), EArenaShape::Cube, FVector(70, 475, 120), FVector(0.15f, 0.2f, 2.4f), FRotator::ZeroRotator,
		SteelCol, 0.f, Parts, EArenaSurface::MetalPlate);
	AddPart(this, NookRoot, TEXT("DoorPostB"), EArenaShape::Cube, FVector(260, 475, 120), FVector(0.15f, 0.2f, 2.4f), FRotator::ZeroRotator,
		SteelCol, 0.f, Parts, EArenaSurface::MetalPlate);
	AddPart(this, NookRoot, TEXT("DoorLintel"), EArenaShape::Cube, FVector(165, 475, 245), FVector(2.05f, 0.2f, 0.15f), FRotator::ZeroRotator,
		SteelCol, 0.f, Parts, EArenaSurface::MetalPlate);

	// ---- #2 Bot Bay: 8 charging docks along the left half of the back wall
	Docks.SetNum(WorkshopEggs::NumDocks);
	for (int32 i = 0; i < WorkshopEggs::NumDocks; ++i)
	{
		const float Y = WorkshopDockY(i);
		FWorkshopDock& Dk = Docks[i];
		Dk.LitColor = WorkshopBots[i].Color;
		AddPart(this, NookRoot, FName(*FString::Printf(TEXT("DockPedestal%d"), i)), EArenaShape::Cylinder, FVector(WorkshopDockX, Y, 30), FVector(0.42f, 0.42f, 0.6f),
			FRotator::ZeroRotator, SteelCol, 0.f, Parts, EArenaSurface::MetalPlate);
		Dk.PadPart = Parts.Num();
		AddPart(this, NookRoot, FName(*FString::Printf(TEXT("DockPad%d"), i)), EArenaShape::Cylinder, FVector(WorkshopDockX, Y, 61), FVector(0.36f, 0.36f, 0.02f),
			FRotator::ZeroRotator, WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);

		switch (i)
		{
		case 0: // Dum-E: base + waving arm + glowing claw
		{
			AddPart(this, NookRoot, TEXT("DumEBase"), EArenaShape::Cylinder, FVector(WorkshopDockX, Y, 68), FVector(0.2f, 0.2f, 0.12f), FRotator::ZeroRotator,
				DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
			DumEArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DumEArmPivot"));
			DumEArmPivot->SetupAttachment(NookRoot);
			DumEArmPivot->SetRelativeLocation(FVector(WorkshopDockX, Y, 74));
			AddPart(this, DumEArmPivot, TEXT("DumEArm"), EArenaShape::Cube, FVector(0, 0, 20), FVector(0.05f, 0.05f, 0.4f), FRotator::ZeroRotator,
				SteelCol, 0.f, Parts, EArenaSurface::RobotMetal);
			Dk.BotPart = Parts.Num();
			AddPart(this, DumEArmPivot, TEXT("DumEClaw"), EArenaShape::Cube, FVector(0, 0, 42), FVector(0.1f, 0.1f, 0.06f), FRotator::ZeroRotator,
				WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);
			break;
		}
		case 1: // U: a stiffer arm, leaning forward
			AddPart(this, NookRoot, TEXT("UBase"), EArenaShape::Cylinder, FVector(WorkshopDockX, Y, 68), FVector(0.2f, 0.2f, 0.12f), FRotator::ZeroRotator,
				DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
			AddPart(this, NookRoot, TEXT("UArm"), EArenaShape::Cube, FVector(WorkshopDockX + 8.f, Y, 92), FVector(0.05f, 0.05f, 0.4f), FRotator(-25.f, 0.f, 0.f),
				SteelCol, 0.f, Parts, EArenaSurface::RobotMetal);
			Dk.BotPart = Parts.Num();
			AddPart(this, NookRoot, TEXT("UClaw"), EArenaShape::Cube, FVector(WorkshopDockX + 17.f, Y, 110), FVector(0.09f, 0.09f, 0.06f), FRotator::ZeroRotator,
				WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);
			break;
		case 2: // Jarvis: a tablet on a stand
			AddPart(this, NookRoot, TEXT("JarvisStand"), EArenaShape::Cube, FVector(WorkshopDockX - 4.f, Y, 70), FVector(0.04f, 0.04f, 0.16f), FRotator::ZeroRotator,
				DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
			Dk.BotPart = Parts.Num();
			AddPart(this, NookRoot, TEXT("JarvisTablet"), EArenaShape::Cube, FVector(WorkshopDockX, Y, 86), FVector(0.03f, 0.32f, 0.22f), FRotator(15.f, 0.f, 0.f),
				WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);
			break;
		case 3: // Friday: a teal orb
			Dk.BotPart = Parts.Num();
			AddPart(this, NookRoot, TEXT("FridayOrb"), EArenaShape::Sphere, FVector(WorkshopDockX, Y, 76), FVector(0.2f), FRotator::ZeroRotator,
				WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);
			break;
		case 4: // Edith: a visor on a post
			AddPart(this, NookRoot, TEXT("EdithPost"), EArenaShape::Cube, FVector(WorkshopDockX, Y, 70), FVector(0.04f, 0.04f, 0.14f), FRotator::ZeroRotator,
				DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
			Dk.BotPart = Parts.Num();
			AddPart(this, NookRoot, TEXT("EdithVisor"), EArenaShape::Cube, FVector(WorkshopDockX, Y, 80), FVector(0.07f, 0.34f, 0.07f), FRotator::ZeroRotator,
				WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);
			break;
		case 5: // Vison: a cone body with a gem
			AddPart(this, NookRoot, TEXT("VisonBody"), EArenaShape::Cone, FVector(WorkshopDockX, Y, 74), FVector(0.2f, 0.2f, 0.24f), FRotator::ZeroRotator,
				FLinearColor(0.45f, 0.3f, 0.5f), 0.f, Parts, EArenaSurface::RobotMetal);
			Dk.BotPart = Parts.Num();
			AddPart(this, NookRoot, TEXT("VisonGem"), EArenaShape::Cube, FVector(WorkshopDockX + 9.f, Y, 80), FVector(0.06f), FRotator(45.f, 45.f, 0.f),
				WorkshopBots[i].Color, 0.05f, Parts, EArenaSurface::Glow);
			break;
		case 6: // Ultron: a sphere
			Dk.BotPart = Parts.Num();
			AddPart(this, NookRoot, TEXT("UltronSphere"), EArenaShape::Sphere, FVector(WorkshopDockX, Y, 78), FVector(0.26f), FRotator::ZeroRotator,
				WorkshopUltronRed, 0.05f, Parts, EArenaSurface::Glow);
			break;
		default: // Jocasta: an empty, dark dock
			break;
		}

		UTextRenderComponent* PlateText = CreateDefaultSubobject<UTextRenderComponent>(FName(*FString::Printf(TEXT("DockPlate%d"), i)));
		PlateText->SetupAttachment(NookRoot);
		PlateText->SetRelativeLocation(FVector(WorkshopDockX + 22.f, Y, 38));
		PlateText->SetHorizontalAlignment(EHTA_Center);
		PlateText->SetVerticalAlignment(EVRTA_TextCenter);
		PlateText->SetWorldSize(i == WorkshopEggs::NumDocks - 1 ? 3.6f : 6.5f);
		PlateText->SetText(FText::AsCultureInvariant(WorkshopBots[i].Plate));
		PlateText->SetTextRenderColor(WorkshopPlateDark.ToFColor(true));
		PlateText->SetCastShadow(false);
		DockPlates.Add(PlateText);

		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(FName(*FString::Printf(TEXT("DockLight%d"), i)));
		L->SetupAttachment(NookRoot);
		L->SetRelativeLocation(FVector(WorkshopDockX + 45.f, Y, 110));
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(0.f);
		L->SetAttenuationRadius(170.f);
		L->SetLightColor(WorkshopBots[i].Color);
		L->SetCastShadows(false);
		DockLights.Add(L);
	}
	JarvisGreeting = CreateDefaultSubobject<UTextRenderComponent>(TEXT("JarvisGreeting"));
	JarvisGreeting->SetupAttachment(NookRoot);
	JarvisGreeting->SetRelativeLocation(FVector(WorkshopDockX + 10.f, WorkshopDockY(2), 128));
	JarvisGreeting->SetHorizontalAlignment(EHTA_Center);
	JarvisGreeting->SetVerticalAlignment(EVRTA_TextCenter);
	JarvisGreeting->SetWorldSize(6.f);
	JarvisGreeting->SetText(LOCTEXT("JarvisGreeting", "Good evening, Axel."));
	JarvisGreeting->SetTextRenderColor(WorkshopBots[2].Color.ToFColor(true));
	JarvisGreeting->SetCastShadow(false);

	// ---- #3 Sticky Note Pinboard (back wall, right half) + workbench + #5 tool tray
	AddPart(this, NookRoot, TEXT("PinboardFrame"), EArenaShape::Cube, FVector(-271, 110, 172), FVector(0.03f, 1.8f, 1.2f), FRotator::ZeroRotator,
		DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, NookRoot, TEXT("PinboardCork"), EArenaShape::Cube, FVector(-268, 110, 172), FVector(0.03f, 1.7f, 1.1f), FRotator::ZeroRotator,
		FLinearColor(0.5f, 0.34f, 0.2f), 0.f, Parts, EArenaSurface::Plain);
	{
		const FLinearColor NoteCols[6] = { FLinearColor(1.f, 0.9f, 0.3f), FLinearColor(1.f, 0.5f, 0.7f), FLinearColor(0.45f, 0.85f, 1.f),
			FLinearColor(0.55f, 1.f, 0.5f), FLinearColor(1.f, 0.65f, 0.3f), FLinearColor(0.95f, 0.95f, 0.92f) };
		const FVector2D NotePos[6] = { FVector2D(55, 200), FVector2D(100, 190), FVector2D(150, 205), FVector2D(70, 150), FVector2D(125, 145), FVector2D(165, 160) };
		for (int32 n = 0; n < 6; ++n)
		{
			AddPart(this, NookRoot, FName(*FString::Printf(TEXT("StickyNote%d"), n)), EArenaShape::Cube, FVector(-265.f, NotePos[n].X, NotePos[n].Y),
				FVector(0.01f, 0.3f, 0.3f), FRotator(0.f, 0.f, (n % 3 - 1) * 6.f), NoteCols[n], 0.f, Parts, EArenaSurface::Plain);
		}
	}
	AddPart(this, NookRoot, TEXT("Workbench"), EArenaShape::Cube, FVector(-215, 190, 44), FVector(0.9f, 2.6f, 0.88f), FRotator::ZeroRotator,
		SteelCol, 0.f, Parts, EArenaSurface::MetalPlate);
	AddPart(this, NookRoot, TEXT("WorkbenchTop"), EArenaShape::Cube, FVector(-215, 190, 90), FVector(0.95f, 2.65f, 0.04f), FRotator::ZeroRotator,
		WoodCol, 0.f, Parts, EArenaSurface::Plain);
	AddPart(this, NookRoot, TEXT("ToolTray"), EArenaShape::Cube, FVector(-210, 280, 93), FVector(0.4f, 0.6f, 0.03f), FRotator::ZeroRotator,
		DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, NookRoot, TEXT("WrenchA"), EArenaShape::Cube, FVector(-215, 265, 96), FVector(0.03f, 0.26f, 0.015f), FRotator(0.f, 12.f, 0.f),
		SteelCol, 0.f, Parts, EArenaSurface::Steel);
	AddPart(this, NookRoot, TEXT("WrenchB"), EArenaShape::Cube, FVector(-200, 290, 96), FVector(0.03f, 0.22f, 0.015f), FRotator(0.f, -18.f, 0.f),
		SteelCol, 0.f, Parts, EArenaSurface::Steel);
	AddPart(this, NookRoot, TEXT("MalletHandle"), EArenaShape::Cylinder, FVector(-225, 300, 97), FVector(0.025f, 0.025f, 0.26f), FRotator(0.f, 0.f, 90.f),
		WoodCol, 0.f, Parts, EArenaSurface::Plain);
	AddPart(this, NookRoot, TEXT("MalletHead"), EArenaShape::Cube, FVector(-225, 315, 99), FVector(0.07f, 0.06f, 0.06f), FRotator::ZeroRotator,
		DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);

	// The Calibration Duck (hidden until Dum-E slides it out).
	DuckPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DuckPivot"));
	DuckPivot->SetupAttachment(NookRoot);
	DuckPivot->SetRelativeLocation(WorkshopDuckHome);
	AddPart(this, DuckPivot, TEXT("DuckBody"), EArenaShape::Sphere, FVector(0, 0, 6), FVector(0.16f, 0.12f, 0.11f), FRotator::ZeroRotator,
		FLinearColor(1.f, 0.85f, 0.1f), 0.f, Parts, EArenaSurface::Plain);
	AddPart(this, DuckPivot, TEXT("DuckHead"), EArenaShape::Sphere, FVector(5, 0, 15), FVector(0.09f), FRotator::ZeroRotator,
		FLinearColor(1.f, 0.85f, 0.1f), 0.f, Parts, EArenaSurface::Plain);
	AddPart(this, DuckPivot, TEXT("DuckBeak"), EArenaShape::Cube, FVector(10, 0, 14), FVector(0.05f, 0.03f, 0.015f), FRotator::ZeroRotator,
		FLinearColor(1.f, 0.45f, 0.05f), 0.f, Parts, EArenaSurface::Plain);
	{
		UTextRenderComponent* DuckLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DuckLabel"));
		DuckLabel->SetupAttachment(DuckPivot);
		DuckLabel->SetRelativeLocation(FVector(12, 0, 30));
		DuckLabel->SetHorizontalAlignment(EHTA_Center);
		DuckLabel->SetWorldSize(3.5f);
		DuckLabel->SetText(LOCTEXT("DuckLabel", "Calibration Duck"));
		DuckLabel->SetTextRenderColor(FColor(255, 230, 120));
		DuckLabel->SetCastShadow(false);
	}

	// ---- #1 Eager Extinguisher (on the side wall by the door)
	ExtPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ExtPivot"));
	ExtPivot->SetupAttachment(NookRoot);
	ExtPivot->SetRelativeLocation(WorkshopExtHome);
	AddPart(this, NookRoot, TEXT("ExtBracket"), EArenaShape::Cube, WorkshopExtHome + FVector(0, 25, 75), FVector(0.25f, 0.05f, 0.5f), FRotator::ZeroRotator,
		DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, ExtPivot, TEXT("ExtBody"), EArenaShape::Cylinder, FVector(0, 0, 75), FVector(0.18f, 0.18f, 0.55f), FRotator::ZeroRotator,
		FLinearColor(0.75f, 0.03f, 0.02f), 0.f, Parts, EArenaSurface::SuitPaint);
	AddPart(this, ExtPivot, TEXT("ExtCap"), EArenaShape::Cylinder, FVector(0, 0, 106), FVector(0.08f, 0.08f, 0.08f), FRotator::ZeroRotator,
		DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, ExtPivot, TEXT("ExtNozzle"), EArenaShape::Cylinder, FVector(9, 0, 108), FVector(0.04f, 0.04f, 0.14f), FRotator(90.f, 0.f, 0.f),
		DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
	{
		UTextRenderComponent* ExtLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ExtLabel"));
		ExtLabel->SetupAttachment(ExtPivot);
		ExtLabel->SetRelativeLocation(FVector(10, 0, 80));
		ExtLabel->SetHorizontalAlignment(EHTA_Center);
		ExtLabel->SetVerticalAlignment(EVRTA_TextCenter);
		ExtLabel->SetWorldSize(3.4f);
		ExtLabel->SetText(LOCTEXT("ExtLabel", "DUM-E\nDO NOT\nENGAGE"));
		ExtLabel->SetTextRenderColor(FColor::White);
		ExtLabel->SetCastShadow(false);
	}
	for (int32 f = 0; f < WorkshopEggs::NumFoam; ++f)
	{
		FoamParts.Add(Parts.Num());
		AddPart(this, NookRoot, FName(*FString::Printf(TEXT("Foam%d"), f)), EArenaShape::Sphere, WorkshopExtHome + FVector(0, 0, 108), FVector(0.06f),
			FRotator::ZeroRotator, FLinearColor(0.95f, 0.97f, 1.f), 0.3f, Parts, EArenaSurface::Plain);
	}
	FoamVel.SetNum(WorkshopEggs::NumFoam);

	// ---- #4 Covered Car That Isn't (the yard, front left): three nested tarps over a scrap Mark I
	const FLinearColor TarpCols[WorkshopEggs::NumTarps] = { FLinearColor(0.24f, 0.27f, 0.31f), FLinearColor(0.3f, 0.33f, 0.27f), FLinearColor(0.38f, 0.33f, 0.25f) };
	for (int32 k = 0; k < WorkshopEggs::NumTarps; ++k)
	{
		const float S = 1.f - 0.12f * k;
		USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(FName(*FString::Printf(TEXT("TarpPivot%d"), k)));
		Pivot->SetupAttachment(NookRoot);
		Pivot->SetRelativeLocation(WorkshopCarCentre);
		TarpPivots.Add(Pivot);
		AddPart(this, Pivot, FName(*FString::Printf(TEXT("TarpBody%d"), k)), EArenaShape::Cube, FVector(0, 0, 42.5f * S + 2.f), FVector(1.9f * S, 4.3f * S, 0.85f * S),
			FRotator::ZeroRotator, TarpCols[k], 0.f, Parts, EArenaSurface::Plain);
		AddPart(this, Pivot, FName(*FString::Printf(TEXT("TarpCabin%d"), k)), EArenaShape::Cube, FVector(0, -20.f * S, 85.f * S + 27.f * S), FVector(1.6f * S, 2.2f * S, 0.55f * S),
			FRotator::ZeroRotator, TarpCols[k], 0.f, Parts, EArenaSurface::Plain);
		for (int32 w = 0; w < 4; ++w)
		{
			const FVector WheelLoc(((w & 1) ? 1.f : -1.f) * 92.f * S, ((w & 2) ? 1.f : -1.f) * 140.f * S, 30.f * S);
			AddPart(this, Pivot, FName(*FString::Printf(TEXT("TarpWheel%d_%d"), k, w)), EArenaShape::Cylinder, WheelLoc, FVector(0.6f * S, 0.6f * S, 0.12f * S),
				FRotator(0.f, 0.f, 90.f), TarpCols[k] * 0.8f, 0.f, Parts, EArenaSurface::Plain);
		}
	}
	ScrapPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ScrapPivot"));
	ScrapPivot->SetupAttachment(NookRoot);
	ScrapPivot->SetRelativeLocation(WorkshopCarCentre);
	AddPart(this, ScrapPivot, TEXT("ScrapLegL"), EArenaShape::Cube, FVector(0, -15, 25), FVector(0.18f, 0.18f, 0.5f), FRotator::ZeroRotator, SteelCol, 0.f, Parts, EArenaSurface::RustyMetal);
	AddPart(this, ScrapPivot, TEXT("ScrapLegR"), EArenaShape::Cube, FVector(0, 15, 25), FVector(0.18f, 0.18f, 0.5f), FRotator::ZeroRotator, SteelCol, 0.f, Parts, EArenaSurface::RustyMetal);
	AddPart(this, ScrapPivot, TEXT("ScrapTorso"), EArenaShape::Cube, FVector(0, 0, 78), FVector(0.4f, 0.5f, 0.55f), FRotator::ZeroRotator, WoodCol, 0.f, Parts, EArenaSurface::Plain);
	AddPart(this, ScrapPivot, TEXT("ScrapArmL"), EArenaShape::Cube, FVector(0, -34, 80), FVector(0.15f, 0.15f, 0.45f), FRotator::ZeroRotator, SteelCol, 0.f, Parts, EArenaSurface::RustyMetal);
	AddPart(this, ScrapPivot, TEXT("ScrapArmR"), EArenaShape::Cube, FVector(0, 34, 80), FVector(0.15f, 0.15f, 0.45f), FRotator::ZeroRotator, SteelCol, 0.f, Parts, EArenaSurface::RustyMetal);
	AddPart(this, ScrapPivot, TEXT("ScrapHead"), EArenaShape::Cube, FVector(0, 0, 122), FVector(0.3f, 0.3f, 0.3f), FRotator::ZeroRotator, SteelCol, 0.f, Parts, EArenaSurface::RustyMetal);
	AddPart(this, ScrapPivot, TEXT("ScrapEyes"), EArenaShape::Cube, FVector(15.5f, 0, 126), FVector(0.01f, 0.18f, 0.03f), FRotator::ZeroRotator, DarkCol, 0.f, Parts, EArenaSurface::RobotMetal);
	AddPart(this, ScrapPivot, TEXT("ScrapReactor"), EArenaShape::Cylinder, FVector(20.5f, 0, 88), FVector(0.11f, 0.11f, 0.02f), FRotator(90.f, 0.f, 0.f),
		ArcBlue, 4.f, Parts, EArenaSurface::Glow);
	ScrapLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ScrapLight"));
	ScrapLight->SetupAttachment(ScrapPivot);
	ScrapLight->SetRelativeLocation(FVector(55, 0, 95));
	ScrapLight->SetIntensityUnits(ELightUnits::Lumens);
	ScrapLight->SetIntensity(200.f);
	ScrapLight->SetAttenuationRadius(250.f);
	ScrapLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.55f));
	ScrapLight->SetCastShadows(false);

	// Purely cosmetic: no collision anywhere (waves, projectiles and the spawn probes never see the nook).
	for (FArenaPaintedPart& P : Parts)
	{
		if (!P.Component) { continue; }
		P.Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P.Component->SetGenerateOverlapEvents(false);
		P.Component->SetCastShadow(P.Component->GetRelativeScale3D().GetMax() >= 0.5f);
	}
}

void AWorkshopEasterEggs::BeginPlay()
{
	Super::BeginPlay();
	if (IsSuppressedRun())
	{
		Destroy();
		return;
	}
	PaintAll(this, Parts);
	for (int32 Idx : FoamParts)
	{
		if (Parts.IsValidIndex(Idx) && Parts[Idx].Component) { Parts[Idx].Component->SetVisibility(false); }
	}
	if (DuckPivot) { DuckPivot->SetVisibility(false, true); }
	if (ScrapPivot) { ScrapPivot->SetVisibility(false, true); }
	if (JarvisGreeting) { JarvisGreeting->SetVisibility(false); }
	for (int32 i = 0; i < WorkshopEggs::NumDocks; ++i) { SetDockLit(i, false); }
}

// ============================================================================ static helpers

bool AWorkshopEasterEggs::IsSuppressedRun()
{
	static const bool bHeadlessCmd = FParse::Param(FCommandLine::Get(), TEXT("ArenaSmoke")) || FParse::Param(FCommandLine::Get(), TEXT("StoryAutoplay"));
	return bHeadlessCmd || GIsAutomationTesting || AStoryAutopilot::IsAnyRunning();
}

AWorkshopEasterEggs* AWorkshopEasterEggs::Find(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) { return nullptr; }
	for (TActorIterator<AWorkshopEasterEggs> It(W); It; ++It)
	{
		if (IsValid(*It)) { return *It; }
	}
	return nullptr;
}

float AWorkshopEasterEggs::GetFoamFrost(const UObject* WorldContext)
{
	const AWorkshopEasterEggs* Nook = Find(WorldContext);
	return Nook ? Nook->FoamFrost : 0.f;
}

void AWorkshopEasterEggs::RemoveAll(UWorld* InWorld)
{
	if (!InWorld) { return; }
	for (TActorIterator<AWorkshopEasterEggs> It(InWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: removed for story play."));
			It->Destroy();
		}
	}
}

AWorkshopEasterEggs* AWorkshopEasterEggs::SpawnForFreePlay(UWorld* InWorld, AIronManCharacter* Player)
{
	if (!InWorld || !Player || IsSuppressedRun()) { return nullptr; }
	if (AWorkshopEasterEggs* Existing = Find(InWorld)) { return Existing; }

	const FVector PLoc = Player->GetActorLocation();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(WorkshopNookPlace), false, Player);
	const FCollisionObjectQueryParams FloorObjs(ECC_WorldStatic);
	FCollisionObjectQueryParams BlockObjs;
	BlockObjs.AddObjectTypesToQuery(ECC_WorldStatic);
	BlockObjs.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;
	if (!InWorld->LineTraceSingleByObjectType(Hit, PLoc + FVector(0, 0, 50.f), PLoc - FVector(0, 0, 5000.f), FloorObjs, Q))
	{
		UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: no floor under the player start - not spawned."));
		return nullptr;
	}
	const float BaseZ = Hit.ImpactPoint.Z;
	const float FwdYaw = Player->GetActorRotation().Yaw;
	static const float SpotDists[] = { 900.f, 1300.f, 1800.f, 2500.f, 3200.f };
	static const float SpotAngles[] = { 0.f, 35.f, -35.f, 70.f, -70.f, 110.f, -110.f, 150.f, -150.f, 180.f };
	const float CornerX[2] = { WorkshopFloorCentre.X - WorkshopHalfExtent.X, WorkshopFloorCentre.X + WorkshopHalfExtent.X };
	const float CornerY[2] = { -WorkshopHalfExtent.Y, WorkshopHalfExtent.Y };

	for (float SpotDist : SpotDists)
	{
		for (float SpotAngle : SpotAngles)
		{
			const FVector Dir = FRotator(0.f, FwdYaw + SpotAngle, 0.f).Vector();
			const FVector Probe = PLoc + Dir * SpotDist;
			FHitResult FloorHit;
			if (!InWorld->LineTraceSingleByObjectType(FloorHit, FVector(Probe.X, Probe.Y, BaseZ + 300.f), FVector(Probe.X, Probe.Y, BaseZ - 600.f), FloorObjs, Q)) { continue; }
			if (FloorHit.bStartPenetrating || FloorHit.ImpactNormal.Z < 0.9f || FMath::Abs(FloorHit.ImpactPoint.Z - BaseZ) > 200.f) { continue; }
			const FVector Spot = FloorHit.ImpactPoint;
			const float FaceYaw = (PLoc - Spot).GetSafeNormal2D().Rotation().Yaw; // local +X faces the player
			const FQuat Rot = FRotator(0.f, FaceYaw, 0.f).Quaternion();

			// Every corner of the floor plate must sit on the same flat floor.
			bool bFlat = true;
			for (int32 cx = 0; cx < 2 && bFlat; ++cx)
			{
				for (int32 cy = 0; cy < 2 && bFlat; ++cy)
				{
					const FVector Corner = Spot + Rot.RotateVector(FVector(CornerX[cx], CornerY[cy], 0.f));
					FHitResult CornerHit;
					bFlat = InWorld->LineTraceSingleByObjectType(CornerHit, Corner + FVector(0, 0, 200.f), Corner - FVector(0, 0, 200.f), FloorObjs, Q)
						&& !CornerHit.bStartPenetrating && FMath::Abs(CornerHit.ImpactPoint.Z - Spot.Z) < 40.f;
				}
			}
			if (!bFlat) { continue; }

			// Nothing (buildings, props, cars, pickups) inside the nook volume.
			const FVector BoxCentre = Spot + Rot.RotateVector(WorkshopFloorCentre) + FVector(0, 0, WorkshopHalfExtent.Z + 15.f);
			if (InWorld->OverlapAnyTestByObjectType(BoxCentre, Rot, BlockObjs, FCollisionShape::MakeBox(WorkshopHalfExtent), Q)) { continue; }

			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AWorkshopEasterEggs* Nook = InWorld->SpawnActor<AWorkshopEasterEggs>(AWorkshopEasterEggs::StaticClass(), FTransform(Rot, Spot), SP);
			if (Nook)
			{
				UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: Dum-E's easter eggs at %s (%.0f m from the player start, bearing %+.0f deg)."),
					*Spot.ToCompactString(), SpotDist / 100.f, SpotAngle);
			}
			return Nook;
		}
	}
	UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: no clear floor spot near the player start - not spawned."));
	return nullptr;
}

bool AWorkshopEasterEggs::LookingAt(const FVector& ViewLoc, const FVector& ViewDir, const FVector& Target, float MaxDist, float MinDot)
{
	const FVector ToTarget = Target - ViewLoc;
	const float Dist = ToTarget.Size();
	return Dist > 1.f && Dist <= MaxDist && FVector::DotProduct(ToTarget / Dist, ViewDir) >= MinDot;
}

// ============================================================================ tick

void AWorkshopEasterEggs::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W) { return; }
	const AArenaGameMode* GM = W->GetAuthGameMode<AArenaGameMode>();
	if (IsSuppressedRun() || (GM && GM->GetPlayMode() == EArenaPlayMode::Story))
	{
		Destroy();
		return;
	}

	TickFoam(DeltaSeconds);
	TickCar(DeltaSeconds);

	APlayerController* PCtl = W->GetFirstPlayerController();
	AIronManCharacter* Char = PCtl ? Cast<AIronManCharacter>(PCtl->GetPawn()) : nullptr;
	if (FoamFrost > 0.f)
	{
		// Thrusters (flight) wipe the visor faster.
		const float Rate = (Char && Char->IsFlying()) ? 1.2f : 0.4f;
		FoamFrost = FMath::Max(0.f, FoamFrost - DeltaSeconds * Rate);
	}
	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	if (PCtl) { PCtl->GetPlayerViewPoint(ViewLoc, ViewRot); }
	const FVector ViewDir = ViewRot.Vector();
	const FVector PawnLoc = Char ? Char->GetActorLocation() : FVector(1e9f, 1e9f, 1e9f);
	const bool bCanReact = Char && !Char->IsDead() && !Char->IsInCinematic();

	TickBotBay(DeltaSeconds, PawnLoc, ViewLoc, bCanReact ? ViewDir : FVector::ZeroVector);
	if (bCanReact)
	{
		TickExtinguisher(DeltaSeconds, PawnLoc, ViewLoc, ViewDir);
		TickToolTray(DeltaSeconds, Char);
	}
	else
	{
		TickExtinguisher(DeltaSeconds, FVector(1e9f), ViewLoc, FVector::ZeroVector);
	}
}

// ---------------------------------------------------------------------------- #2 bot bay

void AWorkshopEasterEggs::ApplyDockLook(int32 DockIndex, float Brightness, const FLinearColor& Col)
{
	if (!Docks.IsValidIndex(DockIndex)) { return; }
	FWorkshopDock& Dk = Docks[DockIndex];
	if (Parts.IsValidIndex(Dk.BotPart))
	{
		FArenaPaintedPart& P = Parts[Dk.BotPart];
		P.Color = Col;
		P.Emissive = 0.05f + Dk.LitGlow * Brightness;
		RepaintPart(P);
	}
	if (Parts.IsValidIndex(Dk.PadPart))
	{
		FArenaPaintedPart& P = Parts[Dk.PadPart];
		P.Color = Col;
		P.Emissive = 0.05f + 1.5f * Brightness;
		RepaintPart(P);
	}
	if (DockLights.IsValidIndex(DockIndex) && DockLights[DockIndex])
	{
		DockLights[DockIndex]->SetIntensity(140.f * Brightness);
		DockLights[DockIndex]->SetLightColor(Col);
	}
}

void AWorkshopEasterEggs::SetDockLit(int32 DockIndex, bool bLit)
{
	if (!Docks.IsValidIndex(DockIndex)) { return; }
	const bool bJocasta = DockIndex == WorkshopEggs::NumDocks - 1;
	FWorkshopDock& Dk = Docks[DockIndex];
	// Jocasta's dock is offline: the plate turns pale but the dock never lights.
	Dk.LitTime = (bLit && !bJocasta) ? 0.f : -1.f;
	const FLinearColor PlateCol = bLit ? (bJocasta ? Dk.LitColor * 0.45f : Dk.LitColor) : WorkshopPlateDark;
	if (DockPlates.IsValidIndex(DockIndex) && DockPlates[DockIndex]) { DockPlates[DockIndex]->SetTextRenderColor(PlateCol.ToFColor(true)); }
	if (DockIndex == 2 && JarvisGreeting) { JarvisGreeting->SetVisibility(bLit); }
	ApplyDockLook(DockIndex, Dk.LitTime >= 0.f ? 1.f : 0.f, DockIndex == 6 ? WorkshopUltronRed : Dk.LitColor);
	if (bLit && !bJocasta) { ArenaAudio::Play2D(this, TEXT("hud_beep_select"), 0.22f, 0.8f + 0.1f * DockIndex); }
}

void AWorkshopEasterEggs::TickBotBay(float DeltaSeconds, const FVector& PawnLoc, const FVector& ViewLoc, const FVector& ViewDir)
{
	BayClock += DeltaSeconds;
	const FVector Bay = LocalToWorld(WorkshopBayCentre);
	const float BayDist = FVector::Dist2D(PawnLoc, Bay);
	const float BayDz = FMath::Abs(PawnLoc.Z - Bay.Z);
	if (!bBayActive && BayDist < 500.f && BayDz < 400.f)
	{
		bBayActive = true;
		BayNextDock = 0;
		BayStepTimer = 0.f;
	}
	else if (bBayActive && (BayDist > 900.f || BayDz > 700.f))
	{
		bBayActive = false;
		BayNextDock = 0;
		for (int32 i = 0; i < WorkshopEggs::NumDocks; ++i) { SetDockLit(i, false); }
	}
	if (bBayActive && BayNextDock < WorkshopEggs::NumDocks)
	{
		BayStepTimer -= DeltaSeconds;
		if (BayStepTimer <= 0.f)
		{
			SetDockLit(BayNextDock, true);
			++BayNextDock;
			BayStepTimer = 0.15f;
		}
	}

	// Ultron's plate: a 3 s look blinks every dock once in unison.
	const FVector UltronPlate = LocalToWorld(FVector(WorkshopDockX + 22.f, WorkshopDockY(6), 50.f));
	if (!ViewDir.IsNearlyZero() && LookingAt(ViewLoc, ViewDir, UltronPlate, 900.f, 0.99f)) { UltronLook += DeltaSeconds; }
	else { UltronLook = 0.f; bChorusArmed = true; }
	if (UltronLook >= 3.f && bChorusArmed)
	{
		bChorusArmed = false;
		ChorusTimer = 0.5f;
		ArenaAudio::Play2D(this, TEXT("hud_beep_confirm"), 0.35f, 0.75f);
		ArenaAudio::Play2D(this, TEXT("hud_beep_select"), 0.3f, 1.25f);
		UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: Ultron chorus blink."));
	}
	const bool bChorus = ChorusTimer > 0.f;
	if (bChorus)
	{
		ChorusTimer -= DeltaSeconds;
		// 0.25 s all dark, then 0.25 s all bright - every dock, Jocasta's included.
		const float B = ChorusTimer > 0.25f ? 0.f : 2.f;
		for (int32 i = 0; i < WorkshopEggs::NumDocks; ++i) { ApplyDockLook(i, B, Docks[i].LitColor); }
		if (ChorusTimer <= 0.f)
		{
			for (int32 i = 0; i < WorkshopEggs::NumDocks; ++i)
			{
				if (Docks[i].LitTime < 0.f) { ApplyDockLook(i, 0.f, Docks[i].LitColor); }
			}
		}
		else { return; }
	}

	for (int32 i = 0; i < WorkshopEggs::NumDocks; ++i)
	{
		FWorkshopDock& Dk = Docks[i];
		if (Dk.LitTime < 0.f) { continue; }
		Dk.LitTime += DeltaSeconds;
		float B = 1.f;
		FLinearColor Col = Dk.LitColor;
		if (i == 6 && Dk.LitTime < 1.2f)
		{
			// Ultron: flickers red, then settles to amber.
			Col = WorkshopUltronRed;
			B = FMath::Frac(FMath::Sin(Dk.LitTime * 97.f) * 43758.5453f) > 0.45f ? 1.f : 0.1f;
		}
		else if (i == 3)
		{
			B = FMath::Fmod(BayClock, 1.f) < 0.8f ? 1.f : 0.12f; // Friday blinks teal
		}
		ApplyDockLook(i, B, Col);
	}
	if (DumEArmPivot)
	{
		const float T = Docks[0].LitTime;
		const float Roll = T < 0.f ? 0.f : (T < 3.f ? 30.f * FMath::Sin(T * 7.f) : 6.f * FMath::Sin(BayClock * 1.3f));
		DumEArmPivot->SetRelativeRotation(FRotator(0.f, 0.f, Roll));
	}
}

// ---------------------------------------------------------------------------- #1 extinguisher

void AWorkshopEasterEggs::StartSpray(const FVector& TargetLoc)
{
	bFoamSprayed = true;
	bFoamEmitted = false;
	SprayTimer = 0.f;
	SprayTarget = TargetLoc;
	ExtLook = 0.f;
	ArenaAudio::Play2D(this, TEXT("hud_beep_confirm"), 0.4f, 1.5f); // Dum-E, delighted
	UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: Dum-E grabbed the extinguisher."));
}

void AWorkshopEasterEggs::Rattle()
{
	if (SprayTimer >= 0.f) { return; }
	RattleTimer = 0.45f;
	RattleCooldown = 3.f;
	if (ExtPivot) { ArenaAudio::PlayAt(this, TEXT("hit_metal"), ExtPivot->GetComponentLocation() + FVector(0, 0, 75), 0.3f); }
}

void AWorkshopEasterEggs::TickExtinguisher(float DeltaSeconds, const FVector& PawnLoc, const FVector& ViewLoc, const FVector& ViewDir)
{
	if (!ExtPivot) { return; }
	RattleCooldown = FMath::Max(0.f, RattleCooldown - DeltaSeconds);
	const FVector ExtLoc = LocalToWorld(WorkshopExtHome + FVector(0, 0, 75));

	if (SprayTimer < 0.f && !ViewDir.IsNearlyZero())
	{
		// The first spray needs you close enough for the foam to reach (7 m); the rattle answers from 10 m.
		if (LookingAt(ViewLoc, ViewDir, ExtLoc, bFoamSprayed ? 1000.f : 700.f, 0.985f)) { ExtLook += DeltaSeconds; } else { ExtLook = 0.f; }
		const bool bPass = FVector::Dist(PawnLoc, ExtLoc) < 150.f;
		if (!bFoamSprayed && (ExtLook >= 1.5f || bPass)) { StartSpray(PawnLoc + FVector(0, 0, 50)); }
		else if (bFoamSprayed && ExtLook >= 1.5f && RattleCooldown <= 0.f) { Rattle(); ExtLook = 0.f; }
	}

	if (SprayTimer >= 0.f)
	{
		SprayTimer += DeltaSeconds;
		const FVector LocalTo = GetActorTransform().InverseTransformPosition(SprayTarget) - WorkshopExtHome;
		const FVector LocalDir = LocalTo.GetSafeNormal2D();
		const float Yaw = LocalDir.IsNearlyZero() ? 0.f : LocalDir.Rotation().Yaw;
		float Yank = 0.f;
		if (SprayTimer < 0.35f) { Yank = SprayTimer / 0.35f; }
		else if (SprayTimer < 1.25f) { Yank = 1.f; }
		else if (SprayTimer < 1.8f) { Yank = 1.f - (SprayTimer - 1.25f) / 0.55f; }
		ExtPivot->SetRelativeLocation(WorkshopExtHome + LocalDir * 55.f * Yank + FVector(0, 0, 25.f * Yank));
		ExtPivot->SetRelativeRotation(FRotator(0.f, Yaw * Yank, 0.f));
		if (SprayTimer >= 0.35f && !bFoamEmitted)
		{
			bFoamEmitted = true;
			FoamAge = 0.f;
			const FVector Nozzle = ExtPivot->GetComponentTransform().TransformPosition(FVector(16, 0, 108));
			const FVector Aim = (SprayTarget - Nozzle).GetSafeNormal();
			for (int32 f = 0; f < FoamParts.Num(); ++f)
			{
				FArenaPaintedPart& P = Parts[FoamParts[f]];
				if (!P.Component) { continue; }
				P.Component->SetWorldLocation(Nozzle);
				P.Component->SetWorldScale3D(FVector(0.05f));
				P.Component->SetVisibility(true);
				FoamVel[f] = FMath::VRandCone(Aim, FMath::DegreesToRadians(13.f)) * FMath::FRandRange(450.f, 700.f);
			}
			FoamFrost = 1.f;
			ArenaAudio::PlayAt(this, TEXT("reactor_overheat_vent"), Nozzle, 0.6f);
			WorkshopToast(this, LOCTEXT("Foam", "FOAM CONTAMINATION - wipe with thrusters"), FLinearColor(0.85f, 0.95f, 1.f));
		}
		if (SprayTimer >= 1.8f)
		{
			SprayTimer = -1.f;
			ExtPivot->SetRelativeLocation(WorkshopExtHome);
			ExtPivot->SetRelativeRotation(FRotator::ZeroRotator);
			ArenaAudio::Play2D(this, TEXT("hud_beep_select"), 0.35f, 0.6f); // sheepish
		}
	}
	else if (RattleTimer > 0.f)
	{
		RattleTimer -= DeltaSeconds;
		const float J = RattleTimer > 0.f ? FMath::Sin(RattleTimer * 90.f) * 1.5f : 0.f;
		ExtPivot->SetRelativeLocation(WorkshopExtHome + FVector(0.f, J, 0.f));
		ExtPivot->SetRelativeRotation(FRotator(0.f, 0.f, J * 2.f));
	}
}

void AWorkshopEasterEggs::TickFoam(float DeltaSeconds)
{
	if (FoamAge < 0.f) { return; }
	FoamAge += DeltaSeconds;
	const bool bDone = FoamAge > 1.1f;
	for (int32 f = 0; f < FoamParts.Num(); ++f)
	{
		FArenaPaintedPart& P = Parts[FoamParts[f]];
		if (!P.Component) { continue; }
		if (bDone) { P.Component->SetVisibility(false); continue; }
		FoamVel[f].Z -= 180.f * DeltaSeconds;
		FoamVel[f] *= FMath::Max(0.f, 1.f - 1.2f * DeltaSeconds);
		P.Component->SetWorldLocation(P.Component->GetComponentLocation() + FoamVel[f] * DeltaSeconds);
		P.Component->SetWorldScale3D(FVector(0.05f + 0.22f * FoamAge));
	}
	if (bDone) { FoamAge = -1.f; }
}

// ---------------------------------------------------------------------------- #5 tool tray

void AWorkshopEasterEggs::TickToolTray(float DeltaSeconds, const AIronManCharacter* Char)
{
	if (!DuckPivot || !Char) { return; }
	const FVector Tray = LocalToWorld(WorkshopTrayPoint);
	const bool bNear = FVector::Dist2D(Char->GetActorLocation(), Tray) < 200.f && FMath::Abs(Char->GetActorLocation().Z - Tray.Z) < 250.f;
	const bool bLow = Char->GetMaxHealth() > 0.f && Char->GetHealth() / Char->GetMaxHealth() < 0.4f;
	if (!bDuckGiven && !bDuckOut && bNear && bLow && Char->GetVelocity().Size() < 30.f) { IdleNearTray += DeltaSeconds; }
	else { IdleNearTray = 0.f; }
	if (IdleNearTray >= 2.f)
	{
		IdleNearTray = 0.f;
		bDuckOut = true;
		DuckSlide = 0.f;
		DuckPivot->SetVisibility(true, true);
		ArenaAudio::Play2D(this, TEXT("hud_beep_confirm"), 0.45f, 1.9f); // quack, roughly
		WorkshopToast(this, LOCTEXT("DuckOut", "Dum-E slides something across the bench..."), WorkshopToastDim);
	}
	if (DuckSlide >= 0.f)
	{
		DuckSlide += DeltaSeconds;
		const float A = FMath::Min(1.f, DuckSlide / 0.5f);
		DuckPivot->SetRelativeLocation(WorkshopDuckHome - FVector(70.f * (1.f - A), 0.f, 0.f));
		if (A >= 1.f) { DuckSlide = -1.f; }
	}
}

// ---------------------------------------------------------------------------- #4 covered car

void AWorkshopEasterEggs::TickCar(float DeltaSeconds)
{
	if (TarpAnim < 0.f || !TarpPivots.IsValidIndex(TarpAnimIndex)) { return; }
	USceneComponent* Pivot = TarpPivots[TarpAnimIndex];
	TarpAnim += DeltaSeconds;
	const float A = FMath::Min(1.f, TarpAnim / 0.7f);
	if (Pivot)
	{
		Pivot->SetRelativeLocation(WorkshopCarCentre + FVector(-60.f * A, 0.f, 230.f * A));
		Pivot->SetRelativeRotation(FRotator(0.f, 0.f, 28.f * A));
	}
	if (A < 1.f) { return; }
	if (Pivot)
	{
		Pivot->SetVisibility(false, true);
		Pivot->SetRelativeLocation(WorkshopCarCentre);
		Pivot->SetRelativeRotation(FRotator::ZeroRotator);
	}
	TarpAnim = -1.f;
	TarpAnimIndex = INDEX_NONE;
	++CarStage;
	if (CarStage == 1) { WorkshopToast(this, LOCTEXT("Car1", "Another covered car. Of course."), WorkshopToastDim); }
	else if (CarStage == 2) { WorkshopToast(this, LOCTEXT("Car2", "...and another one."), WorkshopToastDim); }
	else
	{
		if (ScrapPivot) { ScrapPivot->SetVisibility(true, true); }
		ArenaAudio::Play2D(this, TEXT("suitup_plate_lock"), 0.5f);
		WorkshopToast(this, LOCTEXT("Car3", "Nested prototypes. Classic."), WorkshopToastTeal);
		UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: scrap Mark I revealed."));
	}
}

// ============================================================================ interact (career [E])

bool AWorkshopEasterEggs::GetInteractPrompt(const AIronManCharacter* Char, FText& OutPrompt, int32& OutTarget) const
{
	OutTarget = WorkshopEggs::TargetNone;
	if (!Char || Char->IsDead() || Char->IsInCinematic()) { return false; }
	const FVector PawnLoc = Char->GetActorLocation();
	if (FVector::DistSquared2D(PawnLoc, GetActorLocation()) > FMath::Square(1200.f)) { return false; }

	FVector ViewDir = Char->GetActorForwardVector();
	if (const APlayerController* PCtl = Cast<APlayerController>(Char->GetController()))
	{
		FVector ViewLoc;
		FRotator ViewRot;
		PCtl->GetPlayerViewPoint(ViewLoc, ViewRot);
		ViewDir = ViewRot.Vector();
	}
	const FVector View2D = ViewDir.GetSafeNormal2D();

	struct FCandidate { int32 Target; FVector Point; float Radius; };
	const FCandidate Candidates[] = {
		{ WorkshopEggs::TargetPinboard, LocalToWorld(WorkshopPinboardPoint), 230.f },
		{ WorkshopEggs::TargetToolTray, LocalToWorld(WorkshopTrayPoint), 200.f },
		{ WorkshopEggs::TargetCar, LocalToWorld(WorkshopCarCentre), 380.f },
	};
	float BestFacing = -2.f;
	for (const FCandidate& Cand : Candidates)
	{
		if (FVector::Dist2D(PawnLoc, Cand.Point) > Cand.Radius || FMath::Abs(PawnLoc.Z - Cand.Point.Z) > 300.f) { continue; }
		const float Facing = FVector::DotProduct((Cand.Point - PawnLoc).GetSafeNormal2D(), View2D);
		if (Facing > BestFacing) { BestFacing = Facing; OutTarget = Cand.Target; }
	}
	switch (OutTarget)
	{
	case WorkshopEggs::TargetPinboard:
	{
		int32 Read = 0;
		for (int32 n = 0; n < WorkshopEggs::NumNotes; ++n) { Read += (NotesReadMask >> n) & 1; }
		OutPrompt = bHistorianAwarded ? LOCTEXT("PNotesDone", "[E] re-read a sticky note")
			: FText::Format(LOCTEXT("PNotes", "[E] read a sticky note ({0}/{1})"), FText::AsNumber(Read), FText::AsNumber(WorkshopEggs::NumNotes));
		return true;
	}
	case WorkshopEggs::TargetToolTray:
		if (bDuckOut && !bDuckGiven) { OutPrompt = LOCTEXT("PDuck", "[E] take the Calibration Duck"); }
		else if (const USuitReactiveComponent* ReactiveComp = Char->GetSuitReactive(); ReactiveComp && ReactiveComp->NeedsRepair() && !Char->bCareerSuitOff)
		{
			OutPrompt = LOCTEXT("PRepair", "[E] have Dum-E clean + repair the suit");
		}
		else { OutPrompt = LOCTEXT("PTray", "[E] poke Dum-E's tool tray"); }
		return true;
	case WorkshopEggs::TargetCar:
		OutPrompt = CarStage < WorkshopEggs::NumTarps ? LOCTEXT("PTarp", "[E] pull the tarp") : LOCTEXT("PTarpBack", "[E] put the tarp back");
		return true;
	default:
		return false;
	}
}

void AWorkshopEasterEggs::ReadNextNote()
{
	ArenaAudio::Play2D(this, TEXT("ui_click"), 0.45f);
	if (FMath::FRand() < 0.05f)
	{
		WorkshopToast(this, LOCTEXT("BlankNote", "Sticky note: \"...beep.\""), WorkshopToastDim);
		return;
	}
	const int32 Idx = NextNote;
	NextNote = (NextNote + 1) % WorkshopEggs::NumNotes;
	NotesReadMask |= 1 << Idx;
	WorkshopToast(this, FText::Format(LOCTEXT("Note", "Sticky note {0}/{1}: \"{2}\""), FText::AsNumber(Idx + 1), FText::AsNumber(WorkshopEggs::NumNotes),
		FText::AsCultureInvariant(WorkshopNotes[Idx])), WorkshopToastTeal);
	if (!bHistorianAwarded && NotesReadMask == (1 << WorkshopEggs::NumNotes) - 1)
	{
		bHistorianAwarded = true;
		if (AArenaGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaGameMode>() : nullptr) { GM->AddScore(50); }
		ArenaAudio::Play2D(this, TEXT("ui_confirm"), 0.5f);
		WorkshopToast(this, LOCTEXT("Historian", "+50  Workshop Historian"), WorkshopToastGold);
		UE_LOG(LogIronManArena, Log, TEXT("Workshop nook: all sticky notes read (+50 Workshop Historian)."));
	}
}

void AWorkshopEasterEggs::Interact(AIronManCharacter* Char, int32 Target)
{
	if (!Char) { return; }
	switch (Target)
	{
	case WorkshopEggs::TargetPinboard:
		ReadNextNote();
		break;
	case WorkshopEggs::TargetToolTray:
		if (bDuckOut && !bDuckGiven)
		{
			bDuckGiven = true;
			bDuckOut = false;
			DuckSlide = -1.f;
			if (DuckPivot) { DuckPivot->SetVisibility(false, true); }
			Char->Heal(5.f);
			ArenaAudio::Play2D(this, TEXT("ui_click"), 0.5f, 1.7f); // squeak
			ArenaAudio::Play2D(this, TEXT("pickup_health"), 0.4f);
			WorkshopToast(this, LOCTEXT("DuckTaken", "Calibration Duck: moral support (+5 HP)"), WorkshopToastGold);
		}
		else if (USuitReactiveComponent* ReactiveComp = Char->GetSuitReactive(); ReactiveComp && ReactiveComp->NeedsRepair() && !Char->bCareerSuitOff)
		{
			// Jocasta reactive state: the workshop strips grime, scorch and battle damage off the current suit.
			ReactiveComp->RepairAndClean();
			ArenaAudio::Play2D(this, TEXT("hud_beep_select"), 0.4f, 1.1f);
			ArenaAudio::Play2D(this, TEXT("pickup_health"), 0.35f);
			WorkshopToast(this, LOCTEXT("SuitRepaired", "Dum-E buffs out the scorch marks and hammers out the dents. Suit restored."), WorkshopToastTeal);
		}
		else
		{
			ArenaAudio::Play2D(this, TEXT("hud_beep_select"), 0.35f, 0.7f);
			Rattle();
			WorkshopToast(this, LOCTEXT("TrayPoke", "Dum-E beeps and nudges the extinguisher."), WorkshopToastDim);
		}
		break;
	case WorkshopEggs::TargetCar:
		if (TarpAnim >= 0.f) { break; }
		if (CarStage < WorkshopEggs::NumTarps)
		{
			TarpAnimIndex = CarStage;
			TarpAnim = 0.f;
			ArenaAudio::PlayAt(this, TEXT("melee_swish"), LocalToWorld(WorkshopCarCentre + FVector(0, 0, 80)), 0.5f);
		}
		else
		{
			CarStage = 0;
			for (USceneComponent* Pivot : TarpPivots)
			{
				if (!Pivot) { continue; }
				Pivot->SetRelativeLocation(WorkshopCarCentre);
				Pivot->SetRelativeRotation(FRotator::ZeroRotator);
				Pivot->SetVisibility(true, true);
			}
			if (ScrapPivot) { ScrapPivot->SetVisibility(false, true); }
			ArenaAudio::PlayAt(this, TEXT("melee_swish"), LocalToWorld(WorkshopCarCentre + FVector(0, 0, 80)), 0.5f);
			WorkshopToast(this, LOCTEXT("TarpBack", "Tarp back on. Nothing to see here."), WorkshopToastDim);
		}
		break;
	default:
		break;
	}
}

#undef LOCTEXT_NAMESPACE
