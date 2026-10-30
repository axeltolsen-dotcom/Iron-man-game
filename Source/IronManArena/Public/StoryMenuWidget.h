// UStoryMenuWidget: story-mode hub. Player name, Continue, New Story (with the Tony prologue),
// a chapter selector for every chapter already reached, Friday's side missions
// (side_missions.json) and the lore codex (codex.json). Built in C++.
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
	UFUNCTION() void HandlePrevSide();
	UFUNCTION() void HandleNextSide();
	UFUNCTION() void HandlePlaySide();
	UFUNCTION() void HandleOpenCodex();
	UFUNCTION() void HandleCloseCodex();
	UFUNCTION() void HandlePrevCodex();
	UFUNCTION() void HandleNextCodex();

	void RefreshSide();
	void RefreshCodex();

	int32 SelectedSide = 0;
	TArray<FName> SideDone;
	TArray<FName> CodexGranted;
	TArray<FName> RewardsEarned;
	/** Indices into FStoryBook::Codex the player has unlocked (sorted by category). */
	TArray<int32> CodexUnlocked;
	int32 SelectedCodex = 0;
	bool bCodexOpen = false;

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
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SideText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SideInfoText;
	UPROPERTY(Transient) TObjectPtr<UButton> SidePlayButton;
	UPROPERTY(Transient) TObjectPtr<class UBorder> CodexPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CodexHeader;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CodexTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CodexBody;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CodexRewards;

public:
	// ---- tests
	int32 GetCodexUnlockedCount() const { return CodexUnlocked.Num(); }
	bool IsCodexOpen() const { return bCodexOpen; }
};
