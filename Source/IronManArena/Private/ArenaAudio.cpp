#include "ArenaAudio.h"
#include "ArenaGameUserSettings.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

namespace
{
	struct FSfxEvent
	{
		FString Asset;
		FString SoundClass;
		bool bLooping = false;
		TWeakObjectPtr<USoundBase> Cached;
		bool bTried = false;
	};
	struct FSfxDb
	{
		bool bLoaded = false;
		TMap<FName, FSfxEvent> Events;
		TMap<FName, TArray<FName>> Groups; // base name -> takes
		TMap<FName, int32> LastPick;
		TMap<FString, TWeakObjectPtr<USoundClass>> Classes;
		TWeakObjectPtr<USoundAttenuation> Atten; // rooted (static lifetime would outlive UObject shutdown with strong ptrs)
	};
	FSfxDb& Db() { static FSfxDb D; return D; }

	FName BaseName(const FString& Id)
	{
		FString S = Id;
		int32 Us = INDEX_NONE;
		if (S.FindLastChar(TEXT('_'), Us))
		{
			const FString Tail = S.Mid(Us + 1);
			const bool bVar = Tail.StartsWith(TEXT("var")) && Tail.Len() > 3 && Tail.Mid(3).IsNumeric();
			if (bVar || (Tail.IsNumeric() && Tail.Len() <= 2)) { S = S.Left(Us); }
		}
		return FName(*S);
	}

	void Load()
	{
		FSfxDb& D = Db();
		if (D.bLoaded) { return; }
		D.bLoaded = true;
		const FString Paths[] = {
			FPaths::ProjectContentDir() / TEXT("Audio/sfx_manifest.json"),
			FPaths::ProjectDir() / TEXT("../contrib/audio/sfx_manifest.json"),
		};
		FString Json;
		for (const FString& P : Paths) { if (FFileHelper::LoadFileToString(Json, *P)) { break; } }
		if (Json.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("ArenaAudio: sfx_manifest.json not found (%s) - game SFX are off"), *Paths[0]);
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (Json.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) { return; }
		const TSharedPtr<FJsonObject>* Ev = nullptr;
		if (!Root->TryGetObjectField(TEXT("events"), Ev) || !Ev) { return; }
		for (const auto& Pair : (*Ev)->Values)
		{
			const TSharedPtr<FJsonObject> O = Pair.Value.IsValid() ? Pair.Value->AsObject() : nullptr;
			if (!O.IsValid()) { continue; }
			FSfxEvent E;
			O->TryGetStringField(TEXT("asset"), E.Asset);
			O->TryGetStringField(TEXT("sound_class"), E.SoundClass);
			O->TryGetBoolField(TEXT("looping"), E.bLooping);
			if (E.Asset.IsEmpty()) { continue; }
			const FString Id(*Pair.Key);
			const FName Name(*Id);
			D.Events.Add(Name, E);
			D.Groups.FindOrAdd(BaseName(Id)).AddUnique(Name);
		}
		UE_LOG(LogTemp, Log, TEXT("ArenaAudio: %d sfx events, %d groups"), D.Events.Num(), D.Groups.Num());
	}

	USoundBase* Resolve(FSfxEvent& E)
	{
		if (E.Cached.IsValid()) { return E.Cached.Get(); }
		if (E.bTried) { return nullptr; }
		E.bTried = true;
		FString Pkg = E.Asset, Obj = E.Asset;
		if (!Obj.Contains(TEXT("."))) { Obj = Pkg + TEXT(".") + FPackageName::GetShortName(Pkg); }
		else { Pkg = Obj.Left(Obj.Find(TEXT("."))); }
		if (!FPackageName::DoesPackageExist(Pkg)) { return nullptr; }
		USoundBase* S = LoadObject<USoundBase>(nullptr, *Obj);
		if (USoundWave* W = Cast<USoundWave>(S)) { if (E.bLooping) { W->bLooping = true; } } // the looping cue stubs are empty: loop the wave
		E.Cached = S;
		if (S) { S->AddToRoot(); }
		return S;
	}

	FSfxEvent* Pick(FName Event)
	{
		Load();
		FSfxDb& D = Db();
		if (const TArray<FName>* G = D.Groups.Find(Event))
		{
			if (G->Num() > 0)
			{
				int32& Last = D.LastPick.FindOrAdd(Event, -1);
				int32 I = FMath::RandRange(0, G->Num() - 1);
				if (G->Num() > 1 && I == Last) { I = (I + 1) % G->Num(); }
				Last = I;
				if (FSfxEvent* E = D.Events.Find((*G)[I])) { return E; }
			}
		}
		return D.Events.Find(Event);
	}

