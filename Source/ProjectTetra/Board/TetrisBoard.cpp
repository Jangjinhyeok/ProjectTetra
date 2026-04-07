// Copyright ProjectTetra. All Rights Reserved.

#include "Board/TetrisBoard.h"

using namespace TetrisConstants;

UTetrisBoard::UTetrisBoard()
{
	Clear();
}

void UTetrisBoard::Clear()
{
	Cells.Reset();
	Cells.SetNum(BoardWidth * BoardTotalHeight); // 기본 생성자 = Empty
}

bool UTetrisBoard::IsValidPosition(const TArray<FIntPoint>& BlockCoords) const
{
	// Why: 빈 배열은 충돌 불가능 → true. (방어적 처리, GDD Edge Case 6)
	for (const FIntPoint& P : BlockCoords)
	{
		if (!InBounds(P.X, P.Y))
		{
			return false;
		}
		if (Cells[IndexOf(P.X, P.Y)].IsFilled())
		{
			return false;
		}
	}
	return true;
}

int32 UTetrisBoard::GetDropDistance(const TArray<FIntPoint>& BlockCoords) const
{
	if (BlockCoords.Num() == 0)
	{
		return 0;
	}

	// Why: 한 칸씩 내려보며 충돌까지의 최대 d 탐색.
	// 보드 높이 24이므로 최악 24회 — 성능 부담 없음.
	int32 Distance = 0;
	TArray<FIntPoint> Shifted;
	Shifted.SetNumUninitialized(BlockCoords.Num());

	while (Distance < BoardTotalHeight)
	{
		const int32 Next = Distance + 1;
		for (int32 i = 0; i < BlockCoords.Num(); ++i)
		{
			Shifted[i] = FIntPoint(BlockCoords[i].X, BlockCoords[i].Y - Next);
		}
		if (!IsValidPosition(Shifted))
		{
			break;
		}
		Distance = Next;
	}
	return Distance;
}

FCellState UTetrisBoard::GetCell(int32 X, int32 Y) const
{
	if (!InBounds(X, Y))
	{
		return FCellState();
	}
	return Cells[IndexOf(X, Y)];
}

TArray<FCellState> UTetrisBoard::GetVisibleGrid() const
{
	TArray<FCellState> Out;
	Out.SetNumUninitialized(BoardWidth * BoardVisibleHeight);
	for (int32 Y = 0; Y < BoardVisibleHeight; ++Y)
	{
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			Out[Y * BoardWidth + X] = Cells[IndexOf(X, Y)];
		}
	}
	return Out;
}

void UTetrisBoard::LockPiece(const TArray<FIntPoint>& BlockCoords, EPieceType PieceType)
{
	TArray<FIntPoint> Changed;
	Changed.Reserve(BlockCoords.Num());
	for (const FIntPoint& P : BlockCoords)
	{
		if (InBounds(P.X, P.Y))
		{
			Cells[IndexOf(P.X, P.Y)] = FCellState(PieceType);
			Changed.Add(P);
		}
	}
	if (Changed.Num() > 0)
	{
		OnBoardChanged.Broadcast(Changed);
	}
}

bool UTetrisBoard::IsRowComplete(int32 Y) const
{
	for (int32 X = 0; X < BoardWidth; ++X)
	{
		if (Cells[IndexOf(X, Y)].IsEmpty())
		{
			return false;
		}
	}
	return true;
}

FLineClearResult UTetrisBoard::ClearLines()
{
	FLineClearResult Result;

	// 1) 완성 행 인덱스 수집 (오름차순)
	for (int32 Y = 0; Y < BoardTotalHeight; ++Y)
	{
		if (IsRowComplete(Y))
		{
			Result.ClearedRows.Add(Y);
		}
	}
	Result.LinesCleared = Result.ClearedRows.Num();

	if (Result.LinesCleared == 0)
	{
		return Result; // 이벤트 미발행 (GDD Edge Case 4)
	}

	// 2) 압축 복사: 완성되지 않은 행만 아래부터 채우고, 나머지는 Empty.
	// Why: in-place 삭제는 인덱스 꼬임 위험 → 새 버퍼로 깔끔하게.
	TArray<FCellState> NewCells;
	NewCells.SetNum(BoardWidth * BoardTotalHeight);

	int32 WriteY = 0;
	for (int32 ReadY = 0; ReadY < BoardTotalHeight; ++ReadY)
	{
		if (Result.ClearedRows.Contains(ReadY))
		{
			continue;
		}
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			NewCells[WriteY * BoardWidth + X] = Cells[IndexOf(X, ReadY)];
		}
		++WriteY;
	}
	Cells = MoveTemp(NewCells);

	OnLinesCleared.Broadcast(Result);

	// Board 전체 변경으로 간주 — 변경 셀 목록은 비워서 "전체 갱신" 시그널로 사용.
	OnBoardChanged.Broadcast(TArray<FIntPoint>());

	return Result;
}

void UTetrisBoard::AddGarbageLines(int32 Count, int32 GapColumn)
{
	if (Count <= 0)
	{
		return;
	}
	// GDD Edge Case 5: GapColumn 범위 강제
	GapColumn = FMath::Clamp(GapColumn, 0, BoardWidth - 1);

	TArray<FCellState> NewCells;
	NewCells.SetNum(BoardWidth * BoardTotalHeight);

	// 1) 기존 행을 위로 Count만큼 밀어 복사. 초과(>= TotalHeight) 소멸.
	for (int32 ReadY = 0; ReadY < BoardTotalHeight; ++ReadY)
	{
		const int32 WriteY = ReadY + Count;
		if (WriteY >= BoardTotalHeight)
		{
			continue;
		}
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			NewCells[WriteY * BoardWidth + X] = Cells[IndexOf(X, ReadY)];
		}
	}

	// 2) 하단 Count행을 가비지로 채우되, GapColumn은 Empty
	for (int32 Y = 0; Y < Count && Y < BoardTotalHeight; ++Y)
	{
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			NewCells[Y * BoardWidth + X] =
				(X == GapColumn) ? FCellState() : FCellState(EPieceType::Garbage);
		}
	}

	Cells = MoveTemp(NewCells);
	OnBoardChanged.Broadcast(TArray<FIntPoint>());
}

bool UTetrisBoard::IsBlockOut(const TArray<FIntPoint>& SpawnCoords) const
{
	for (const FIntPoint& P : SpawnCoords)
	{
		if (!InBounds(P.X, P.Y))
		{
			continue; // 범위 밖은 BlockOut 판정 대상이 아님 (스폰 시 위쪽 버퍼 안에 있어야 함)
		}
		if (Cells[IndexOf(P.X, P.Y)].IsFilled())
		{
			return true;
		}
	}
	return false;
}

bool UTetrisBoard::IsLockOut(const TArray<FIntPoint>& LockCoords) const
{
	if (LockCoords.Num() == 0)
	{
		return false;
	}
	for (const FIntPoint& P : LockCoords)
	{
		if (P.Y < BoardVisibleHeight)
		{
			return false; // 한 블록이라도 Visible 영역 안에 있으면 Lock Out 아님
		}
	}
	return true;
}
