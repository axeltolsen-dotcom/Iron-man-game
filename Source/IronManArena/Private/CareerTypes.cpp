#include "CareerTypes.h"

#define LOCTEXT_NAMESPACE "Career"

namespace Career
{
	FText UpgradeName(ECareerUpgrade U)
	{
		switch (U)
		{
		case ECareerUpgrade::Armor:        return LOCTEXT("UArmor", "ARMOR PLATING");
		case ECareerUpgrade::Repulsor:     return LOCTEXT("URep", "REPULSOR DAMAGE");
		case ECareerUpgrade::Flight:       return LOCTEXT("UFly", "FLIGHT SPEED");
		case ECareerUpgrade::HeatCapacity: return LOCTEXT("UHeat", "REACTOR HEAT CAPACITY");
		case ECareerUpgrade::Unibeam:      return LOCTEXT("UBeam", "UNIBEAM");
		default:                           return FText::GetEmpty();
		}
	}

	FText UpgradeDescription(ECareerUpgrade U)
	{
		switch (U)
		{
		case ECareerUpgrade::Armor:        return LOCTEXT("DArmor", "+12% max health per level");
		case ECareerUpgrade::Repulsor:     return LOCTEXT("DRep", "+12% weapon damage per level");
		case ECareerUpgrade::Flight:       return LOCTEXT("DFly", "+8% flight speed per level");
		case ECareerUpgrade::HeatCapacity: return LOCTEXT("DHeat", "+15% heat before overheat per level");
		case ECareerUpgrade::Unibeam:      return LOCTEXT("DBeam", "+15% beam damage and length, -6% energy cost per level");
		default:                           return FText::GetEmpty();
		}
	}

	int32 UpgradeCost(ECareerUpgrade U, int32 Level)
	{
		if (Level >= MaxUpgradeLevel) { return 0; }
		static const int32 Base[] = { 600, 700, 500, 550, 900 };
		const int32 B = Base[FMath::Clamp(int32(U), 0, 4)];
		// 600, 1500, 2700, 4200, 6000 ... (B * (L+1) * (L+2) / 2)
		return B * (Level + 1) * (Level + 2) / 2;
	}

	float UpgradeMultiplier(ECareerUpgrade U, int32 Level)
	{
		const float L = float(FMath::Clamp(Level, 0, MaxUpgradeLevel));
		switch (U)
		{
		case ECareerUpgrade::Armor:        return 1.f + 0.12f * L;
		case ECareerUpgrade::Repulsor:     return 1.f + 0.12f * L;
		case ECareerUpgrade::Flight:       return 1.f + 0.08f * L;
		case ECareerUpgrade::HeatCapacity: return 1.f + 0.15f * L;
		case ECareerUpgrade::Unibeam:      return 1.f + 0.15f * L;
		default:                           return 1.f;
		}
	}

	const TArray<FCityHouseDef>& Houses()
	{
		static const TArray<FCityHouseDef> H = {
			{ FName(TEXT("harbor-loft")), TEXT("Harbor Loft"), TEXT("Small walk-up apartment over the harbor street. A bed, a bench, a place to start."), 2500, 0, false },
			{ FName(TEXT("skyline-penthouse")), TEXT("Skyline Penthouse"), TEXT("Glass penthouse on top of a downtown tower with a rooftop pad. Earns +40 credits a minute (rent from the floors below)."), 15000, 40, false },
			{ FName(TEXT("malibu-mansion")), TEXT("Cliffside Mansion"), TEXT("Malibu-style cliff house with a workshop and an armory bay: repairs, recharges and cools your suit. Earns +120 credits a minute."), 45000, 120, true },
			{ FName(TEXT("workshop-bay")), TEXT("Workshop Bay"), TEXT("Converted garage loft near the hangar district. Quiet workbench, scrap bins, a place to tinker. Earns +15 credits a minute."), 8000, 15, false },
			{ FName(TEXT("canal-flat")), TEXT("Canal Flat"), TEXT("Two-room walk-up over the canal promenade. Quiet mornings, damp evenings. Earns +25 credits a minute."), 11000, 25, false },
			{ FName(TEXT("night-watch-lease")), TEXT("Night Watch Lease"), TEXT("Cheap dockside lease: low deposit, then -8 CR/min rent. A place to sleep until you buy something better."), 1200, -8, false },
		};
		return H;
	}

	const FCityHouseDef* FindHouse(FName Id)
	{
		for (const FCityHouseDef& H : Houses()) { if (H.Id == Id) { return &H; } }
		return nullptr;
	}

