#include "ArenaUI.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Styling/CoreStyle.h"

namespace ArenaUI
{
	const FLinearColor Gold(1.f, 0.82f, 0.38f);
	const FLinearColor Red(0.95f, 0.15f, 0.1f);
	const FLinearColor Ice(0.75f, 0.88f, 1.f);
	const FLinearColor ButtonBg(0.55f, 0.06f, 0.05f, 1.f);
	const FLinearColor ButtonBgSelected(0.95f, 0.55f, 0.1f, 1.f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color, const char* Weight)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(Text);
		T->SetFont(FCoreStyle::GetDefaultFontStyle(Weight, Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		T->SetShadowOffset(FVector2D(2.f, 2.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		return T;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FText& Label, int32 Size, UTextBlock*& OutLabel)
	{
		UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		B->SetBackgroundColor(ButtonBg);
		OutLabel = MakeText(Tree, Label, Size, Gold);
		B->SetContent(OutLabel);
		return B;
	}

	FString SanitizeName(const FString& Name, const FString& Default, int32 MaxLen)
	{
		FString Clean;
		for (TCHAR C : Name.TrimStartAndEnd())
		{
			if (FChar::IsAlnum(C) || C == TEXT(' ') || C == TEXT('-') || C == TEXT('\'') || C == TEXT('.')) { Clean.AppendChar(C); }
		}
		Clean = Clean.Left(FMath::Max(1, MaxLen)).TrimStartAndEnd();
		return Clean.IsEmpty() ? Default : Clean;
	}
}
