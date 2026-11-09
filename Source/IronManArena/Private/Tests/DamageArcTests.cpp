// IronManArena.HUD.DamageArc: FRIDAY's damage-arc math (DamageArcMath.h). No world needed:
//   UnrealEditor IronManArena.uproject -game -nullrhi -unattended \
//     -ExecCmds="Automation RunTests IronManArena.HUD" -TestExit="Automation Test Queue Empty" -log
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DamageArcMath.h"
#include "HUDDataSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFridayDamageArcTest, "IronManArena.HUD.DamageArc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FFridayDamageArcTest::RunTest(const FString& Parameters)
{
	using namespace DamageArcMath;
	auto ToDeg = [](float Rad) { return FMath::RadiansToDegrees(Rad); };
	for (const float Yaw : { 0.f, 90.f, -135.f, 270.f })
	{
		// Unreal: X forward, Y right, Z up; yaw turns X toward Y.
		const float Y = FMath::DegreesToRadians(Yaw);
		const FVector F(FMath::Cos(Y), FMath::Sin(Y), 0.f), R(-FMath::Sin(Y), FMath::Cos(Y), 0.f);
		TestTrue(TEXT("basis matches FRotator"), FRotator(0.f, Yaw, 0.f).Vector().Equals(F, 1e-4f));
		TestEqual(FString::Printf(TEXT("yaw %g front"), Yaw), ToDeg(RelativeAngleRad(F.X, F.Y, Yaw)), 0.f, 0.01f);
		TestEqual(FString::Printf(TEXT("yaw %g right"), Yaw), ToDeg(RelativeAngleRad(R.X, R.Y, Yaw)), 90.f, 0.01f);
		TestEqual(FString::Printf(TEXT("yaw %g left"), Yaw), ToDeg(RelativeAngleRad(-R.X, -R.Y, Yaw)), -90.f, 0.01f);
		TestEqual(FString::Printf(TEXT("yaw %g behind"), Yaw), FMath::Abs(ToDeg(RelativeAngleRad(-F.X, -F.Y, Yaw))), 180.f, 0.01f);
	}
	TestEqual(TEXT("fade 1 at hit"), FadeAlpha(0.f, 0.5f), 1.f);
	TestTrue(TEXT("fade < 1% at 0.49 s"), FadeAlpha(0.49f, 0.5f) < 0.01f);
	TestEqual(TEXT("fade 0 at 0.5 s"), FadeAlpha(0.5f, 0.5f), 0.f);
	FArcConfig C;
	const FArcLook L5 = LookForDamage(5.f, 100.f, C), L20 = LookForDamage(20.f, 100.f, C);
	TestTrue(TEXT("size scales with damage"), L20.HalfWidthRad > L5.HalfWidthRad && L20.Thickness > L5.Thickness && L20.Alpha > L5.Alpha);
	TestTrue(TEXT("opacity capped"), LookForDamage(90.f, 100.f, C).Alpha <= 0.85f + 1e-4f);

	FArcSet Set;
	Set.Add(FMath::DegreesToRadians(10.f), 8.f, 100.f, false);
	Set.Add(FMath::DegreesToRadians(30.f), 8.f, 100.f, false);
	TestEqual(TEXT("close hits merge"), Set.Arcs.Num(), 1);
	Set.Add(FMath::DegreesToRadians(150.f), 8.f, 100.f, false);
	TestEqual(TEXT("far hit is separate"), Set.Arcs.Num(), 2);
	for (int32 i = 0; i < 30; ++i) { Set.Advance(1.f / 60.f); }
	TestEqual(TEXT("all arcs gone after 0.5 s"), Set.Arcs.Num(), 0);

	TestEqual(TEXT("hit from behind at chest height -> spine mount"), int32(UHUDDataSubsystem::RegionForHit(170.f, 0.6f)), int32(EHUDBodyRegion::SpineMount));
	TestEqual(TEXT("head height -> helmet"), int32(UHUDDataSubsystem::RegionForHit(0.f, 0.95f)), int32(EHUDBodyRegion::Helmet));
	return true;
}

#endif