	USoundClass* ClassFor(const FString& Name)
	{
		if (Name.IsEmpty()) { return nullptr; }
		FSfxDb& D = Db();
		if (TWeakObjectPtr<USoundClass>* C = D.Classes.Find(Name)) { return C->Get(); }
		const FString Pkg = FString::Printf(TEXT("/Game/Audio/Classes/%s"), *Name);
		USoundClass* C = FPackageName::DoesPackageExist(Pkg) ? LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("%s.%s"), *Pkg, *Name)) : nullptr;
		if (C) { C->AddToRoot(); }
		D.Classes.Add(Name, C);
		return C;
	}

	bool Allowed(const FSfxEvent& E) { return E.SoundClass != TEXT("SC_Voice") || UArenaGameUserSettings::AreVoicesEnabled(); }

	USoundAttenuation* Atten()
	{
		FSfxDb& D = Db();
		if (!D.Atten.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>();
			A->Attenuation.bAttenuate = true;
			A->Attenuation.bSpatialize = true;
			A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			A->Attenuation.AttenuationShapeExtents = FVector(600.f, 0.f, 0.f);
			A->Attenuation.FalloffDistance = 9000.f;
			A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
			A->AddToRoot();
			D.Atten = A;
		}
		return D.Atten.Get();
	}

	void Setup(UAudioComponent* C, const FSfxEvent& E)
	{
		if (!C) { return; }
		if (USoundClass* SC = ClassFor(E.SoundClass)) { C->SoundClassOverride = SC; }
	}
}

USoundBase* ArenaAudio::Find(FName Event)
{
	FSfxEvent* E = Pick(Event);
	return E ? Resolve(*E) : nullptr;
}

bool ArenaAudio::Has(FName Event) { return Find(Event) != nullptr; }

int32 ArenaAudio::NumEvents() { Load(); return Db().Events.Num(); }

void ArenaAudio::Play2D(const UObject* Ctx, FName Event, float Volume, float Pitch)
{
	FSfxEvent* E = Pick(Event);
	USoundBase* S = E ? Resolve(*E) : nullptr;
	if (!S || !Ctx || !Allowed(*E)) { return; }
	if (UAudioComponent* C = UGameplayStatics::CreateSound2D(Ctx, S, Volume, Pitch, 0.f, nullptr, false, true))
	{
		Setup(C, *E);
		C->bIsUISound = E->SoundClass == TEXT("SC_UI"); // UI clicks still play while paused
		C->Play();
	}
}

void ArenaAudio::PlayAt(const UObject* Ctx, FName Event, const FVector& Location, float Volume, float PitchJitter)
{
	FSfxEvent* E = Pick(Event);
	USoundBase* S = E ? Resolve(*E) : nullptr;
	if (!S || !Ctx || !Allowed(*E)) { return; }
	const float Pitch = 1.f + FMath::FRandRange(-PitchJitter, PitchJitter);
	if (UAudioComponent* C = UGameplayStatics::SpawnSoundAtLocation(Ctx, S, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Atten(), nullptr, true))
	{
		Setup(C, *E);
	}
}

UAudioComponent* ArenaAudio::Attach(FName Event, USceneComponent* AttachTo, float Volume, bool bLoop)
{
	FSfxEvent* E = Pick(Event);
	USoundBase* S = E ? Resolve(*E) : nullptr;
	if (!S || !AttachTo || !Allowed(*E)) { return nullptr; }
	if (bLoop) { if (USoundWave* W = Cast<USoundWave>(S)) { W->bLooping = true; } }
	UAudioComponent* C = UGameplayStatics::SpawnSoundAttached(S, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, Volume, 1.f, 0.f, Atten(), nullptr, false);
	Setup(C, *E);
	return C;
}

UAudioComponent* ArenaAudio::Loop2D(const UObject* Ctx, FName Event, float Volume)
{
	FSfxEvent* E = Pick(Event);
	USoundBase* S = E ? Resolve(*E) : nullptr;
	if (!S || !Ctx || !Allowed(*E)) { return nullptr; }
	if (USoundWave* W = Cast<USoundWave>(S)) { W->bLooping = true; }
	UAudioComponent* C = UGameplayStatics::CreateSound2D(Ctx, S, Volume, 1.f, 0.f, nullptr, true, false);
	if (C) { Setup(C, *E); C->bIsUISound = true; C->Play(); }
	return C;
}
