// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetrisTypes.generated.h"

/**
 * 테트로미노 종류.
 * None은 빈 셀, Garbage는 공격으로 추가된 회색 블록을 의미한다.
 * 색상 정보 또한 이 enum 값에서 파생되므로 셀 상태 표현으로 충분하다.
 */
UENUM(BlueprintType)
enum class EPieceType : uint8
{
	None    UMETA(DisplayName = "None"),
	I       UMETA(DisplayName = "I"),
	O       UMETA(DisplayName = "O"),
	T       UMETA(DisplayName = "T"),
	S       UMETA(DisplayName = "S"),
	Z       UMETA(DisplayName = "Z"),
	J       UMETA(DisplayName = "J"),
	L       UMETA(DisplayName = "L"),
	Garbage UMETA(DisplayName = "Garbage"),
};

/** 피스 회전 상태. SRS 표준 4상태. */
UENUM(BlueprintType)
enum class ERotationState : uint8
{
	State0 UMETA(DisplayName = "0"),   // 스폰 방향
	StateR UMETA(DisplayName = "R"),   // CW 1회
	State2 UMETA(DisplayName = "2"),   // 180도
	StateL UMETA(DisplayName = "L"),   // CCW 1회
};

/** 회전 방향. */
UENUM(BlueprintType)
enum class ERotateDirection : uint8
{
	CW  UMETA(DisplayName = "CW"),
	CCW UMETA(DisplayName = "CCW"),
};

/** 피스 이동 방향. */
UENUM(BlueprintType)
enum class EMoveDirection : uint8
{
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
	Down  UMETA(DisplayName = "Down"),
};

/** Top-out 종류. */
UENUM(BlueprintType)
enum class ETopOutType : uint8
{
	None     UMETA(DisplayName = "None"),
	BlockOut UMETA(DisplayName = "BlockOut"),  // 스폰 시 충돌
	LockOut  UMETA(DisplayName = "LockOut"),   // Lock된 블록이 모두 버퍼존
};

/**
 * 단일 셀 상태.
 * Why: 향후 셀 메타데이터(예: 가비지 색상, 폭탄 블록 등)를 확장 가능하도록 struct로 둔다.
 */
USTRUCT(BlueprintType)
struct FCellState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tetris")
	EPieceType Type = EPieceType::None;

	FCellState() = default;
	explicit FCellState(EPieceType InType) : Type(InType) {}

	FORCEINLINE bool IsEmpty() const { return Type == EPieceType::None; }
	FORCEINLINE bool IsFilled() const { return Type != EPieceType::None; }
};

/** 라인 클리어 결과. */
USTRUCT(BlueprintType)
struct FLineClearResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tetris")
	int32 LinesCleared = 0;

	/** 삭제 이전 인덱스 기준의 행 번호 목록 (오름차순). */
	UPROPERTY(BlueprintReadOnly, Category = "Tetris")
	TArray<int32> ClearedRows;
};

/** 보드 상수. GDD Tuning Knob 기본값. */
namespace TetrisConstants
{
	constexpr int32 BoardWidth         = 10;
	constexpr int32 BoardVisibleHeight = 20;
	constexpr int32 BoardBufferHeight  = 4;
	constexpr int32 BoardTotalHeight   = BoardVisibleHeight + BoardBufferHeight; // 24
}
