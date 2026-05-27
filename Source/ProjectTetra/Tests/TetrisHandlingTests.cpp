// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Input/TetrisHandling.h"
#include "Input/TetrisHandlingTypes.h"
#include "Core/TetrisTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// 모든 테스트는 PC/Session/Board 인스턴스 없이 FTetrisHandling 하나로 동작한다 —
// input-handling.md Acceptance "격리 / 결정성"(의존성 0)을 검증한다.

namespace
{
	// 한 스텝 입력을 간결히 구성하는 헬퍼.
	FHandlingInput MakeInput(bool bLeftHeld, bool bRightHeld, std::initializer_list<EInputEdge> Edges)
	{
		FHandlingInput In;
		In.bLeftHeld = bLeftHeld;
		In.bRightHeld = bRightHeld;
		In.Edges = Edges;
		return In;
	}

	// 빈 입력(엣지 없음)으로 held 상태만 유지.
	FHandlingInput Held(bool bLeftHeld, bool bRightHeld)
	{
		return MakeInput(bLeftHeld, bRightHeld, {});
	}

	int32 CountOf(const TArray<EGameCommand>& Cmds, EGameCommand Target)
	{
		int32 N = 0;
		for (EGameCommand C : Cmds) { if (C == Target) { ++N; } }
		return N;
	}
}

//~ ms→스텝 환산 (H1) ----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingMsToStepsTest,
	"Tetris.Handling.MsToStepsConversion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingMsToStepsTest::RunTest(const FString&)
{
	FTetrisHandling H; // 기본 DAS=100, ARR=17, DCD=0 @ 60Hz
	TestEqual(TEXT("DAS 100ms@60Hz → 6스텝"), H.GetDASSteps(), 6);
	TestEqual(TEXT("ARR 17ms@60Hz → 1스텝"), H.GetARRSteps(), 1);
	TestEqual(TEXT("DCD 0ms → 0스텝"), H.GetDCDSteps(), 0);

	FHandlingConfig Cfg;
	Cfg.DCDms = 1000; // DASms(100ms=6) 초과 → DASSteps로 클램프
	FTetrisHandling H2(Cfg);
	TestEqual(TEXT("DCD는 DASSteps로 클램프"), H2.GetDCDSteps(), H2.GetDASSteps());
	return true;
}

//~ press 즉시 1칸 + DAS 충전 (H2) ---------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingPressEmitsOneTest,
	"Tetris.Handling.PressEmitsOneCell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingPressEmitsOneTest::RunTest(const FString&)
{
	FTetrisHandling H;
	const TArray<EGameCommand> Out = H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress }));
	TestEqual(TEXT("press → 정확히 1개 명령"), Out.Num(), 1);
	TestEqual(TEXT("그 명령은 MoveLeft"), Out[0], EGameCommand::MoveLeft);
	TestTrue(TEXT("press 후 active"), H.IsActive());
	TestFalse(TEXT("아직 Repeating 아님(Charging)"), H.IsRepeating());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingHoldBelowDasTest,
	"Tetris.Handling.HoldBelowDasNoEmit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingHoldBelowDasTest::RunTest(const FString&)
{
	FTetrisHandling H; // DAS=6
	H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress })); // press

	// DASSteps-1(=5) 충전 스텝 동안 추가 방출 없음.
	for (int32 i = 0; i < 5; ++i)
	{
		const TArray<EGameCommand> Out = H.AdvanceStep(Held(true, false));
		TestEqual(FString::Printf(TEXT("충전 스텝 %d → 방출 없음"), i), Out.Num(), 0);
	}
	TestFalse(TEXT("아직 Charging"), H.IsRepeating());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingRepeatAfterDasTest,
	"Tetris.Handling.RepeatAfterDas", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingRepeatAfterDasTest::RunTest(const FString&)
{
	FTetrisHandling H; // DAS=6, ARR=1
	H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress })); // press(1칸)
	for (int32 i = 0; i < 5; ++i) { H.AdvanceStep(Held(true, false)); }   // 5 충전 스텝

	// 6번째 충전 스텝 = DAS 경과 → 첫 자동반복 방출 + Repeating 진입.
	const TArray<EGameCommand> First = H.AdvanceStep(Held(true, false));
	TestEqual(TEXT("DAS 경과 스텝에서 첫 반복 1칸"), First.Num(), 1);
	TestEqual(TEXT("MoveLeft 반복"), First[0], EGameCommand::MoveLeft);
	TestTrue(TEXT("Repeating 진입"), H.IsRepeating());

	// ARR=1 → 이후 매 스텝 1칸.
	const TArray<EGameCommand> Next = H.AdvanceStep(Held(true, false));
	TestEqual(TEXT("ARR=1 → 다음 스텝도 1칸"), Next.Num(), 1);
	return true;
}

