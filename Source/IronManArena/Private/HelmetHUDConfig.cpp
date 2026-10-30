#include "HelmetHUDConfig.h"

#include "IronManArena.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ------------------------------------------------------------------------------------------------
// Easing

float HelmetHUDEase(EHUDEase Ease, float T)
{
	const float X = FMath::Clamp(T, 0.f, 1.f);
	switch (Ease)
	{
	case EHUDEase::Step:            return X < 1.f ? 0.f : 1.f;
	case EHUDEase::SinusoidalIn:    return 1.f - FMath::Cos(X * HALF_PI);
	case EHUDEase::SinusoidalOut:   return FMath::Sin(X * HALF_PI);
	case EHUDEase::SinusoidalInOut: return -0.5f * (FMath::Cos(PI * X) - 1.f);
	case EHUDEase::EaseIn:          return X * X;
	case EHUDEase::EaseOut:         return 1.f - (1.f - X) * (1.f - X);
	case EHUDEase::EaseInOut:       return X < 0.5f ? 2.f * X * X : 1.f - 2.f * (1.f - X) * (1.f - X);
	case EHUDEase::ExpoIn:          return X <= 0.f ? 0.f : FMath::Pow(2.f, 10.f * (X - 1.f));
	case EHUDEase::ExpoOut:         return X >= 1.f ? 1.f : 1.f - FMath::Pow(2.f, -10.f * X);
	case EHUDEase::ExpoInOut:
		if (X <= 0.f || X >= 1.f) { return X; }
		return X < 0.5f ? 0.5f * FMath::Pow(2.f, 20.f * X - 10.f) : 1.f - 0.5f * FMath::Pow(2.f, -20.f * X + 10.f);
	case EHUDEase::CircularIn:      return 1.f - FMath::Sqrt(FMath::Max(0.f, 1.f - X * X));
	case EHUDEase::CircularOut:     return FMath::Sqrt(FMath::Max(0.f, 1.f - (X - 1.f) * (X - 1.f)));
	case EHUDEase::CircularInOut:
		return X < 0.5f ? 0.5f * (1.f - FMath::Sqrt(FMath::Max(0.f, 1.f - 4.f * X * X)))
		                : 0.5f * (FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square(-2.f * X + 2.f))) + 1.f);
	case EHUDEase::Linear:
	default:                        return X;
	}
}

EHUDEase HelmetHUDParseEase(const FString& EaseName)
{
	static const TCHAR* Names[] = {
		TEXT("Linear"), TEXT("Step"), TEXT("SinusoidalIn"), TEXT("SinusoidalOut"), TEXT("SinusoidalInOut"),
		TEXT("EaseIn"), TEXT("EaseOut"), TEXT("EaseInOut"), TEXT("ExpoIn"), TEXT("ExpoOut"), TEXT("ExpoInOut"),
		TEXT("CircularIn"), TEXT("CircularOut"), TEXT("CircularInOut") };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Names); ++i)
	{
		if (EaseName.Equals(Names[i], ESearchCase::IgnoreCase)) { return static_cast<EHUDEase>(i); }
	}
	return EHUDEase::Linear;
}

// ------------------------------------------------------------------------------------------------
// Defaults (generated from hud_layout.json)

