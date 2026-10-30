#include "StoryLibrary.h"

#include "IronManArena.h"
#include "JarvisComponent.h"
#include "SuitTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"

namespace
{
	FStoryBook GStory;
	bool GStoryLoaded = false;

	FString GetStr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString S;
		if (O.IsValid()) { O->TryGetStringField(Key, S); }
		return S;
	}

	const TArray<TSharedPtr<FJsonValue>>* GetArr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, A)) { return A; }
		return nullptr;
	}

	TSharedPtr<FJsonObject> GetObj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Key, Child) && Child) { return *Child; }
		return nullptr;
	}

	EStoryObjectiveType ParseObjectiveType(const FString& S)
	{
		if (S.Equals(TEXT("ground"), ESearchCase::IgnoreCase))     { return EStoryObjectiveType::Ground; }
		if (S.Equals(TEXT("flight"), ESearchCase::IgnoreCase))     { return EStoryObjectiveType::Flight; }
		if (S.Equals(TEXT("drone"), ESearchCase::IgnoreCase))      { return EStoryObjectiveType::Drone; }
		if (S.Equals(TEXT("tank"), ESearchCase::IgnoreCase))       { return EStoryObjectiveType::Tank; }
		if (S.Equals(TEXT("wave"), ESearchCase::IgnoreCase))       { return EStoryObjectiveType::Wave; }
		if (S.Equals(TEXT("boss"), ESearchCase::IgnoreCase))       { return EStoryObjectiveType::Boss; }
		if (S.Equals(TEXT("hulkbuster"), ESearchCase::IgnoreCase)) { return EStoryObjectiveType::Hulkbuster; }
		if (S.Equals(TEXT("energy"), ESearchCase::IgnoreCase))     { return EStoryObjectiveType::Energy; }
		return EStoryObjectiveType::Unknown;
	}

	EReactorMount ParseMount(const FString& S)
	{
		return S.Equals(TEXT("forearm"), ESearchCase::IgnoreCase) ? EReactorMount::Forearm : EReactorMount::Spine;
	}

	FStoryLine ParseLine(const TSharedPtr<FJsonObject>& O, const FString& Id)
	{
		FStoryLine L;
		L.Id = FName(*Id);
		L.Speaker = FName(*GetStr(O, TEXT("speaker")));
		L.Text = GetStr(O, TEXT("text"));
		return L;
	}

	void ParseShots(const TSharedPtr<FJsonObject>& Holder, const FString& IdPrefix, TArray<FStoryShot>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Shots = GetArr(Holder, TEXT("shots"));
		if (!Shots) { return; }
		int32 Index = 0;
		for (const TSharedPtr<FJsonValue>& V : *Shots)
		{
			++Index;
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid()) { continue; }
			FStoryShot Shot;
			Shot.Camera = GetStr(O, TEXT("camera"));
			Shot.Sfx = GetStr(O, TEXT("sfx"));
			if (const TArray<TSharedPtr<FJsonValue>>* Chars = GetArr(O, TEXT("characters")))
			{
				for (const TSharedPtr<FJsonValue>& C : *Chars)
				{
					if (C.IsValid()) { Shot.Characters.Add(FName(*C->AsString())); }
				}
			}
			if (const TSharedPtr<FJsonObject> LineObj = GetObj(O, TEXT("line")))
			{
				Shot.Line = ParseLine(LineObj, FString::Printf(TEXT("%s_%02d"), *IdPrefix, Index));
				Shot.bHasLine = !Shot.Line.Text.IsEmpty();
			}
			Out.Add(MoveTemp(Shot));
		}
	}

	FStoryReactor ParseReactor(const TSharedPtr<FJsonObject>& O)
	{
		FStoryReactor R;
		if (!O.IsValid()) { return R; }
		R.Stage = UStoryLibrary::ParseReactorStage(GetStr(O, TEXT("stage")));
		R.Mount = ParseMount(GetStr(O, TEXT("mount")));
		R.Note = GetStr(O, TEXT("note"));
		return R;
	}

	void ParseChapter(const TSharedPtr<FJsonObject>& C, FStoryChapter& Ch)
	{
		const FString Id = GetStr(C, TEXT("id"));
		Ch.Id = FName(*Id);
		Ch.Title = GetStr(C, TEXT("title"));
		Ch.Location = FName(*GetStr(C, TEXT("location")));
		Ch.UnlockSuit = FName(*GetStr(C, TEXT("unlockSuit")));
		const FString Ally = GetStr(C, TEXT("unlockAlly"));
		Ch.UnlockAlly = Ally.IsEmpty() ? NAME_None : FName(*Ally.ToLower());
		Ch.Reactor = ParseReactor(GetObj(C, TEXT("reactor")));
		Ch.Lore = GetStr(C, TEXT("lore"));
		ParseShots(GetObj(C, TEXT("intro")), Id + TEXT("_intro"), Ch.Intro);
		ParseShots(GetObj(C, TEXT("outro")), Id + TEXT("_outro"), Ch.Outro);

		if (const TArray<TSharedPtr<FJsonValue>>* Objs = GetArr(C, TEXT("objectives")))
		{
			int32 ObjIndex = 0;
			for (const TSharedPtr<FJsonValue>& OV : *Objs)
			{
				++ObjIndex;
				const TSharedPtr<FJsonObject> O = OV.IsValid() ? OV->AsObject() : nullptr;
				if (!O.IsValid()) { continue; }
				FStoryObjective Obj;
				Obj.TypeName = GetStr(O, TEXT("type"));
				Obj.Type = ParseObjectiveType(Obj.TypeName);
				Obj.Target = GetStr(O, TEXT("target"));
				Obj.Text = GetStr(O, TEXT("text"));
				int32 Count = 1;
				if (O->TryGetNumberField(TEXT("count"), Count)) { Obj.Count = FMath::Max(1, Count); }
				// allyAvailable is a bool in the script ("the chapter's ally"); a string names one.
				bool bAlly = false;
				FString AllyName;
				if (O->TryGetBoolField(TEXT("allyAvailable"), bAlly) && bAlly) { Obj.AllyAvailable = FName(TEXT("vison")); }
				else if (O->TryGetStringField(TEXT("allyAvailable"), AllyName) && !AllyName.IsEmpty()) { Obj.AllyAvailable = FName(*AllyName.ToLower()); }

				if (const TArray<TSharedPtr<FJsonValue>>* Comms = GetArr(O, TEXT("comms")))
				{
					for (const TSharedPtr<FJsonValue>& CV : *Comms)
					{
						const TSharedPtr<FJsonObject> CO = CV.IsValid() ? CV->AsObject() : nullptr;
						if (!CO.IsValid()) { continue; }
						const FString Trig = GetStr(CO, TEXT("trigger")).ToLower();
						FStoryComms Cm;
						Cm.Trigger = Trig == TEXT("progress") ? EStoryCommsTrigger::Progress
							: (Trig == TEXT("complete") ? EStoryCommsTrigger::Complete : EStoryCommsTrigger::Start);
						Cm.Line = ParseLine(CO, FString::Printf(TEXT("%s_obj%d_%s"), *Id, ObjIndex, *Trig));
						if (!Cm.Line.Text.IsEmpty()) { Obj.Comms.Add(MoveTemp(Cm)); }
					}
				}
				if (Obj.Type == EStoryObjectiveType::Unknown)
				{
					UE_LOG(LogIronManArena, Warning, TEXT("Story: %s objective %d has unknown type '%s' (treated as a wave)"), *Id, ObjIndex, *Obj.TypeName);
				}
				Ch.Objectives.Add(MoveTemp(Obj));
			}
		}
	}

	FName GetName(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const FString S = GetStr(O, Key);
		return S.IsEmpty() ? NAME_None : FName(*S);
	}

	bool LoadJsonRoot(const FString& FileName, TSharedPtr<FJsonObject>& Root, bool& bFileFound)
	{
		const FString Path = FPaths::Combine(UStoryLibrary::GetStoryDir(), FileName);
		FString Json;
		bFileFound = FFileHelper::LoadFileToString(Json, *Path);
		if (!bFileFound) { return false; }
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogIronManArena, Error, TEXT("Story: failed to parse %s: %s"), *Path, *Reader->GetErrorMessage());
			return false;
		}
		return true;
	}

	bool ParseEventName(const FString& Key, EAIEvent& Out)
	{
		static const TMap<FString, EAIEvent> Map =
		{
			{ TEXT("start"), EAIEvent::Start }, { TEXT("suitup"), EAIEvent::SuitUp }, { TEXT("suitswapin"), EAIEvent::SuitSwapIn },
			{ TEXT("lowenergy"), EAIEvent::LowEnergy }, { TEXT("lowhealth"), EAIEvent::LowHealth },
			{ TEXT("toflight"), EAIEvent::ToFlight }, { TEXT("toground"), EAIEvent::ToGround },
			{ TEXT("hulkbusterready"), EAIEvent::HulkbusterReady }, { TEXT("hulkbusterdeploy"), EAIEvent::HulkbusterDeploy },
			{ TEXT("wavestart"), EAIEvent::WaveStart }, { TEXT("droneswarm"), EAIEvent::DroneSwarm },
			{ TEXT("tankspotted"), EAIEvent::TankSpotted }, { TEXT("bossincoming"), EAIEvent::BossIncoming },
			{ TEXT("bossphase2"), EAIEvent::BossPhase2 }, { TEXT("bossphase3"), EAIEvent::BossPhase3 },
			{ TEXT("bossdefeated"), EAIEvent::BossDefeated }, { TEXT("combo"), EAIEvent::Combo },
			{ TEXT("pickuphealth"), EAIEvent::PickupHealth }, { TEXT("pickupenergy"), EAIEvent::PickupEnergy },
			{ TEXT("pickuphulkbuster"), EAIEvent::PickupHulkbuster }, { TEXT("death"), EAIEvent::Death },
			{ TEXT("highscore"), EAIEvent::HighScore }, { TEXT("idle"), EAIEvent::Idle },
			{ TEXT("hack"), EAIEvent::Hack }, { TEXT("intel"), EAIEvent::Intel }, { TEXT("encourage"), EAIEvent::Encourage },
		};
		if (const EAIEvent* E = Map.Find(Key.ToLower())) { Out = *E; return true; }
		return false;
	}

	FString StoryAudioPackage(const FString& Folder, const FString& Name)
	{
		return FString::Printf(TEXT("/Game/StoryAudio/%s/%s"), *Folder, *Name);
	}

	USoundBase* LoadIfExists(const FString& Folder, const FString& Name)
	{
		const FString Package = StoryAudioPackage(Folder, Name);
		if (!FPackageName::DoesPackageExist(Package)) { return nullptr; }
		return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *Name));
	}
}