	const TArray<FShopSuitDef>& ShopSuits()
	{
		auto P = [](const TCHAR* Id, int32 Tier, int32 Price, bool bFem, const TCHAR* Perk) { FShopSuitDef D; D.SuitId = FName(Id); D.Tier = Tier; D.Price = Price; D.bFeminine = bFem; D.Perk = Perk; return D; };
		static const TArray<FShopSuitDef> S = [&P]()
		{
			TArray<FShopSuitDef> L;
			FShopSuitDef D;
			D = P(TEXT("mark-iii"), 1, 0, false, TEXT("Classic: balanced, +10% unibeam")); D.BeamMul = 1.1f; L.Add(D);
			D = P(TEXT("mark-i"), 1, 0, false, TEXT("Scrap iron: flamethrower, +30% heat capacity, +15% armour, -15% flight")); D.HeatMul = 1.3f; D.ArmorMul = 1.15f; D.FlightMul = 0.85f; L.Add(D);
			D = P(TEXT("mark-ii"), 1, 1500, false, TEXT("Test pilot: +15% flight, +20% boost, -15% armour (fragile)")); D.FlightMul = 1.15f; D.BoostMul = 1.2f; D.ArmorMul = 0.85f; L.Add(D);
			D = P(TEXT("ironheart"), 2, 3000, true, TEXT("Home-built: +25% unibeam")); D.BeamMul = 1.25f; L.Add(D);
			D = P(TEXT("mark-xv-sneaky"), 2, 4000, false, TEXT("Stealth: cloak special, +15% damage from the shadows")); D.DamageMul = 1.15f; L.Add(D);
			D = P(TEXT("aurora"), 2, 5000, true, TEXT("Long range: +2 missiles per salvo")); D.ExtraMissiles = 2; L.Add(D);
			D = P(TEXT("nightshade"), 2, 6000, true, TEXT("Slim stealth: cloak, +10% flight, +10% damage")); D.FlightMul = 1.1f; D.DamageMul = 1.1f; L.Add(D);
			D = P(TEXT("deep-space"), 3, 7000, false, TEXT("Deep-space / dive suit: +50% heat capacity, +10% armour")); D.HeatMul = 1.5f; D.ArmorMul = 1.1f; L.Add(D);
			D = P(TEXT("rescue"), 3, 8000, true, TEXT("Guardian: repair drones, self-repair 1.5 HP/s, +10% armour")); D.RepairPerSecond = 1.5f; D.ArmorMul = 1.1f; L.Add(D);
			D = P(TEXT("rescue-mk2"), 3, 9000, true, TEXT("Rescue Mk II: shield special, self-repair 1 HP/s")); D.RepairPerSecond = 1.f; L.Add(D);
			D = P(TEXT("iron-patriot"), 3, 10000, false, TEXT("Sentry: +4 missiles per salvo, +10% damage")); D.ExtraMissiles = 4; D.DamageMul = 1.1f; L.Add(D);
			D = P(TEXT("impulse"), 4, 12000, false, TEXT("Impulse thrusters: +25% flight, +35% boost")); D.FlightMul = 1.25f; D.BoostMul = 1.35f; L.Add(D);
			D = P(TEXT("valkyrie"), 4, 12000, true, TEXT("Interceptor: +20% flight, laser")); D.FlightMul = 1.2f; L.Add(D);
			D = P(TEXT("mark-xxxviii-igor"), 4, 15000, false, TEXT("Heavy (Hulkbuster-style): +45% armour, -25% flight")); D.ArmorMul = 1.45f; D.FlightMul = 0.75f; L.Add(D);
			D = P(TEXT("mark-l"), 5, 20000, false, TEXT("Bleeding edge nanotech: nano blades, self-repair 2 HP/s")); D.RepairPerSecond = 2.f; L.Add(D);
			D = P(TEXT("ultron"), 5, 25000, false, TEXT("Ultron: +25% damage, +20% heat. Beat a free-play boss to unlock")); D.DamageMul = 1.25f; D.HeatMul = 1.2f; D.bBossUnlock = true; L.Add(D);
			return L;
		}();
		return S;
	}

	const FShopSuitDef* FindShopSuit(FName SuitId)
	{
		for (const FShopSuitDef& S : ShopSuits()) { if (S.SuitId == SuitId) { return &S; } }
		return nullptr;
	}

	bool IsFeminineSuit(FName SuitId)
	{
		const FShopSuitDef* S = FindShopSuit(SuitId);
		return S && S->bFeminine;
	}

