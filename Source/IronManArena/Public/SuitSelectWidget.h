// USuitSelectWidget: pick one of the data-driven suits before an Arena run (built in C++).
// Left/Right (A/D, D-pad) cycle, Enter suits up, Esc goes back.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuitSelectWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UBorder;

UCLASS()
class IRONMANARENA_API USuitSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Show the screen starting on this suit. */
	void OpenAt(int32 SuitIndex);

	int32 GetCurrentIndex() const { return Index; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildTree();
	void Refresh();
	void Step(int32 Delta);

	UFUNCTION() void HandlePrev();
	UFUNCTION() void HandleNext();
	UFUNCTION() void HandleConfirm();
	UFUNCTION() void HandleBack();

	int32 Index = 0;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> CounterText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MarkText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DescText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AIText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeaponText;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> StatBars;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> Swatches;
};
