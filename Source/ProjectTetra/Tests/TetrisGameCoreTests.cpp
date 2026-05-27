// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "FSM/TetrisGameCore.h"
#include "Board/TetrisBoard.h"
#include "Block/TetrisPiece.h"
#include "System/TetrisRandomizer.h"
#include "System/TetrisScoring.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FCoreFixture
	{
		UTetrisGameCore* Core = nullptr;
		UTetrisBoard* Board = nullptr;
		UTetrisRandomizer* Rand = nullptr;
		UTetrisScoring* Scoring = nullptr;
	};

	FCoreFixture MakeCore()
	{
		FCoreFixture F;
		F.Board = NewObject<UTetrisBoard>();
		F.Rand = NewObject<UTetrisRandomizer>();
		F.Scoring = NewObject<UTetrisScoring>();
		F.Core = NewObject<UTetrisGameCore>();
		F.Core->Initialize(F.Board, F.Rand, F.Scoring);
		return F;
	}

	int32 CountFilled(UTetrisBoard* Board)
	{
		int32 N = 0;
		for (const FCellState& C : Board->GetVisibleGrid())
		{
			if (C.IsFilled()) { ++N; }
		}
		return N;
	}

	int32 LowestBlockY(const FActivePiece& Piece)
	{
		int32 MinY = MAX_int32;
		for (const FIntPoint& P : FTetrisPieceOps::GetAbsoluteBlockPositions(Piece))
		{
			MinY = FMath::Min(MinY, P.Y);
		}
		return MinY;
	}
}

//~ 상태 머신 / 전이 -----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreStartEntersFallingTest,
	"Tetris.GameCore.StartGameEntersFalling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreStartEntersFallingTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	bool bPieceUpdated = false;
	F.Core->OnActivePieceUpdated.AddLambda([&](const FActivePiece&) { bPieceUpdated = true; });

	F.Core->StartGame(12345);
	TestTrue(TEXT("StartGame → Falling"), F.Core->GetState() == EGameState::Falling);
	TestTrue(TEXT("첫 피스 활성화"), F.Core->GetActivePiece().Type != EPieceType::None);
	TestTrue(TEXT("OnActivePieceUpdated 발행"), bPieceUpdated);
	TestTrue(TEXT("Hold 사용 가능"), F.Core->IsHoldAvailable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreStateChangedOnlyOnTransitionTest,
	"Tetris.GameCore.StateChangedOnlyOnTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreStateChangedOnlyOnTransitionTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	int32 Changes = 0;
	F.Core->OnStateChanged.AddLambda([&](EGameState, EGameState) { ++Changes; });

	F.Core->StartGame(1); // Idle→Spawn, Spawn→Falling = 2회
	TestEqual(TEXT("StartGame → 2회 전이"), Changes, 2);

	for (int32 i = 0; i < 5; ++i) { F.Core->Step(); } // 상단 낙하 중 — 전이 없음
	TestEqual(TEXT("Falling 스텝은 전이 미발생"), Changes, 2);
	return true;
}