FString UStoryLibrary::GetStoryDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Story"));
}

EReactorStage UStoryLibrary::ParseReactorStage(const FString& S)
{
	if (S.Equals(TEXT("unstable"), ESearchCase::IgnoreCase)) { return EReactorStage::Unstable; }
	if (S.Equals(TEXT("failed"), ESearchCase::IgnoreCase))   { return EReactorStage::Failed; }
	if (S.Equals(TEXT("upgraded"), ESearchCase::IgnoreCase)) { return EReactorStage::Upgraded; }
	return EReactorStage::Makeshift;
}

bool UStoryLibrary::ParseStory(const FString& Json, FStoryBook& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = Reader->GetErrorMessage();
		return false;
	}

	Out = FStoryBook();
	Out.Title = GetStr(Root, TEXT("title"));
	const FString DefaultName = GetStr(Root, TEXT("playerNameDefault"));
	if (!DefaultName.IsEmpty()) { Out.PlayerNameDefault = DefaultName; }

	if (const TArray<TSharedPtr<FJsonValue>>* Speakers = GetArr(Root, TEXT("speakers")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Speakers) { if (V.IsValid()) { Out.Speakers.Add(FName(*V->AsString())); } }
	}

	ParseShots(GetObj(Root, TEXT("prologue")), TEXT("prologue"), Out.Prologue);

	if (const TArray<TSharedPtr<FJsonValue>>* Chapters = GetArr(Root, TEXT("chapters")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Chapters)
		{
			const TSharedPtr<FJsonObject> C = V.IsValid() ? V->AsObject() : nullptr;
			if (!C.IsValid()) { continue; }
			FStoryChapter Ch;
			ParseChapter(C, Ch);
			Out.Chapters.Add(MoveTemp(Ch));
		}
	}

	if (const TSharedPtr<FJsonObject> F = GetObj(Root, TEXT("finale")))
	{
		ParseShots(F, TEXT("finale"), Out.Finale.Shots);
		F->TryGetBoolField(TEXT("promptHeroName"), Out.Finale.bPromptHeroName);
		Out.Finale.Reactor = ParseReactor(GetObj(F, TEXT("reactor")));
		if (const TSharedPtr<FJsonObject> A = GetObj(F, TEXT("allySuit")))
		{
			Out.Finale.AllySpeaker = FName(*GetStr(A, TEXT("speaker")));
			Out.Finale.AllySuit = FName(*GetStr(A, TEXT("suit")));
		}
	}

	if (Out.Chapters.Num() == 0)
	{
		OutError = TEXT("no chapters");
		return false;
	}
	return true;
}

