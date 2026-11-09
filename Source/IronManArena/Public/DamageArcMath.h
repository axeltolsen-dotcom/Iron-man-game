// DamageArcMath: the pure math behind FRIDAY's directional damage arcs (contrib/friday/realism).
//
// Header-only with no UObject dependencies, so the same code is checked by
//   - the UE automation test IronManArena.HUD.DamageArc (Private/Tests/DamageArcTests.cpp), and
//   - the standalone clang test contrib/friday/realism/tests/test_damage_arc_math.cpp
//     (built with -DFRIDAY_DAMAGEARC_STANDALONE, no engine needed).
// It mirrors the JS reference in ironman-3d/contrib/friday/realism/realistic_hud.js
// (hitAngle / arcParams / fadeAlpha / DamageArcs) value for value.
//
// Conventions (Unreal): Z up, X forward, Y right, yaw in DEGREES, clockwise seen from above.
// The screen angle is 0 = ahead (top of the screen), +90 = right, -90 = left, +-180 = behind.
#pragma once

#if defined(FRIDAY_DAMAGEARC_STANDALONE)
#include <cmath>
#include <cstdint>
#include <vector>
namespace FridayArcMathImpl
{
	inline float Atan2(float Y, float X) { return std::atan2(Y, X); }
	inline float Sin(float A) { return std::sin(A); }
	inline float Cos(float A) { return std::cos(A); }
	inline float Sqrt(float A) { return std::sqrt(A); }
	inline float Pow(float A, float B) { return std::pow(A, B); }
	inline float Fmod(float A, float B) { return std::fmod(A, B); }
	template <typename T> using TArrayImpl = std::vector<T>;
}
#define FRIDAY_ARC_ARRAY_ADD(Arr, V) (Arr).push_back(V)
#define FRIDAY_ARC_ARRAY_NUM(Arr) static_cast<int32_t>((Arr).size())
#define FRIDAY_ARC_ARRAY_REMOVEAT(Arr, I) (Arr).erase((Arr).begin() + (I))
#define FRIDAY_ARC_INT int32_t
#else
#include "CoreMinimal.h"
namespace FridayArcMathImpl
{
	inline float Atan2(float Y, float X) { return FMath::Atan2(Y, X); }
	inline float Sin(float A) { return FMath::Sin(A); }
	inline float Cos(float A) { return FMath::Cos(A); }
	inline float Sqrt(float A) { return FMath::Sqrt(A); }
	inline float Pow(float A, float B) { return FMath::Pow(A, B); }
	inline float Fmod(float A, float B) { return FMath::Fmod(A, B); }
	template <typename T> using TArrayImpl = TArray<T>;
}
#define FRIDAY_ARC_ARRAY_ADD(Arr, V) (Arr).Add(V)
#define FRIDAY_ARC_ARRAY_NUM(Arr) (Arr).Num()
#define FRIDAY_ARC_ARRAY_REMOVEAT(Arr, I) (Arr).RemoveAt(I)
#define FRIDAY_ARC_INT int32
#endif

namespace DamageArcMath
{
	constexpr float Pi = 3.14159265358979f;
	constexpr float Deg = Pi / 180.f;

