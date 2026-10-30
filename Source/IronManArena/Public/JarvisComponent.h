// UJarvisComponent: the suit AI's voice. Lives on AArenaPlayerController.
//
// Gameplay code calls UJarvisComponent::Notify(this, EAIEvent::LowEnergy) (static, finds the
// local player's component). Each event has a priority and a cooldown; lines go into a small
// priority queue and are played one at a time as a subtitle ("JARVIS: ...") plus, if available,
// a voice clip.
//
// Lines come from the generated table (Private/Generated/AIVoiceLines.inl). Each line has an
// optional USoundBase slot. At BeginPlay any empty slot is auto-filled from the asset
// /Game/Audio/<PersonaFolder>/<LineId>  (which is exactly where Content/Python/import_jarvis_audio.py
// imports the generated WAVs). You can also assign sounds by hand in a Blueprint child.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuitTypes.h"
#include "JarvisComponent.generated.h"

class USoundBase;
class UAudioComponent;

UENUM(BlueprintType)
enum class EAIEvent : uint8
{
	Start, SuitUp, SuitSwapIn, LowEnergy, LowHealth, ToFlight, ToGround,
	HulkbusterReady, HulkbusterDeploy, WaveStart, DroneSwarm, TankSpotted,
	BossIncoming, BossPhase2, BossPhase3, BossDefeated, Combo,
	PickupHealth, PickupEnergy, PickupHulkbuster, Death, HighScore, Idle,
	/** Story comms only (Morgan): hacking, scouting intel, encouragement. */
	Hack, Intel, Encourage,
	/** Spoken by the NEW persona when a swap changes AI; Context = outgoing persona folder name. */
	Handover,
	/** Per-suit intro during the suit-up cinematic; Context = suit Id. */
	SuitIntro,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct IRONMANARENA_API FAILine
{
	GENERATED_BODY()

	/** Unique id, also the audio asset name, e.g. JARVIS_lowEnergy_2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	EAIPersona Persona = EAIPersona::Jarvis;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	EAIEvent Event = EAIEvent::Start;

	/** Suit id (SuitIntro) or outgoing persona (Handover); None otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FName Context;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FText Text;

	/** Optional voice clip. Auto-filled from /Game/Audio/<Persona>/<Id> if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<USoundBase> Sound = nullptr;

	/** Non-persona speaker (story comms, e.g. MORGAN). None = the line belongs to Persona. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FName Speaker;

	/** Subtitle label / colour for Speaker lines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FText SpeakerLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FLinearColor SpeakerColor = FLinearColor::White;
};

UCLASS(ClassGroup = (IronMan), meta = (BlueprintSpawnableComponent))
class IRONMANARENA_API UJarvisComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJarvisComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Convenience: route an event to the local player's AI (no-op if none). */
	static void Notify(const UObject* WorldContext, EAIEvent Event, FName Context = NAME_None);

	/** Static helper for the local player's component. */
	static UJarvisComponent* Get(const UObject* WorldContext);

	/** Queue a line for this event (respects cooldowns and priority). */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void Say(EAIEvent Event, FName Context = NAME_None, bool bInterrupt = false);

	/** Switch the active persona. bAnnounce plays a handover line from the new AI. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetPersona(EAIPersona NewPersona, bool bAnnounce);

	UFUNCTION(BlueprintPure, Category = "AI")
	EAIPersona GetPersona() const { return Persona; }

	/** Drop everything queued and stop the current clip. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopAll();

	// ------------------------------------------------------------------ story comms
	/** Adds event lines for a non-persona comms speaker (Speaker must be set on each line). */
	void AddSpeakerLines(const TArray<FAILine>& InLines);

	/**
	 * A comms speaker who shares the event chatter with the suit AI (story mode: MORGAN, the
	 * "guy in the chair"). Share = chance 0..1 that an event is answered by the speaker.
	 * NAME_None turns it off. The speaker is NOT a suit persona and never takes over the suit.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetCommsSpeaker(FName Speaker, float Share = 0.6f);

	UFUNCTION(BlueprintPure, Category = "AI")
	FName GetCommsSpeaker() const { return CommsSpeaker; }

	/** Scripted line (story objective comms). Priority like the events (0..100). */
	void SayScripted(FName Speaker, const FText& Label, const FText& Text, const FLinearColor& Color,
		USoundBase* Sound, int32 Priority = 70, bool bInterrupt = false);

	/** Settings menu: voice volume (also updates the line that is playing). */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetVoiceVolume(float NewVolume);

	/** Current subtitle for the HUD. False if nothing is being said. */
	bool GetSubtitle(FText& OutLabel, FText& OutText, FLinearColor& OutColor, float& OutAlpha) const;

	/** Every line (generated table, editable in a Blueprint child). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<FAILine> Lines;

	/** Minimum silence between two lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float GapBetweenLines = 0.6f;

	/** Queued lines older than this are dropped (stale chatter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float QueueTimeout = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float VoiceVolume = 0.7f;

	/** Seconds of no events before an "idle" remark. 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float IdleChatterAfter = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bAutoLoadVoiceAssets = true;

protected:
	struct FQueued
	{
		int32 LineIndex = INDEX_NONE;
		int32 Priority = 0;
		float Age = 0.f;
	};

	static int32 GetPriority(EAIEvent Event);
	static float GetCooldown(EAIEvent Event);
	int32 PickLine(EAIPersona InPersona, EAIEvent Event, FName Context);
	int32 PickSpeakerLine(FName Speaker, EAIEvent Event);
	void Enqueue(int32 LineIndex, int32 Priority, bool bInterrupt, bool bDedupeEvent);
	void StartLine(int32 LineIndex);
	void LoadDefaultLines();
	void ResolveSounds();

	EAIPersona Persona = EAIPersona::Jarvis;
	FName CommsSpeaker;
	float CommsShare = 0.6f;
	TArray<FQueued> Queue;
	TMap<uint8, float> CooldownLeft;
	TMap<FName, int32> LastVariant;

	int32 CurrentLine = INDEX_NONE;
	int32 CurrentPriority = 0;
	int32 ScriptedCounter = 0;
	TArray<int32> ScriptedSlots;
	float CurrentTimeLeft = 0.f;
	bool bDucking = false;
	float CurrentDuration = 0.f;
	float GapLeft = 0.f;
	float IdleTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudio;
};