bool UStoryLibrary::ParseSideMissions(const TSharedPtr<FJsonObject>& Root, TArray<FStoryChapter>& Out)
{
	Out.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Missions = GetArr(Root, TEXT("missions"));
	if (!Missions) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Missions)
	{
		const TSharedPtr<FJsonObject> M = V.IsValid() ? V->AsObject() : nullptr;
		if (!M.IsValid()) { continue; }
		FStoryChapter Ch;
		ParseChapter(M, Ch);
		if (Ch.Id.IsNone()) { continue; }
		Ch.bSideMission = true;
		Ch.AvailableFrom = GetName(M, TEXT("availableFrom"));
		Ch.AvailableUntil = GetName(M, TEXT("availableUntil"));
		if (const TSharedPtr<FJsonObject> R = GetObj(M, TEXT("reward")))
		{
			Ch.RewardCodex = GetName(R, TEXT("codex"));
			Ch.RewardUpgrade = GetName(R, TEXT("upgrade"));
			Ch.RewardCosmetic = GetName(R, TEXT("cosmetic"));
		}
		Out.Add(MoveTemp(Ch));
	}
	return Out.Num() > 0;
}

bool UStoryLibrary::ParseCodex(const TSharedPtr<FJsonObject>& Root, TArray<FStoryCodexEntry>& Out)
{
	Out.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Entries = GetArr(Root, TEXT("entries"));
	if (!Entries) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Entries)
	{
		const TSharedPtr<FJsonObject> E = V.IsValid() ? V->AsObject() : nullptr;
		if (!E.IsValid()) { continue; }
		FStoryCodexEntry Entry;
		Entry.Id = GetName(E, TEXT("id"));
		if (Entry.Id.IsNone()) { continue; }
		Entry.Title = GetStr(E, TEXT("title"));
		Entry.Category = GetName(E, TEXT("category"));
		Entry.UnlockChapter = GetName(E, TEXT("unlockChapter"));
		Entry.UnlockedBy = GetName(E, TEXT("unlockedBy"));
		Entry.Body = GetStr(E, TEXT("body"));
		if (const TArray<TSharedPtr<FJsonValue>>* Suits = GetArr(E, TEXT("suits")))
		{
			for (const TSharedPtr<FJsonValue>& SV : *Suits) { if (SV.IsValid()) { Entry.Suits.Add(FName(*SV->AsString())); } }
		}
		Out.Add(MoveTemp(Entry));
	}
	return Out.Num() > 0;
}

