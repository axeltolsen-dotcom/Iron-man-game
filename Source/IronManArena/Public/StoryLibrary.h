// UStoryLibrary: loads Content/Story/story.json + morgan.json, resolves tokens and voice clips,
// and knows how every speaker looks / sounds on subtitles.
//
// Audio convention (Tools/generate_story_audio.py writes it, Content/Python/import_jarvis_audio.py
// imports it):   /Game/StoryAudio/<SPEAKER_FOLDER>/<LineId>          default name (Harley)
//                /Game/StoryAudio/<SPEAKER_FOLDER>/<LineId>_noname   all name tokens removed
// Speaker folder = upper case, letters/digits/underscore (DUM-E -> DUME, Vison -> VISON).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StoryTypes.h"
#include "StoryLibrary.generated.h"

struct FAILine;
class FJsonObject;
class USoundBase;

UCLASS()
class IRONMANARENA_API UStoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Parsed story (loaded on first use and cached). Empty book if the file is missing/invalid. */
	static const FStoryBook& GetStory();

	/** Re-reads the json (editor iteration). Returns false with a log message on failure. */
	UFUNCTION(BlueprintCallable, Category = "Story")
	static bool ReloadStory();

	/** Parses a story json string (used by GetStory; public for tests / tools). */
	static bool ParseStory(const FString& Json, FStoryBook& Out, FString& OutError);

	/** side_missions.json / codex.json parsers (public for tests). Root = the parsed file. */
	static bool ParseSideMissions(const TSharedPtr<FJsonObject>& Root, TArray<FStoryChapter>& Out);
	static bool ParseCodex(const TSharedPtr<FJsonObject>& Root, TArray<FStoryCodexEntry>& Out);

	/** Loads the optional side_missions.json + codex.json next to story.json into Book. */
	static void LoadExtras(FStoryBook& Book);

	/** Chapter id -> order (prologue -1, finale Chapters.Num(), unknown MAX_int32). */
	static int32 ChapterOrder(const FStoryBook& Book, FName ChapterId);

	/** ChapterReached = UArenaSaveGame::StoryChapterReached (next chapter the player may start). */
	static bool IsSideMissionAvailable(const FStoryBook& Book, const FStoryChapter& Mission, int32 ChapterReached, bool bStoryComplete);

	/** Chapter entries unlock when their chapter is finished; side-mission entries when granted. */
	static bool IsCodexUnlocked(const FStoryBook& Book, const FStoryCodexEntry& Entry, int32 ChapterReached, bool bStoryComplete,
		bool bPrologueSeen, const TArray<FName>& Granted);

	/** Morgan's comms chatter (Content/Story/morgan.json) as AI lines for UJarvisComponent. */
	static TArray<FAILine> LoadCommsSpeakerLines(FName Speaker, const FString& FileName, bool bLoadSounds = true);

	/** Logs whether story mode is available and how many story voice takes exist as assets
	 *  (package checks only, nothing is loaded). Called once at game start. Returns true if the story loaded. */
	static bool LogStoryStatus();

	/** {playerName} / {heroName} substitution. Empty names fall back to the defaults. */
	UFUNCTION(BlueprintPure, Category = "Story")
	static FString ResolveTokens(const FString& Text, const FString& PlayerName, const FString& HeroName);

	/** Display name on subtitles: PLAYER -> the player's name, Vison -> VISON, DUM-E ... */
	static FText GetSpeakerLabel(FName Speaker, const FString& PlayerName);

	static FLinearColor GetSpeakerColor(FName Speaker);

	/** Asset folder for a speaker (same rule as the python tools). */
	static FString GetSpeakerFolder(FName Speaker);

	/** Voice clip for a line given the current player name (no-name take if the player renamed). */
	static USoundBase* GetLineSound(const FStoryLine& Line, const FString& PlayerName, const FString& DefaultName);

	static FString GetStoryDir();
	static EReactorStage ParseReactorStage(const FString& S);
};