FHelmetHUDConfig FHelmetHUDConfig::MakeDefaults()
{
	FHelmetHUDConfig C;

	auto AddPersona = [&C](const TCHAR* Key, const TCHAR* Label, const FLinearColor& Primary, const FLinearColor& Secondary,
		const FLinearColor& Warn, const FLinearColor& Crit, const FLinearColor& Sub, int32 Glyph, const TCHAR* FontFamily, int32 Spacing)
	{
		FHUDPersonaPalette P;
		P.Key = Key;
		P.Label = Label;
		P.Primary = Primary;
		P.Secondary = Secondary;
		P.Warning = Warn;
		P.Critical = Crit;
		P.Subtitle = Sub;
		P.GlyphIndex = Glyph;
		P.FontFamily = FontFamily;
		P.LetterSpacingBias = Spacing;
		C.Personas.Add(P);
	};
	auto AddFont = [&C](const TCHAR* Key, const TCHAR* Family, const TCHAR* Asset)
	{
		FHUDFontDef F;
		F.Key = FName(Key);
		F.Family = Family;
		F.Asset = Asset;
		C.Fonts.Add(F.Key, F);
	};
	auto AddGlitchTier = [&C](int32 Tier, float HealthBelow, float Glitch)
	{
		FHUDGlitchTier G;
		G.Tier = Tier;
		G.HealthBelow = HealthBelow;
		G.Glitch = Glitch;
		C.GlitchTiers.Add(G);
	};
	auto AddWarning = [&C](const TCHAR* Id, int32 Prio, const TCHAR* Text, const TCHAR* ColorKey, const TCHAR* Icon, float FlashHz, float MinShowS)
	{
		FHUDWarningDef W;
		W.Id = FName(Id);
		W.Priority = Prio;
		W.Text = Text;
		W.ColorKey = FName(ColorKey);
		W.Icon = FName(Icon);
		W.FlashHz = FlashHz;
		W.MinShowS = MinShowS;
		C.Warnings.Add(W);
	};
	auto AddAnim = [&C](const TCHAR* Key, float DurationS, float DelayS, float LoopHz, float Amplitude, float FromScale, float ToScale, int32 Loops, const TCHAR* Ease)
	{
		FHUDAnimDef A;
		A.DurationS = DurationS;
		A.DelayS = DelayS;
		A.LoopHz = LoopHz;
		A.Amplitude = Amplitude;
		A.FromScale = FromScale;
		A.ToScale = ToScale;
		A.Loops = Loops;
		A.Ease = HelmetHUDParseEase(Ease);
		C.Anims.Add(FName(Key), A);
	};
	auto AddElement = [&C](const TCHAR* Id, const TCHAR* Parent, const TCHAR* BindAs, const FVector2D& AMin, const FVector2D& AMax,
		const FVector2D& Align, const FVector2D& Pos, const FVector2D& Ext, int32 Z, float Depth,
		const TCHAR* FontKey, const TCHAR* Typeface, int32 FontSize, int32 LetterSpacing, const TCHAR* Material)
	{
		FHUDElementDef E;
		E.Id = FName(Id);
		E.Parent = FName(Parent);
		E.BindAs = FName(BindAs);
		E.AnchorMin = AMin;
		E.AnchorMax = AMax;
		E.Alignment = Align;
		E.Position = Pos;
		E.Extent = Ext;
		E.ZOrder = Z;
		E.Depth = Depth;
		E.FontKey = FName(FontKey);
		E.Typeface = FName(Typeface);
		E.FontSize = FontSize;
		E.LetterSpacing = LetterSpacing;
		E.Material = FName(Material);
		C.Elements.Add(E);
	};

#include "Generated/HelmetHUDDefaults.inl"

	return C;
}

// ------------------------------------------------------------------------------------------------
// JSON overlay

namespace HelmetHUDJson
{
	using FObj = TSharedPtr<FJsonObject>;

