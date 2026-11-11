// Career UI (all Slate, built in C++): dev bar (` / F1), suit shop (U), avatar creator / first-launch character
// setup / edit character (K), barber + clothing store (walk in, E), phone (N), casino games (walk up to a table, E)
// and the always-on career HUD (credits, toasts, wanted stars, map markers, prompts, speedo, FPS).
// UCareerInput (added to the player controller) polls the keys, owns the screens and drives vehicles / interact /
// suit-off on foot.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"
#include "CareerTypes.h"
#include "CareerWidgets.generated.h"

class SBox;
class SWidget;
class APilotAvatar;
class ACityVehicle;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPointLightComponent;
class APlayerController;
class AArenaPlayerController;
struct FCityPoi;

UENUM()
enum class ECareerScreen : uint8 { None, DevBar, Shop, Avatar, FirstLaunch, Barber, Clothing, Phone, Casino };

/** Off-world photo booth for the avatar / barber / clothing previews (render target shown in Slate). */
UCLASS(NotPlaceable)
class IRONMANARENA_API ACareerPreviewStage : public AActor
{
	GENERATED_BODY()
public:
	ACareerPreviewStage();
	void ShowAvatar(const FAvatarConfig& Config);
	/** 0 = full body, 1 = head close-up (barber mirror). */
	void SetFraming(int32 Mode);
	void Spin(float Degrees);
	UTextureRenderTarget2D* GetTarget() const { return Target; }
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Target;
	UPROPERTY() TObjectPtr<UPointLightComponent> Key;
	UPROPERTY() TObjectPtr<UPointLightComponent> Rim;
	UPROPERTY() TObjectPtr<APilotAvatar> Avatar;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Floor;
	float Yaw = 200.f;
	int32 Framing = 0;
};

UCLASS()
class IRONMANARENA_API UCareerScreen : public UUserWidget
{
	GENERATED_BODY()
public:
	void Open(ECareerScreen InScreen, int32 Param = 0);
	ECareerScreen GetScreen() const { return Screen; }
	/** Dev bar keeps the game running (game + UI input); every other screen pauses. */
	bool IsGameplayScreen() const { return Screen == ECareerScreen::DevBar; }
	void Refresh();
	void CloseScreen();

	TWeakObjectPtr<class UCareerInput> InputOwner;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	TSharedRef<SWidget> BuildPage();
	TSharedRef<SWidget> BuildDevBar();
	TSharedRef<SWidget> BuildShop();
	TSharedRef<SWidget> BuildShopUpgrades();
	TSharedRef<SWidget> BuildShopSuits();
	TSharedRef<SWidget> BuildShopHouses();
	TSharedRef<SWidget> BuildShopVehicles();
	TSharedRef<SWidget> BuildAvatar(bool bFirstLaunch);
	TSharedRef<SWidget> BuildBarber();
	TSharedRef<SWidget> BuildClothing();
	TSharedRef<SWidget> BuildPhone();
	TSharedRef<SWidget> BuildCasino();
	TSharedRef<SWidget> BuildPreview(float Width, float Height);
	TSharedRef<SWidget> Frame(const FText& Title, TSharedRef<SWidget> Content, float Width = 1180.f);
	void EnsurePreview(int32 Framing);
	void PushPreview();
	/** Save the draft avatar and close the creator (START / Enter on first launch, SAVE CHARACTER otherwise). */
	void ConfirmDraft();

	// casino
	void CasinoPlay();
	void BlackjackDeal();
	void BlackjackHit();
	void BlackjackStand(bool bDouble);
	int32 HandValue(const TArray<int32>& Cards) const;
	int32 MaxBet() const;

	TSharedPtr<SBox> Body;
	ECareerScreen Screen = ECareerScreen::None;
	int32 Param = 0;
	int32 ShopTab = 0;
	int32 SelectedSuit = 0;
	bool bGirlSuitsOnly = false;
	FAvatarConfig Draft;
	int32 VoiceIndex = 0;
	int32 PaintPreset = 0;

