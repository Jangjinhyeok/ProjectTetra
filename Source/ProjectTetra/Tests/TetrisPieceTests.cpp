// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Block/TetrisPiece.h"
#include "Block/PieceData.h"
#include "Board/TetrisBoard.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace TetrisConstants;
using namespace TetrisPieceData;

namespace
{
	UTetrisBoard* MakeBoard() { UTetrisBoard* B = NewObject<UTetrisBoard>(); B->Clear(); return B; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPieceSpawnTest,
	"Tetris.Piece.Spawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisPieceSpawnTest::RunTest(const FString&)
{
	const EPieceType Types[] = { EPieceType::I, EPieceType::O, EPieceType::T,
		EPieceType::S, EPieceType::Z, EPieceType::J, EPieceType::L };

	for (EPieceType T : Types)
	{
		FActivePiece P = FTetrisPieceOps::Spawn(T);
		TestEqual(TEXT("Type"), P.Type, T);
		TestEqual(TEXT("RotationState=0"), P.RotationState, ERotationState::State0);
		TArray<FIntPoint> Blocks = FTetrisPieceOps::GetAbsoluteBlockPositions(P);
		TestEqual(TEXT("4 blocks"), Blocks.Num(), 4);
		for (const FIntPoint& B : Blocks)
		{
			TestTrue(TEXT("스폰 좌표 X 범위"), B.X >= 0 && B.X < BoardWidth);
			TestTrue(TEXT("스폰 좌표 Y 범위"), B.Y >= 0 && B.Y < BoardTotalHeight);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPieceFullRotationTest,
	"Tetris.Piece.FullRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisPieceFullRotationTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	const EPieceType Types[] = { EPieceType::I, EPieceType::O, EPieceType::T,
		EPieceType::S, EPieceType::Z, EPieceType::J, EPieceType::L };

	for (EPieceType T : Types)
	{
		FActivePiece P = FTetrisPieceOps::Spawn(T);
		// 빈 보드 한가운데에서는 4회 CW 회전이 모두 성공해야 함.
		for (int32 i = 0; i < 4; ++i)
		{
			TestTrue(TEXT("CW 회전 성공"), FTetrisPieceOps::TryRotate(P, ERotateDirection::CW, *B));
		}
		TestEqual(TEXT("4회 CW 후 State0 복귀"), P.RotationState, ERotationState::State0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPieceMoveTest,
	"Tetris.Piece.Move", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisPieceMoveTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	FActivePiece P = FTetrisPieceOps::Spawn(EPieceType::T);
	const FIntPoint Start = P.PivotPosition;

	TestTrue(TEXT("좌 이동"), FTetrisPieceOps::TryMove(P, EMoveDirection::Left, *B));
	TestEqual(TEXT("X-1"), P.PivotPosition.X, Start.X - 1);

	TestTrue(TEXT("우 이동"), FTetrisPieceOps::TryMove(P, EMoveDirection::Right, *B));
	TestEqual(TEXT("X 복귀"), P.PivotPosition.X, Start.X);

	// 좌측 벽에 닿을 때까지 이동
	while (FTetrisPieceOps::TryMove(P, EMoveDirection::Left, *B)) {}
	TestFalse(TEXT("벽에서 좌 이동 실패"), FTetrisPieceOps::TryMove(P, EMoveDirection::Left, *B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPieceHardDropTest,
	"Tetris.Piece.HardDrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisPieceHardDropTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	FActivePiece P = FTetrisPieceOps::Spawn(EPieceType::T);
	const int32 StartY = P.PivotPosition.Y;
	const int32 Distance = FTetrisPieceOps::HardDrop(P, *B);
	TestTrue(TEXT("드롭 거리 > 0"), Distance > 0);
	TestEqual(TEXT("Y 갱신"), P.PivotPosition.Y, StartY - Distance);
	// 드롭 후엔 더 내려갈 수 없어야 함
	TestFalse(TEXT("드롭 후 추가 하강 불가"),
		FTetrisPieceOps::TryMove(P, EMoveDirection::Down, *B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPieceWallKickTest,
	"Tetris.Piece.WallKick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisPieceWallKickTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	// T 피스를 좌측 벽에 붙이고 회전 → Kick으로 성공해야 함
	FActivePiece P = FTetrisPieceOps::Spawn(EPieceType::T);
	while (FTetrisPieceOps::TryMove(P, EMoveDirection::Left, *B)) {}
	const int32 PivotXBeforeRotate = P.PivotPosition.X;
	TestTrue(TEXT("벽에서 CW 회전 성공 (Kick 또는 직접)"),
		FTetrisPieceOps::TryRotate(P, ERotateDirection::CW, *B));
	TestEqual(TEXT("RotationState=R"), P.RotationState, ERotationState::StateR);
	// 회전 결과가 벽 안쪽에 있어야 함
	TArray<FIntPoint> Blocks = FTetrisPieceOps::GetAbsoluteBlockPositions(P);
	for (const FIntPoint& Bk : Blocks)
	{
		TestTrue(TEXT("회전 후 X 범위"), Bk.X >= 0 && Bk.X < BoardWidth);
	}
	(void)PivotXBeforeRotate;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisPieceOPieceRotateTest,
	"Tetris.Piece.OPieceRotate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisPieceOPieceRotateTest::RunTest(const FString&)
{
	UTetrisBoard* B = MakeBoard();
	FActivePiece P = FTetrisPieceOps::Spawn(EPieceType::O);
	TArray<FIntPoint> Before = FTetrisPieceOps::GetAbsoluteBlockPositions(P);
	TestTrue(TEXT("O 회전 성공"), FTetrisPieceOps::TryRotate(P, ERotateDirection::CW, *B));
	TArray<FIntPoint> After = FTetrisPieceOps::GetAbsoluteBlockPositions(P);
	// O는 형태 불변 — 좌표 동일
	for (int32 i = 0; i < 4; ++i)
	{
		TestEqual(TEXT("O 좌표 불변"), Before[i], After[i]);
	}
	TestEqual(TEXT("회전 상태 전이"), P.RotationState, ERotationState::StateR);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
