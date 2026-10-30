// UStoryNamePromptWidget: "What do we call you?" text prompt (finale {heroName}, story menu
// player name). Built in C++; the owner gets the name through OnSubmitted.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StoryNamePromptWidget.generated.h"

class UTextBlock;
class UEditableTextBox;
class UButton;

UCLASS()
class IRONMANARENA_API UStoryNamePromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& Title, const FText& Prompt, const FString& DefaultName, int32 MaxLength = 20);

	/** Called once with the trimmed name (DefaultName when left empty). */
	TFunction<void(const FString&)> OnSubmitted;

	/** Keyboard focus to the text box. */
	void FocusInput(APlayerController* PC);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleOkClicked();

	void BuildTree();
	void Submit();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient)
	TObjectPtr<UButton> OkButton;

	FText PendingTitle;
	FText PendingPrompt;
	FString Default;
	int32 MaxLen = 20;
	bool bSubmitted = false;
};
