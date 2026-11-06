// Career data (Axel, 2026-11-05 "make it look like GTA 6"): credits, suit upgrades, shop suits, buyable houses and
// the "make your own avatar" pilot + suit paint. Plain data + static tables; UCareerSubsystem owns the live state and
// persists it in UArenaSaveGame (slot "IronManArena").
#pragma once

#include "CoreMinimal.h"
#include "SuitTypes.h"
#include "CareerTypes.generated.h"

/** Suit upgrade tracks sold in the shop (U). Each has levels 0..MaxLevel. */
UENUM(BlueprintType)
enum class ECareerUpgrade : uint8
{
	Armor,          // + max health
	Repulsor,       // + repulsor / primary weapon damage
	Flight,         // + flight speed
	HeatCapacity,   // + arc reactor heat ceiling
	Unibeam,        // + chest beam damage / duration, - energy cost
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EAvatarBody : uint8
{
	Masculine,
	Feminine
};

UENUM(BlueprintType)
enum class EAvatarHair : uint8
{
	Short,
	Buzz,
	Long,
	Ponytail,
	Bun,
	Curly,
	Bald,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EAvatarFacialHair : uint8 { None, Stubble, Moustache, Goatee, FullBeard, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EOutfitTop : uint8 { TShirt, BandTee, Hoodie, ShirtAndTie, TankTop, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EOutfitJacket : uint8 { None, Leather, SuitJacket, Bomber, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EOutfitPants : uint8 { Jeans, Chinos, SuitTrousers, Shorts, Skirt, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EOutfitShoes : uint8 { Sneakers, Boots, DressShoes, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EOutfitHat : uint8 { None, Cap, Beanie, Fedora, Count UMETA(Hidden) };
UENUM(BlueprintType)
enum class EOutfitGlasses : uint8 { None, Aviators, Round, Shades, Count UMETA(Hidden) };

/** Clothes (clothing store, outfit slots). Colour fields index Career::OutfitColors(). */
USTRUCT(BlueprintType)
struct IRONMANARENA_API FAvatarOutfit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") EOutfitTop Top = EOutfitTop::TShirt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") int32 TopColor = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") EOutfitJacket Jacket = EOutfitJacket::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") int32 JacketColor = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") EOutfitPants Pants = EOutfitPants::Jeans;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") int32 PantsColor = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") EOutfitShoes Shoes = EOutfitShoes::Sneakers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") int32 ShoesColor = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") EOutfitHat Hat = EOutfitHat::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") int32 HatColor = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outfit") EOutfitGlasses Glasses = EOutfitGlasses::None;
};

/** Everything the avatar creator edits. Stored as-is in the save game. */
USTRUCT(BlueprintType)
struct IRONMANARENA_API FAvatarConfig
{
	GENERATED_BODY()

	/** {playerName} (story subtitles use the same name). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar") FString PilotName = TEXT("Harley");

	// ---- pilot (suit off: houses, the creator preview, story cutscenes) ----
	/** Voice set id from Content/NPC/npc_manifest.json (or a placeholder id); used for the pilot's lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar") FName VoiceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") EAvatarBody Body = EAvatarBody::Masculine;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") int32 SkinTone = 3;     // index into SkinTones()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") EAvatarHair Hair = EAvatarHair::Short;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") int32 HairColor = 1;    // index into HairColors()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") int32 Face = 0;         // index into FaceNames()
	/** Barber: boys only (ignored on a feminine body). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") EAvatarFacialHair FacialHair = EAvatarFacialHair::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") FAvatarOutfit Outfit;
	/** Real character body (index among the bodies of this body type in AvatarCatalog); -1 = default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") int32 BodyModel = -1;
	/** 0.85 .. 1.1 (teen .. tall adult). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Pilot") float Height = 0.95f;

	// ---- suit paint (applied on top of the worn suit when bCustomPaint) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Suit") bool bCustomPaint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Suit") FLinearColor SuitPrimary = FLinearColor(0.42f, 0.012f, 0.01f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Suit") FLinearColor SuitSecondary = FLinearColor(0.8f, 0.52f, 0.1f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Suit") FLinearColor ReactorGlow = FLinearColor(0.35f, 0.9f, 1.f);
	/** Helmet variant: 0 = keep the suit's own, else ESuitHelmet value + 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Suit") int32 HelmetStyle = 0;
	/** Slimmer (feminine) armour silhouette for every suit, not only the shop's girl suits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Suit") bool bSlimArmor = false;
};

UENUM(BlueprintType)
enum class ECityVehicleKind : uint8 { Car, Bike, Plane, Helicopter };

/** A vehicle sold at the dealership. */
struct FVehicleDef
{
	FName Id;
	const TCHAR* Name;
	const TCHAR* Blurb;
	ECityVehicleKind Kind;
	int32 Price;
	float TopSpeedKmh;
	FLinearColor Paint;
	/** Body proportions (cm): length, width, height. */
	FVector Size;
};

/** A clothing-store item (style unlock). Kind: 0 top, 1 jacket, 2 pants, 3 shoes, 4 hat, 5 glasses, 6 hair, 7 facial hair. */
struct FClothingItem
{
	FName Id;
	const TCHAR* Name;
	int32 Kind;
	int32 Style;
	int32 Price;
};

UENUM(BlueprintType)
enum class ECasinoGame : uint8 { Slots, Blackjack, Roulette, Wheel, Count UMETA(Hidden) };

/** A paint preset in the avatar creator. */
struct FSuitPaintPreset
{
	const TCHAR* Name;
	FLinearColor Primary;
	FLinearColor Secondary;
	FLinearColor Glow;
};

/** A house for sale in the city. */
struct FCityHouseDef
{
	FName Id;
	const TCHAR* Name;
	const TCHAR* Blurb;
	int32 Price;
	/** Credits per in-game minute while owned (0 = none). */
	int32 IncomePerMinute;
	/** Suit repair / rearm bay (stand on the pad). */
	bool bArmory;
};

/** A suit on the shop's tier list (locked in free play until bought). Each has a signature perk on top of its own
 *  weapon / special / silhouette from the suit table. */
struct FShopSuitDef
{
	FName SuitId;
	int32 Tier;          // 1..5 (shop order)
	int32 Price;         // 0 = free starter
	bool bFeminine;      // feminine armour proportions
	const TCHAR* Perk;   // shown in the shop
	float ArmorMul = 1.f, DamageMul = 1.f, FlightMul = 1.f, HeatMul = 1.f, BeamMul = 1.f, BoostMul = 1.f;
	float RepairPerSecond = 0.f;  // self-repair (nanotech / Rescue)
	int32 ExtraMissiles = 0;
	bool bBossUnlock = false;     // buyable only after a free-play boss kill (Ultron)
};

namespace Career
{
	constexpr int32 MaxUpgradeLevel = 5;