	inline float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }
	inline float Lerp(float A, float B, float T) { return A + (B - A) * T; }
	inline float Smooth(float U) { U = Clamp01(U); return U * U * (3.f - 2.f * U); }

	/** Wraps radians to [-pi, pi). */
	inline float WrapRad(float A)
	{
		A = FridayArcMathImpl::Fmod(A + Pi, 2.f * Pi);
		if (A < 0.f) { A += 2.f * Pi; }
		return A - Pi;
	}

	/**
	 * Screen angle (radians) of a hit relative to the view.
	 * FromX/FromY: horizontal components of the vector from the player TOWARD the hit source
	 * (world X/Y). ControlYawDeg: the controller's yaw (GetControlRotation().Yaw).
	 * bOutValid is false when the vector is vertical (directly above or below); the angle is then 0.
	 */
	inline float RelativeAngleRad(float FromX, float FromY, float ControlYawDeg, bool* bOutValid = nullptr)
	{
		const float Len = FridayArcMathImpl::Sqrt(FromX * FromX + FromY * FromY);
		if (bOutValid) { *bOutValid = Len > 1e-4f; }
		if (Len <= 1e-4f) { return 0.f; }
		const float Yaw = ControlYawDeg * Deg;
		const float Fx = FridayArcMathImpl::Cos(Yaw), Fy = FridayArcMathImpl::Sin(Yaw);   // forward
		const float Rx = -Fy, Ry = Fx;                                                     // right (Y right, Z up)
		return FridayArcMathImpl::Atan2(FromX * Rx + FromY * Ry, FromX * Fx + FromY * Fy);
	}

	struct FArcConfig
	{
		float LifeSeconds = 0.5f;        // alpha reaches exactly 0 here
		float MergeDeg = 30.f;
		float FullSeverityAt = 0.25f;    // damage / max health that gives a full-size arc
		float MinHalfWidthDeg = 11.f, MaxHalfWidthDeg = 38.f;
		float MinThickness = 0.006f, MaxThickness = 0.02f;   // fraction of min(viewport w, h)
		float MinAlpha = 0.32f, MaxAlpha = 0.85f;            // never a full-strength flash
		float CriticalSeverity = 0.8f;
		FRIDAY_ARC_INT MaxArcs = 6;
	};

	struct FArcLook { float Severity = 0.f; float HalfWidthRad = 0.f; float Thickness = 0.f; float Alpha = 0.f; };

	inline FArcLook LookForSeverity(float Severity, const FArcConfig& C)
	{
		FArcLook L;
		L.Severity = Clamp01(Severity);
		L.HalfWidthRad = Lerp(C.MinHalfWidthDeg, C.MaxHalfWidthDeg, L.Severity) * Deg;
		L.Thickness = Lerp(C.MinThickness, C.MaxThickness, L.Severity);
		L.Alpha = Lerp(C.MinAlpha, C.MaxAlpha, L.Severity);
		return L;
	}

	/** Size/opacity from damage. Monotonic, saturates at FullSeverityAt of max health. */
	inline FArcLook LookForDamage(float Damage, float MaxHealth, const FArcConfig& C)
	{
		const float Frac = (Damage > 0.f ? Damage : 0.f) / (MaxHealth > 0.f ? MaxHealth : 100.f);
		return LookForSeverity(FridayArcMathImpl::Pow(Clamp01(Frac / C.FullSeverityAt), 0.75f), C);
	}

	/** 1 at the hit, eases to exactly 0 at Life. */
	inline float FadeAlpha(float Age, float Life)
	{
		const float L = Life > 0.f ? Life : 0.5f;
		if (!(Age >= 0.f)) { return 1.f; }
		if (Age >= L) { return 0.f; }
		return 1.f - Smooth(Age / L);
	}

	struct FArc
	{
		float AngleRad = 0.f;
		float Age = 0.f;
		FRIDAY_ARC_INT Hits = 1;
		bool bCritical = false;
		FArcLook Look;
		float CurrentAlpha(const FArcConfig& C) const { return Look.Alpha * FadeAlpha(Age, C.LifeSeconds); }
	};

	/** The live arc set: Add hits, Advance with UNDILATED time (hit-stop must not freeze the fade). */
	struct FArcSet
	{
		FArcConfig Config;
		FridayArcMathImpl::TArrayImpl<FArc> Arcs;

		/** Returns the index of the new or merged arc. */
		FRIDAY_ARC_INT Add(float AngleRad, float Damage, float MaxHealth, bool bForceCritical)
		{
			AngleRad = WrapRad(AngleRad);
			const FArcLook New = LookForDamage(Damage, MaxHealth, Config);
			FRIDAY_ARC_INT Best = -1; float BestD = Config.MergeDeg * Deg;
			for (FRIDAY_ARC_INT i = 0; i < FRIDAY_ARC_ARRAY_NUM(Arcs); ++i)
			{
				const float D = WrapRad(Arcs[i].AngleRad - AngleRad);
				const float AD = D < 0.f ? -D : D;
				if (AD <= BestD) { BestD = AD; Best = i; }
			}
			if (Best >= 0)
			{
				FArc& A = Arcs[Best];
				const float Live = A.Look.Severity * FadeAlpha(A.Age, Config.LifeSeconds);
				const float Sev = Clamp01(1.f - (1.f - Live) * (1.f - New.Severity));     // stack, saturating
				const float Wa = Live > 1e-3f ? Live : 1e-3f, Wb = New.Severity > 1e-3f ? New.Severity : 1e-3f;
				const float X = FridayArcMathImpl::Cos(A.AngleRad) * Wa + FridayArcMathImpl::Cos(AngleRad) * Wb;
				const float Y = FridayArcMathImpl::Sin(A.AngleRad) * Wa + FridayArcMathImpl::Sin(AngleRad) * Wb;
				A.AngleRad = WrapRad(FridayArcMathImpl::Atan2(Y, X));
				A.Look = LookForSeverity(Sev, Config);
				A.Age = 0.f; A.Hits += 1;
				A.bCritical = A.bCritical || bForceCritical || Sev >= Config.CriticalSeverity;
				return Best;
			}
			FArc A; A.AngleRad = AngleRad; A.Look = New;
			A.bCritical = bForceCritical || New.Severity >= Config.CriticalSeverity;
			FRIDAY_ARC_ARRAY_ADD(Arcs, A);
			if (FRIDAY_ARC_ARRAY_NUM(Arcs) > Config.MaxArcs)
			{
				FRIDAY_ARC_INT Weakest = 0; float WeakestA = 1e9f;
				for (FRIDAY_ARC_INT i = 0; i < FRIDAY_ARC_ARRAY_NUM(Arcs); ++i)
				{
					const float V = Arcs[i].CurrentAlpha(Config);
					if (V < WeakestA) { WeakestA = V; Weakest = i; }
				}
				FRIDAY_ARC_ARRAY_REMOVEAT(Arcs, Weakest);
			}
			return FRIDAY_ARC_ARRAY_NUM(Arcs) - 1;
		}

		void Advance(float RealDeltaSeconds)
		{
			const float Dt = RealDeltaSeconds > 0.f ? RealDeltaSeconds : 0.f;
			for (FRIDAY_ARC_INT i = FRIDAY_ARC_ARRAY_NUM(Arcs) - 1; i >= 0; --i)
			{
				Arcs[i].Age += Dt;
				if (Arcs[i].Age >= Config.LifeSeconds - 1e-6f) { FRIDAY_ARC_ARRAY_REMOVEAT(Arcs, i); }
			}
		}
	};
}
