// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Input/TetrisHandling.h" // FTetrisHandling(값 멤버), FHandlingConfig/FHandlingInput/EInputEdge
#include "TetrisSessionSubsystem.generated.h"

class UTetrisBoard;
class UTetrisRandomizer;
class UTetrisScoring;
class UTetrisGameCore;
class UTetrisHUDViewModel;
class UTetrisHUDViewModelBinder;
class UTetrisBoardViewModel;
class UTetrisBoardViewModelBinder;

/**
 * Tetris 시뮬레이션 호스트 (ADR-0001).
 *
 * Why UTickableWorldSubsystem: 검증된 Core Loop(Board/Randomizer/Scoring/GameCore)를 월드 수명 동안
 *   생성·소유하고, 매 프레임 고정스텝 accumulator로 GameCore::Step()을 구동한다. ViewModel/UI는
 *   World->GetSubsystem<>()으로 전역 접근해 GetGameCore()로 이벤트를 구독한다.
 * 결정성: 가변 DeltaTime을 Step()에 직접 넘기지 않고, SimDelta=1/SimHz 단위로만 쪼개 호출한다.
 */
UCLASS()
class PROJECTTETRA_API UTetrisSessionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem / UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** 게임/PIE 월드에서만 생성 — 에디터 프리뷰/인스펙터 월드 제외. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	//~ 세션 제어
	/** 새 게임 시작 (시드 지정). 누적기 리셋 + 구동 시작. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Session")
	void StartGame(int64 Seed);

	/** 같은 시드로 재시작. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Session")
	void RestartGame();

	/** 일시정지 토글 — 누적/구동 중단·재개. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Session")
	void SetPaused(bool bPaused);

	//~ 입력 수신 (PlayerController가 frame마다 호출 — 핸들링 버퍼에 의도만 기록, 타이밍 판단은 고정스텝 핸들링이)
	/** 좌/우 held 상태 갱신. bLeft=true면 좌, false면 우. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Session|Input")
	void SetMoveHeld(bool bLeft, bool bHeld);

	/** 이산 입력 엣지를 버퍼에 적재. 다음 고정스텝에서 드레인된다(빠른 탭 유실 방지). */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Session|Input")
	void PushInputEdge(EInputEdge Edge);

	/**
	 * 고정스텝 전진 (fsm.md §F1). DeltaTime을 누적해 SimDelta 단위로 GameCore->Step()을 0..MaxStepsPerFrame회 호출.
	 * Tick()이 위임하며, 라이브 월드 틱에 의존하지 않고 단독 검증 가능하도록 분리했다.
	 * @return 이번 호출에서 실행한 Step 수.
	 */
	int32 AdvanceFixedSteps(float DeltaTime);

	//~ 접근 (ViewModel/바인더용)
	UTetrisGameCore* GetGameCore() const { return GameCore; }
	UTetrisScoring* GetScoring() const { return Scoring; }
	UTetrisRandomizer* GetRandomizer() const { return Randomizer; }
	UTetrisBoard* GetBoard() const { return Board; }

	/** HUD가 바인딩할 ViewModel. 바인더가 소유하며, 미시작 시 null. (실제 위젯 바인딩은 #13) */
	UTetrisHUDViewModel* GetHUDViewModel() const;

	/** Board 위젯(#12)이 바인딩할 ViewModel. BoardBinder가 소유하며, 미시작 시 null. */
	UTetrisBoardViewModel* GetBoardViewModel() const;

	//~ 튜닝 (fsm.md §F1)
	/** 고정 시뮬 주파수. GameCore/LockDelay/Scoring의 SimHz와 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Session", meta = (ClampMin = "30", ClampMax = "240"))
	int32 SimHz = 60;

	/** 프레임당 최대 Step 수 — 프레임 히치 후 따라잡기 폭주(spiral-of-death) 방지 캡. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Session", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxStepsPerFrame = 5;

	/** 핸들링 튜닝값(DAS/ARR/DCD). StartGame/RestartGame 시 SimHz를 세션과 일치시켜 Handling에 주입. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Session")
	FHandlingConfig HandlingConfig;

protected:
	/** 소유 Core Loop 객체를 생성하고 와이어링한다. Initialize 및 StartGame(테스트 경로)에서 호출. 멱등. */
	void CreateAndWireCore();

	/** 한 고정스텝 직전: 입력 버퍼 스냅샷·드레인 → 순수 핸들링 → 명령을 GameCore에 적재. */
	void DriveHandlingForStep();

	/** 핸들링/버퍼 초기화 + 세션 SimHz로 Config 주입. StartGame/RestartGame에서 호출. */
	void ResetHandling();

	/** tetra.DebugBoard CVar가 켜져 있을 때 보드+활성 피스+상태/점수를 ASCII로 출력(PIE 육안용). UI 미사용. */
	void DebugDrawBoard();
	int32 DebugLogThrottle = 0; // UE_LOG 스팸 완화용 프레임 카운터

	UPROPERTY(Transient)
	TObjectPtr<UTetrisBoard> Board;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisRandomizer> Randomizer;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisScoring> Scoring;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisGameCore> GameCore;

	/** HUD ViewModel 바인더. Session이 소유·구동(LockDelay가 GameCore에, Handling이 Session에 소유되는 것과 동일 composition). */
	UPROPERTY(Transient)
	TObjectPtr<UTetrisHUDViewModelBinder> HUDBinder;

	/** Board ViewModel 바인더. Session이 소유·구동 — HUD 바인더와 병렬 생명주기(#12 Board Renderer). */
	UPROPERTY(Transient)
	TObjectPtr<UTetrisBoardViewModelBinder> BoardBinder;

	/** 시간 누적·구동 여부. Idle/Pause 시 false → 틱·누적 중단. */
	bool bRunning = false;

	/** 고정스텝 잔여 시간(초). 멤버로 이월해 드리프트 없이 누적. */
	double TimeAccumulator = 0.0;

	/** 순수 DAS/ARR/DCD 핸들러(세션 소유·구동). LockDelay가 GameCore에 소유되는 것과 동일 composition. */
	FTetrisHandling Handling;

	/** frame→fixed-step 입력 핸드오프 버퍼. PC가 쓰고 매 Step에서 스냅샷·드레인. 단일 게임스레드 가정(락 불필요). */
	FHandlingInput InputBuffer;
};