	FObj Child(const FObj& O, const FString& Key)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		if (O.IsValid() && O->TryGetObjectField(Key, Out) && Out && Out->IsValid()) { return *Out; }
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* Array(const FObj& O, const FString& Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* Out = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Key, Out)) { return Out; }
		return nullptr;
	}

	/** Reads a number into Out if present, finite and in [Min, Max]; otherwise keeps Out (warns if present but bad). */
	bool Num(const FObj& O, const FString& Key, float& Out, float Min, float Max, const FString& Ctx)
	{
		if (!O.IsValid() || !O->HasField(Key)) { return false; }
		double V = 0.0;
		if (O->TryGetNumberField(Key, V) && FMath::IsFinite(V) && V >= Min && V <= Max)
		{
			Out = float(V);
			return true;
		}
		UE_LOG(LogIronManArena, Warning, TEXT("hud_layout.json: %s.%s invalid, using default %g"), *Ctx, *Key, Out);
		return false;
	}

	bool Int(const FObj& O, const FString& Key, int32& Out, int32 Min, int32 Max, const FString& Ctx)
	{
		float F = float(Out);
		if (Num(O, Key, F, float(Min), float(Max), Ctx)) { Out = FMath::RoundToInt(F); return true; }
		return false;
	}

	bool Str(const FObj& O, const FString& Key, FString& Out)
	{
		FString S;
		if (O.IsValid() && O->TryGetStringField(Key, S)) { Out = S; return true; }
		return false;
	}

	/** {"hex":"#RRGGBBAA","linear":{"R","G","B","A"}}: linear wins, hex (sRGB) is the fallback. */
	bool Color(const FObj& O, const FString& Key, FLinearColor& Out, const FString& Ctx)
	{
		const FObj C = Child(O, Key);
		if (!C.IsValid()) { return false; }
		if (const FObj Lin = Child(C, TEXT("linear")))
		{
			double R = -1.0, G = -1.0, B = -1.0, A = 1.0;
			const bool bHas = Lin->TryGetNumberField(TEXT("R"), R) && Lin->TryGetNumberField(TEXT("G"), G) && Lin->TryGetNumberField(TEXT("B"), B);
			Lin->TryGetNumberField(TEXT("A"), A);
			auto InRange = [](double V) { return FMath::IsFinite(V) && V >= 0.0 && V <= 1.0; };
			if (bHas && InRange(R) && InRange(G) && InRange(B) && InRange(A))
			{
				Out = FLinearColor(float(R), float(G), float(B), float(A));
				return true;
			}
		}
		FString Hex;
		if (C->TryGetStringField(TEXT("hex"), Hex))
		{
			Hex.RemoveFromStart(TEXT("#"));
			if ((Hex.Len() == 6 || Hex.Len() == 8))
			{
				bool bHexOk = true;
				for (const TCHAR Ch : Hex) { bHexOk &= FChar::IsHexDigit(Ch); }
				if (bHexOk)
				{
					Out = FLinearColor(FColor::FromHex(Hex)); // sRGB -> linear
					return true;
				}
			}
		}
		UE_LOG(LogIronManArena, Warning, TEXT("hud_layout.json: colour %s.%s invalid, using default"), *Ctx, *Key);
		return false;
	}

	void Element(const FObj& E, FHUDElementDef& D, const FHelmetHUDConfig& Cfg)
	{
		const FString Ctx = D.Id.ToString();
		if (const FObj SlotObj = Child(E, TEXT("slot")))
		{
			if (const FObj An = Child(SlotObj, TEXT("anchors")))
			{
				FVector2D AMin = D.AnchorMin, AMax = D.AnchorMax;
				float V0 = AMin.X, V1 = AMin.Y, V2 = AMax.X, V3 = AMax.Y;
				// Read all four (each one logs its own problem), then combine.
				const bool bMinX = Num(An, TEXT("minX"), V0, 0.f, 1.f, Ctx);
				const bool bMinY = Num(An, TEXT("minY"), V1, 0.f, 1.f, Ctx);
				const bool bMaxX = Num(An, TEXT("maxX"), V2, 0.f, 1.f, Ctx);
				const bool bMaxY = Num(An, TEXT("maxY"), V3, 0.f, 1.f, Ctx);
				const bool bOk = bMinX && bMinY && bMaxX && bMaxY;
				if (bOk && V0 <= V2 && V1 <= V3)
				{
					D.AnchorMin = FVector2D(V0, V1);
					D.AnchorMax = FVector2D(V2, V3);
				}
				else if (bOk)
				{
					UE_LOG(LogIronManArena, Warning, TEXT("hud_layout.json: %s anchors min > max, using defaults"), *Ctx);
				}
			}
			if (const FObj Al = Child(SlotObj, TEXT("alignment")))
			{
				float AX = float(D.Alignment.X), AY = float(D.Alignment.Y);
				Num(Al, TEXT("x"), AX, 0.f, 1.f, Ctx);
				Num(Al, TEXT("y"), AY, 0.f, 1.f, Ctx);
				D.Alignment = FVector2D(AX, AY);
			}
			if (const FObj Off = Child(SlotObj, TEXT("offsets")))
			{
				float L = float(D.Position.X), T = float(D.Position.Y), R = float(D.Extent.X), B = float(D.Extent.Y);
				Num(Off, TEXT("left"), L, -10000.f, 10000.f, Ctx);
				Num(Off, TEXT("top"), T, -10000.f, 10000.f, Ctx);
				Num(Off, TEXT("right"), R, -10000.f, 10000.f, Ctx);
				Num(Off, TEXT("bottom"), B, -10000.f, 10000.f, Ctx);
				D.Position = FVector2D(L, T);
				D.Extent = FVector2D(R, B);
			}
			Int(SlotObj, TEXT("zOrder"), D.ZOrder, -1000, 1000, Ctx);
			if (D.Parent.IsNone()) { Num(SlotObj, TEXT("parallaxDepth"), D.Depth, 0.f, 1.f, Ctx); }
		}
		if (const FObj F = Child(E, TEXT("font")))
		{
			FString Family;
			if (Str(F, TEXT("family"), Family))
			{
				if (Cfg.Fonts.Contains(FName(*Family))) { D.FontKey = FName(*Family); }
				else { UE_LOG(LogIronManArena, Warning, TEXT("hud_layout.json: %s font %s unknown"), *Ctx, *Family); }
			}
			FString Typeface;
			if (Str(F, TEXT("typeface"), Typeface)) { D.Typeface = FName(*Typeface); }
			Int(F, TEXT("size"), D.FontSize, 6, 72, Ctx);
			Int(F, TEXT("letterSpacing"), D.LetterSpacing, -200, 1000, Ctx);
		}
		FString Mat;
		if (Str(E, TEXT("material"), Mat)) { D.Material = FName(*Mat); }
	}
}

