#include "ContribLighting.h"

#include "CineCameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureCube.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Scalability.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogContribLighting, Log, All);

namespace
{
	struct FLightingFile
	{
		TMap<FName, TSharedPtr<FJsonObject>> Presets;
		TMap<FName, FName> IdToPreset;
		/** postprocess_realism.json "_tier_overrides" per preset ({"Epic": {...}, "Medium": {...}}). */
		TMap<FName, TSharedPtr<FJsonObject>> TierOverrides;
		int32 NumRealism = 0;
		bool bLoaded = false;
	};

	TSharedPtr<FJsonObject> CloneObject(const TSharedPtr<FJsonObject>& Src)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		for (const auto& Pair : Src->Values)
		{
			const FString Key(*Pair.Key);
			const TSharedPtr<FJsonObject>* Child = nullptr;
			if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Object && Pair.Value->TryGetObject(Child) && Child)
			{
				Out->SetObjectField(Key, CloneObject(*Child));
			}
			else
			{
				Out->SetField(Key, Pair.Value);
			}
		}
		return Out;
	}

	/** Deep-merges Src into Dst (Src wins; nested objects merge key by key). "_" keys are documentation and skipped. */
	int32 MergeInto(const TSharedPtr<FJsonObject>& Dst, const TSharedPtr<FJsonObject>& Src)
	{
		int32 Merged = 0;
		for (const auto& Pair : Src->Values)
		{
			const FString Key(*Pair.Key);
			if (Key.StartsWith(TEXT("_")) || !Pair.Value.IsValid()) { continue; }
			const TSharedPtr<FJsonObject>* SrcChild = nullptr;
			const TSharedPtr<FJsonObject>* DstChild = nullptr;
			if (Pair.Value->Type == EJson::Object && Pair.Value->TryGetObject(SrcChild) && SrcChild
				&& Dst->TryGetObjectField(Key, DstChild) && DstChild && DstChild->IsValid())
			{
				Merged += MergeInto(*DstChild, *SrcChild);
			}
			else
			{
				Dst->SetField(Key, Pair.Value);
				++Merged;
			}
		}
		return Merged;
	}

	/** Vison's realism layer (Content/Story/postprocess_realism.json): same 11 preset ids as lighting_presets.json.
	 *  Its SkyAtmosphere / VolumetricCloud / ExponentialHeightFog / PostProcessVolume / CineCamera blocks merge over
	 *  the lighting presets (it has no light, exposure or grading keys, so the physical sun and EV100 stay). */
	void LoadRealismLayer(FLightingFile& File)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/postprocess_realism.json");
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogContribLighting, Warning, TEXT("postprocess_realism.json not found or invalid (%s); lighting presets only."), *Path);
			return;
		}
		const TSharedPtr<FJsonObject>* Presets = nullptr;
		if (!Root->TryGetObjectField(TEXT("presets"), Presets) || !Presets) { return; }
		static const TCHAR* const Blocks[] = { TEXT("SkyAtmosphere"), TEXT("VolumetricCloud"), TEXT("ExponentialHeightFog"),
			TEXT("PostProcessVolume"), TEXT("CineCamera") };
		int32 Keys = 0;
		for (const auto& Pair : (*Presets)->Values)
		{
			const FString Id(*Pair.Key);
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Obj) || !Obj) { continue; }
			TSharedPtr<FJsonObject>& Target = File.Presets.FindOrAdd(FName(*Id));
			if (!Target.IsValid()) { Target = MakeShared<FJsonObject>(); }
			for (const TCHAR* BlockName : Blocks)
			{
				const TSharedPtr<FJsonObject>* Src = nullptr;
				if (!(*Obj)->TryGetObjectField(BlockName, Src) || !Src) { continue; }
				const TSharedPtr<FJsonObject>* Dst = nullptr;
				if (Target->TryGetObjectField(BlockName, Dst) && Dst && Dst->IsValid()) { Keys += MergeInto(*Dst, *Src); }
				else
				{
					TSharedPtr<FJsonObject> Fresh = MakeShared<FJsonObject>();
					Keys += MergeInto(Fresh, *Src);
					Target->SetObjectField(BlockName, Fresh);
				}
			}
			const TSharedPtr<FJsonObject>* Tiers = nullptr;
			if ((*Obj)->TryGetObjectField(TEXT("_tier_overrides"), Tiers) && Tiers) { File.TierOverrides.Add(FName(*Id), *Tiers); }
			++File.NumRealism;
		}
		UE_LOG(LogContribLighting, Log, TEXT("Merged %d realism presets (%d keys) from postprocess_realism.json."), File.NumRealism, Keys);
	}

	const FLightingFile& GetFile()
	{
		static FLightingFile File;
		if (File.bLoaded) { return File; }
		File.bLoaded = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Story/lighting_presets.json");
		if (!FFileHelper::LoadFileToString(Text, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogContribLighting, Warning, TEXT("lighting_presets.json not found or invalid (%s); using C++ presets only."), *Path);
			return File;
		}
		const TSharedPtr<FJsonObject>* Presets = nullptr;
		if (Root->TryGetObjectField(TEXT("presets"), Presets) && Presets)
		{
			for (const auto& Pair : (*Presets)->Values)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (Pair.Value.IsValid() && Pair.Value->TryGetObject(Obj) && Obj)
				{
					File.Presets.Add(FName(*Pair.Key), *Obj);
				}
			}
		}
		const TSharedPtr<FJsonObject>* Mapping = nullptr;
		const TSharedPtr<FJsonObject>* IdMap = nullptr;
		if (Root->TryGetObjectField(TEXT("chapter_mapping"), Mapping) && Mapping
			&& (*Mapping)->TryGetObjectField(TEXT("FEnvironmentPreset_Id_to_preset"), IdMap) && IdMap)
		{
			for (const auto& Pair : (*IdMap)->Values)
			{
				FString Target;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(Target) && !Target.IsEmpty())
				{
					File.IdToPreset.Add(FName(*Pair.Key), FName(*Target));
				}
			}
		}
		UE_LOG(LogContribLighting, Log, TEXT("Loaded %d contrib lighting presets (%d id mappings)."), File.Presets.Num(), File.IdToPreset.Num());
		LoadRealismLayer(File);
		return File;
	}

	FProperty* FindProp(UStruct* Struct, const FString& Key)
	{
		if (FProperty* P = Struct->FindPropertyByName(FName(*Key))) { return P; }
		// Renamed across 5.x.
		if (Key == TEXT("FogInscatteringColor")) { return Struct->FindPropertyByName(TEXT("FogInscatteringLuminance")); }
		if (Key == TEXT("AerialPespectiveViewDistanceScale")) { return Struct->FindPropertyByName(TEXT("AerialPerspectiveViewDistanceScale")); }
		return nullptr;
	}

	bool WriteValue(FProperty* Prop, void* Container, const TSharedPtr<FJsonValue>& Value)
	{
		void* Addr = Prop->ContainerPtrToValuePtr<void>(Container);
		// FColor fields (e.g. VolumetricFogAlbedo) are given as linear 0..1 colours in the JSON.
		if (const FStructProperty* SP = CastField<FStructProperty>(Prop))
		{
			if (SP->Struct == TBaseStructure<FColor>::Get())
			{
				const TSharedPtr<FJsonObject>* C = nullptr;
				if (!Value->TryGetObject(C) || !C) { return false; }
				double R = 1, G = 1, B = 1, A = 1;
				(*C)->TryGetNumberField(TEXT("R"), R);
				(*C)->TryGetNumberField(TEXT("G"), G);
				(*C)->TryGetNumberField(TEXT("B"), B);
				(*C)->TryGetNumberField(TEXT("A"), A);
				*static_cast<FColor*>(Addr) = FLinearColor(float(R), float(G), float(B), float(A)).ToFColor(true);
				return true;
			}
		}
		return FJsonObjectConverter::JsonValueToUProperty(Value, Prop, Addr, 0, 0);
	}

	/** Writes every non-"_" key of Obj into Container (of type Struct). Skips the keys in Special. */
	int32 ApplyObject(UStruct* Struct, void* Container, const TSharedPtr<FJsonObject>& Obj, const TSet<FString>& Special,
		bool bPostProcess, float LuxScale, bool bLight, const TCHAR* Block, int32& Skipped)
	{
		int32 Written = 0;
		const float EvShift = FMath::Log2(FMath::Max(LuxScale, 1e-12f));
		for (const auto& Pair : Obj->Values)
		{
			const FString Key(*Pair.Key);
			if (Key.StartsWith(TEXT("_")) || Special.Contains(Key) || !Pair.Value.IsValid()) { continue; }
			FProperty* Prop = FindProp(Struct, Key);
			if (!Prop || !WriteValue(Prop, Container, Pair.Value))
			{
				++Skipped;
				UE_LOG(LogContribLighting, Verbose, TEXT("  skip %s.%s"), Block, *Key);
				continue;
			}
			if (bLight && Key == TEXT("Intensity"))
			{
				if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
				{
					FP->SetPropertyValue_InContainer(Container, FP->GetPropertyValue_InContainer(Container) * LuxScale);
				}
			}
			if (bPostProcess)
			{
				if ((Key == TEXT("AutoExposureMinBrightness") || Key == TEXT("AutoExposureMaxBrightness")))
				{
					if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
					{
						FP->SetPropertyValue_InContainer(Container, FP->GetPropertyValue_InContainer(Container) + EvShift);
					}
				}
				if (FBoolProperty* Override = CastField<FBoolProperty>(Struct->FindPropertyByName(FName(*(TEXT("bOverride_") + Prop->GetName())))))
				{
					Override->SetPropertyValue_InContainer(Container, true);
				}
			}
			++Written;
		}
		return Written;
	}

	bool ReadRotator(const TSharedPtr<FJsonObject>& Obj, FRotator& Out)
	{
		const TSharedPtr<FJsonObject>* R = nullptr;
		if (!Obj->TryGetObjectField(TEXT("Rotation"), R) || !R) { return false; }
		double P = 0, Y = 0, Ro = 0;
		(*R)->TryGetNumberField(TEXT("Pitch"), P);
		(*R)->TryGetNumberField(TEXT("Yaw"), Y);
		(*R)->TryGetNumberField(TEXT("Roll"), Ro);
		Out = FRotator(float(P), float(Y), float(Ro));
		return true;
	}

	void ApplyVisible(USceneComponent* Comp, const TSharedPtr<FJsonObject>& Obj)
	{
		bool bVisible = true;
		if (Obj->TryGetBoolField(TEXT("Visible"), bVisible)) { Comp->SetVisibility(bVisible); }
	}

	/** The merged preset plus the realism layer's delta for the active tier (High = base; Epic = sg 3+;
	 *  Medium = sg 0-1). Keyed off GlobalIlluminationQuality because the deltas are mostly Lumen. */
	TSharedPtr<FJsonObject> EffectivePreset(FName PresetKey)
	{
		const FLightingFile& File = GetFile();
		const TSharedPtr<FJsonObject>* PresetPtr = File.Presets.Find(PresetKey);
		if (!PresetPtr || !PresetPtr->IsValid()) { return nullptr; }
		const TSharedPtr<FJsonObject>* Tiers = File.TierOverrides.Find(PresetKey);
		const int32 Tier = Scalability::GetQualityLevels().GlobalIlluminationQuality;
		const TCHAR* TierName = Tier >= 3 ? TEXT("Epic") : (Tier <= 1 ? TEXT("Medium") : nullptr);
		const TSharedPtr<FJsonObject>* Delta = nullptr;
		if (!TierName || !Tiers || !Tiers->IsValid() || !(*Tiers)->TryGetObjectField(TierName, Delta) || !Delta) { return *PresetPtr; }
		TSharedPtr<FJsonObject> Out = CloneObject(*PresetPtr);
		MergeInto(Out, *Delta);
		return Out;
	}
}