void UStoryLibrary::LoadExtras(FStoryBook& Book)
{
	TSharedPtr<FJsonObject> Root;
	bool bFound = false;
	if (LoadJsonRoot(TEXT("side_missions.json"), Root, bFound)) { ParseSideMissions(Root, Book.SideMissions); }
	else if (!bFound) { UE_LOG(LogIronManArena, Log, TEXT("Story: no side_missions.json (side missions off)")); }

	Root.Reset();
	if (LoadJsonRoot(TEXT("codex.json"), Root, bFound)) { ParseCodex(Root, Book.Codex); }
	else if (!bFound) { UE_LOG(LogIronManArena, Log, TEXT("Story: no codex.json (codex off)")); }

	for (const FStoryChapter& M : Book.SideMissions)
	{
		if (!M.AvailableFrom.IsNone() && Book.FindChapter(M.AvailableFrom) == INDEX_NONE)
		{
			UE_LOG(LogIronManArena, Warning, TEXT("Story: side mission %s availableFrom '%s' is not a chapter"), *M.Id.ToString(), *M.AvailableFrom.ToString());
		}
		if (!M.RewardCodex.IsNone() && !Book.Codex.ContainsByPredicate([&M](const FStoryCodexEntry& E) { return E.Id == M.RewardCodex; }))
		{
			UE_LOG(LogIronManArena, Warning, TEXT("Story: side mission %s rewards unknown codex entry '%s'"), *M.Id.ToString(), *M.RewardCodex.ToString());
		}
	}
	UE_LOG(LogIronManArena, Log, TEXT("Story: %d side missions, %d codex entries"), Book.SideMissions.Num(), Book.Codex.Num());
}

