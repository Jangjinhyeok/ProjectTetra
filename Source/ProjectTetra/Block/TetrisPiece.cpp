// Copyright ProjectTetra. All Rights Reserved.

#include "Block/TetrisPiece.h"
#include "Block/PieceData.h"
#include "Board/TetrisBoard.h"

using namespace TetrisPieceData;

FActivePiece FTetrisPieceOps::Spawn(EPieceType Type, int32 SpawnY)
{
	FActivePiece Piece;
	Piece.Type = Type;
	Piece.RotationState = ERotationState::State0;
	Piece.bWasLastActionRotation = false;
	Piece.LastKickIndex = INDEX_NONE;

	const FPieceShape* Shape = GetShape(Type);
	const int32 SpawnX = Shape ? Shape->SpawnOffsetX : 4;
	Piece.PivotPosition = FIntPoint(SpawnX, SpawnY);
	return Piece;
}

TArray<FIntPoint> FTetrisPieceOps::GetAbsoluteBlockPositions(const FActivePiece& Piece)
{
	return GetAbsoluteBlockPositionsAt(Piece.Type, Piece.PivotPosition, Piece.RotationState);
}

TArray<FIntPoint> FTetrisPieceOps::GetAbsoluteBlockPositionsAt(EPieceType Type, FIntPoint Pivot, ERotationState Rotation)
{
	TArray<FIntPoint> Out;
	const FPieceShape* Shape = GetShape(Type);
	if (!Shape)
	{
		return Out;
	}
	const int32 R = static_cast<int32>(Rotation);
	Out.Reserve(4);
	for (int32 i = 0; i < 4; ++i)
	{
		const FIntPoint& Rel = Shape->Blocks[R][i];
		Out.Add(FIntPoint(Pivot.X + Rel.X, Pivot.Y + Rel.Y));
	}
	return Out;
}

bool FTetrisPieceOps::TryMove(FActivePiece& Piece, EMoveDirection Direction, const UTetrisBoard& Board)
{
	FIntPoint Delta(0, 0);
	switch (Direction)
	{
		case EMoveDirection::Left:  Delta = FIntPoint(-1,  0); break;
		case EMoveDirection::Right: Delta = FIntPoint(+1,  0); break;
		case EMoveDirection::Down:  Delta = FIntPoint( 0, -1); break;
	}

	const FIntPoint NewPivot = Piece.PivotPosition + Delta;
	const TArray<FIntPoint> Candidate = GetAbsoluteBlockPositionsAt(Piece.Type, NewPivot, Piece.RotationState);
	if (!Board.IsValidPosition(Candidate))
	{
		return false;
	}
	Piece.PivotPosition = NewPivot;
	Piece.bWasLastActionRotation = false;
	Piece.LastKickIndex = INDEX_NONE;
	return true;
}

ERotationState FTetrisPieceOps::NextRotation(ERotationState Current, ERotateDirection Direction)
{
	const int32 C = static_cast<int32>(Current);
	const int32 N = (Direction == ERotateDirection::CW) ? (C + 1) % 4 : (C + 3) % 4;
	return static_cast<ERotationState>(N);
}

bool FTetrisPieceOps::TryRotate(FActivePiece& Piece, ERotateDirection Direction, const UTetrisBoard& Board)
{
	const ERotationState From = Piece.RotationState;
	const ERotationState To   = NextRotation(From, Direction);

	const EKickGroup Group = GetKickGroup(Piece.Type);

	// O 피스: Kick 없음, 위치 그대로 상태만 전이.
	if (Group == EKickGroup::None)
	{
		Piece.RotationState = To;
		Piece.bWasLastActionRotation = true;
		Piece.LastKickIndex = 0;
		return true;
	}

	const int32 TIdx = KickTransitionIndex(From, To);
	if (TIdx == INDEX_NONE)
	{
		return false;
	}

	const FIntPoint(*Table)[5] =
		(Group == EKickGroup::I) ? KickTable_I : KickTable_JLSTZ;

	for (int32 K = 0; K < 5; ++K)
	{
		const FIntPoint Offset = Table[TIdx][K];
		const FIntPoint NewPivot = Piece.PivotPosition + Offset;
		const TArray<FIntPoint> Candidate = GetAbsoluteBlockPositionsAt(Piece.Type, NewPivot, To);
		if (Board.IsValidPosition(Candidate))
		{
			Piece.PivotPosition = NewPivot;
			Piece.RotationState = To;
			Piece.bWasLastActionRotation = true;
			Piece.LastKickIndex = K;
			return true;
		}
	}
	return false; // 모든 Kick 실패 → 회전 취소
}

int32 FTetrisPieceOps::HardDrop(FActivePiece& Piece, const UTetrisBoard& Board)
{
	const TArray<FIntPoint> Current = GetAbsoluteBlockPositions(Piece);
	const int32 Distance = Board.GetDropDistance(Current);
	if (Distance > 0)
	{
		Piece.PivotPosition.Y -= Distance;
		Piece.bWasLastActionRotation = false;
		Piece.LastKickIndex = INDEX_NONE;
	}
	return Distance;
}
