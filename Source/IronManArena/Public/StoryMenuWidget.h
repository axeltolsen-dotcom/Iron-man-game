// UStoryMenuWidget: story-mode hub. Player name, Continue, New Story (with the Tony prologue),
// and a chapter selector for every chapter already reached. Built in C++.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StoryMenuWidget.generated.h"

class UTextBlock;
class UEditableTextBox;
class UButton;

UCLASS()
class IRONMANARENA_API UStoryMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Re-read the save game (chapter reached, name, completion). */
	void RefreshFromSave();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildTree();
	void RefreshChapter();
	void CommitName();
	void Launch(int32 Chapter, bool bPrologue);

	UFUNCTION() void HandleContinue();
	UFUNCTION() void HandleNewStory();
	UFUNCTION() void HandlePrevChapter();
	UFUNCTION() void HandleNextChapter();
	UFUNCTION() void HandlePlayChapter();
	UFUNCTION() void HandleBack();

	int32 ChapterReached = 0;
	int32 SelectedChapter = 0;
	bool bComplete = false;
	bool bPrologueSeen = false;
	FString DefaultName;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StoryTitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ProgressText;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ContinueLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChapterText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChapterInfoText;
};
