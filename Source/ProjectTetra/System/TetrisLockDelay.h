// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetrisLockDelay.generated.h"

/**
 * Lock Delay 튜닝값 묶음.
 *
 * Why USTRUCT: FSM(UObject)이 UPROPERTY(EditDefaultsOnly)로 에디터에 노출하기 위함.
 *   HANDOFF 제약(매직 넘버 금지) — 모든 튜닝값은 여기서 조정 가능하다.
 */
USTRUCT(BlueprintType)
struct FLockDelayConfig
{
	GENERATED_BODY()

	/** 접지 후 Lock까지의 유예 시간(ms). 0이면 접지 즉시 Lock(클래식 모드). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|LockDelay", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 LockDelayMs = 500;

	/** 피스당 타이머 리셋 상한. 0이면 리셋 불가(가장 엄격). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|LockDelay", meta = (ClampMin = "0", ClampMax = "30"))
	int32 MoveResetLimit = 15;

	/** true면 최저행 갱신 시 리셋 예산을 리필(Extended Placement). false면 단순 Move Reset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|LockDelay")
	bool bLowestRowRefresh = true;

	/** 시뮬레이션 주파수(Hz). ms→스텝 환산에 사용. 결정성을 위해 FSM의 SimHz와 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|LockDelay", meta = (ClampMin = "1"))
	int32 SimHz = 60;
};

/**
 * Lock Delay — 접지 후 고정 유예를 "정수 스텝"으로 카운트하는 순수 타이머/카운터.
 *
 * Why 비-UObject 순수 클래스: 의존성 0(Board/Piece 비참조). 접지(grounded)와 최저 블록 Y는
 *   FSM이 계산해 주입한다. → 단위 테스트가 Board/Piece 인스턴스 없이 가능하고,
 *   float 초 누적 대신 정수 스텝으로 플랫폼 무관 결정성을 보장한다.
 * 좌표계: 최저 블록 Y가 작을수록 아래(0=바닥). 즉 LowestBlockY 감소 = 정당한 하강.
 */
class PROJECTTETRA_API FTetrisLockDelay
{
public:
	FTetrisLockDelay() = default;
	explicit FTetrisLockDelay(const FLockDelayConfig& InConfig) : Config(InConfig) {}

	/** 튜닝값 교체. 진행 중인 카운터/타이머 상태는 건드리지 않는다. */
	void SetConfig(const FLockDelayConfig& InConfig) { Config = InConfig; }
	const FLockDelayConfig& GetConfig() const { return Config; }

	/** 새 피스 스폰 / Hold 교환 — 모든 상태 초기화. LowestBlockY = 현재 최저 블록 Y. */
	void OnNewPiece(int32 LowestBlockY);

	/** Falling→Locking 진입 — 타이머 시작. */
	void OnGrounded();

	/** 1 스텝 진행. 만료(= Lock 신호)면 true. 비활성 상태면 false. */
	bool Tick();

	/** 이동/회전 성공 후 호출. FSM이 접지 유지 여부와 새 최저 블록 Y를 주입한다. */
	void NotifyMoveOrRotate(bool bStillGrounded, int32 NewLowestBlockY);

	//~ Queries (ViewModel/디버그/테스트)
	int32 GetElapsedSteps() const { return LockElapsedSteps; }
	int32 GetMoveResetCounter() const { return MoveResetCounter; }
	bool IsActive() const { return bActive; }

	/** ms→스텝 환산값. round(LockDelayMs / 1000 × SimHz). 예) 500ms@60Hz → 30. */
	int32 GetLockDelaySteps() const;

private:
	FLockDelayConfig Config;

	int32 LockElapsedSteps = 0;  // 접지 후 누적 스텝
	int32 MoveResetCounter = 0;  // 피스당 사용한 리셋 횟수
	int32 LowestRowRecord = 0;   // 이 피스가 도달한 최저 블록 Y
	bool bActive = false;        // Locking 상태(타이머 진행) 여부
};