int32 UStoryLibrary::ChapterOrder(const FStoryBook& Book, FName ChapterId)
{
	if (ChapterId.IsNone()) { return INDEX_NONE; }
	if (ChapterId == FName(TEXT("prologue"))) { return -1; }
	if (ChapterId == FName(TEXT("finale"))) { return Book.Chapters.Num(); }
	const int32 Index = Book.FindChapter(ChapterId);
	return Index == INDEX_NONE ? MAX_int32 : Index;
}

bool UStoryLibrary::IsSideMissionAvailable(const FStoryBook& Book, const FStoryChapter& Mission, int32 ChapterReached, bool bStoryComplete)
{
	if (bStoryComplete) { return true; } // replay everything after the credits
	const int32 From = Mission.AvailableFrom.IsNone() ? 0 : ChapterOrder(Book, Mission.AvailableFrom);
	const int32 Until = Mission.AvailableUntil.IsNone() ? MAX_int32 : ChapterOrder(Book, Mission.AvailableUntil);
	// ChapterReached = the next chapter the player may start; a mission opens once its
	// availableFrom chapter has been reached and closes after availableUntil is finished.
	return ChapterReached >= From && ChapterReached <= Until;
}

bool UStoryLibrary::IsCodexUnlocked(const FStoryBook& Book, const FStoryCodexEntry& Entry, int32 ChapterReached, bool bStoryComplete,
	bool bPrologueSeen, const TArray<FName>& Granted)
{
	if (Granted.Contains(Entry.Id)) { return true; }
	if (!Entry.UnlockedBy.IsNone()) { return false; } // side-mission reward only
	if (Entry.UnlockChapter.IsNone()) { return true; }
	if (Entry.UnlockChapter == FName(TEXT("prologue"))) { return bPrologueSeen || ChapterReached > 0 || bStoryComplete; }
	if (Entry.UnlockChapter == FName(TEXT("finale"))) { return bStoryComplete; }
	const int32 Index = Book.FindChapter(Entry.UnlockChapter);
	// Unlocked by finishing the chapter (ChapterReached is past it).
	return bStoryComplete || (Index != INDEX_NONE && ChapterReached > Index);
}

bool UStoryLibrary::ReloadStory()
{
	GStoryLoaded = true;
	GStory = FStoryBook();
	const FString Path = FPaths::Combine(GetStoryDir(), TEXT("story.json"));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogIronManArena, Warning, TEXT("Story: %s not found - story mode disabled"), *Path);
		return false;
	}
	FString Error;
	if (!ParseStory(Json, GStory, Error))
	{
		UE_LOG(LogIronManArena, Error, TEXT("Story: failed to parse %s: %s"), *Path, *Error);
		GStory = FStoryBook();
		return false;
	}
	LoadExtras(GStory);
	for (const FStoryChapter& C : GStory.Chapters)
	{
		if (!C.UnlockSuit.IsNone() && USuitLibrary::FindSuitIndex(C.UnlockSuit) == INDEX_NONE)
		{
			UE_LOG(LogIronManArena, Warning, TEXT("Story: chapter %s unlocks unknown suit '%s'"), *C.Id.ToString(), *C.UnlockSuit.ToString());
		}
	}
	UE_LOG(LogIronManArena, Log, TEXT("Story: '%s' loaded, %d chapters, %d prologue / %d finale shots"),
		*GStory.Title, GStory.Chapters.Num(), GStory.Prologue.Num(), GStory.Finale.Shots.Num());
	return true;
}

const FStoryBook& UStoryLibrary::GetStory()
{
	if (!GStoryLoaded) { ReloadStory(); }
	return GStory;
}