//~ 고정 스텝 / 중력 -----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreGravityOneCellTest,
	"Tetris.GameCore.GravityOneCellPer60Steps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreGravityOneCellTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->StartGame(1);
	const int32 SpawnY = F.Core->GetActivePiece().PivotPosition.Y;

	for (int32 i = 0; i < 59; ++i) { F.Core->Step(); }
	TestEqual(TEXT("59스텝 후 미하강 (누적기 floor 이월)"), F.Core->GetActivePiece().PivotPosition.Y, SpawnY);

	F.Core->Step(); // 60번째
	TestEqual(TEXT("60스텝 후 정확히 1칸 하강 (Lv1 BaseG≈0.0167)"), F.Core->GetActivePiece().PivotPosition.Y, SpawnY - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreSoftDropFasterTest,
	"Tetris.GameCore.SoftDropIncreasesGravity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreSoftDropFasterTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore(); // SDF 기본 20 → g=20/60≈0.333/step
	F.Core->StartGame(1);
	const int32 SpawnY = F.Core->GetActivePiece().PivotPosition.Y;

	F.Core->EnqueueCommand(EGameCommand::SoftDropOn);
	F.Core->Step(); F.Core->Step(); F.Core->Step(); // 3스텝 ≈ 1.0 → 1칸
	TestEqual(TEXT("SoftDrop → 3스텝에 1칸 하강 (일반은 60스텝)"), F.Core->GetActivePiece().PivotPosition.Y, SpawnY - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreSoftDropInfiniteNoLockTest,
	"Tetris.GameCore.SoftDropInfiniteToBottomNoLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreSoftDropInfiniteNoLockTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->SoftDropFactor = 100000; // 사실상 ∞
	F.Core->StartGame(1);

	F.Core->EnqueueCommand(EGameCommand::SoftDropOn);
	F.Core->Step(); // 한 스텝에 바닥까지
	TestTrue(TEXT("바닥 도달 → Locking"), F.Core->GetState() == EGameState::Locking);
	TestEqual(TEXT("최저 블록 Y = 0 (바닥)"), LowestBlockY(F.Core->GetActivePiece()), 0);
	TestEqual(TEXT("아직 Lock 안 됨 (Lock Delay 적용)"), CountFilled(F.Board), 0);
	return true;
}

//~ HardDrop / Lock ------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreHardDropLocksTest,
	"Tetris.GameCore.HardDropLocksImmediately", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreHardDropLocksTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	int32 LockCount = 0;
	F.Core->OnPieceLocked.AddLambda([&](int32, const TArray<int32>&, bool, int32, int32, int32) { ++LockCount; });

	F.Core->StartGame(1);
	F.Core->EnqueueCommand(EGameCommand::HardDrop);
	F.Core->Step();

	TestEqual(TEXT("HardDrop → 1회 lock"), LockCount, 1);
	TestEqual(TEXT("4셀 보드에 고정"), CountFilled(F.Board), 4);
	TestTrue(TEXT("즉시 다음 피스 Falling"), F.Core->GetState() == EGameState::Falling);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreHardDropBuffersCommandsTest,
	"Tetris.GameCore.HardDropRemainingCommandsBuffered", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreHardDropBuffersCommandsTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->StartGame(1);
	F.Core->EnqueueCommand(EGameCommand::HardDrop);
	F.Core->EnqueueCommand(EGameCommand::MoveLeft);
	F.Core->Step();
	TestEqual(TEXT("HardDrop로 drain break → 잔여 1개 버퍼"), F.Core->GetPendingCommandCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreLockDelayExpiryTest,
	"Tetris.GameCore.LockDelayExpiryLocks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreLockDelayExpiryTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->SoftDropFactor = 100000;
	int32 LockCount = 0;
	F.Core->OnPieceLocked.AddLambda([&](int32, const TArray<int32>&, bool, int32, int32, int32) { ++LockCount; });

	F.Core->StartGame(1);
	F.Core->EnqueueCommand(EGameCommand::SoftDropOn);
	F.Core->Step(); // 바닥 접지 → Locking
	TestTrue(TEXT("Locking 진입"), F.Core->GetState() == EGameState::Locking);
	TestEqual(TEXT("아직 미락"), LockCount, 0);

	for (int32 i = 0; i < 30; ++i) { F.Core->Step(); } // 기본 Lock Delay 30스텝
	TestEqual(TEXT("Lock Delay 만료 → lock"), LockCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreMoveResetsLockTimerTest,
	"Tetris.GameCore.MoveResetsLockTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreMoveResetsLockTimerTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->SoftDropFactor = 100000;
	int32 LockCount = 0;
	F.Core->OnPieceLocked.AddLambda([&](int32, const TArray<int32>&, bool, int32, int32, int32) { ++LockCount; });

	F.Core->StartGame(1);
	F.Core->EnqueueCommand(EGameCommand::SoftDropOn);
	F.Core->Step(); // Locking

	for (int32 i = 0; i < 29; ++i) { F.Core->Step(); } // Lock Delay 만료 직전 (elapsed 29)
	TestEqual(TEXT("만료 직전 미락"), LockCount, 0);

	// 이동으로 타이머 리셋 → 이 스텝에서 lock되지 않아야 함 (리셋 없으면 elapsed 30 → lock)
	F.Core->EnqueueCommand(EGameCommand::MoveLeft);
	F.Core->Step();
	TestEqual(TEXT("이동이 Lock Delay 리셋 → lock 지연"), LockCount, 0);
	TestTrue(TEXT("여전히 Locking (평지 접지 유지)"), F.Core->GetState() == EGameState::Locking);
	return true;
}

//~ Hold -----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreHoldEmptyTest,
	"Tetris.GameCore.HoldEmptySlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreHoldEmptyTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	bool bHoldFired = false;
	F.Core->OnHoldChanged.AddLambda([&](EPieceType, bool) { bHoldFired = true; });

	F.Core->StartGame(1);
	const EPieceType P0 = F.Core->GetActivePiece().Type;
	F.Core->EnqueueCommand(EGameCommand::Hold);
	F.Core->Step();

	TestTrue(TEXT("HoldSlot = 직전 피스"), F.Core->GetHoldSlot() == P0);
	TestTrue(TEXT("새 피스 스폰 (다음 큐)"), F.Core->GetActivePiece().Type != P0);
	TestTrue(TEXT("OnHoldChanged 발행"), bHoldFired);
	TestTrue(TEXT("Hold 재사용 불가"), !F.Core->IsHoldAvailable());
	TestTrue(TEXT("Falling 유지"), F.Core->GetState() == EGameState::Falling);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreHoldSwapTest,
	"Tetris.GameCore.HoldSwap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreHoldSwapTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->StartGame(1);
	const EPieceType P0 = F.Core->GetActivePiece().Type;

	F.Core->EnqueueCommand(EGameCommand::Hold);
	F.Core->Step(); // slot=P0, active=P1

	F.Core->EnqueueCommand(EGameCommand::HardDrop);
	F.Core->Step(); // P1 lock, P2 spawn, hold 재사용 가능
	const EPieceType P2 = F.Core->GetActivePiece().Type;

	F.Core->EnqueueCommand(EGameCommand::Hold);
	F.Core->Step(); // 교환: active=P0, slot=P2

	TestTrue(TEXT("교환 후 HoldSlot = 직전 피스(P2)"), F.Core->GetHoldSlot() == P2);
	TestTrue(TEXT("교환 후 활성 = 이전 Hold(P0)"), F.Core->GetActivePiece().Type == P0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreHoldDoubleIgnoredTest,
	"Tetris.GameCore.HoldDoubleIgnored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreHoldDoubleIgnoredTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->StartGame(1);
	const EPieceType P0 = F.Core->GetActivePiece().Type;

	F.Core->EnqueueCommand(EGameCommand::Hold);
	F.Core->Step();
	const EPieceType P1 = F.Core->GetActivePiece().Type;

	F.Core->EnqueueCommand(EGameCommand::Hold);
	F.Core->Step(); // 두 번째 Hold 무시

	TestTrue(TEXT("HoldSlot 불변(P0)"), F.Core->GetHoldSlot() == P0);
	TestTrue(TEXT("활성 피스 불변(P1)"), F.Core->GetActivePiece().Type == P1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreHoldDisabledTest,
	"Tetris.GameCore.HoldDisabled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreHoldDisabledTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->bHoldEnabled = false;
	F.Core->StartGame(1);
	const EPieceType P0 = F.Core->GetActivePiece().Type;

	F.Core->EnqueueCommand(EGameCommand::Hold);
	F.Core->Step();

	TestTrue(TEXT("Hold 비활성 → 슬롯 비어 있음"), F.Core->GetHoldSlot() == EPieceType::None);
	TestTrue(TEXT("활성 피스 불변"), F.Core->GetActivePiece().Type == P0);
	return true;
}

//~ Top-out --------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreBlockOutTest,
	"Tetris.GameCore.BlockOutGameOver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreBlockOutTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->SoftDropFactor = 100000;
	ETopOutType GotType = ETopOutType::None;
	int32 GameOverCount = 0;
	F.Core->OnGameOver.AddLambda([&](ETopOutType T) { GotType = T; ++GameOverCount; });

	F.Core->StartGame(1);
	// 현재 피스를 바닥으로 내려 스폰 영역을 비운다.
	F.Core->EnqueueCommand(EGameCommand::SoftDropOn);
	F.Core->Step();

	// 스폰 영역(중앙 cols 2~6, Y=20~21)을 채워 다음 스폰이 충돌하게 만든다.
	TArray<FIntPoint> Fill;
	for (int32 X = 2; X <= 6; ++X) { Fill.Add(FIntPoint(X, 20)); Fill.Add(FIntPoint(X, 21)); }
	F.Board->LockPiece(Fill, EPieceType::Garbage);

	// 현재 피스를 lock(바닥, 0줄) → 다음 스폰이 채워진 영역과 충돌 → BlockOut.
	F.Core->EnqueueCommand(EGameCommand::HardDrop);
	F.Core->Step();

	TestTrue(TEXT("스폰 충돌 → GameOver(BlockOut)"), GotType == ETopOutType::BlockOut);
	TestTrue(TEXT("GameOver 상태"), F.Core->GetState() == EGameState::GameOver);
	TestEqual(TEXT("GameOver 1회"), GameOverCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreLockOutTest,
	"Tetris.GameCore.LockOutGameOver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreLockOutTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	ETopOutType GotType = ETopOutType::None;
	F.Core->OnGameOver.AddLambda([&](ETopOutType T) { GotType = T; });

	F.Core->StartGame(1);
	// cols 0~8을 Y=0~19 전부 채움(col 9는 비워 라인 클리어 방지) → 피스가 버퍼존에서 lock.
	TArray<FIntPoint> Fill;
	for (int32 Y = 0; Y < TetrisConstants::BoardVisibleHeight; ++Y)
	{
		for (int32 X = 0; X < TetrisConstants::BoardWidth - 1; ++X)
		{
			Fill.Add(FIntPoint(X, Y));
		}
	}
	F.Board->LockPiece(Fill, EPieceType::Garbage);

	F.Core->EnqueueCommand(EGameCommand::HardDrop);
	F.Core->Step();

	TestTrue(TEXT("버퍼존 lock → GameOver(LockOut)"), GotType == ETopOutType::LockOut);
	TestTrue(TEXT("GameOver 상태"), F.Core->GetState() == EGameState::GameOver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreGameOverIgnoresInputTest,
	"Tetris.GameCore.GameOverIgnoresInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreGameOverIgnoresInputTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	int32 GameOverCount = 0;
	F.Core->OnGameOver.AddLambda([&](ETopOutType) { ++GameOverCount; });

	F.Core->StartGame(1);
	TArray<FIntPoint> Fill;
	for (int32 Y = 0; Y < TetrisConstants::BoardVisibleHeight; ++Y)
	{
		for (int32 X = 0; X < TetrisConstants::BoardWidth - 1; ++X)
		{
			Fill.Add(FIntPoint(X, Y));
		}
	}
	F.Board->LockPiece(Fill, EPieceType::Garbage);
	F.Core->EnqueueCommand(EGameCommand::HardDrop);
	F.Core->Step(); // GameOver(LockOut)
	TestEqual(TEXT("GameOver 1회"), GameOverCount, 1);

	for (int32 i = 0; i < 10; ++i)
	{
		F.Core->EnqueueCommand(EGameCommand::MoveLeft);
		F.Core->Step();
	}
	TestEqual(TEXT("GameOver 후 이벤트 중복 없음"), GameOverCount, 1);
	TestTrue(TEXT("GameOver 상태 유지"), F.Core->GetState() == EGameState::GameOver);
	TestEqual(TEXT("명령 큐 비워짐 (no-op)"), F.Core->GetPendingCommandCount(), 0);
	return true;
}

//~ 결정성 (핵심) --------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreDeterminismTest,
	"Tetris.GameCore.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreDeterminismTest::RunTest(const FString&)
{
	auto Script = [](FCoreFixture& F)
	{
		F.Core->StartGame(777);
		for (int32 S = 0; S < 300; ++S)
		{
			if (S % 7 == 0)  { F.Core->EnqueueCommand(EGameCommand::MoveLeft); }
			if (S % 5 == 0)  { F.Core->EnqueueCommand(EGameCommand::RotateCW); }
			if (S % 13 == 0) { F.Core->EnqueueCommand(EGameCommand::HardDrop); }
			if (S % 11 == 0) { F.Core->EnqueueCommand(EGameCommand::Hold); }
			F.Core->Step();
		}
	};

	FCoreFixture A = MakeCore();
	FCoreFixture B = MakeCore();
	Script(A);
	Script(B);

	const TArray<FCellState> GA = A.Board->GetVisibleGrid();
	const TArray<FCellState> GB = B.Board->GetVisibleGrid();
	bool bBoardSame = (GA.Num() == GB.Num());
	for (int32 i = 0; bBoardSame && i < GA.Num(); ++i)
	{
		bBoardSame = (GA[i].Type == GB[i].Type);
	}
	TestTrue(TEXT("동일 시드+명령 → 동일 보드"), bBoardSame);
	TestTrue(TEXT("동일 활성 피스 타입"), A.Core->GetActivePiece().Type == B.Core->GetActivePiece().Type);
	TestTrue(TEXT("동일 피봇"), A.Core->GetActivePiece().PivotPosition == B.Core->GetActivePiece().PivotPosition);
	TestTrue(TEXT("동일 회전"), A.Core->GetActivePiece().RotationState == B.Core->GetActivePiece().RotationState);
	TestTrue(TEXT("동일 상태"), A.Core->GetState() == B.Core->GetState());
	TestTrue(TEXT("동일 Hold"), A.Core->GetHoldSlot() == B.Core->GetHoldSlot());
	return true;
}

//~ G5: Score ↔ FSM 통합 -------------------------------------------------------

namespace
{
	// 완성 라인을 사전 구성: rows [0, RowCount) 를 전부 채운다(즉시 클리어되지 않음 — 다음 lock의 ClearLines에서 처리).
	void FillFullRows(UTetrisBoard* Board, int32 RowCount)
	{
		TArray<FIntPoint> Fill;
		for (int32 Y = 0; Y < RowCount; ++Y)
		{
			for (int32 X = 0; X < TetrisConstants::BoardWidth; ++X)
			{
				Fill.Add(FIntPoint(X, Y));
			}
		}
		Board->LockPiece(Fill, EPieceType::Garbage);
	}

	// 입력 드롭 없이(자연 낙하만) 다음 lock이 일어날 때까지 Step. → SoftDrop/HardDrop 점수 잡음 0.
	int32 StepUntilLock(UTetrisGameCore* Core, int32& OutLockCount, int32 MaxSteps = 3000)
	{
		int32 Steps = 0;
		while (OutLockCount == 0 && Steps < MaxSteps)
		{
			Core->Step();
			++Steps;
		}
		return Steps;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreIntegrationSingleScoreTest,
	"Tetris.GameCore.Integration.SingleLineClearScores", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreIntegrationSingleScoreTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	int32 LockCount = 0;
	F.Core->OnPieceLocked.AddLambda([&](int32, const TArray<int32>&, bool, int32, int32, int32) { ++LockCount; });

	F.Core->StartGame(1);
	FillFullRows(F.Board, 1); // row 0 완성 라인 1개

	StepUntilLock(F.Core, LockCount); // 자연 낙하 → 접지 → Lock Delay 만료 → lock → ClearLines

	TestEqual(TEXT("1회 lock"), LockCount, 1);
	TestEqual(TEXT("누적 삭제 줄 1"), F.Scoring->GetTotalLinesCleared(), 1);
	TestEqual(TEXT("Single 정확히 +100 (Lv1, 콤보 0, 드롭 점수 0)"), F.Scoring->GetScore(), (int64)100);
	TestEqual(TEXT("첫 클리어 Combo 0"), F.Scoring->GetCombo(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreIntegrationTetrisScoreTest,
	"Tetris.GameCore.Integration.TetrisClearScores", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreIntegrationTetrisScoreTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	int32 LockCount = 0;
	F.Core->OnPieceLocked.AddLambda([&](int32, const TArray<int32>&, bool, int32, int32, int32) { ++LockCount; });

	F.Core->StartGame(1);
	FillFullRows(F.Board, 4); // rows 0~3 완성 → Tetris

	StepUntilLock(F.Core, LockCount);

	TestEqual(TEXT("1회 lock"), LockCount, 1);
	TestEqual(TEXT("누적 삭제 줄 4"), F.Scoring->GetTotalLinesCleared(), 4);
	TestEqual(TEXT("Tetris 정확히 +800 (Lv1, 첫 difficult, 드롭 0)"), F.Scoring->GetScore(), (int64)800);
	TestEqual(TEXT("첫 difficult → B2BCount 0"), F.Scoring->GetB2BCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreIntegrationLevelUpGravityTest,
	"Tetris.GameCore.Integration.LevelUpIncreasesGravity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreIntegrationLevelUpGravityTest::RunTest(const FString&)
{
	FCoreFixture F = MakeCore();
	F.Core->StartGame(1);

	// 3회 Tetris(=12줄) → Level 2 (1 + 12/10). HardDrop으로 빠르게 처리(점수는 검증 안 함).
	for (int32 T = 0; T < 3; ++T)
	{
		FillFullRows(F.Board, 4);
		F.Core->EnqueueCommand(EGameCommand::HardDrop);
		F.Core->Step(); // lock → 4줄 클리어
	}
	TestEqual(TEXT("12줄 → Level 2"), F.Scoring->GetLevel(), 2);
	TestTrue(TEXT("BaseG(2) > BaseG(1) (커브 증가)"), F.Scoring->GetBaseG(2) > F.Scoring->GetBaseG(1));

	// Level 2 중력 반영: 50스텝에 1칸 하강 (Level 1이면 60스텝 필요 → 50스텝엔 미하강).
	const int32 Y0 = F.Core->GetActivePiece().PivotPosition.Y;
	for (int32 i = 0; i < 50; ++i) { F.Core->Step(); }
	TestEqual(TEXT("Level 2 중력: 50스텝에 1칸"), F.Core->GetActivePiece().PivotPosition.Y, Y0 - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisCoreIntegrationScoringDeterminismTest,
	"Tetris.GameCore.Integration.ScoringDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisCoreIntegrationScoringDeterminismTest::RunTest(const FString&)
{
	auto Script = [](FCoreFixture& F)
	{
		F.Core->StartGame(2024);
		for (int32 S = 0; S < 400; ++S)
		{
			if (S % 6 == 0)  { F.Core->EnqueueCommand(EGameCommand::MoveRight); }
			if (S % 4 == 0)  { F.Core->EnqueueCommand(EGameCommand::RotateCCW); }
			if (S % 9 == 0)  { F.Core->EnqueueCommand(EGameCommand::HardDrop); }
			F.Core->Step();
		}
	};

	FCoreFixture A = MakeCore();
	FCoreFixture B = MakeCore();
	Script(A);
	Script(B);

	TestEqual(TEXT("동일 시드+명령 → 동일 점수"), A.Scoring->GetScore(), B.Scoring->GetScore());
	TestEqual(TEXT("동일 레벨"), A.Scoring->GetLevel(), B.Scoring->GetLevel());
	TestEqual(TEXT("동일 삭제 줄"), A.Scoring->GetTotalLinesCleared(), B.Scoring->GetTotalLinesCleared());
	TestEqual(TEXT("동일 콤보"), A.Scoring->GetCombo(), B.Scoring->GetCombo());
	TestEqual(TEXT("동일 B2B"), A.Scoring->GetB2BCount(), B.Scoring->GetB2BCount());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
