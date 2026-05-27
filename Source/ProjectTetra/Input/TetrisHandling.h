// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/TetrisTypes.h"           // EGameCommand, TetrisConstants
#include "Input/TetrisHandlingTypes.h"  // FHandlingConfig, FHandlingInput, EInputEdge

/**
 * Tetris Handling — DAS/ARR/DCD 자동반복을 "정수 스텝"으로 처리하는 순수 입력 핸들러.
 *
 * Why 비-UObject 순수 클래스: 의존성 0(Board/Piece/Session/PC 비참조). 매 고정스텝 입력 스냅샷
 *   (FHandlingInput)만 주입받아 추상 명령(EGameCommand)을 방출한다. → 단위 테스트가 인스턴스
 *   하나로 가능하고, float 초 누적 대신 정수 스텝으로 플랫폼 무관 결정성을 보장한다(LockDelay와 동일 패턴).
 *
 * 명령 순서(input-handling.md §4): ① 이산 명령(회전/하드드롭/홀드/소프트드롭) 순서 보존 →
 *   ② 방향 이동(즉시 1칸 + DAS 지연 + ARR 반복). 동시 좌우는 last-input priority.
 */
class PROJECTTETRA_API FTetrisHandling
{
public:
	FTetrisHandling() = default;
	explicit FTetrisHandling(const FHandlingConfig& InConfig) : Config(InConfig) {}

	/** 튜닝값 교체. 진행 중 타이머/방향 상태는 건드리지 않는다(런타임 SimHz 변경 시 Reset 권장 — OQ#2). */
	void SetConfig(const FHandlingConfig& InConfig) { Config = InConfig; }
	const FHandlingConfig& GetConfig() const { return Config; }

	/** 스폰/게임 시작 시 — 방향/타이머 상태 초기화. held는 이 클래스가 보유하지 않으므로 건드리지 않는다. */
	void Reset();

	/** 한 고정스텝 전진. 입력 스냅샷 → 명령 스트림(이산 먼저, 방향 이동 뒤). */
	TArray<EGameCommand> AdvanceStep(const FHandlingInput& In);

	//~ ms→스텝 환산 (Formula H1, round). 예) DAS 100ms@60Hz → 6, ARR 17ms@60Hz → 1.
	int32 GetDASSteps() const;
	int32 GetARRSteps() const;
	/** DCD는 [0, DASSteps]로 클램프(H5) — 전환 시 DAS를 깎을 수 있는 상한은 DAS 자신. */
	int32 GetDCDSteps() const;

	//~ Queries (테스트/디버그)
	bool IsActive() const { return ActiveDir != EActiveDir::None; }
	bool IsRepeating() const { return bRepeating; }
	int32 GetDASCharge() const { return DASCharge; }
	int32 GetARRCounter() const { return ARRCounter; }

private:
	/** 현재 자동반복 대상 방향. */
	enum class EActiveDir : uint8 { None, Left, Right };

	FHandlingConfig Config;

	EActiveDir ActiveDir = EActiveDir::None;
	int32 DASCharge = 0;    // press 후 경과 스텝(DAS 충전). >= DASSteps면 Repeating 진입.
	int32 ARRCounter = 0;   // 직전 반복 후 경과 스텝.
	bool bRepeating = false; // Charging(false) / Repeating(true).

	/** 방향 (재)시작: 즉시 1칸 방출 + 타이머 설정. bApplyDCD이고 기존 active와 다른 방향이면 DCD로 DAS 부분 보존(H5). */
	void StartDirection(EActiveDir NewDir, bool bApplyDCD, TArray<EGameCommand>& Out);
	void StopDirection();

	/** held 지속 시 DAS 충전/ARR 반복 진행(H2~H4). 시작 스텝에는 호출하지 않는다(그 스텝은 1칸만). */
	void AdvanceAutoShift(TArray<EGameCommand>& Out);

	/** ARR>0이면 1칸, ARR==0이면 벽까지(BoardWidth개) 방출. FSM이 벽에서 무해 클램프(H4). */
	void EmitRepeat(EGameCommand Cmd, int32 ARRSteps, TArray<EGameCommand>& Out) const;

	static EGameCommand MoveCommandFor(EActiveDir Dir);
};
