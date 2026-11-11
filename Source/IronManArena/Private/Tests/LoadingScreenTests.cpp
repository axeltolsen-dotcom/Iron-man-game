// IronManArena.UI.LoadingScreen: progress-bar maths and the headless skip of the Stark boot screen.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "LoadingScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingScreenTest, "IronManArena.UI.LoadingScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FLoadingScreenTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("starts empty"), ULoadingScreenWidget::FakeProgress(0.f, false, 0.f), 0.f, 1e-4f);
	float Prev = 0.f;
	for (float T = 0.1f; T < 30.f; T += 0.1f)
	{
		const float P = ULoadingScreenWidget::FakeProgress(T, false, 0.f);
		TestTrue(TEXT("monotonic while waiting"), P >= Prev);
		TestTrue(TEXT("never full while waiting"), P < 0.9001f);
		Prev = P;
	}
	TestTrue(TEXT("done eases up"), ULoadingScreenWidget::FakeProgress(0.3f, true, 0.1f) > ULoadingScreenWidget::FakeProgress(0.3f, false, 0.f));
	TestEqual(TEXT("done reaches 100%"), ULoadingScreenWidget::FakeProgress(0.3f, true, 5.f), 1.f, 1e-4f);
	TestTrue(TEXT("skipped in automation"), ULoadingScreenWidget::ShouldSkip());
	return true;
}

#endif
