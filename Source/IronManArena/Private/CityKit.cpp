#include "CityKit.h"

#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FKit
	{
		bool bTried = false;
		TMap<FString, FString> Materials; // key / name -> package path
		TMap<FString, FString> Props;
	};

	FKit& Kit()
	{
		static FKit K;
		if (K.bTried) { return K; }
		K.bTried = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::ProjectContentDir() / TEXT("CityKit/citykit_manifest.json");
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return K;
		}
		auto ReadMap = [&Root](const TCHAR* Field, TMap<FString, FString>& Out)
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (!Root->TryGetObjectField(Field, O) || !O || !O->IsValid()) { return; }
			for (const auto& Pair : (*O)->Values)
			{
				FString V;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(V)) { Out.Add(FString(*Pair.Key), V); }
			}
		};
		ReadMap(TEXT("materials"), K.Materials);
		ReadMap(TEXT("fallbacks"), K.Materials); // fallback keys win (Asphalt, Sidewalk ...)
		ReadMap(TEXT("props"), K.Props);
		return K;
	}

	FString ObjectPath(const FString& Pkg)
	{
		if (Pkg.Contains(TEXT("."))) { return Pkg; }
		return Pkg + TEXT(".") + FPackageName::GetShortName(Pkg);
	}

	FString PackageOnly(const FString& Path)
	{
		FString Pkg = Path, Obj;
		if (Path.Split(TEXT("."), &Pkg, &Obj)) { return Pkg; }
		return Path;
	}
}

namespace CityKit
{
	bool IsKitPresent() { return Kit().Materials.Num() > 0 || Kit().Props.Num() > 0; }

	UMaterialInterface* LoadMaterialPath(const FString& PackagePath)
	{
		if (PackagePath.IsEmpty()) { return nullptr; }
		static TMap<FString, TWeakObjectPtr<UMaterialInterface>> Cache;
		if (const TWeakObjectPtr<UMaterialInterface>* Hit = Cache.Find(PackagePath)) { if (Hit->IsValid()) { return Hit->Get(); } }
		UMaterialInterface* M = FPackageName::DoesPackageExist(PackageOnly(PackagePath))
			? LoadObject<UMaterialInterface>(nullptr, *ObjectPath(PackagePath)) : nullptr;
		if (M) { Cache.Add(PackagePath, M); }
		return M;
	}

	UStaticMesh* LoadMeshPath(const FString& PackagePath)
	{
		if (PackagePath.IsEmpty()) { return nullptr; }
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
		if (const TWeakObjectPtr<UStaticMesh>* Hit = Cache.Find(PackagePath)) { if (Hit->IsValid()) { return Hit->Get(); } }
		UStaticMesh* M = FPackageName::DoesPackageExist(PackageOnly(PackagePath))
			? LoadObject<UStaticMesh>(nullptr, *ObjectPath(PackagePath)) : nullptr;
		if (M) { Cache.Add(PackagePath, M); }
		return M;
	}

	UMaterialInterface* Material(const TCHAR* Key)
	{
		const FString* P = Kit().Materials.Find(Key);
		return P ? LoadMaterialPath(*P) : nullptr;
	}

	UStaticMesh* Prop(const TCHAR* Key)
	{
		const FString* P = Kit().Props.Find(Key);
		if (P) { return LoadMeshPath(*P); }
		// Convention fallback: /Game/CityKit/Props/<key>/SM_<key>
		return LoadMeshPath(FString::Printf(TEXT("/Game/CityKit/Props/%s/SM_%s"), Key, Key));
	}
}
