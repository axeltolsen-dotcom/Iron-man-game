#include "NpcManifest.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

namespace
{
	struct FData
	{
		bool bTried = false;
		bool bLoaded = false;
		TArray<FNpcBodyDef> Bodies;
		TArray<FNpcVoiceDef> Voices;
		TMap<FName, TArray<FString>> Anims;
		TMap<FString, TArray<FString>> ConventionCache; // "voice|cat" -> found paths
	};

	FString PkgOf(const FString& Path)
	{
		FString Pkg = Path, Obj;
		if (Path.Split(TEXT("."), &Pkg, &Obj)) { return Pkg; }
		return Path;
	}
	FString ObjOf(const FString& Path)
	{
		if (Path.Contains(TEXT("."))) { return Path; }
		return Path + TEXT(".") + FPackageName::GetShortName(Path);
	}
	bool Exists(const FString& Path) { return !Path.IsEmpty() && FPackageName::IsValidLongPackageName(PkgOf(Path)) && FPackageName::DoesPackageExist(PkgOf(Path)); }

	TArray<FString> StrArray(const TSharedPtr<FJsonObject>& O, const FString& Key)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, A) && A)
		{
			for (const TSharedPtr<FJsonValue>& V : *A) { FString S; if (V.IsValid() && V->TryGetString(S)) { Out.Add(S); } }
		}
		return Out;
	}

	void AddPlaceholders(FData& D)
	{
		// Placeholder voice choices for the character setup when no manifest voices exist (silent; the UI says so).
		const struct { const TCHAR* Id; const TCHAR* Name; bool bF; } P[] = {
			{ TEXT("placeholder-warm"), TEXT("Warm (placeholder)"), false },
			{ TEXT("placeholder-bright"), TEXT("Bright (placeholder)"), true },
			{ TEXT("placeholder-calm"), TEXT("Calm (placeholder)"), false },
			{ TEXT("placeholder-crisp"), TEXT("Crisp (placeholder)"), true } };
		for (const auto& E : P)
		{
			FNpcVoiceDef V; V.Id = E.Id; V.Name = E.Name; V.bFeminine = E.bF; V.bPlaceholder = true;
			D.Voices.Add(V);
		}
	}

	FData& Data()
	{
		static FData D;
		if (D.bTried) { return D; }
		D.bTried = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString Paths[] = { FPaths::ProjectContentDir() / TEXT("NPC/npc_manifest.json"), FPaths::ProjectContentDir() / TEXT("NPC/Data/npc_manifest.json") };
		for (const FString& P : Paths)
		{
			if (FFileHelper::LoadFileToString(Text, *P) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid()) { break; }
			Root.Reset();
		}
		if (Root.IsValid())
		{
			D.bLoaded = true;
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Root->TryGetArrayField(TEXT("bodies"), Arr) && Arr)
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const TSharedPtr<FJsonObject>* O = nullptr;
					if (!V.IsValid() || !V->TryGetObject(O) || !O) { continue; }
					FNpcBodyDef B;
					(*O)->TryGetStringField(TEXT("id"), B.Id);
					if (!(*O)->TryGetStringField(TEXT("mesh"), B.MeshPath)) { (*O)->TryGetStringField(TEXT("skeletalMesh"), B.MeshPath); }
					if (!(*O)->TryGetStringField(TEXT("animBlueprint"), B.AnimBlueprintPath)) { (*O)->TryGetStringField(TEXT("animClass"), B.AnimBlueprintPath); }
					(*O)->TryGetBoolField(TEXT("feminine"), B.bFeminine);
					FString G; if ((*O)->TryGetStringField(TEXT("gender"), G)) { B.bFeminine = G.StartsWith(TEXT("f")); }
					double N = 0.0;
					if ((*O)->TryGetNumberField(TEXT("weight"), N)) { B.Weight = float(N); }
					if ((*O)->TryGetNumberField(TEXT("scaleMin"), N)) { B.ScaleMin = float(N); }
					if ((*O)->TryGetNumberField(TEXT("scaleMax"), N)) { B.ScaleMax = float(N); }
					if (Exists(B.MeshPath)) { D.Bodies.Add(B); }
				}
			}
			const TSharedPtr<FJsonObject>* AnimObj = nullptr;
			if ((Root->TryGetObjectField(TEXT("animCategories"), AnimObj) || Root->TryGetObjectField(TEXT("anims"), AnimObj)) && AnimObj)
			{
				for (const auto& Pair : (*AnimObj)->Values)
				{
					TArray<FString>& L = D.Anims.FindOrAdd(FName(*FString(*Pair.Key).ToLower()));
					const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
					FString One;
					if (Pair.Value->TryGetArray(A) && A) { for (const TSharedPtr<FJsonValue>& V : *A) { FString S; if (V->TryGetString(S) && Exists(S)) { L.Add(S); } } }
					else if (Pair.Value->TryGetString(One) && Exists(One)) { L.Add(One); }
				}
			}
			if ((Root->TryGetArrayField(TEXT("voiceSets"), Arr) || Root->TryGetArrayField(TEXT("voices"), Arr)) && Arr)
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					FNpcVoiceDef Voice;
					FString IdStr;
					const TSharedPtr<FJsonObject>* O = nullptr;
					if (V->TryGetString(IdStr)) { Voice.Id = FName(*IdStr); Voice.Name = IdStr; }
					else if (V->TryGetObject(O) && O)
					{
						(*O)->TryGetStringField(TEXT("id"), IdStr);
						Voice.Id = FName(*IdStr);
						if (!(*O)->TryGetStringField(TEXT("name"), Voice.Name))
						{
							Voice.Name = IdStr;
							Voice.Name.RemoveFromStart(TEXT("ped_"));
							if (Voice.Name.Len() > 0) { Voice.Name = Voice.Name.Left(1).ToUpper() + Voice.Name.Mid(1); }
							FString Note;
							if ((*O)->TryGetStringField(TEXT("note"), Note) && !Note.IsEmpty()) { Voice.Name += TEXT(" - ") + Note; }
						}
						FString G;
						if ((*O)->TryGetStringField(TEXT("gender"), G)) { Voice.bFeminine = G.StartsWith(TEXT("f")); }
						(*O)->TryGetStringField(TEXT("preview"), Voice.PreviewPath);
						const TSharedPtr<FJsonObject>* Lines = nullptr;
						if (((*O)->TryGetObjectField(TEXT("categories"), Lines) || (*O)->TryGetObjectField(TEXT("lines"), Lines)) && Lines)
						{
							for (const auto& Pair : (*Lines)->Values)
							{
								TArray<FString>& L = Voice.Lines.FindOrAdd(FName(*FString(*Pair.Key).ToLower()));
								const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
								if (Pair.Value->TryGetArray(A) && A) { for (const TSharedPtr<FJsonValue>& LV : *A) { FString S; if (LV->TryGetString(S)) { L.Add(S); } } }
							}
						}
					}
					if (!Voice.Id.IsNone()) { D.Voices.Add(Voice); }
				}
			}
		}
		if (D.Bodies.Num() == 0)
		{
			// No "bodies" block: use any skeletal mesh imported under /Game/NPC (the UAL / Mannequin_F GLB imports),
			// so the UAL clips play on their own skeleton without a retarget.
			if (IAssetRegistry* AR = IAssetRegistry::Get())
			{
				TArray<FAssetData> Found;
				AR->GetAssetsByPath(FName(TEXT("/Game/NPC")), Found, true);
				for (const FAssetData& A : Found)
				{
					if (A.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName()) { continue; }
					FNpcBodyDef B;
					B.Id = A.AssetName.ToString();
					B.MeshPath = A.GetObjectPathString();
					const FString Lower = A.GetObjectPathString().ToLower();
					B.bFeminine = Lower.Contains(TEXT("_f")) || Lower.Contains(TEXT("female")) || Lower.Contains(TEXT("quinn"));
					D.Bodies.Add(B);
				}
			}
		}
		if (D.Voices.Num() == 0) { AddPlaceholders(D); }
		return D;
	}

	const TArray<FString>& LinesFor(FName VoiceId, FName Category)
	{
		FData& D = Data();
		const FString Key = VoiceId.ToString() + TEXT("|") + Category.ToString();
		if (const TArray<FString>* Hit = D.ConventionCache.Find(Key)) { return *Hit; }
		TArray<FString> Found;
		if (const FNpcVoiceDef* V = NpcManifest::FindVoice(VoiceId))
		{
			if (const TArray<FString>* L = V->Lines.Find(Category)) { for (const FString& P : *L) { if (Exists(P)) { Found.Add(P); } } }
		}
		if (Found.Num() == 0 && Category == FName(TEXT("explosion"))) { Found = LinesFor(VoiceId, FName(TEXT("car_explode"))); }
		if (Found.Num() == 0 && Category == FName(TEXT("greeting"))) { Found = LinesFor(VoiceId, FName(TEXT("thanks"))); }
		if (Found.Num() == 0 && !VoiceId.IsNone())
		{
			for (int32 n = 1; n <= 12; ++n)
			{
				const FString P2 = FString::Printf(TEXT("/Game/NPC/Voices/%s/%s_%02d"), *VoiceId.ToString(), *Category.ToString(), n);
				const FString P1 = FString::Printf(TEXT("/Game/NPC/Voices/%s/%s_%d"), *VoiceId.ToString(), *Category.ToString(), n);
				if (Exists(P2)) { Found.Add(P2); }
				else if (Exists(P1)) { Found.Add(P1); }
				else if (n > 2) { break; }
			}
		}
		return D.ConventionCache.Add(Key, Found);
	}
}