	UPROPERTY(Transient) TObjectPtr<ACareerPreviewStage> Stage;
	FSlateBrush PreviewBrush;
	float SmoothedDt = 0.016f;
	float DevRefreshAcc = 0.f;

	// casino state
	int32 Bet = 100;
	int32 RouletteBet = 0;      // 0 red 1 black 2 odd 3 even 4 low 5 high 6 dozen1 7 dozen2 8 dozen3 9 straight
	int32 RouletteNumber = 17;
	TArray<int32> Reels;
	float SpinTime = 0.f;
	int32 PendingNet = 0;
	bool bPendingSettle = false;
	FText ResultText;
	TArray<int32> PlayerCards, DealerCards;
	bool bHandActive = false;
	bool bDoubled = false;
	int32 WheelSegment = 0;
	float WheelAngle = 0.f;
	FRandomStream CasinoRng;
};

/** Always-on overlay: credits, toasts, wanted stars, markers, interact prompt, vehicle speedo, FPS. */
UCLASS()
class IRONMANARENA_API UCareerHUD : public UUserWidget
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<class UCareerInput> InputOwner;
	bool bShowFps = false;
	bool bShowMarkers = true;
	FText Prompt;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	float Fps = 60.f;
};

UCLASS()
class IRONMANARENA_API UCareerInput : public UActorComponent
{
	GENERATED_BODY()
public:
	UCareerInput();
	static UCareerInput* Get(const UObject* WorldContext);
	/** Dev bar gate (Axel only, never players): always in Editor / Development builds (ima.DevMenu 1, default on);
	 *  in Shipping only when Saved/AllowDevMenu.txt exists on the local disk. */
	static bool IsDevMenuAllowed();
	/** Rebindable career keys: DefaultGame.ini / Saved GameUserSettings [IronManArena.Keys] SuitExit=O, Faceplate=H, WebSwing=B, WebZip=R. */
	static FKey Key(const TCHAR* Name, const FKey& Default);

	void OpenScreen(ECareerScreen Screen, int32 Param = 0);
	void CloseScreen();
	bool IsScreenOpen() const;
	ECareerScreen GetOpenScreen() const;

	/** Suit off: walk around as the pilot (shops, casino); no flight or weapons. */
	void SetSuitOff(bool bOff);
	/** O while on foot: near the parked suit = suit up (cinematic); far away = call it (it flies to you). */
	void RequestSuitUp();
	bool IsSuitOff() const { return bSuitOff; }
	void RefreshPilotLook();

	/** Phone: bring an owned vehicle to you (cars / bikes / helicopter at the kerb, planes at the airstrip). */
	bool DeliverVehicle(FName VehicleId);
	ACityVehicle* GetDrivenVehicle() const { return Driving.Get(); }
	UCareerHUD* GetHUD() const { return HUD; }

	/** Free play start: first-launch setup, home spawn, greeting. */
	void OnFreePlayStarted();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	APlayerController* PC() const;
	void TickKeys();
	void TickInteract();
	void TickVehicle(float DeltaTime);
	void Greet();
	const FCityPoi* NearestPoi(float& OutDist) const;

	UPROPERTY(Transient) TObjectPtr<UCareerScreen> ScreenWidget;
	UPROPERTY(Transient) TObjectPtr<UCareerHUD> HUD;
	UPROPERTY(Transient) TObjectPtr<APilotAvatar> OnFootPilot;
	UPROPERTY(Transient) TObjectPtr<class AParkedSuit> Parked;
	TWeakObjectPtr<ACityVehicle> Driving;
	TWeakObjectPtr<ACityVehicle> Delivered;
	bool bSuitOff = false;
	bool bFirstLaunchShown = false;
	bool bGreeted = false;
	float SavedWalkSpeed = 600.f;
	int32 InteractKind = 0; // 0 none 1 vehicle 2 poi 3 house 4 workshop easter egg
	FName InteractId;
	int32 InteractParam = -1;
	/** Edge-detects the game entering Playing (arena / free roam / story) to run OnFreePlayStarted once per run. */
	bool bRunActive = false;
};
