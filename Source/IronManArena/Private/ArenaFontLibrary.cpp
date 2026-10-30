#include "ArenaFontLibrary.h"

#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"

int32 UArenaFontLibrary::BuildCompositeFont(UFont* Font, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces)
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
	Font->PostEditChange();
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
