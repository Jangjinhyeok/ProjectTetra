// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/TetrisTypes.h"
#include "Block/TetrisPiece.h"
#include "System/TetrisLockDelay.h"
#include "TetrisGameCore.generated.h"

class UTetrisBoard;
class UTetrisRandomizer;
class UTetrisScoring;

// Why non-dynamic: 구독자는 C++ ViewModel/Score. 람다 바인딩과 성능을 위해 non-dynamic (Board 컨벤션과 일치).
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged, EGameState /*Old*/, EGameState /*New*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActivePieceUpdated, const FActivePiece& /*Piece*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHoldChanged, EPieceType /*HoldType*/, bool /*bCanHold*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameOver, ETopOutType /*Type*/);
// 매 lock마다(0줄 포함) 발행 — Score/Level의 콤보 리셋이 0줄 lock을 알아야 하기 때문.
DECLARE_MULTICAST_DELEGATE_SixParams(FOnPieceLocked, int32 /*LinesCleared*/, const TArray<int32>& /*ClearedRows*/, bool /*bWasLastActionRotation*/, int32 /*LastKickIndex*/, int32 /*SoftDropCells*/, int32 /*HardDropCells*/);

/**
 * Tetris Game Core (FSM) — 테트리스 한 판을 구동하는 결정적 오케스트레이터.
 *
 * Why UObject: ViewModel이 델리게이트로 구독하기 위함. UI/Actor/Blueprint는 참조하지 않는다.
 * Board / Piece(SRS) / Randomizer / LockDelay / Scoring을 호출자로서 조율하되, 각자의 공개
 *   인터페이스만 사용한다. 입력은 직접 받지 않고 추상 명령 큐(EGameCommand)만 소비한다.
 * 결정성: 동일 시드 + 동일 명령 시퀀스 + 동일 스텝 수 → 동일 결과. 고정 타임스텝 Step()으로만 전진.
 */
UCLASS(BlueprintType)
class PROJECTTETRA_API UTetrisGameCore : public UObject
{
	GENERATED_BODY()

public:
	UTetrisGameCore();

	/** 의존성 주입. Session 계층이 각 객체를 생성한 뒤 와이어링한다. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|GameCore")
	void Initialize(UTetrisBoard* InBoard, UTetrisRandomizer* InRandomizer, UTetrisScoring* InScoring);

	/** 새 게임 시작 (시드 지정). Board.Clear + Randomizer.Initialize + 첫 피스 스폰. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|GameCore")
	void StartGame(int64 Seed);

	/** 같은 시드로 재시작. Board.Clear + Randomizer.Reset + 상태 초기화. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|GameCore")
	void RestartGame();

	/** 고정 타임스텝 1회 전진. (외부 Session이 accumulator로 호출 횟수를 제어) */
	UFUNCTION(BlueprintCallable, Category = "Tetris|GameCore")
	void Step();

	/** 추상 명령 적재 (Input → FSM 단방향). 다음 Step에서 소비. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|GameCore")
	void EnqueueCommand(EGameCommand Command);

	//~ Queries
	UFUNCTION(BlueprintPure, Category = "Tetris|GameCore")
	EGameState GetState() const { return CurrentState; }

	const FActivePiece& GetActivePiece() const { return ActivePiece; }

	UFUNCTION(BlueprintPure, Category = "Tetris|GameCore")
	EPieceType GetHoldSlot() const { return HoldSlot; }

	UFUNCTION(BlueprintPure, Category = "Tetris|GameCore")
	bool IsHoldAvailable() const { return bHoldEnabled && !bHoldUsedThisPiece; }

	/** 큐에 남은 명령 수 (디버그/테스트용). */
	int32 GetPendingCommandCount() const { return CommandQueue.Num(); }

	//~ Delegates
	FOnGameStateChanged OnStateChanged;
	FOnActivePieceUpdated OnActivePieceUpdated;
	FOnHoldChanged OnHoldChanged;
	FOnGameOver OnGameOver;
	FOnPieceLocked OnPieceLocked;

	//~ Tuning
	/** 고정 시뮬 주파수. LockDelay/Scoring의 SimHz와 일치시켜 결정성 보장. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|GameCore", meta = (ClampMin = "30", ClampMax = "240"))
	int32 SimHz = 60;

	/** 소프트드롭 중력 배수. 클수록 빠르게 하강(매우 크면 한 스텝에 바닥까지). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|GameCore", meta = (ClampMin = "1"))
	int32 SoftDropFactor = 20;

	/** Hold 기능 on/off (게임 모드용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|GameCore")
	bool bHoldEnabled = true;

	/** Lock Delay 튜닝값. Initialize/StartGame 시 LockDelay에 주입. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|GameCore")
	FLockDelayConfig LockDelayConfig;

protected:
	//~ 의존 객체 (Session이 소유, 여기선 참조만)
	UPROPERTY()
	TObjectPtr<UTetrisBoard> Board;

	UPROPERTY()
	TObjectPtr<UTetrisRandomizer> Randomizer;

	UPROPERTY()
	TObjectPtr<UTetrisScoring> Scoring;

private:
	//~ 소유 상태
	FActivePiece ActivePiece;
	EPieceType HoldSlot = EPieceType::None;
	EGameState CurrentState = EGameState::Idle;
	FTetrisLockDelay LockDelay;
	TArray<EGameCommand> CommandQueue;

	double GravityAccumulator = 0.0;
	bool bSoftDropHeld = false;
	bool bHoldUsedThisPiece = false;
	int32 SoftDropCells = 0;   // 입력 기반 소프트드롭 하강 칸 (자연 낙하 제외)
	int32 HardDropCells = 0;   // 하드드롭 하강 칸

	//~ 상태 전이 / 스텝 처리
	void EnterState(EGameState NewState);
	void TickFalling();
	void TickLocking();

	/** 명령 큐를 FIFO로 소비. HardDrop으로 lock이 발생하면 true 반환(잔여 명령 유지 + 스텝 종료). */
	bool DrainCommands();
	void HandleNonHardDropCommand(EGameCommand Command);

	void IntegrateGravity();
	void EnterLocking();
	void BeginSpawn();
	void DoHold();
	void DoHardDrop();
	void DoLock();
	void TriggerGameOver(ETopOutType Type);

	//~ 헬퍼
	void TryMoveCommand(EMoveDirection Direction);
	void TryRotateCommand(ERotateDirection Direction);
	bool IsGrounded(const FActivePiece& Piece) const;
	int32 GetLowestBlockY(const FActivePiece& Piece) const;
	double GetGravityPerStep() const;
	int32 GetCurrentLevel() const;
	void ResetPerPieceState(int32 LowestBlockY);
};