FString FHelmetHUDConfig::DefaultPath()
{
	return FPaths::ProjectContentDir() / TEXT("UI/hud_layout.json");
}

FHelmetHUDConfig FHelmetHUDConfig::Load(const FString& Path)
{
	using namespace HelmetHUDJson;
	FHelmetHUDConfig Cfg = MakeDefaults();
	Cfg.SourcePath = Path;

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogIronManArena, Log, TEXT("Helmet HUD: %s not found, using compiled-in defaults"), *Path);
		return Cfg;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogIronManArena, Warning, TEXT("Helmet HUD: %s does not parse (%s), using defaults"), *Path, *Reader->GetErrorMessage());
		return Cfg;
	}
	Cfg.bLoadedFromFile = true;

	if (const FObj Ref = Child(Root, TEXT("referenceResolution")))
	{
		float RX = float(Cfg.ReferenceResolution.X), RY = float(Cfg.ReferenceResolution.Y);
		Num(Ref, TEXT("x"), RX, 320.f, 16384.f, TEXT("referenceResolution"));
		Num(Ref, TEXT("y"), RY, 240.f, 16384.f, TEXT("referenceResolution"));
		Cfg.ReferenceResolution = FVector2D(RX, RY);
	}

	// Personas (known keys only; no iteration over FJsonObject::Values).
	if (const FObj PersonasObj = Child(Root, TEXT("personas")))
	{
		for (FHUDPersonaPalette& P : Cfg.Personas)
		{
			const FObj E = Child(PersonasObj, P.Key);
			if (!E.IsValid()) { continue; }
			const FString Ctx = TEXT("personas.") + P.Key;
			Str(E, TEXT("label"), P.Label);
			Color(E, TEXT("primary"), P.Primary, Ctx);
			Color(E, TEXT("secondary"), P.Secondary, Ctx);
			Color(E, TEXT("warning"), P.Warning, Ctx);
			Color(E, TEXT("critical"), P.Critical, Ctx);
			Color(E, TEXT("subtitle"), P.Subtitle, Ctx);
			Int(E, TEXT("glyphIndex"), P.GlyphIndex, 0, 15, Ctx);
			Str(E, TEXT("font"), P.FontFamily);
			Int(E, TEXT("letterSpacingBias"), P.LetterSpacingBias, -200, 1000, Ctx);
		}
	}

	auto OverlayColors = [](const FObj& Src, TMap<FName, FLinearColor>& Dst, const TCHAR* Ctx)
	{
		if (!Src.IsValid()) { return; }
		for (TPair<FName, FLinearColor>& Entry : Dst)
		{
			Color(Src, Entry.Key.ToString(), Entry.Value, Ctx);
		}
	};
	OverlayColors(Child(Root, TEXT("colors")), Cfg.Colors, TEXT("colors"));
	OverlayColors(Child(Root, TEXT("colorBlindPalette")), Cfg.ColorBlind, TEXT("colorBlindPalette"));

	if (const FObj Fonts = Child(Root, TEXT("fonts")))
	{
		for (TPair<FName, FHUDFontDef>& Entry : Cfg.Fonts)
		{
			if (const FObj F = Child(Fonts, Entry.Key.ToString()))
			{
				Str(F, TEXT("family"), Entry.Value.Family);
				Str(F, TEXT("asset"), Entry.Value.Asset);
			}
		}
	}

	if (const FObj T = Child(Root, TEXT("thresholds")))
	{
		if (const FObj H = Child(T, TEXT("heat")))
		{
			Num(H, TEXT("max"), Cfg.HeatMax, 1.f, 100000.f, TEXT("heat"));
			Num(H, TEXT("caution"), Cfg.HeatCaution, 0.f, Cfg.HeatMax, TEXT("heat"));
			Num(H, TEXT("warning"), Cfg.HeatWarning, 0.f, Cfg.HeatMax, TEXT("heat"));
			Num(H, TEXT("critical"), Cfg.HeatCritical, 0.f, Cfg.HeatMax, TEXT("heat"));
			Num(H, TEXT("overheat"), Cfg.HeatOverheat, 0.f, Cfg.HeatMax, TEXT("heat"));
			Num(H, TEXT("recoverAt"), Cfg.HeatRecoverAt, 0.f, Cfg.HeatMax, TEXT("heat"));
			Num(H, TEXT("ventDurationS"), Cfg.VentDurationS, 0.f, 60.f, TEXT("heat"));
			Num(H, TEXT("ventCooldownS"), Cfg.VentCooldownS, 0.f, 600.f, TEXT("heat"));
			if (!(Cfg.HeatRecoverAt < Cfg.HeatCaution && Cfg.HeatCaution < Cfg.HeatWarning && Cfg.HeatWarning < Cfg.HeatCritical && Cfg.HeatCritical <= Cfg.HeatOverheat))
			{
				const FHelmetHUDConfig D = MakeDefaults();
				Cfg.HeatCaution = D.HeatCaution; Cfg.HeatWarning = D.HeatWarning; Cfg.HeatCritical = D.HeatCritical;
				Cfg.HeatOverheat = D.HeatOverheat; Cfg.HeatRecoverAt = D.HeatRecoverAt; Cfg.HeatMax = D.HeatMax;
				UE_LOG(LogIronManArena, Warning, TEXT("hud_layout.json: heat thresholds not ordered, using defaults"));
			}
		}
		if (const FObj P = Child(T, TEXT("power")))
		{
			Num(P, TEXT("low"), Cfg.PowerLow, 0.f, 1.f, TEXT("power"));
			Num(P, TEXT("critical"), Cfg.PowerCritical, 0.f, 1.f, TEXT("power"));
		}
		if (const FObj I = Child(T, TEXT("integrity")))
		{
			Num(I, TEXT("damaged"), Cfg.IntegrityDamaged, 0.f, 1.f, TEXT("integrity"));
			Num(I, TEXT("critical"), Cfg.IntegrityCritical, 0.f, 1.f, TEXT("integrity"));
			Num(I, TEXT("offline"), Cfg.IntegrityOffline, 0.f, 1.f, TEXT("integrity"));
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Tiers = Array(T, TEXT("glitchTiers")))
		{
			TArray<FHUDGlitchTier> Parsed;
			for (const TSharedPtr<FJsonValue>& V : *Tiers)
			{
				const FObj G = V.IsValid() ? V->AsObject() : nullptr;
				if (!G.IsValid()) { continue; }
				FHUDGlitchTier Tier;
				Int(G, TEXT("tier"), Tier.Tier, 0, 9, TEXT("glitchTiers"));
				Num(G, TEXT("healthBelow"), Tier.HealthBelow, 0.f, 2.f, TEXT("glitchTiers"));
				Num(G, TEXT("glitch"), Tier.Glitch, 0.f, 1.f, TEXT("glitchTiers"));
				Parsed.Add(Tier);
			}
			if (Parsed.Num() > 0) { Cfg.GlitchTiers = Parsed; }
		}
		if (const FObj Lk = Child(T, TEXT("lock")))
		{
			Num(Lk, TEXT("coneDeg"), Cfg.LockConeDeg, 0.f, 90.f, TEXT("lock"));
			Num(Lk, TEXT("acquireRangeM"), Cfg.LockAcquireRangeM, 0.f, 100000.f, TEXT("lock"));
			Num(Lk, TEXT("lockTimeRepulsorS"), Cfg.LockTimeRepulsorS, 0.01f, 30.f, TEXT("lock"));
			Num(Lk, TEXT("lockTimeMissileS"), Cfg.LockTimeMissileS, 0.01f, 30.f, TEXT("lock"));
			Num(Lk, TEXT("loseGraceS"), Cfg.LockLoseGraceS, 0.f, 10.f, TEXT("lock"));
			Int(Lk, TEXT("maxBrackets"), Cfg.MaxBrackets, 0, 128, TEXT("lock"));
			Int(Lk, TEXT("maxLocks"), Cfg.MaxLocks, 0, 32, TEXT("lock"));
			Int(Lk, TEXT("maxOffscreen"), Cfg.MaxOffscreen, 0, 64, TEXT("lock"));
			Num(Lk, TEXT("offscreenEllipseInset"), Cfg.OffscreenEllipseInset, 0.f, 0.45f, TEXT("lock"));
		}
		if (const FObj Rd = Child(T, TEXT("radar")))
		{
			Num(Rd, TEXT("rangeGroundM"), Cfg.RadarRangeGroundM, 1.f, 100000.f, TEXT("radar"));
			Num(Rd, TEXT("rangeFlightM"), Cfg.RadarRangeFlightM, 1.f, 100000.f, TEXT("radar"));
			Num(Rd, TEXT("rangeBossM"), Cfg.RadarRangeBossM, 1.f, 100000.f, TEXT("radar"));
			Num(Rd, TEXT("elevationMarkerM"), Cfg.RadarElevationMarkerM, 0.f, 10000.f, TEXT("radar"));
			Num(Rd, TEXT("sweepPeriodS"), Cfg.RadarSweepPeriodS, 0.1f, 60.f, TEXT("radar"));
		}
		if (const FObj Tp = Child(T, TEXT("tapes")))
		{
			if (const FObj S = Child(Tp, TEXT("speed")))
			{
				Num(S, TEXT("windowHalf"), Cfg.SpeedWindowHalf, 1.f, 10000.f, TEXT("tapes.speed"));
				Num(S, TEXT("minor"), Cfg.SpeedMinor, 0.1f, 1000.f, TEXT("tapes.speed"));
				Num(S, TEXT("major"), Cfg.SpeedMajor, 0.1f, 1000.f, TEXT("tapes.speed"));
				Num(S, TEXT("pxPerUnit"), Cfg.SpeedPxPerUnit, 0.01f, 100.f, TEXT("tapes.speed"));
				Num(S, TEXT("machFrom"), Cfg.MachFrom, 0.f, 50.f, TEXT("tapes.speed"));
			}
			if (const FObj A = Child(Tp, TEXT("altitude")))
			{
				Num(A, TEXT("windowHalf"), Cfg.AltWindowHalf, 1.f, 100000.f, TEXT("tapes.altitude"));
				Num(A, TEXT("minor"), Cfg.AltMinor, 0.1f, 10000.f, TEXT("tapes.altitude"));
				Num(A, TEXT("major"), Cfg.AltMajor, 0.1f, 10000.f, TEXT("tapes.altitude"));
				Num(A, TEXT("pxPerUnit"), Cfg.AltPxPerUnit, 0.01f, 100.f, TEXT("tapes.altitude"));
				Num(A, TEXT("aglBoxBelowM"), Cfg.AglBoxBelowM, 0.f, 100000.f, TEXT("tapes.altitude"));
			}
			if (const FObj Hd = Child(Tp, TEXT("heading")))
			{
				Num(Hd, TEXT("windowHalf"), Cfg.HeadingWindowHalf, 1.f, 180.f, TEXT("tapes.heading"));
				Num(Hd, TEXT("minor"), Cfg.HeadingMinor, 0.1f, 90.f, TEXT("tapes.heading"));
				Num(Hd, TEXT("major"), Cfg.HeadingMajor, 0.1f, 180.f, TEXT("tapes.heading"));
				Num(Hd, TEXT("pxPerUnit"), Cfg.HeadingPxPerUnit, 0.01f, 100.f, TEXT("tapes.heading"));
			}
			if (const FObj Pl = Child(Tp, TEXT("pitchLadder")))
			{
				Num(Pl, TEXT("stepDeg"), Cfg.LadderStepDeg, 1.f, 90.f, TEXT("tapes.pitchLadder"));
				Num(Pl, TEXT("pxPerDeg"), Cfg.LadderPxPerDeg, 0.1f, 100.f, TEXT("tapes.pitchLadder"));
				Num(Pl, TEXT("visibleDeg"), Cfg.LadderVisibleDeg, 1.f, 90.f, TEXT("tapes.pitchLadder"));
			}
		}
		Num(T, TEXT("flashMaxHz"), Cfg.FlashMaxHz, 0.f, 3.f, TEXT("thresholds")); // photosensitivity cap: never above 3 Hz
	}

	if (const TArray<TSharedPtr<FJsonValue>>* Warn = Array(Root, TEXT("warnings")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Warn)
		{
			const FObj W = V.IsValid() ? V->AsObject() : nullptr;
			FString Id;
			if (!W.IsValid() || !Str(W, TEXT("id"), Id)) { continue; }
			FHUDWarningDef* Def = Cfg.Warnings.FindByPredicate([&Id](const FHUDWarningDef& D) { return D.Id == FName(*Id); });
			if (!Def)
			{
				FHUDWarningDef NewDef;
				NewDef.Id = FName(*Id);
				Def = &Cfg.Warnings.Add_GetRef(NewDef);
			}
			Int(W, TEXT("priority"), Def->Priority, 0, 99, Id);
			Str(W, TEXT("text"), Def->Text);
			FString Key;
			if (Str(W, TEXT("color"), Key)) { Def->ColorKey = FName(*Key); }
			if (Str(W, TEXT("icon"), Key)) { Def->Icon = FName(*Key); }
			Num(W, TEXT("flashHz"), Def->FlashHz, 0.f, Cfg.FlashMaxHz, Id);
			Num(W, TEXT("minShowS"), Def->MinShowS, 0.f, 60.f, Id);
		}
	}
	if (const FObj St = Child(Root, TEXT("warningStacking")))
	{
		Int(St, TEXT("maxVisible"), Cfg.WarningMaxVisible, 1, 3, TEXT("warningStacking")); // 3 slots exist
		Num(St, TEXT("refreshExtendsS"), Cfg.WarningRefreshExtendsS, 0.f, 60.f, TEXT("warningStacking"));
		Num(St, TEXT("demoteSlideMs"), Cfg.WarningDemoteSlideMs, 0.f, 5000.f, TEXT("warningStacking"));
		Num(St, TEXT("lowerPriorityDimTo"), Cfg.WarningLowerDim, 0.f, 1.f, TEXT("warningStacking"));
	}

	if (const FObj Px = Child(Root, TEXT("parallax")))
	{
		if (const FObj Mo = Child(Px, TEXT("maxOffsetPx")))
		{
			float MX = float(Cfg.ParallaxMaxOffsetPx.X), MY = float(Cfg.ParallaxMaxOffsetPx.Y);
			Num(Mo, TEXT("x"), MX, 0.f, 500.f, TEXT("parallax"));
			Num(Mo, TEXT("y"), MY, 0.f, 500.f, TEXT("parallax"));
			Cfg.ParallaxMaxOffsetPx = FVector2D(MX, MY);
		}
		Num(Px, TEXT("pxPerDegPerSec"), Cfg.ParallaxPxPerDegPerSec, 0.f, 10.f, TEXT("parallax"));
		if (const FObj Hl = Child(Px, TEXT("lagHalfLifeMs")))
		{
			Num(Hl, TEXT("L1_Near"), Cfg.LagHalfLifeNearMs, 0.f, 2000.f, TEXT("parallax"));
			Num(Hl, TEXT("L2_Mid"), Cfg.LagHalfLifeMidMs, 0.f, 2000.f, TEXT("parallax"));
			Num(Hl, TEXT("L3_Far"), Cfg.LagHalfLifeFarMs, 0.f, 2000.f, TEXT("parallax"));
		}
		Num(Px, TEXT("impactKickPx"), Cfg.ImpactKickPx, 0.f, 200.f, TEXT("parallax"));
		Num(Px, TEXT("impactRecoverMs"), Cfg.ImpactRecoverMs, 1.f, 5000.f, TEXT("parallax"));
	}

	if (const FObj Vw = Child(Root, TEXT("visorWarp")))
	{
		if (const FObj Params = Child(Vw, TEXT("params")))
		{
			for (TPair<FName, float>& Entry : Cfg.VisorWarp)
			{
				Num(Params, Entry.Key.ToString(), Entry.Value, -1000.f, 10000.f, TEXT("visorWarp.params"));
			}
		}
	}

	if (const FObj Anims = Child(Root, TEXT("animations")))
	{
		for (TPair<FName, FHUDAnimDef>& Entry : Cfg.Anims)
		{
			const FObj A = Child(Anims, Entry.Key.ToString());
			if (!A.IsValid()) { continue; }
			const FString Ctx = Entry.Key.ToString();
			FHUDAnimDef& D = Entry.Value;
			float Ms = D.DurationS * 1000.f;
			if (Num(A, TEXT("durationMs"), Ms, 0.f, 60000.f, Ctx)) { D.DurationS = Ms / 1000.f; }
			Ms = D.DelayS * 1000.f;
			if (Num(A, TEXT("delayMs"), Ms, 0.f, 60000.f, Ctx)) { D.DelayS = Ms / 1000.f; }
			Num(A, TEXT("loopHz"), D.LoopHz, 0.f, 60.f, Ctx);
			Num(A, TEXT("amplitude"), D.Amplitude, 0.f, 100.f, Ctx);
			Num(A, TEXT("amplitudePx"), D.Amplitude, 0.f, 100.f, Ctx);
			Num(A, TEXT("fromScale"), D.FromScale, 0.f, 10.f, Ctx);
			Num(A, TEXT("fromScaleY"), D.FromScale, 0.f, 10.f, Ctx);
			Num(A, TEXT("toScale"), D.ToScale, 0.f, 10.f, Ctx);
			Int(A, TEXT("loops"), D.Loops, 1, 100, Ctx);
			FString Ease;
			if (Str(A, TEXT("easing"), Ease)) { D.Ease = HelmetHUDParseEase(Ease); }
		}
		if (const FObj Boot = Child(Anims, TEXT("bootSequence")))
		{
			if (const FObj Stagger = Child(Boot, TEXT("stagger")))
			{
				static const TCHAR* LayerKeys[] = { TEXT("L0_Visor"), TEXT("L1_Near"), TEXT("L2_Mid"), TEXT("L3_Far") };
				for (int32 i = 0; i < UE_ARRAY_COUNT(LayerKeys) && i < Cfg.BootStaggerS.Num(); ++i)
				{
					float Ms = Cfg.BootStaggerS[i] * 1000.f;
					if (Num(Stagger, LayerKeys[i], Ms, 0.f, 10000.f, TEXT("bootSequence.stagger"))) { Cfg.BootStaggerS[i] = Ms / 1000.f; }
				}
			}
		}
	}

	if (const TArray<TSharedPtr<FJsonValue>>* Els = Array(Root, TEXT("elements")))
	{
		auto Apply = [&Cfg](const FObj& E)
		{
			FString Id;
			if (!E.IsValid() || !Str(E, TEXT("id"), Id)) { return; }
			FHUDElementDef* D = Cfg.Elements.FindByPredicate([&Id](const FHUDElementDef& X) { return X.Id == FName(*Id); });
			if (!D)
			{
				UE_LOG(LogIronManArena, Verbose, TEXT("hud_layout.json: element %s has no C++ binding, ignored"), *Id);
				return;
			}
			Element(E, *D, Cfg);
		};
		for (const TSharedPtr<FJsonValue>& V : *Els)
		{
			const FObj E = V.IsValid() ? V->AsObject() : nullptr;
			Apply(E);
			if (const TArray<TSharedPtr<FJsonValue>>* Kids = Array(E, TEXT("children")))
			{
				for (const TSharedPtr<FJsonValue>& K : *Kids) { Apply(K.IsValid() ? K->AsObject() : nullptr); }
			}
		}
	}

	UE_LOG(LogIronManArena, Log, TEXT("Helmet HUD: layout loaded from %s"), *Path);
	return Cfg;
}