	IRONMANARENA_API FText UpgradeName(ECareerUpgrade U);
	IRONMANARENA_API FText UpgradeDescription(ECareerUpgrade U);
	/** Price of the next level (Level = current level), 0 when maxed. */
	IRONMANARENA_API int32 UpgradeCost(ECareerUpgrade U, int32 Level);
	/** Stat multiplier at a level (Armor 1 + 0.12 L, Repulsor 1 + 0.12 L, Flight 1 + 0.08 L, Heat 1 + 0.15 L, Unibeam 1 + 0.15 L). */
	IRONMANARENA_API float UpgradeMultiplier(ECareerUpgrade U, int32 Level);

	IRONMANARENA_API const TArray<FCityHouseDef>& Houses();
	IRONMANARENA_API const FCityHouseDef* FindHouse(FName Id);

	IRONMANARENA_API const TArray<FShopSuitDef>& ShopSuits();
	IRONMANARENA_API const FShopSuitDef* FindShopSuit(FName SuitId);
	/** Rescue + every bFeminine shop suit. */
	IRONMANARENA_API bool IsFeminineSuit(FName SuitId);

	IRONMANARENA_API const TArray<FSuitPaintPreset>& PaintPresets();
	IRONMANARENA_API const TArray<FLinearColor>& SkinTones();
	IRONMANARENA_API const TArray<FLinearColor>& HairColors();
	IRONMANARENA_API const TArray<FLinearColor>& OutfitColors();
	IRONMANARENA_API const TArray<FText>& FaceNames();
	IRONMANARENA_API FText HairName(EAvatarHair Hair);
	IRONMANARENA_API FText HelmetName(int32 HelmetStyle);
	IRONMANARENA_API int32 NumHelmetStyles();

	IRONMANARENA_API const TArray<FVehicleDef>& Vehicles();
	IRONMANARENA_API const FVehicleDef* FindVehicle(FName Id);

	/** Clothing store / barber catalogue (styles not listed are free). */
	IRONMANARENA_API const TArray<FClothingItem>& ClothingItems();
	IRONMANARENA_API const FClothingItem* FindClothing(int32 Kind, int32 Style);
	IRONMANARENA_API FText ClothingStyleName(int32 Kind, int32 Style);
	IRONMANARENA_API int32 ClothingStyleCount(int32 Kind);
	constexpr int32 BarberCutPrice = 150;
	constexpr int32 BarberColorPrice = 100;
	constexpr int32 OutfitSlots = 3;

	/** Casino: credits only. Max net loss per real day; table bet limits. */
	constexpr int32 CasinoDailyLossLimit = 5000;
	constexpr int32 CasinoMinBet = 10;
	constexpr int32 CasinoMaxBet = 1000;
	IRONMANARENA_API FText CasinoGameName(ECasinoGame Game);

	/** Credits per kill by enemy score value (drone 100 -> 25, tank 350 -> 70, boss 5000 -> 800). */
	IRONMANARENA_API int32 CreditsForKill(int32 ScoreValue);
}