TArray<FAILine> UStoryLibrary::LoadCommsSpeakerLines(FName Speaker, const FString& FileName, bool bLoadSounds)
{
	TArray<FAILine> Result;
	const FString Path = FPaths::Combine(GetStoryDir(), FileName);
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogIronManArena, Warning, TEXT("Story: comms file %s not found"), *Path);
		return Result;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		UE_LOG(LogIronManArena, Error, TEXT("Story: failed to parse %s"), *Path);
		return Result;
	}
	const TSharedPtr<FJsonObject> Events = GetObj(Root, TEXT("events"));
	if (!Events.IsValid()) { return Result; }

	const FString Folder = GetSpeakerFolder(Speaker);
	const FText Label = GetSpeakerLabel(Speaker, FString());
	const FLinearColor Color = GetSpeakerColor(Speaker);
	for (const TPair<FString, TSharedPtr<FJsonValue>> Pair : Events->Values)
	{
		EAIEvent Event;
		if (!ParseEventName(Pair.Key, Event) || !Pair.Value.IsValid()) { continue; }
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Pair.Value->TryGetArray(Items) || !Items) { continue; }
		int32 i = 0;
		for (const TSharedPtr<FJsonValue>& V : *Items)
		{
			++i;
			FString Text;
			if (!V.IsValid() || !V->TryGetString(Text) || Text.IsEmpty()) { continue; }
			FAILine L;
			const FString Id = FString::Printf(TEXT("%s_%s_%d"), *Folder, *Pair.Key, i);
			L.Id = FName(*Id);
			L.Event = Event;
			L.Speaker = Speaker;
			L.SpeakerLabel = Label;
			L.SpeakerColor = Color;
			L.Text = FText::AsCultureInvariant(Text);
			if (bLoadSounds) { L.Sound = LoadIfExists(Folder, Id); }
			Result.Add(L);
		}
	}
	UE_LOG(LogIronManArena, Log, TEXT("Story: %d comms lines for %s"), Result.Num(), *Speaker.ToString());
	return Result;
}

bool UStoryLibrary::LogStoryStatus()
{
	const FStoryBook& Story = GetStory();
	if (Story.Chapters.Num() == 0)
	{
		UE_LOG(LogIronManArena, Warning, TEXT("Story mode: DISABLED (no chapters in %s/story.json)"), *GetStoryDir());
		return false;
	}

	int32 Takes = 0;
	int32 Found = 0;
	auto Check = [&Takes, &Found](const FStoryLine& Line)
	{
		if (Line.Id.IsNone()) { return; }
		const FString Folder = GetSpeakerFolder(Line.Speaker);
		const FString Id = Line.Id.ToString();
		++Takes;
		if (FPackageName::DoesPackageExist(StoryAudioPackage(Folder, Id))) { ++Found; }
		if (Line.HasTokens())
		{
			++Takes;
			if (FPackageName::DoesPackageExist(StoryAudioPackage(Folder, Id + TEXT("_noname")))) { ++Found; }
		}
	};
	auto CheckShots = [&Check](const TArray<FStoryShot>& Shots)
	{
		for (const FStoryShot& Shot : Shots) { if (Shot.bHasLine) { Check(Shot.Line); } }
	};
	CheckShots(Story.Prologue);
	for (const FStoryChapter& C : Story.Chapters)
	{
		CheckShots(C.Intro);
		CheckShots(C.Outro);
		for (const FStoryObjective& O : C.Objectives)
		{
			for (const FStoryComms& Comms : O.Comms) { Check(Comms.Line); }
		}
	}
	CheckShots(Story.Finale.Shots);

	const TArray<FAILine> Morgan = LoadCommsSpeakerLines(FName(TEXT("MORGAN")), TEXT("morgan.json"), false);
	const FString MorganFolder = GetSpeakerFolder(FName(TEXT("MORGAN")));
	int32 MorganFound = 0;
	for (const FAILine& L : Morgan)
	{
		if (FPackageName::DoesPackageExist(StoryAudioPackage(MorganFolder, L.Id.ToString()))) { ++MorganFound; }
	}

	UE_LOG(LogIronManArena, Log, TEXT("Story mode: ENABLED - '%s', %d chapters; story voice: %d / %d script takes, %d / %d MORGAN comms takes have audio"),
		*Story.Title, Story.Chapters.Num(), Found, Takes, MorganFound, Morgan.Num());
	return true;
}