	const TArray<FSuitPaintPreset>& PaintPresets()
	{
		static const TArray<FSuitPaintPreset> P = {
			{ TEXT("Mark Red & Gold"), FLinearColor(0.42f, 0.012f, 0.01f), FLinearColor(0.8f, 0.52f, 0.1f), FLinearColor(0.35f, 0.9f, 1.f) },
			{ TEXT("Hot Rod"), FLinearColor(0.55f, 0.02f, 0.015f), FLinearColor(0.62f, 0.42f, 0.12f), FLinearColor(0.5f, 0.95f, 1.f) },
			{ TEXT("Rescue Blue"), FLinearColor(0.023f, 0.16f, 0.45f), FLinearColor(0.53f, 0.56f, 0.6f), FLinearColor(0.52f, 0.87f, 1.f) },
			{ TEXT("Stealth Black"), FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.08f, 0.08f, 0.09f), FLinearColor(0.9f, 0.15f, 0.1f) },
			{ TEXT("War Silver"), FLinearColor(0.2f, 0.21f, 0.23f), FLinearColor(0.05f, 0.05f, 0.055f), FLinearColor(0.9f, 0.95f, 1.f) },
			{ TEXT("Chrome Prototype"), FLinearColor(0.53f, 0.55f, 0.58f), FLinearColor(0.2f, 0.22f, 0.25f), FLinearColor(0.52f, 0.86f, 1.f) },
			{ TEXT("Patriot"), FLinearColor(0.02f, 0.05f, 0.25f), FLinearColor(0.5f, 0.02f, 0.02f), FLinearColor(0.9f, 0.9f, 1.f) },
			{ TEXT("Rose Gold"), FLinearColor(0.62f, 0.3f, 0.24f), FLinearColor(0.82f, 0.8f, 0.78f), FLinearColor(1.f, 0.65f, 0.5f) },
			{ TEXT("Violet Storm"), FLinearColor(0.16f, 0.05f, 0.42f), FLinearColor(0.6f, 0.62f, 0.68f), FLinearColor(0.6f, 0.4f, 1.f) },
			{ TEXT("Emerald"), FLinearColor(0.02f, 0.25f, 0.08f), FLinearColor(0.7f, 0.55f, 0.15f), FLinearColor(0.4f, 1.f, 0.5f) },
		};
		return P;
	}

	const TArray<FLinearColor>& SkinTones()
	{
		// Linear albedo, light -> deep (measured skin reflectance ranges).
		static const TArray<FLinearColor> T = {
			FLinearColor(0.80f, 0.62f, 0.52f), FLinearColor(0.72f, 0.52f, 0.42f), FLinearColor(0.62f, 0.43f, 0.33f),
			FLinearColor(0.52f, 0.34f, 0.24f), FLinearColor(0.40f, 0.25f, 0.17f), FLinearColor(0.28f, 0.17f, 0.11f),
			FLinearColor(0.18f, 0.11f, 0.07f), FLinearColor(0.12f, 0.07f, 0.045f) };
		return T;
	}

	const TArray<FLinearColor>& HairColors()
	{
		static const TArray<FLinearColor> C = {
			FLinearColor(0.015f, 0.012f, 0.01f), FLinearColor(0.06f, 0.035f, 0.02f), FLinearColor(0.16f, 0.09f, 0.045f),
			FLinearColor(0.45f, 0.3f, 0.14f), FLinearColor(0.7f, 0.55f, 0.32f), FLinearColor(0.35f, 0.08f, 0.03f),
			FLinearColor(0.55f, 0.55f, 0.56f), FLinearColor(0.25f, 0.05f, 0.35f), FLinearColor(0.04f, 0.2f, 0.45f), FLinearColor(0.6f, 0.15f, 0.3f),
			FLinearColor(0.85f, 0.78f, 0.55f), FLinearColor(0.12f, 0.35f, 0.18f) };
		return C;
	}

	const TArray<FLinearColor>& OutfitColors()
	{
		static const TArray<FLinearColor> C = {
			FLinearColor(0.18f, 0.18f, 0.2f), FLinearColor(0.6f, 0.6f, 0.6f), FLinearColor(0.02f, 0.02f, 0.022f),
			FLinearColor(0.05f, 0.08f, 0.16f), FLinearColor(0.35f, 0.03f, 0.03f), FLinearColor(0.08f, 0.15f, 0.06f),
			FLinearColor(0.32f, 0.22f, 0.12f), FLinearColor(0.5f, 0.38f, 0.05f), FLinearColor(0.25f, 0.08f, 0.32f), FLinearColor(0.5f, 0.2f, 0.28f),
			FLinearColor(0.9f, 0.88f, 0.82f), FLinearColor(0.08f, 0.35f, 0.4f), FLinearColor(0.55f, 0.28f, 0.08f) };
		return C;
	}

	const TArray<FText>& FaceNames()
	{
		static const TArray<FText> F = { LOCTEXT("F0", "Classic"), LOCTEXT("F1", "Soft"), LOCTEXT("F2", "Angular"), LOCTEXT("F3", "Strong jaw"), LOCTEXT("F4", "Youthful") };
		return F;
	}

	FText HairName(EAvatarHair Hair)
	{
		switch (Hair)
		{
		case EAvatarHair::Short:    return LOCTEXT("HShort", "Short");
		case EAvatarHair::Buzz:     return LOCTEXT("HBuzz", "Buzz cut");
		case EAvatarHair::Long:     return LOCTEXT("HLong", "Long");
		case EAvatarHair::Ponytail: return LOCTEXT("HPony", "Ponytail");
		case EAvatarHair::Bun:      return LOCTEXT("HBun", "Bun");
		case EAvatarHair::Curly:    return LOCTEXT("HCurly", "Curly");
		case EAvatarHair::Bald:     return LOCTEXT("HBald", "Bald");
		case EAvatarHair::Undercut: return LOCTEXT("HUnder", "Undercut");
		case EAvatarHair::Braids:   return LOCTEXT("HBraids", "Braids");
		default:                    return FText::GetEmpty();
		}
	}

	int32 NumHelmetStyles() { return 6; } // suit default + 5 ESuitHelmet values

	FText HelmetName(int32 HelmetStyle)
	{
		switch (HelmetStyle)
		{
		case 1:  return LOCTEXT("Hm1", "Classic faceplate");
		case 2:  return LOCTEXT("Hm2", "Sleek nano");
		case 3:  return LOCTEXT("Hm3", "Angular");
		case 4:  return LOCTEXT("Hm4", "Bulky");
		case 5:  return LOCTEXT("Hm5", "Crude Mark I");
		default: return LOCTEXT("Hm0", "Suit default");
		}
	}

	const TArray<FVehicleDef>& Vehicles()
	{
		static const TArray<FVehicleDef> V = {
			{ FName(TEXT("sport-coupe")), TEXT("Stingray GT"), TEXT("Two-door sports coupe. Sharp, light, fun."), ECityVehicleKind::Car, 6000, 230.f, FLinearColor(0.55f, 0.02f, 0.02f), FVector(440, 190, 125) },
			{ FName(TEXT("muscle")), TEXT("Thunder Muscle"), TEXT("V8 muscle car with a long hood and a short temper."), ECityVehicleKind::Car, 5000, 210.f, FLinearColor(0.02f, 0.05f, 0.2f), FVector(500, 195, 135) },
			{ FName(TEXT("suv")), TEXT("Ranger SUV"), TEXT("Big, tall and comfortable. Pushes through traffic."), ECityVehicleKind::Car, 4500, 180.f, FLinearColor(0.03f, 0.03f, 0.035f), FVector(490, 200, 180) },
			{ FName(TEXT("supercar")), TEXT("Arc Hypercar"), TEXT("Mid-engine hypercar, arc-reactor hybrid. The fastest thing on wheels."), ECityVehicleKind::Car, 25000, 330.f, FLinearColor(0.75f, 0.5f, 0.08f), FVector(460, 200, 112) },
			{ FName(TEXT("sport-bike")), TEXT("Velocity 1000"), TEXT("Sport motorbike. Lean into the corners."), ECityVehicleKind::Bike, 3500, 260.f, FLinearColor(0.5f, 0.02f, 0.02f), FVector(210, 75, 115) },
			{ FName(TEXT("cruiser-bike")), TEXT("Iron Cruiser"), TEXT("Low chopper with a big twin and lots of chrome."), ECityVehicleKind::Bike, 2800, 190.f, FLinearColor(0.02f, 0.02f, 0.02f), FVector(240, 85, 110) },
			{ FName(TEXT("prop-plane")), TEXT("Skylark Prop Plane"), TEXT("Single-prop trainer. Takes off from the airstrip."), ECityVehicleKind::Plane, 15000, 280.f, FLinearColor(0.8f, 0.8f, 0.78f), FVector(800, 1100, 260) },
			{ FName(TEXT("private-jet")), TEXT("Stark Industries Jet"), TEXT("Twin-engine private jet. Needs the full runway."), ECityVehicleKind::Plane, 60000, 750.f, FLinearColor(0.85f, 0.85f, 0.86f), FVector(2000, 1800, 520) },
			{ FName(TEXT("helicopter")), TEXT("City Helicopter"), TEXT("Light helicopter. Lifts straight off any helipad."), ECityVehicleKind::Helicopter, 30000, 250.f, FLinearColor(0.05f, 0.07f, 0.12f), FVector(1000, 260, 300) },
			{ FName(TEXT("city-sedan")), TEXT("Midtown Sedan"), TEXT("Everyday four-door. Cheap, reliable, blends into traffic."), ECityVehicleKind::Car, 2200, 160.f, FLinearColor(0.25f, 0.27f, 0.3f), FVector(470, 185, 140) },
			{ FName(TEXT("cargo-van")), TEXT("Arc Hauler Van"), TEXT("Box van for scrap runs and shop deliveries."), ECityVehicleKind::Car, 3800, 150.f, FLinearColor(0.55f, 0.55f, 0.52f), FVector(520, 200, 200) },
			{ FName(TEXT("hatchback")), TEXT("Metro Hatch"), TEXT("Compact city hatch. Easy parking, thrifty."), ECityVehicleKind::Car, 2800, 170.f, FLinearColor(0.12f, 0.35f, 0.28f), FVector(400, 175, 145) },
			{ FName(TEXT("alley-scooter")), TEXT("Alley Scooter"), TEXT("Narrow electric scooter. Slips through Midtown gaps."), ECityVehicleKind::Bike, 900, 95.f, FLinearColor(0.85f, 0.75f, 0.15f), FVector(160, 55, 110) },
			{ FName(TEXT("ultralight")), TEXT("Harbor Ultralight"), TEXT("Single-seat ultralight. Short hops from the airstrip."), ECityVehicleKind::Plane, 9000, 160.f, FLinearColor(0.7f, 0.72f, 0.55f), FVector(620, 900, 180) },
		};
		return V;
	}

	const FVehicleDef* FindVehicle(FName Id)
	{
		for (const FVehicleDef& V : Vehicles()) { if (V.Id == Id) { return &V; } }
		return nullptr;
	}

	const TArray<FClothingItem>& ClothingItems()
	{
		// Style 0 of every kind is free (and the default). Hair / facial hair are the barber's.
		static const TArray<FClothingItem> C = {
			{ FName(TEXT("top-bandtee")), TEXT("Band tee"), 0, int32(EOutfitTop::BandTee), 250 },
			{ FName(TEXT("top-hoodie")), TEXT("Hoodie"), 0, int32(EOutfitTop::Hoodie), 400 },
			{ FName(TEXT("top-shirttie")), TEXT("Shirt and tie"), 0, int32(EOutfitTop::ShirtAndTie), 600 },
			{ FName(TEXT("top-tank")), TEXT("Tank top"), 0, int32(EOutfitTop::TankTop), 150 },
			{ FName(TEXT("jk-leather")), TEXT("Leather jacket"), 1, int32(EOutfitJacket::Leather), 1200 },
			{ FName(TEXT("jk-suit")), TEXT("Suit jacket"), 1, int32(EOutfitJacket::SuitJacket), 1500 },
			{ FName(TEXT("jk-bomber")), TEXT("Bomber jacket"), 1, int32(EOutfitJacket::Bomber), 900 },
			{ FName(TEXT("pt-chinos")), TEXT("Chinos"), 2, int32(EOutfitPants::Chinos), 300 },
			{ FName(TEXT("pt-suit")), TEXT("Suit trousers"), 2, int32(EOutfitPants::SuitTrousers), 500 },
			{ FName(TEXT("pt-shorts")), TEXT("Shorts"), 2, int32(EOutfitPants::Shorts), 200 },
			{ FName(TEXT("pt-skirt")), TEXT("Skirt"), 2, int32(EOutfitPants::Skirt), 300 },
			{ FName(TEXT("sh-boots")), TEXT("Boots"), 3, int32(EOutfitShoes::Boots), 450 },
			{ FName(TEXT("sh-dress")), TEXT("Dress shoes"), 3, int32(EOutfitShoes::DressShoes), 500 },
			{ FName(TEXT("hat-cap")), TEXT("Baseball cap"), 4, int32(EOutfitHat::Cap), 150 },
			{ FName(TEXT("hat-beanie")), TEXT("Beanie"), 4, int32(EOutfitHat::Beanie), 150 },
			{ FName(TEXT("hat-fedora")), TEXT("Fedora"), 4, int32(EOutfitHat::Fedora), 400 },
			{ FName(TEXT("gl-aviators")), TEXT("Aviators"), 5, int32(EOutfitGlasses::Aviators), 350 },
			{ FName(TEXT("gl-round")), TEXT("Round glasses"), 5, int32(EOutfitGlasses::Round), 250 },
			{ FName(TEXT("gl-shades")), TEXT("Tinted shades"), 5, int32(EOutfitGlasses::Shades), 300 },
			{ FName(TEXT("top-crop")), TEXT("Crop top"), 0, int32(EOutfitTop::CropTop), 220 },
			{ FName(TEXT("top-coveralls")), TEXT("Workshop coveralls"), 0, int32(EOutfitTop::Coveralls), 550 },
			{ FName(TEXT("jk-trench")), TEXT("Trench coat"), 1, int32(EOutfitJacket::Trench), 1800 },
			{ FName(TEXT("pt-joggers")), TEXT("Joggers"), 2, int32(EOutfitPants::Joggers), 280 },
			{ FName(TEXT("sh-loafers")), TEXT("Loafers"), 3, int32(EOutfitShoes::Loafers), 420 },
			{ FName(TEXT("hat-visor")), TEXT("Sun visor"), 4, int32(EOutfitHat::Visor), 120 },
			{ FName(TEXT("gl-clear")), TEXT("Clear frames"), 5, int32(EOutfitGlasses::ClearFrames), 200 },
		};
		return C;
	}

	const FClothingItem* FindClothing(int32 Kind, int32 Style)
	{
		for (const FClothingItem& I : ClothingItems()) { if (I.Kind == Kind && I.Style == Style) { return &I; } }
		return nullptr;
	}

	int32 ClothingStyleCount(int32 Kind)
	{
		switch (Kind)
		{
		case 0: return int32(EOutfitTop::Count);
		case 1: return int32(EOutfitJacket::Count);
		case 2: return int32(EOutfitPants::Count);
		case 3: return int32(EOutfitShoes::Count);
		case 4: return int32(EOutfitHat::Count);
		case 5: return int32(EOutfitGlasses::Count);
		case 6: return int32(EAvatarHair::Count);
		case 7: return int32(EAvatarFacialHair::Count);
		default: return 1;
		}
	}

	FText ClothingStyleName(int32 Kind, int32 Style)
	{
		if (Kind == 6) { return HairName(EAvatarHair(Style)); }
		if (Kind == 7)
		{
			static const TCHAR* F[] = { TEXT("Clean shaven"), TEXT("Stubble"), TEXT("Moustache"), TEXT("Goatee"), TEXT("Full beard"), TEXT("Chinstrap") };
			return FText::FromString(F[FMath::Clamp(Style, 0, 5)]);
		}
		if (Style == 0)
		{
			static const TCHAR* Free[] = { TEXT("T-shirt"), TEXT("No jacket"), TEXT("Jeans"), TEXT("Sneakers"), TEXT("No hat"), TEXT("No glasses") };
			return FText::FromString(Free[FMath::Clamp(Kind, 0, 5)]);
		}
		const FClothingItem* I = FindClothing(Kind, Style);
		return I ? FText::FromString(I->Name) : FText::GetEmpty();
	}

	FText CasinoGameName(ECasinoGame Game)
	{
		switch (Game)
		{
		case ECasinoGame::Slots:     return LOCTEXT("CSlots", "SLOT MACHINE");
		case ECasinoGame::Blackjack: return LOCTEXT("CBj", "BLACKJACK");
		case ECasinoGame::Roulette:  return LOCTEXT("CRou", "ROULETTE");
		case ECasinoGame::Wheel:     return LOCTEXT("CWheel", "WHEEL OF FORTUNE");
		case ECasinoGame::CoinFlip:  return LOCTEXT("CFlip", "COIN FLIP");
		case ECasinoGame::HighLow:   return LOCTEXT("CHL", "HIGH-LOW");
		default:                     return FText::GetEmpty();
		}
	}

	int32 CreditsForKill(int32 ScoreValue)
	{
		if (ScoreValue >= 4000) { return 800; }
		return FMath::Max(10, FMath::RoundToInt(ScoreValue * 0.22f));
	}
}

#undef LOCTEXT_NAMESPACE
