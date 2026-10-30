// Editor-script helper: fills a UFont's composite font from imported UFontFace assets.
// FFontData::FontFaceAsset is not reachable from Python, so Content/Python/import_ui_fonts.py
// imports the OFL .ttf files as Font Faces and calls this to assemble F_Rajdhani, F_Exo2, ...
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArenaFontLibrary.generated.h"

class UFont;
class UFontFace;

UCLASS()
class IRONMANARENA_API UArenaFontLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Replaces Font's default typeface with one entry per (TypefaceNames[i], Faces[i]) and makes it a
	 *  runtime-cached font. Returns the number of typeface entries written (0 on bad input). */
	UFUNCTION(BlueprintCallable, Category = "Arena|Fonts")
	static int32 BuildCompositeFont(UFont* Font, const TArray<FName>& TypefaceNames, const TArray<UObject*>& Faces);

	/** Editor scripts: create a Font Face asset at PackagePath (e.g. /Game/UI/Fonts/Faces/FF_Rajdhani_Bold)
	 *  holding the .ttf/.otf bytes of Filename. The interactive font import factory needs Slate, so it cannot
	 *  run under -nullrhi; this does the same thing without a dialog. Caller saves the package. */
	UFUNCTION(BlueprintCallable, Category = "Arena|Fonts")
	static UObject* CreateFontFaceFromFile(const FString& Filename, const FString& PackagePath);

	/** Typeface names on a font's default typeface (for checks). */
	UFUNCTION(BlueprintPure, Category = "Arena|Fonts")
	static TArray<FName> GetTypefaceNames(const UFont* Font);
};