namespace NpcManifest
{
	bool IsLoaded() { return Data().bLoaded; }
	const TArray<FNpcBodyDef>& Bodies() { return Data().Bodies; }
	const TArray<FNpcVoiceDef>& Voices() { return Data().Voices; }

	const FNpcVoiceDef* FindVoice(FName Id)
	{
		for (const FNpcVoiceDef& V : Data().Voices) { if (V.Id == Id) { return &V; } }
		return nullptr;
	}

	const TArray<FString>& Anims(FName Category)
	{
		static const TArray<FString> Empty;
		const TArray<FString>* L = Data().Anims.Find(Category);
		return L ? *L : Empty;
	}

	USoundBase* LoadSound(const FString& Path)
	{
		if (!Exists(Path)) { return nullptr; }
		return LoadObject<USoundBase>(nullptr, *ObjOf(Path));
	}

	UAnimSequenceBase* LoadAnim(const FString& Path)
	{
		if (!Exists(Path)) { return nullptr; }
		return LoadObject<UAnimSequenceBase>(nullptr, *ObjOf(Path));
	}

	USkeletalMesh* LoadSkeletalMesh(const FString& Path)
	{
		if (!Exists(Path)) { return nullptr; }
		return LoadObject<USkeletalMesh>(nullptr, *ObjOf(Path));
	}

	UClass* LoadAnimClass(const FString& Path)
	{
		if (!Exists(Path)) { return nullptr; }
		FString Obj = ObjOf(Path);
		if (!Obj.EndsWith(TEXT("_C"))) { Obj += TEXT("_C"); }
		return LoadClass<UAnimInstance>(nullptr, *Obj);
	}

	USoundBase* PickLine(FName VoiceId, FName Category, FRandomStream& Rng)
	{
		const TArray<FString>& L = LinesFor(VoiceId, Category);
		if (L.Num() == 0) { return nullptr; }
		return LoadSound(L[Rng.RandRange(0, L.Num() - 1)]);
	}

	USoundBase* PreviewLine(FName VoiceId)
	{
		if (const FNpcVoiceDef* V = FindVoice(VoiceId))
		{
			if (USoundBase* S = LoadSound(V->PreviewPath)) { return S; }
		}
		FRandomStream Rng(1);
		for (const TCHAR* Cat : { TEXT("greeting"), TEXT("preview"), TEXT("idle"), TEXT("see_ironman") })
		{
			if (USoundBase* S = PickLine(VoiceId, FName(Cat), Rng)) { return S; }
		}
		return nullptr;
	}
}