//~ ARR=0 벽 버스트 (H4) -------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingArrZeroWallTest,
	"Tetris.Handling.ArrZeroWallBurst", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingArrZeroWallTest::RunTest(const FString&)
{
	FHandlingConfig Cfg;
	Cfg.ARRms = 0; // 벽까지 즉시
	FTetrisHandling H(Cfg); // DAS=6, ARR=0
	H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress }));
	for (int32 i = 0; i < 5; ++i) { H.AdvanceStep(Held(true, false)); }

	// DAS 경과 스텝 → 벽까지(BoardWidth개) 한꺼번에.
	const TArray<EGameCommand> Burst = H.AdvanceStep(Held(true, false));
	TestEqual(TEXT("ARR=0 → BoardWidth개 방출"), Burst.Num(), TetrisConstants::BoardWidth);
	TestEqual(TEXT("전부 MoveLeft"), CountOf(Burst, EGameCommand::MoveLeft), TetrisConstants::BoardWidth);

	// 이후 매 스텝도 벽 버스트.
	const TArray<EGameCommand> Burst2 = H.AdvanceStep(Held(true, false));
	TestEqual(TEXT("매 스텝 BoardWidth개"), Burst2.Num(), TetrisConstants::BoardWidth);
	return true;
}

//~ 방향 전환 / DCD (H5) -------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingDcdZeroSnapTest,
	"Tetris.Handling.DcdZeroSnapSwitch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingDcdZeroSnapTest::RunTest(const FString&)
{
	FTetrisHandling H; // DCD=0
	H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress })); // 좌 active

	// 우 press(둘 다 held) → 전환: 1칸 MoveRight, DCD=0이라 DAS 풀 유지.
	const TArray<EGameCommand> Switch = H.AdvanceStep(MakeInput(true, true, { EInputEdge::MoveRightPress }));
	TestEqual(TEXT("전환 즉시 1칸"), Switch.Num(), 1);
	TestEqual(TEXT("새 방향 MoveRight"), Switch[0], EGameCommand::MoveRight);

	// DCD=0 → 풀 DAS 유지 → 바로 다음 스텝에 반복 시작(스냅).
	const TArray<EGameCommand> NextStep = H.AdvanceStep(Held(true, true));
	TestEqual(TEXT("DCD=0 → 전환 다음 스텝에 즉시 반복"), NextStep.Num(), 1);
	TestEqual(TEXT("MoveRight 반복"), NextStep[0], EGameCommand::MoveRight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingDcdFullRechargeTest,
	"Tetris.Handling.DcdFullRecharge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingDcdFullRechargeTest::RunTest(const FString&)
{
	FHandlingConfig Cfg;
	Cfg.DCDms = Cfg.DASms; // DCD=DAS → 전환 시 풀 재충전
	FTetrisHandling H(Cfg); // DAS=6, DCD=6
	H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress }));

	// 우 press 전환 → 1칸 + DAS 풀 재충전.
	H.AdvanceStep(MakeInput(true, true, { EInputEdge::MoveRightPress }));

	// 전환 후 5스텝은 충전(방출 없음), 6스텝째 반복 시작.
	for (int32 i = 0; i < 5; ++i)
	{
		const TArray<EGameCommand> Out = H.AdvanceStep(Held(true, true));
		TestEqual(FString::Printf(TEXT("재충전 스텝 %d → 방출 없음"), i), Out.Num(), 0);
	}
	const TArray<EGameCommand> Repeat = H.AdvanceStep(Held(true, true));
	TestEqual(TEXT("DCD=DAS → 6스텝 재충전 후 반복"), Repeat.Num(), 1);
	TestEqual(TEXT("MoveRight"), Repeat[0], EGameCommand::MoveRight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingLastInputPriorityTest,
	"Tetris.Handling.LastInputPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingLastInputPriorityTest::RunTest(const FString&)
{
	// 같은 스텝에 좌+우 press → 마지막 것이 ActiveDir, 1칸만 방출(jitter 없음).
	{
		FTetrisHandling H;
		const TArray<EGameCommand> Out = H.AdvanceStep(
			MakeInput(true, true, { EInputEdge::MoveLeftPress, EInputEdge::MoveRightPress }));
		TestEqual(TEXT("좌→우 press: 1칸"), Out.Num(), 1);
		TestEqual(TEXT("마지막 우 우선 → MoveRight"), Out[0], EGameCommand::MoveRight);
	}
	{
		FTetrisHandling H;
		const TArray<EGameCommand> Out = H.AdvanceStep(
			MakeInput(true, true, { EInputEdge::MoveRightPress, EInputEdge::MoveLeftPress }));
		TestEqual(TEXT("우→좌 press: 1칸"), Out.Num(), 1);
		TestEqual(TEXT("마지막 좌 우선 → MoveLeft"), Out[0], EGameCommand::MoveLeft);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingReleaseFallbackTest,
	"Tetris.Handling.ActiveReleasedOppositeHeldSwitches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingReleaseFallbackTest::RunTest(const FString&)
{
	FTetrisHandling H;
	// 좌+우 둘 다 누른 상태에서 좌가 active(좌를 마지막에 press).
	H.AdvanceStep(MakeInput(true, true, { EInputEdge::MoveRightPress, EInputEdge::MoveLeftPress }));
	TestTrue(TEXT("좌 active"), H.IsActive());

	// 좌 해제, 우는 여전히 held → 우로 전환(press처럼 1칸 + 풀 DAS).
	const TArray<EGameCommand> Out = H.AdvanceStep(MakeInput(false, true, { EInputEdge::MoveLeftRelease }));
	TestEqual(TEXT("폴백 전환 1칸"), Out.Num(), 1);
	TestEqual(TEXT("우로 전환 → MoveRight"), Out[0], EGameCommand::MoveRight);
	TestEqual(TEXT("폴백은 풀 DAS(=0 충전)"), H.GetDASCharge(), 0);
	return true;
}

//~ 이산 입력 순서 / 2-phase ---------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingDiscreteOrderTest,
	"Tetris.Handling.DiscreteOrderPreserved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingDiscreteOrderTest::RunTest(const FString&)
{
	FTetrisHandling H;
	const TArray<EGameCommand> Out = H.AdvanceStep(MakeInput(false, false, {
		EInputEdge::RotateCW, EInputEdge::RotateCCW, EInputEdge::Hold,
		EInputEdge::HardDrop, EInputEdge::SoftDropOn, EInputEdge::SoftDropOff }));

	TestEqual(TEXT("이산 6개 그대로"), Out.Num(), 6);
	TestEqual(TEXT("[0] RotateCW"), Out[0], EGameCommand::RotateCW);
	TestEqual(TEXT("[1] RotateCCW"), Out[1], EGameCommand::RotateCCW);
	TestEqual(TEXT("[2] Hold"), Out[2], EGameCommand::Hold);
	TestEqual(TEXT("[3] HardDrop"), Out[3], EGameCommand::HardDrop);
	TestEqual(TEXT("[4] SoftDropOn"), Out[4], EGameCommand::SoftDropOn);
	TestEqual(TEXT("[5] SoftDropOff"), Out[5], EGameCommand::SoftDropOff);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingDiscreteThenMoveTest,
	"Tetris.Handling.DiscreteEmittedBeforeMove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingDiscreteThenMoveTest::RunTest(const FString&)
{
	// 큐 순서상 move press가 회전 사이에 있어도 §4: 이산 먼저 → 방향 이동 뒤.
	FTetrisHandling H;
	const TArray<EGameCommand> Out = H.AdvanceStep(MakeInput(true, false, {
		EInputEdge::RotateCW, EInputEdge::MoveLeftPress, EInputEdge::Hold }));

	TestEqual(TEXT("이산 2개 + 이동 1개"), Out.Num(), 3);
	TestEqual(TEXT("[0] RotateCW"), Out[0], EGameCommand::RotateCW);
	TestEqual(TEXT("[1] Hold"), Out[1], EGameCommand::Hold);
	TestEqual(TEXT("[2] MoveLeft(이동은 이산 뒤)"), Out[2], EGameCommand::MoveLeft);
	return true;
}

//~ 탭 / 유실 방지 -------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingTapOneCellTest,
	"Tetris.Handling.TapEmitsExactlyOne", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingTapOneCellTest::RunTest(const FString&)
{
	FTetrisHandling H;
	// 한 스텝 사이 press→release: press 엣지로 1칸 보장, held=false라 반복 없음.
	const TArray<EGameCommand> Out = H.AdvanceStep(
		MakeInput(false, false, { EInputEdge::MoveLeftPress, EInputEdge::MoveLeftRelease }));
	TestEqual(TEXT("탭 → 정확히 1칸"), Out.Num(), 1);
	TestEqual(TEXT("MoveLeft"), Out[0], EGameCommand::MoveLeft);
	TestFalse(TEXT("탭 후 비active(반복 없음)"), H.IsActive());

	// 이후 빈 입력은 아무것도 방출하지 않음.
	const TArray<EGameCommand> After = H.AdvanceStep(Held(false, false));
	TestEqual(TEXT("탭 후 추가 방출 없음"), After.Num(), 0);
	return true;
}

//~ 리셋 -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingResetTest,
	"Tetris.Handling.ResetClearsState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingResetTest::RunTest(const FString&)
{
	FTetrisHandling H;
	H.AdvanceStep(MakeInput(true, false, { EInputEdge::MoveLeftPress }));
	for (int32 i = 0; i < 8; ++i) { H.AdvanceStep(Held(true, false)); } // Repeating까지 진행
	TestTrue(TEXT("리셋 전 active"), H.IsActive());

	H.Reset();
	TestFalse(TEXT("Reset 후 비active"), H.IsActive());
	TestFalse(TEXT("Reset 후 비Repeating"), H.IsRepeating());
	TestEqual(TEXT("Reset 후 DASCharge 0"), H.GetDASCharge(), 0);
	return true;
}

//~ 결정성 ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHandlingDeterminismTest,
	"Tetris.Handling.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHandlingDeterminismTest::RunTest(const FString&)
{
	// 혼합 입력 시퀀스(press/hold/전환/이산/탭)를 두 인스턴스에 동일 적용 → 명령 시퀀스 동일.
	auto RunSequence = [](FTetrisHandling& H) -> TArray<EGameCommand>
	{
		TArray<EGameCommand> All;
		const TArray<FHandlingInput> Seq = {
			MakeInput(true, false, { EInputEdge::MoveLeftPress }),
			Held(true, false),
			Held(true, false),
			MakeInput(true, false, { EInputEdge::RotateCW }),
			Held(true, false),
			Held(true, false),
			Held(true, false),
			Held(true, false),               // DAS 경과 → 반복
			MakeInput(true, true, { EInputEdge::MoveRightPress }), // 전환
			Held(true, true),
			Held(true, true),
			MakeInput(false, false, { EInputEdge::MoveRightRelease, EInputEdge::HardDrop }), // 해제 + 하드드롭
			Held(false, false),
		};
		for (const FHandlingInput& In : Seq)
		{
			All.Append(H.AdvanceStep(In));
		}
		return All;
	};

	FTetrisHandling A;
	FTetrisHandling B;
	const TArray<EGameCommand> OutA = RunSequence(A);
	const TArray<EGameCommand> OutB = RunSequence(B);

	TestEqual(TEXT("동일 입력 → 동일 명령 개수"), OutA.Num(), OutB.Num());
	bool bSame = (OutA.Num() == OutB.Num());
	for (int32 i = 0; bSame && i < OutA.Num(); ++i)
	{
		bSame = (OutA[i] == OutB[i]);
	}
	TestTrue(TEXT("동일 입력 → 바이트 동일 명령 시퀀스"), bSame);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
