// ArenaUI: small helpers shared by the C++-built widgets (menus, settings, story screens).
#pragma once

#include "CoreMinimal.h"

class UWidgetTree;
class UTextBlock;
class UButton;
class UPanelWidget;

namespace ArenaUI
{
	/** Palette. */
	IRONMANARENA_API extern const FLinearColor Gold;
	IRONMANARENA_API extern const FLinearColor Red;
	IRONMANARENA_API extern const FLinearColor Ice;
	IRONMANARENA_API extern const FLinearColor ButtonBg;
	IRONMANARENA_API extern const FLinearColor ButtonBgSelected;

	/** Centered, shadowed text block. Weight: "Bold" / "Regular" / "Italic". */
	IRONMANARENA_API UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size,
		const FLinearColor& Color, const char* Weight = "Bold");

	/** Red/gold button with a text label. OutLabel receives the label. */
	/** Printable, short name for subtitles / save game: letters, digits, space - ' . ; falls back to Default. */
	IRONMANARENA_API FString SanitizeName(const FString& Name, const FString& Default, int32 MaxLen = 20);

	IRONMANARENA_API UButton* MakeButton(UWidgetTree* Tree, const FText& Label, int32 Size, UTextBlock*& OutLabel);
}
