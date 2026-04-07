// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/TetrisTypes.h"
#include "TetrisBoard.generated.h"

// Why non-dynamic: 구독자는 C++ ViewModel/FSM. 람다 바인딩과 성능을 위해 non-dynamic.
//                   Blueprint에서 직접 바인딩이 필요하면 별도 래퍼 UFUNCTION으로 노출.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoardChanged, const TArray<FIntPoint>& /*ChangedCells*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLinesCleared, const FLineClearResult& /*Result*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTopOut, ETopOutType /*TopOutType*/);

/**
 * Tetris Playfield (10x24, 상단 4행은 버퍼존).
 *
 * Why UObject: ViewModel/FSM이 델리게이트로 구독하기 위함. UI/Actor는 참조하지 않는다.
 * 좌표계: 좌하단 (0,0), X 오른쪽, Y 위쪽. Visible 영역은 Y ∈ [0, 19].
 */
UCLASS(BlueprintType)
class PROJECTTETRA_API UTetrisBoard : public UObject
{
	GENERATED_BODY()

public:
	UTetrisBoard();

	//~ Lifecycle
	/** 모든 셀을 Empty로 초기화. 게임 시작/재시작 시 호출. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	void Clear();

	//~ Queries
	/** 주어진 블록 좌표 목록이 모두 그리드 내 + Empty인지 검증. 빈 배열은 true. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	bool IsValidPosition(const TArray<FIntPoint>& BlockCoords) const;

	/** 주어진 좌표 묶음이 아래로 몇 칸 더 내려갈 수 있는지. (Ghost / 하드드롭) */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	int32 GetDropDistance(const TArray<FIntPoint>& BlockCoords) const;

	/** 단일 셀 읽기. 범위 밖이면 Empty 반환. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	FCellState GetCell(int32 X, int32 Y) const;

	/** Visible 영역(Y ∈ [0, 19]) 셀 스냅샷. Row-major: index = Y * Width + X. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	TArray<FCellState> GetVisibleGrid() const;

	//~ Mutations
	/** 좌표들에 PieceType을 기록 (Lock). OnBoardChanged 발행. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	void LockPiece(const TArray<FIntPoint>& BlockCoords, EPieceType PieceType);

	/** 완성 행 검색 + 제거 + 위쪽 행 낙하. 결과 반환 + OnLinesCleared 발행(0줄이면 미발행). */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	FLineClearResult ClearLines();

	/** 하단에 가비지 N행 삽입(GapColumn은 빈 채로). 기존 행은 위로 밀림. 초과 시 소멸. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	void AddGarbageLines(int32 Count, int32 GapColumn);

	//~ Top-out
	/** 스폰 좌표 충돌 여부 (Block Out). */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	bool IsBlockOut(const TArray<FIntPoint>& SpawnCoords) const;

	/** Lock된 좌표가 모두 Visible 영역 위(Y >= VisibleHeight)인지. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	bool IsLockOut(const TArray<FIntPoint>& LockCoords) const;

	//~ Delegates
FOnBoardChanged OnBoardChanged;

FOnLinesCleared OnLinesCleared;

FOnTopOut OnTopOut;

private:
	/** Row-major 1차원 저장. index = Y * Width + X. */
	TArray<FCellState> Cells;

	FORCEINLINE int32 IndexOf(int32 X, int32 Y) const
	{
		return Y * TetrisConstants::BoardWidth + X;
	}

	FORCEINLINE bool InBounds(int32 X, int32 Y) const
	{
		return X >= 0 && X < TetrisConstants::BoardWidth
		    && Y >= 0 && Y < TetrisConstants::BoardTotalHeight;
	}

	bool IsRowComplete(int32 Y) const;
};