// ------------------------------------------------------------------------------------------------
// Lookups

FLinearColor FHelmetHUDConfig::GetColor(FName Key, bool bColorBlindSafe, const FLinearColor& Fallback) const
{
	if (bColorBlindSafe)
	{
		if (const FLinearColor* CB = ColorBlind.Find(Key)) { return *CB; }
	}
	if (const FLinearColor* C = Colors.Find(Key)) { return *C; }
	return Fallback;
}

const FHUDAnimDef& FHelmetHUDConfig::GetAnim(FName Key) const
{
	static const FHUDAnimDef Empty;
	const FHUDAnimDef* A = Anims.Find(Key);
	return A ? *A : Empty;
}

const FHUDElementDef* FHelmetHUDConfig::FindElement(FName Id) const
{
	return Elements.FindByPredicate([Id](const FHUDElementDef& E) { return E.Id == Id; });
}

const FHUDWarningDef* FHelmetHUDConfig::FindWarning(FName Id) const
{
	return Warnings.FindByPredicate([Id](const FHUDWarningDef& W) { return W.Id == Id; });
}

const FHUDPersonaPalette& FHelmetHUDConfig::GetPersona(int32 PersonaIndex) const
{
	static const FHUDPersonaPalette Fallback;
	if (Personas.Num() == 0) { return Fallback; }
	return Personas[FMath::Clamp(PersonaIndex, 0, Personas.Num() - 1)];
}

int32 FHelmetHUDConfig::LayerForZ(int32 ZOrder)
{
	if (ZOrder >= 20 && ZOrder <= 29) { return 2; }
	if (ZOrder >= 30 && ZOrder <= 49) { return 3; }
	if (ZOrder >= 50 && ZOrder <= 79) { return 1; }
	return 0;
}

float FHelmetHUDConfig::GlitchForHealth(float HealthFrac, int32* OutTier) const
{
	float Best = 0.f;
	int32 BestTier = 0;
	for (const FHUDGlitchTier& G : GlitchTiers)
	{
		if (HealthFrac < G.HealthBelow && G.Glitch >= Best)
		{
			Best = G.Glitch;
			BestTier = G.Tier;
		}
	}
	if (OutTier) { *OutTier = BestTier; }
	return Best;
}
