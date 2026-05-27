// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Session/TetrisSessionSubsystem.h"
#include "FSM/TetrisGameCore.h"
#include "Block/TetrisPiece.h"
#include "Input/TetrisHandlingTypes.h"

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

//~ 입력 핸들링 통합 (PC→Session→Handling→GameCore) ---------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionPressMovesOneTest,
	"Tetris.Session.InputPressMovesOneCell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionPressMovesOneTest::RunTest(const FString&)
{
	UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
	Sub->StartGame(1);
	const int32 X0 = Sub->GetGameCore()->GetActivePiece().PivotPosition.X;

	// 좌 press 1회 + held → 1 고정스텝에서 정확히 1칸 좌이동.
	Sub->SetMoveHeld(true, true);
	Sub->PushInputEdge(EInputEdge::MoveLeftPress);
	Sub->AdvanceFixedSteps(1.0f / 60.0f);

	const int32 X1 = Sub->GetGameCore()->GetActivePiece().PivotPosition.X;
	TestEqual(TEXT("press 1회 → 좌로 정확히 1칸"), X1, X0 - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionHeldDasArrTest,
	"Tetris.Session.InputHeldDasArrDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionHeldDasArrTest::RunTest(const FString&)
{
	// 좌 held로 N스텝 누적 → DAS/ARR로 반복 이동. 두 번 실행해도 동일(결정적).
	auto Run = [](int32 NSteps) -> int32
	{
		UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
		Sub->StartGame(777);
		Sub->SetMoveHeld(true, true);
		Sub->PushInputEdge(EInputEdge::MoveLeftPress);
		for (int32 i = 0; i < NSteps; ++i)
		{
			Sub->AdvanceFixedSteps(1.0f / 60.0f);
		}
		return Sub->GetGameCore()->GetActivePiece().PivotPosition.X;
	};

	const int32 AfterPress = Run(1);   // press만 → 1칸
	const int32 AfterHold = Run(20);   // press + DAS 경과 후 ARR 반복 → 더 많이 좌이동(벽 클램프)
	const int32 AfterHold2 = Run(20);  // 동일 입력 재현

	TestTrue(TEXT("held 지속 → press보다 더 좌측(자동반복 발생)"), AfterHold < AfterPress);
	TestEqual(TEXT("동일 입력 → 동일 X(결정적)"), AfterHold, AfterHold2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisSessionStartClearsInputTest,
	"Tetris.Session.StartGameClearsStaleInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisSessionStartClearsInputTest::RunTest(const FString&)
{
	// StartGame 직전의 잔여 입력(held + edge)은 무시되어야 한다(Edge 7).
	UTetrisSessionSubsystem* Sub = NewObject<UTetrisSessionSubsystem>();
	Sub->SetMoveHeld(true, true);
	Sub->PushInputEdge(EInputEdge::MoveLeftPress);

	Sub->StartGame(1); // 버퍼·핸들링 클리어
	const int32 X0 = Sub->GetGameCore()->GetActivePiece().PivotPosition.X;
	Sub->AdvanceFixedSteps(1.0f / 60.0f);
	const int32 X1 = Sub->GetGameCore()->GetActivePiece().PivotPosition.X;

	TestEqual(TEXT("시작 전 잔여 입력 무시 → 첫 스텝 좌이동 없음"), X1, X0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
