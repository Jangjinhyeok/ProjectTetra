// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Session/TetrisSessionSubsystem.h"
#include "FSM/TetrisGameCore.h"
#include "Block/TetrisPiece.h"

#if WITH_DEV_AUTOMATION_TESTS

// 서브시스템을 NewObject로 생성해 AdvanceFixedSteps를 직접 구동한다 — 라이브 월드 틱에 의존하지 않음.
// (NewObject 인스턴스는 IsInitialized()=false라 틱 프레임워크가 자동 틱하지 않으므로 결정적)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionSingleStepTest,
	"Tetris.Session.SingleStepAdvance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionSingleStepTest::RunTest(const FString&)
{
	UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
	Sub->StartGame(1);

	const int32 Steps = Sub->AdvanceFixedSteps(1.0f / 60.0f);
	TestEqual(TEXT("1/60초 누적 → 정확히 1 Step"), Steps, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionAccumulatorCarryTest,
	"Tetris.Session.AccumulatorCarry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionAccumulatorCarryTest::RunTest(const FString&)
{
	UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
	Sub->StartGame(1);

	const int32 First = Sub->AdvanceFixedSteps(0.5f / 60.0f);  // 절반 — 미달
	const int32 Second = Sub->AdvanceFixedSteps(0.5f / 60.0f); // 합산 → 1 Step
	TestEqual(TEXT("첫 절반 → 0 Step"), First, 0);
	TestEqual(TEXT("둘째 절반 → 누적 이월로 1 Step"), Second, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionMaxStepsCapTest,
	"Tetris.Session.MaxStepsPerFrameCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionMaxStepsCapTest::RunTest(const FString&)
{
	UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
	Sub->StartGame(1);

	const int32 Steps = Sub->AdvanceFixedSteps(1.0f); // 1초 = 60스텝 분량이지만 캡
	TestEqual(TEXT("거대 dt → MaxStepsPerFrame로 캡"), Steps, Sub->MaxStepsPerFrame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionNotRunningTest,
	"Tetris.Session.NotRunningOrPausedNoStep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionNotRunningTest::RunTest(const FString&)
{
	// 시작 전: bRunning=false → 0 Step
	UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
	TestEqual(TEXT("StartGame 전 → 0 Step"), Sub->AdvanceFixedSteps(1.0f), 0);

	// 시작 후 일시정지 → 0 Step
	Sub->StartGame(1);
	Sub->SetPaused(true);
	TestEqual(TEXT("Pause 중 → 0 Step"), Sub->AdvanceFixedSteps(1.0f), 0);

	// 재개 → 다시 구동
	Sub->SetPaused(false);
	TestTrue(TEXT("Resume 후 → Step 발생"), Sub->AdvanceFixedSteps(1.0f / 60.0f) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionDeterminismTest,
	"Tetris.Session.DeterministicProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionDeterminismTest::RunTest(const FString&)
{
	auto Run = []() -> UTetrisSessionSubsystem*
	{
		UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
		Sub->StartGame(777);
		for (int32 i = 0; i < 600; ++i) // 600스텝 = Lv1에서 10칸 낙하
		{
			Sub->AdvanceFixedSteps(1.0f / 60.0f);
		}
		return Sub;
	};

	UTetrisSessionSubsystem* A = Run();
	UTetrisSessionSubsystem* B = Run();

	const FActivePiece& PA = A->GetGameCore()->GetActivePiece();
	const FActivePiece& PB = B->GetGameCore()->GetActivePiece();

	TestTrue(TEXT("중력으로 전진함 (피봇 Y < 스폰 20)"), PA.PivotPosition.Y < 20);
	TestTrue(TEXT("동일 시드+누적 → 동일 피스 타입"), PA.Type == PB.Type);
	TestTrue(TEXT("동일 피봇"), PA.PivotPosition == PB.PivotPosition);
	TestTrue(TEXT("동일 회전"), PA.RotationState == PB.RotationState);
	TestTrue(TEXT("동일 상태"), A->GetGameCore()->GetState() == B->GetGameCore()->GetState());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
