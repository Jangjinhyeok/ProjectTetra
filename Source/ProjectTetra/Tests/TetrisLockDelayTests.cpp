// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "System/TetrisLockDelay.h"

#if WITH_DEV_AUTOMATION_TESTS

// 모든 테스트는 Board/Piece 인스턴스 없이 동작한다 — Lock Delay의 순수 격리(의존성 0)를 검증한다.
// (lock-delay.md Acceptance "격리 / 결정성")

//~ 타이머 기본 ----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayMsToStepsTest,
	"Tetris.LockDelay.MsToStepsConversion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayMsToStepsTest::RunTest(const FString&)
{
	FTetrisLockDelay LD; // 기본 500ms @ 60Hz
	TestEqual(TEXT("500ms@60Hz → 30스텝"), LD.GetLockDelaySteps(), 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayExpiryTest,
	"Tetris.LockDelay.ExpiresAfterDelaySteps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayExpiryTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();

	// 만료 전 29스텝은 false
	for (int32 i = 0; i < 29; ++i)
	{
		TestFalse(TEXT("만료 전 Tick은 false"), LD.Tick());
	}
	// 30번째 스텝에서 만료
	TestTrue(TEXT("30스텝째 만료(true)"), LD.Tick());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayZeroImmediateTest,
	"Tetris.LockDelay.ZeroDelayImmediateExpiry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayZeroImmediateTest::RunTest(const FString&)
{
	FLockDelayConfig Config;
	Config.LockDelayMs = 0; // 즉시 Lock 모드
	FTetrisLockDelay LD(Config);
	TestEqual(TEXT("0ms → 0스텝"), LD.GetLockDelaySteps(), 0);

	LD.OnNewPiece(5);
	LD.OnGrounded();
	TestTrue(TEXT("접지 직후 첫 Tick에서 즉시 만료"), LD.Tick());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayInactiveTickTest,
	"Tetris.LockDelay.InactiveTickNeverExpires", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayInactiveTickTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10); // 아직 OnGrounded 호출 안 함 → bActive=false
	TestFalse(TEXT("비활성 상태에서 Tick은 만료 안 됨"), LD.Tick());
	TestFalse(TEXT("비활성 유지"), LD.IsActive());
	return true;
}

//~ 리셋 (Extended Placement) --------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayMoveResetTest,
	"Tetris.LockDelay.MoveResetClearsTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayMoveResetTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();
	for (int32 i = 0; i < 10; ++i) { LD.Tick(); }
	TestEqual(TEXT("리셋 전 경과 10"), LD.GetElapsedSteps(), 10);

	LD.NotifyMoveOrRotate(true, 10); // 같은 행(최저행 불변) 이동
	TestEqual(TEXT("이동 후 경과 0으로 리셋"), LD.GetElapsedSteps(), 0);
	TestEqual(TEXT("MoveResetCounter +1"), LD.GetMoveResetCounter(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayLimitReachedTest,
	"Tetris.LockDelay.MoveResetLimitReached", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayLimitReachedTest::RunTest(const FString&)
{
	FLockDelayConfig Config;
	Config.MoveResetLimit = 3;
	FTetrisLockDelay LD(Config);
	LD.OnNewPiece(10);
	LD.OnGrounded();

	// 한도(3)까지 리셋 소진
	for (int32 i = 0; i < 3; ++i) { LD.NotifyMoveOrRotate(true, 10); }
	TestEqual(TEXT("카운터 한도 도달"), LD.GetMoveResetCounter(), 3);

	// 타이머 진행 후 한도 초과 이동 → 리셋 안 됨
	for (int32 i = 0; i < 5; ++i) { LD.Tick(); }
	LD.NotifyMoveOrRotate(true, 10);
	TestEqual(TEXT("한도 초과 이동은 타이머 리셋 안 함"), LD.GetElapsedSteps(), 5);
	TestEqual(TEXT("카운터 한도 유지"), LD.GetMoveResetCounter(), 3);

	// 한도 도달 후에도 만료까지 Tick하면 Lock 신호 발생 (기본 30스텝, 이미 5 경과 → 25 더)
	bool bExpired = false;
	for (int32 i = 0; i < 25; ++i) { bExpired = LD.Tick(); }
	TestTrue(TEXT("한도 도달 후에도 만료 발생"), bExpired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayLimitZeroTest,
	"Tetris.LockDelay.MoveResetLimitZero", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayLimitZeroTest::RunTest(const FString&)
{
	FLockDelayConfig Config;
	Config.MoveResetLimit = 0; // 리셋 불가
	FTetrisLockDelay LD(Config);
	LD.OnNewPiece(10);
	LD.OnGrounded();
	for (int32 i = 0; i < 5; ++i) { LD.Tick(); }

	LD.NotifyMoveOrRotate(true, 10);
	TestEqual(TEXT("Limit=0이면 첫 이동부터 리셋 없음"), LD.GetElapsedSteps(), 5);
	TestEqual(TEXT("카운터 증가 없음"), LD.GetMoveResetCounter(), 0);
	return true;
}

//~ 최저행 리프레시 ------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayLowestRefreshTest,
	"Tetris.LockDelay.LowestRowRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayLowestRefreshTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();

	// 같은 행 이동으로 카운터 5까지 소진
	for (int32 i = 0; i < 5; ++i) { LD.NotifyMoveOrRotate(true, 10); }
	TestEqual(TEXT("카운터 5"), LD.GetMoveResetCounter(), 5);

	// 더 아래로(최저행 갱신) → 카운터 0 리프레시
	LD.NotifyMoveOrRotate(true, 9);
	TestEqual(TEXT("최저행 갱신 시 카운터 0 리프레시"), LD.GetMoveResetCounter(), 0);
	TestEqual(TEXT("타이머도 리셋"), LD.GetElapsedSteps(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayRefreshOverridesLimitTest,
	"Tetris.LockDelay.LowestRowRefreshOverridesLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayRefreshOverridesLimitTest::RunTest(const FString&)
{
	FLockDelayConfig Config;
	Config.MoveResetLimit = 3;
	FTetrisLockDelay LD(Config);
	LD.OnNewPiece(10);
	LD.OnGrounded();

	// 한도(3)까지 소진
	for (int32 i = 0; i < 3; ++i) { LD.NotifyMoveOrRotate(true, 10); }
	TestEqual(TEXT("한도 도달"), LD.GetMoveResetCounter(), 3);

	// 한도 초과 상태에서 최저행 갱신 → 0으로 회복 (우선순위 검증)
	LD.NotifyMoveOrRotate(true, 9);
	TestEqual(TEXT("최저행 리프레시는 한도보다 우선 → 카운터 0"), LD.GetMoveResetCounter(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelaySameRowNoRefreshTest,
	"Tetris.LockDelay.SameRowNoRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelaySameRowNoRefreshTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();

	// 카운터 5까지 소진
	for (int32 i = 0; i < 5; ++i) { LD.NotifyMoveOrRotate(true, 10); }
	// 같은 행 이동 → 리프레시 아님(0으로 안 떨어지고 일반 증가)
	LD.NotifyMoveOrRotate(true, 10);
	TestEqual(TEXT("같은 행 이동은 리프레시 안 됨(일반 증가)"), LD.GetMoveResetCounter(), 6);
	return true;
}

//~ Ungrounded / 피스 교체 -----------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayUngroundedTest,
	"Tetris.LockDelay.UngroundedStopsTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayUngroundedTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();
	for (int32 i = 0; i < 5; ++i) { LD.Tick(); }

	LD.NotifyMoveOrRotate(false, 11); // 다시 떠오름
	TestFalse(TEXT("ungrounded → 비활성"), LD.IsActive());

	// 비활성이므로 아무리 Tick해도 만료 안 됨
	bool bExpired = false;
	for (int32 i = 0; i < 60; ++i) { bExpired = bExpired || LD.Tick(); }
	TestFalse(TEXT("ungrounded 후 Tick은 만료 안 시킴"), bExpired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayCounterRetainedTest,
	"Tetris.LockDelay.CounterRetainedAcrossRegrounding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayCounterRetainedTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();
	for (int32 i = 0; i < 3; ++i) { LD.NotifyMoveOrRotate(true, 10); }
	TestEqual(TEXT("카운터 3"), LD.GetMoveResetCounter(), 3);

	// 같은 행에서 떠올랐다가(최저행 불변) 다시 접지 → 카운터 유지(진동 우회 차단)
	LD.NotifyMoveOrRotate(false, 11);
	LD.OnGrounded();
	TestEqual(TEXT("재접지 후 카운터 유지"), LD.GetMoveResetCounter(), 3);
	TestTrue(TEXT("재접지 → 다시 활성"), LD.IsActive());
	TestEqual(TEXT("재접지 시 타이머는 0"), LD.GetElapsedSteps(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayNewPieceResetTest,
	"Tetris.LockDelay.OnNewPieceResetsAll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayNewPieceResetTest::RunTest(const FString&)
{
	FTetrisLockDelay LD;
	LD.OnNewPiece(10);
	LD.OnGrounded();
	for (int32 i = 0; i < 5; ++i) { LD.Tick(); LD.NotifyMoveOrRotate(true, 10); }

	// 새 피스 → 전부 초기화
	LD.OnNewPiece(20);
	TestEqual(TEXT("새 피스 → 경과 0"), LD.GetElapsedSteps(), 0);
	TestEqual(TEXT("새 피스 → 카운터 0"), LD.GetMoveResetCounter(), 0);
	TestFalse(TEXT("새 피스 → 비활성"), LD.IsActive());
	return true;
}

//~ 결정성 ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisLockDelayDeterminismTest,
	"Tetris.LockDelay.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisLockDelayDeterminismTest::RunTest(const FString&)
{
	auto RunSequence = [](FTetrisLockDelay& LD) -> int32
	{
		LD.OnNewPiece(15);
		LD.OnGrounded();
		int32 ExpiredAt = INDEX_NONE;
		for (int32 i = 0; i < 100; ++i)
		{
			if (i == 10) { LD.NotifyMoveOrRotate(true, 15); }   // 같은 행 리셋
			if (i == 20) { LD.NotifyMoveOrRotate(true, 14); }   // 하강 리프레시
			if (LD.Tick() && ExpiredAt == INDEX_NONE) { ExpiredAt = i; }
		}
		return ExpiredAt;
	};

	FTetrisLockDelay A;
	FTetrisLockDelay B;
	const int32 ExpiredA = RunSequence(A);
	const int32 ExpiredB = RunSequence(B);

	TestEqual(TEXT("동일 호출 시퀀스 → 동일 만료 시점"), ExpiredA, ExpiredB);
	TestEqual(TEXT("동일 최종 카운터"), A.GetMoveResetCounter(), B.GetMoveResetCounter());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
