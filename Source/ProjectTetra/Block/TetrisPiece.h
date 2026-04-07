// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/TetrisTypes.h"
#include "TetrisPiece.generated.h"

class UTetrisBoard;

/**
 * 활성 피스 (보드 위에서 조작 중인 테트로미노).
 *
 * Why USTRUCT: 값 타입. FSM이 소유하며 단순 복사로 스냅샷을 뜰 수 있다.
 *   회전/이동 메서드는 비멤버 헬퍼(FTetrisPieceOps)에서 제공 — Board와의 결합을 분리.
 */
USTRUCT(BlueprintType)
struct PROJECTTETRA_API FActivePiece
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tetris|Piece")
	EPieceType Type = EPieceType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Tetris|Piece")
	FIntPoint PivotPosition = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Tetris|Piece")
	ERotationState RotationState = ERotationState::State0;

	/** 마지막 동작이 회전이었는지 (T-Spin 판정 정보용). */
	UPROPERTY(BlueprintReadOnly, Category = "Tetris|Piece")
	bool bWasLastActionRotation = false;

	/** 마지막 회전이 성공한 Kick Test 인덱스 (0~4). 회전이 아니거나 실패면 INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Tetris|Piece")
	int32 LastKickIndex = INDEX_NONE;
};

/**
 * 활성 피스 연산 모음 (정적).
 *
 * Why 비멤버: 피스 데이터(struct)와 보드 의존(UTetrisBoard*)을 분리해
 *   순수 함수처럼 테스트 가능하게 한다.
 */
class PROJECTTETRA_API FTetrisPieceOps
{
public:
	/** 스폰 시 새 활성 피스 생성. SpawnY는 보통 BoardVisibleHeight (=20). */
	static FActivePiece Spawn(EPieceType Type, int32 SpawnY = TetrisConstants::BoardVisibleHeight);

	/** 현재 회전/위치에서 4블록의 보드 절대 좌표 반환. */
	static TArray<FIntPoint> GetAbsoluteBlockPositions(const FActivePiece& Piece);

	/** 가상의 (Pivot, Rotation)에서의 절대 좌표. SRS Kick 후보 검증용. */
	static TArray<FIntPoint> GetAbsoluteBlockPositionsAt(EPieceType Type, FIntPoint Pivot, ERotationState Rotation);

	/** 좌/우/하 이동 시도. 성공 시 Piece 갱신 + true 반환. */
	static bool TryMove(FActivePiece& Piece, EMoveDirection Direction, const UTetrisBoard& Board);

	/** SRS 회전 시도 (5회 Kick 포함). 성공 시 Piece 갱신 + true 반환. */
	static bool TryRotate(FActivePiece& Piece, ERotateDirection Direction, const UTetrisBoard& Board);

	/** 하드드롭 — 보드 충돌까지 즉시 하강. 이동 거리 반환. (Lock 자체는 호출자가 수행) */
	static int32 HardDrop(FActivePiece& Piece, const UTetrisBoard& Board);

private:
	/** CW/CCW 회전 후의 다음 상태 계산. */
	static ERotationState NextRotation(ERotationState Current, ERotateDirection Direction);
};