namespace ContribLighting
{
	FName PresetKeyFor(FName EnvironmentPresetId)
	{
		const FName* Found = GetFile().IdToPreset.Find(EnvironmentPresetId);
		return Found ? *Found : NAME_None;
	}

	bool HasPreset(FName PresetKey)
	{
		return GetFile().Presets.Contains(PresetKey);
	}

	int32 GetNumPresets()
	{
		return GetFile().Presets.Num();
	}

	int32 GetNumRealismPresets()
	{
		return GetFile().NumRealism;
	}

	FName KeyForEnvironment(FName EnvironmentPresetId)
	{
		if (HasPreset(EnvironmentPresetId)) { return EnvironmentPresetId; }
		const FName Key = PresetKeyFor(EnvironmentPresetId);
		if (!Key.IsNone() && HasPreset(Key)) { return Key; }
		// Free play has no story look; Vison recommends reusing "city" (clear 13:00 daylight).
		if (EnvironmentPresetId == FName(TEXT("arena")) && HasPreset(TEXT("city"))) { return TEXT("city"); }
		return NAME_None;
	}

	int32 Apply(AActor* EnvActor, FName PresetKey, float LuxScale, int32* OutSkipped)
	{
		int32 Skipped = 0;
		int32 Written = 0;
		const TSharedPtr<FJsonObject> Preset = EffectivePreset(PresetKey);
		if (!EnvActor || !Preset.IsValid())
		{
			if (OutSkipped) { *OutSkipped = 0; }
			return 0;
		}
		const TSet<FString> LightSpecial = { TEXT("Rotation"), TEXT("Visible") };
		const TSet<FString> SkySpecial = { TEXT("Cubemap"), TEXT("Visible") };
		const TSet<FString> NoSpecial = { TEXT("Visible") };

		auto Block = [&Preset](const TCHAR* Name) -> TSharedPtr<FJsonObject>
		{
			const TSharedPtr<FJsonObject>* B = nullptr;
			return (Preset->TryGetObjectField(Name, B) && B) ? *B : nullptr;
		};

		TInlineComponentArray<UActorComponent*> Comps(EnvActor);
		for (UActorComponent* Comp : Comps)
		{
			if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Comp))
			{
				const bool bMoon = L->GetAtmosphereSunLightIndex() == 1 || L->GetName().Contains(TEXT("Moon"));
				const TCHAR* Name = bMoon ? TEXT("MoonLight") : TEXT("DirectionalLight");
				if (TSharedPtr<FJsonObject> B = Block(Name))
				{
					Written += ApplyObject(L->GetClass(), L, B, LightSpecial, false, LuxScale, true, Name, Skipped);
					FRotator Rot;
					if (ReadRotator(B, Rot)) { L->SetWorldRotation(Rot); }
					ApplyVisible(L, B);
					L->MarkRenderStateDirty();
				}
			}
			else if (USkyLightComponent* S = Cast<USkyLightComponent>(Comp))
			{
				if (TSharedPtr<FJsonObject> B = Block(TEXT("SkyLight")))
				{
					Written += ApplyObject(S->GetClass(), S, B, SkySpecial, false, LuxScale, false, TEXT("SkyLight"), Skipped);
					const TSharedPtr<FJsonObject>* Cube = nullptr;
					FString AssetPath;
					UTextureCube* Tex = nullptr;
					if (B->TryGetObjectField(TEXT("Cubemap"), Cube) && Cube && (*Cube)->TryGetStringField(TEXT("asset"), AssetPath))
					{
						Tex = LoadObject<UTextureCube>(nullptr, *AssetPath);
					}
					if (S->SourceType == SLS_SpecifiedCubemap)
					{
						if (Tex) { S->SetCubemap(Tex); }
						else
						{
							// Vison's documented fallback when the HDRI is missing.
							S->SourceType = SLS_CapturedScene;
							S->SetRealTimeCapture(true);
						}
					}
					S->MarkRenderStateDirty();
					S->RecaptureSky();
				}
			}
			else if (USkyAtmosphereComponent* A = Cast<USkyAtmosphereComponent>(Comp))
			{
				if (TSharedPtr<FJsonObject> B = Block(TEXT("SkyAtmosphere")))
				{
					Written += ApplyObject(A->GetClass(), A, B, NoSpecial, false, LuxScale, false, TEXT("SkyAtmosphere"), Skipped);
					A->MarkRenderStateDirty();
				}
			}
			else if (UExponentialHeightFogComponent* F = Cast<UExponentialHeightFogComponent>(Comp))
			{
				if (TSharedPtr<FJsonObject> B = Block(TEXT("ExponentialHeightFog")))
				{
					Written += ApplyObject(F->GetClass(), F, B, NoSpecial, false, LuxScale, false, TEXT("ExponentialHeightFog"), Skipped);
					F->MarkRenderStateDirty();
				}
			}
			else if (UVolumetricCloudComponent* C = Cast<UVolumetricCloudComponent>(Comp))
			{
				if (TSharedPtr<FJsonObject> B = Block(TEXT("VolumetricCloud")))
				{
					Written += ApplyObject(C->GetClass(), C, B, NoSpecial, false, LuxScale, false, TEXT("VolumetricCloud"), Skipped);
					ApplyVisible(C, B);
					C->MarkRenderStateDirty();
				}
			}
			else if (UPostProcessComponent* PP = Cast<UPostProcessComponent>(Comp))
			{
				if (TSharedPtr<FJsonObject> B = Block(TEXT("PostProcessVolume")))
				{
					Written += ApplyObject(FPostProcessSettings::StaticStruct(), &PP->Settings, B, NoSpecial, true, LuxScale, false,
						TEXT("PostProcessVolume"), Skipped);
				}
			}
		}
		UE_LOG(LogContribLighting, Log, TEXT("Applied contrib lighting preset '%s' (lux scale %g): %d properties, %d skipped."),
			*PresetKey.ToString(), LuxScale, Written, Skipped);
		if (OutSkipped) { *OutSkipped = Skipped; }
		return Written;
	}

	int32 ApplyCamera(UCineCameraComponent* Camera, FName PresetKey, bool bAllowDOF, int32* OutSkipped)
	{
		int32 Skipped = 0;
		int32 Written = 0;
		const TSharedPtr<FJsonObject> PresetObj = EffectivePreset(PresetKey);
		const TSharedPtr<FJsonObject>* CamPtr = nullptr;
		if (!Camera || !PresetObj.IsValid() || !PresetObj->TryGetObjectField(TEXT("CineCamera"), CamPtr) || !CamPtr)
		{
			if (OutSkipped) { *OutSkipped = 0; }
			return 0;
		}
		const TSharedPtr<FJsonObject>& Cam = *CamPtr;
		// FilmbackPreset is a name only (Filmback carries the real sensor size).
		const TSet<FString> Special = { TEXT("FilmbackPreset"), TEXT("PostProcessOverrides") };
		Written += ApplyObject(Camera->GetClass(), Camera, Cam, Special, false, 1.f, false, TEXT("CineCamera"), Skipped);
		// The director frames every shot itself: there's no actor to track, so focus is manual at the
		// subject distance (AStoryDirector::UpdateCamera keeps ManualFocusDistance on the subject).
		if (Camera->FocusSettings.FocusMethod == ECameraFocusMethod::Tracking) { Camera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual; }
		if (!bAllowDOF)
		{
			Camera->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
		}
		const TSharedPtr<FJsonObject>* PPO = nullptr;
		if (Cam->TryGetObjectField(TEXT("PostProcessOverrides"), PPO) && PPO)
		{
			const TSet<FString> None;
			Written += ApplyObject(FPostProcessSettings::StaticStruct(), &Camera->PostProcessSettings, *PPO, None, true, 1.f, false,
				TEXT("CineCamera.PostProcessOverrides"), Skipped);
			Camera->PostProcessBlendWeight = 1.f;
		}
		UE_LOG(LogContribLighting, Log, TEXT("Applied contrib cine camera '%s': %d properties, %d skipped."), *PresetKey.ToString(), Written, Skipped);
		if (OutSkipped) { *OutSkipped = Skipped; }
		return Written;
	}
}
