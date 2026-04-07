// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"
#include "Core/TetrisTypes.h"
#include "TetrisRandomizer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNextQueueChanged, const TArray<EPieceType>&, NextQueue);

/**
 * 7-Bag 랜덤마이저.
 *
 * Why UObject: ViewModel(Next 미리보기)이 델리게이트로 구독해야 하므로.
 * Why FRandomStream: UE 내장이며 시드 고정으로 결정성 보장 → 리플레이/테스트에 활용.
 */
UCLASS(BlueprintType)
class PROJECTTETRA_API UTetrisRandomizer : public UObject
{
	GENERATED_BODY()

public:
	UTetrisRandomizer();

	/** 시드 지정 초기화. Seed=0이면 시간 기반 자동 생성. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Randomizer")
	void Initialize(int64 InSeed = 0);

	/** 동일 시드로 재초기화 (게임 재시작). */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Randomizer")
	void Reset();

	/** 다음 피스 1개 꺼내기 + 큐 보충. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Randomizer")
	EPieceType Dequeue();

	/** 현재 Next Queue 스냅샷 (읽기 전용). */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Randomizer")
	const TArray<EPieceType>& GetNextQueue() const { return NextQueue; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Randomizer")
	int32 NextQueueMinSize = 5;

	UPROPERTY(BlueprintAssignable, Category = "Tetris|Randomizer")
	FOnNextQueueChanged OnNextQueueChanged;

private:
	UPROPERTY()
	TArray<EPieceType> NextQueue;

	FRandomStream RandomStream;
	int64 CurrentSeed = 0;
	bool bInitialized = false;

	/** 한 Bag(7종)을 셔플해 NextQueue 끝에 추가. */
	void GenerateAndAppendBag();

	/** 큐가 최소 크기에 미달하면 Bag을 추가로 생성. */
	void RefillQueue();
};