FString UStoryLibrary::ResolveTokens(const FString& Text, const FString& PlayerName, const FString& HeroName)
{
	FString Out = Text;
	const FString Player = PlayerName.IsEmpty() ? GetStory().PlayerNameDefault : PlayerName;
	Out.ReplaceInline(TEXT("{playerName}"), *Player, ESearchCase::CaseSensitive);
	Out.ReplaceInline(TEXT("{heroName}"), HeroName.IsEmpty() ? TEXT("Iron Lad") : *HeroName, ESearchCase::CaseSensitive);
	return Out;
}

FString UStoryLibrary::GetSpeakerFolder(FName Speaker)
{
	FString S = Speaker.ToString().ToUpper();
	FString Out;
	for (TCHAR C : S)
	{
		if (FChar::IsAlnum(C)) { Out.AppendChar(C); }
		else if (C == TEXT(' ') || C == TEXT('_')) { Out.AppendChar(TEXT('_')); }
		// '-' and other punctuation are dropped (DUM-E -> DUME)
	}
	return Out.IsEmpty() ? FString(TEXT("UNKNOWN")) : Out;
}

FText UStoryLibrary::GetSpeakerLabel(FName Speaker, const FString& PlayerName)
{
	const FString Key = Speaker.ToString().ToUpper();
	if (Key == TEXT("PLAYER"))
	{
		const FString Name = PlayerName.IsEmpty() ? GetStory().PlayerNameDefault : PlayerName;
		return FText::AsCultureInvariant(Name.ToUpper());
	}
	if (Key == TEXT("VISON")) { return FText::AsCultureInvariant(TEXT("VISON")); }
	return FText::AsCultureInvariant(Key);
}

FLinearColor UStoryLibrary::GetSpeakerColor(FName Speaker)
{
	const FString Key = Speaker.ToString().ToUpper();
	// Suit AIs keep their persona colours so subtitles match the arena.
	if (Key == TEXT("JARVIS")) { return USuitLibrary::GetPersona(EAIPersona::Jarvis).SubtitleColor; }
	if (Key == TEXT("FRIDAY")) { return USuitLibrary::GetPersona(EAIPersona::Friday).SubtitleColor; }
	if (Key == TEXT("EDITH"))  { return USuitLibrary::GetPersona(EAIPersona::Edith).SubtitleColor; }
	if (Key == TEXT("ULTRON")) { return USuitLibrary::GetPersona(EAIPersona::Ultron).SubtitleColor; }
	if (Key == TEXT("MORGAN")) { return FLinearColor(0.55f, 0.75f, 1.f); }  // Rescue blue
	if (Key == TEXT("PLAYER")) { return FLinearColor(1.f, 0.82f, 0.45f); }  // workshop amber
	if (Key == TEXT("TONY"))   { return FLinearColor(1.f, 0.35f, 0.25f); }
	if (Key == TEXT("PEPPER")) { return FLinearColor(0.95f, 0.6f, 0.85f); }
	if (Key == TEXT("VISON"))  { return FLinearColor(1.f, 0.9f, 0.35f); }   // mind-stone gold
	if (Key == TEXT("U") || Key == TEXT("DUM-E")) { return FLinearColor(0.75f, 0.75f, 0.75f); }
	if (Key == TEXT("OBSIDIAN TITAN")) { return FLinearColor(0.7f, 0.3f, 1.f); }
	return FLinearColor::White;
}

USoundBase* UStoryLibrary::GetLineSound(const FStoryLine& Line, const FString& PlayerName, const FString& DefaultName)
{
	if (Line.Id.IsNone()) { return nullptr; }
	const FString Folder = GetSpeakerFolder(Line.Speaker);
	const FString Id = Line.Id.ToString();
	// The default take says "Harley". If the player picked another name, use the take
	// recorded without the name (generate_story_audio.py writes both for token lines).
	const bool bRenamed = !PlayerName.IsEmpty() && !PlayerName.Equals(DefaultName, ESearchCase::IgnoreCase);
	const bool bHero = Line.Text.Contains(TEXT("{heroName}"));
	if (Line.HasTokens() && (bRenamed || bHero))
	{
		if (USoundBase* S = LoadIfExists(Folder, Id + TEXT("_noname"))) { return S; }
	}
	return LoadIfExists(Folder, Id);
}
