#include "ArenaFontLibrary.h"

#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontFaceInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

int32 UArenaFontLibrary::BuildCompositeFont(UFont* Font, const TArray<FName>& TypefaceNames, const TArray<UObject*>& Faces)
{
	if (!Font || TypefaceNames.Num() == 0 || TypefaceNames.Num() != Faces.Num()) { return 0; }
	Font->Modify();
	Font->FontCacheType = EFontCacheType::Runtime;
	FTypeface& Typeface = Font->GetMutableInternalCompositeFont().DefaultTypeface;
	Typeface.Fonts.Reset();
	int32 Written = 0;
	for (int32 i = 0; i < TypefaceNames.Num(); ++i)
	{
		if (!Faces[i]) { continue; }
		FTypefaceEntry& Entry = Typeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TypefaceNames[i];
		Entry.Font = FFontData(Faces[i]);
		++Written;
	}
#if WITH_EDITOR
	Font->PostEditChange();
#endif
	Font->MarkPackageDirty();
	return Written;
}

TArray<FName> UArenaFontLibrary::GetTypefaceNames(const UFont* Font)
{
	TArray<FName> Out;
	if (!Font) { return Out; }
	for (const FTypefaceEntry& Entry : Font->GetInternalCompositeFont().DefaultTypeface.Fonts) { Out.Add(Entry.Name); }
	return Out;
}

UObject* UArenaFontLibrary::CreateFontFaceFromFile(const FString& Filename, const FString& PackagePath)
{
#if WITH_EDITORONLY_DATA
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() == 0) { return nullptr; }
	if (!FPackageName::IsValidLongPackageName(PackagePath)) { return nullptr; }
	UClass* FaceClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.FontFace"));
	if (!FaceClass) { return nullptr; }
	UPackage* Pkg = CreatePackage(*PackagePath);
	if (!Pkg) { return nullptr; }
	Pkg->FullyLoad();
	const FName AssetName(*FPackageName::GetShortName(PackagePath));
	UObject* Face = StaticFindObject(FaceClass, Pkg, *AssetName.ToString());
	if (!Face) { Face = NewObject<UObject>(Pkg, FaceClass, AssetName, RF_Public | RF_Standalone | RF_Transactional); }
	IFontFaceInterface* Iface = Cast<IFontFaceInterface>(Face);
	if (!Iface) { return nullptr; }
	Iface->InitializeFromBulkData(Filename, EFontHinting::Default, Bytes.GetData(), Bytes.Num());
	Face->PostEditChange();
	Face->MarkPackageDirty();
	return Face;
#else
	return nullptr; // editor-only (needs the font's bulk-data initialiser)
#endif
}
