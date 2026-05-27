// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TetrisScoring.generated.h"

// Why non-dynamic: 구독자는 C++ ViewModel/FSM. 람다 바인딩과 성능을 위해 non-dynamic (Board 컨벤션과 일치).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int64 /*NewScore*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelChanged, int32 /*NewLevel*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLinesChanged, int32 /*TotalLinesCleared*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnComboChanged, int32 /*Combo*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnB2BChanged, int32 /*B2BCount*/);

/**
 * Score/Level 튜닝값 묶음.
 *
 * Why USTRUCT: UTetrisScoring이 UPROPERTY(EditDefaultsOnly)로 에디터에 노출하기 위함.
 *   HANDOFF 제약(매직 넘버 금지) — 점수 테이블/배수/레벨 커브를 데이터로 관리한다.
 */
USTRUCT(BlueprintType)
struct FScoringConfig
{
	GENERATED_BODY()

	/** LineBase[N] = N줄 클리어 기본점. 인덱스 = 클리어 줄 수(0~4). 생성자에서 {0,100,300,500,800}. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring")
	TArray<int32> LineBase;

	/** Back-to-Back 점수 배수 (difficult 연속 시). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float B2BMultiplier = 1.5f;

	/** 콤보 단위점. 보너스 = ComboBaseValue × Combo × Level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "0", ClampMax = "200"))
	int32 ComboBaseValue = 50;

	/** 레벨 1단계당 필요한 누적 삭제 줄 수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "1"))
	int32 LinesPerLevel = 10;

	/** 레벨 상한 — 중력 커브 발산 방지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "1", ClampMax = "99"))
	int32 LevelCap = 20;

	/** BaseG 커브 시작점 (낮을수록 초반부터 빠름). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "0.5", ClampMax = "0.99"))
	float GravityBase = 0.8f;

	/** 레벨당 가속률 (클수록 후반 급가속). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "0.001", ClampMax = "0.02"))
	float GravityDecay = 0.007f;

	/** 시뮬레이션 주파수(Hz). BaseG 환산에 사용. FSM/LockDelay의 SimHz와 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring", meta = (ClampMin = "1"))
	int32 SimHz = 60;

	FScoringConfig()
	{
		LineBase = { 0, 100, 300, 500, 800 };
	}
};

/**
 * Score / Level — 점수·삭제 줄 수·레벨·콤보·Back-to-Back을 추적하는 이벤트 주도 누산기.
 *
 * Why UObject: ViewModel이 델리게이트(On*Changed)로 구독하기 위함. UI/Actor는 참조하지 않는다.
 * 입력 진입점은 FSM의 매 lock(0줄 포함)마다 호출되는 HandlePieceLocked 하나뿐 — Board/FSM
 *   인스턴스 없이 이 함수 호출만으로 단위 테스트가 완결된다.
 */
UCLASS(BlueprintType)
class PROJECTTETRA_API UTetrisScoring : public UObject
{
	GENERATED_BODY()

public:
	UTetrisScoring();

	/** 모든 상태 초기화. 게임 시작/재시작 시 호출. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Scoring")
	void ResetScoring();

	/**
	 * 매 lock마다(0줄 포함) FSM이 호출하는 단일 진입점.
	 * 콤보/B2B/점수/레벨을 갱신하고 변경된 값마다 On*Changed를 발행한다.
	 * @param LinesCleared          이번 lock으로 삭제된 줄 수 (0~4)
	 * @param bWasLastActionRotation 마지막 동작이 회전이었는지 (VS T-Spin 판정용 — MVP 미사용)
	 * @param LastKickIndex         마지막 회전의 Kick 인덱스 (VS T-Spin 판정용 — MVP 미사용)
	 * @param SoftDropCells         이 피스가 소프트드롭으로 하강한 칸 수
	 * @param HardDropCells         이 피스가 하드드롭으로 하강한 칸 수
	 */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Scoring")
	void HandlePieceLocked(int32 LinesCleared, bool bWasLastActionRotation, int32 LastKickIndex, int32 SoftDropCells, int32 HardDropCells);

	//~ Queries
	UFUNCTION(BlueprintPure, Category = "Tetris|Scoring")
	int64 GetScore() const { return Score; }

	UFUNCTION(BlueprintPure, Category = "Tetris|Scoring")
	int32 GetTotalLinesCleared() const { return TotalLinesCleared; }

	UFUNCTION(BlueprintPure, Category = "Tetris|Scoring")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category = "Tetris|Scoring")
	int32 GetCombo() const { return Combo; }

	UFUNCTION(BlueprintPure, Category = "Tetris|Scoring")
	int32 GetB2BCount() const { return B2BCount; }

	/** 레벨별 기본 중력(cells/step). FSM이 중력 커브 입력으로 조회. (이 시스템이 커브 소유) */
	UFUNCTION(BlueprintPure, Category = "Tetris|Scoring")
	float GetBaseG(int32 InLevel) const;

	//~ Delegates
	FOnScoreChanged OnScoreChanged;
	FOnLevelChanged OnLevelChanged;
	FOnLinesChanged OnLinesChanged;
	FOnComboChanged OnComboChanged;
	FOnB2BChanged OnB2BChanged;

	/** 튜닝값. EditDefaultsOnly로 CDO/에디터에서 조정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Scoring")
	FScoringConfig Config;

private:
	int64 Score = 0;
	int32 TotalLinesCleared = 0;
	int32 Level = 1;
	int32 Combo = -1;                    // -1 = 비활성
	int32 B2BCount = 0;                  // 0 = 비활성
	bool bLastClearWasDifficult = false;
};
