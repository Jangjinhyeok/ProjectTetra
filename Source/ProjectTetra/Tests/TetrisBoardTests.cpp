// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Board/TetrisBoard.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace TetrisConstants;

namespace
{
	UTetrisBoard* MakeBoard()
	{
		UTetrisBoard* B = NewObject<UTetrisBoard>();
		B->Clear();
		return B;
	}

	/** 한 행을 GapColumn만 비우고 모두 채움. */
	void FillRow(UTetrisBoard* B, int32 Y, int32 GapColumn = -1)
	{
		TArray<FIntPoint> Coords;
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			if (X != GapColumn) Coords.Add(FIntPoint(X, Y));
		}
		B->LockPiece(Coords, EPieceType::I);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardClearTest,
	"Tetris.Board.Clear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardClearTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	for (int32 Y = 0; Y < BoardTotalHeight; ++Y)
	{
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			TestTrue(TEXT("초기화 후 모든 셀은 Empty"), B->GetCell(X, Y).IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardIsValidPositionTest,
	"Tetris.Board.IsValidPosition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardIsValidPositionTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();

	TestTrue(TEXT("빈 좌표 배열 → true"), B->IsValidPosition({}));
	TestTrue(TEXT("그리드 내 빈 셀"), B->IsValidPosition({ FIntPoint(0,0), FIntPoint(9,23) }));
	TestFalse(TEXT("X<0"), B->IsValidPosition({ FIntPoint(-1,0) }));
	TestFalse(TEXT("X>9"), B->IsValidPosition({ FIntPoint(10,0) }));
	TestFalse(TEXT("Y<0"), B->IsValidPosition({ FIntPoint(0,-1) }));
	TestFalse(TEXT("Y>=24"), B->IsValidPosition({ FIntPoint(0,24) }));

	B->LockPiece({ FIntPoint(5,5) }, EPieceType::T);
	TestFalse(TEXT("Filled 셀과 겹침"), B->IsValidPosition({ FIntPoint(5,5) }));
	TestTrue(TEXT("인접 빈 셀"), B->IsValidPosition({ FIntPoint(4,5), FIntPoint(6,5) }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardLockBroadcastsTest,
	"Tetris.Board.LockBroadcasts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardLockBroadcastsTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	int32 BroadcastCount = 0;
	B->OnBoardChanged.AddLambda([&BroadcastCount](const TArray<FIntPoint>&){ ++BroadcastCount; });

	B->LockPiece({ FIntPoint(0,0), FIntPoint(1,0) }, EPieceType::O);
	TestEqual(TEXT("Lock 시 OnBoardChanged 1회"), BroadcastCount, 1);
	TestEqual(TEXT("셀이 O 타입으로 기록"), B->GetCell(0,0).Type, EPieceType::O);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardSingleLineClearTest,
	"Tetris.Board.SingleLineClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardSingleLineClearTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	FillRow(B, 0);
	// Row 1 위에 단일 블록 — 라인 클리어 후 Row 0으로 내려와야 함.
	B->LockPiece({ FIntPoint(3, 1) }, EPieceType::T);

	bool bEventFired = false;
	FLineClearResult Captured;
	B->OnLinesCleared.AddLambda([&](const FLineClearResult& R){ bEventFired = true; Captured = R; });

	const FLineClearResult R = B->ClearLines();
	TestEqual(TEXT("1줄 클리어"), R.LinesCleared, 1);
	TestEqual(TEXT("이벤트 발행"), bEventFired, true);
	TestEqual(TEXT("Row 0 인덱스"), R.ClearedRows[0], 0);
	TestEqual(TEXT("위 블록 낙하"), B->GetCell(3, 0).Type, EPieceType::T);
	TestTrue(TEXT("Row 1은 비어있음"), B->GetCell(3, 1).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardTetrisClearTest,
	"Tetris.Board.TetrisClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardTetrisClearTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	for (int32 Y = 0; Y < 4; ++Y) FillRow(B, Y);
	const FLineClearResult R = B->ClearLines();
	TestEqual(TEXT("4줄 동시 클리어"), R.LinesCleared, 4);
	for (int32 X = 0; X < BoardWidth; ++X)
	{
		TestTrue(TEXT("Row 0 비어있음"), B->GetCell(X, 0).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardEmptyClearTest,
	"Tetris.Board.EmptyClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardEmptyClearTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	bool bFired = false;
	B->OnLinesCleared.AddLambda([&](const FLineClearResult&){ bFired = true; });
	const FLineClearResult R = B->ClearLines();
	TestEqual(TEXT("0줄"), R.LinesCleared, 0);
	TestFalse(TEXT("빈 보드 클리어 시 이벤트 미발행"), bFired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardGarbageTest,
	"Tetris.Board.AddGarbage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardGarbageTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	B->LockPiece({ FIntPoint(2, 0) }, EPieceType::T); // 기존 블록
	B->AddGarbageLines(2, 5);

	// 기존 블록은 2칸 위로 이동
	TestEqual(TEXT("기존 블록 위로 이동"), B->GetCell(2, 2).Type, EPieceType::T);
	TestTrue(TEXT("원래 위치 비어있음"), B->GetCell(2, 0).IsEmpty());

	// 가비지 행 검증
	for (int32 X = 0; X < BoardWidth; ++X)
	{
		const bool bIsGap = (X == 5);
		TestEqual(TEXT("Gap만 Empty"), B->GetCell(X, 0).IsEmpty(), bIsGap);
		TestEqual(TEXT("Gap만 Empty (Row1)"), B->GetCell(X, 1).IsEmpty(), bIsGap);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardTopOutTest,
	"Tetris.Board.TopOut", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardTopOutTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	B->LockPiece({ FIntPoint(4, 20) }, EPieceType::T);
	TestTrue(TEXT("스폰 위치 충돌 → BlockOut"), B->IsBlockOut({ FIntPoint(4, 20), FIntPoint(5, 20) }));
	TestFalse(TEXT("Visible 영역에 한 블록이라도 → LockOut 아님"),
		B->IsLockOut({ FIntPoint(4, 19), FIntPoint(4, 20) }));
	TestTrue(TEXT("모두 버퍼존 → LockOut"),
		B->IsLockOut({ FIntPoint(4, 20), FIntPoint(4, 21) }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardDropDistanceTest,
	"Tetris.Board.DropDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardDropDistanceTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	// 빈 보드, Y=20에 1블록 → 거리 20
	TestEqual(TEXT("빈 보드 드롭"), B->GetDropDistance({ FIntPoint(4, 20) }), 20);

	// Row 5에 차단 블록 배치
	B->LockPiece({ FIntPoint(4, 5) }, EPieceType::T);
	TestEqual(TEXT("차단 블록 위까지"), B->GetDropDistance({ FIntPoint(4, 20) }), 14);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
