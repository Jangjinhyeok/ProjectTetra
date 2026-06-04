// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/TetrisTypes.h"
#include "TetrisHUDViewModelBinder.generated.h"

class UTetrisHUDViewModel;
class UTetrisGameCore;
class UTetrisScoring;
class UTetrisRandomizer;
class UMVVMViewModelCollectionObject;

/**
 * HUD ViewModel 바인더 — Model 델리게이트 → VM setter 연결과 컬렉션 등록/해제를 한 곳에 격리한다.
 *
 * Why 별도 UObject: VM을 순수 데이터 홀더로 유지하기 위해(viewmodel.md 핵심 프레이밍 1) Model 의존을 여기로 몰아넣는다.
 *   VM은 Model을 모르고, View는 Model/바인더를 모른다(Global View Model Collection으로 resolve). Session이 이 바인더를 소유한다.
 * 단일 게임스레드 가정 — 락 불필요(기존 컨벤션 일치).
 */
UCLASS()
class PROJECTTETRA_API UTetrisHUDViewModelBinder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * VM 확보(EnsureViewModel) → 초기 스냅샷(getter pull, V2) → Model 델리게이트 구독.
	 * 재진입 안전: 이미 바인드된 상태면 먼저 DetachModel한다(VM은 보존 — View 연결 유지).
	 */
	void Bind(UTetrisGameCore* Core, UTetrisScoring* Scoring, UTetrisRandomizer* Rand);

	/** Model 구독 해제(핸들/dynamic) + 컬렉션 제거 + VM 참조 해제(전체 teardown — Session Deinitialize용). */
	void Unbind();

	/**
	 * VM을 1회 생성하고 컬렉션에 등록(멱등). Session 초기화에서 위젯 construct 이전에 호출되어
	 * View가 "TetrisHUD"로 resolve할 대상을 미리 준비한다. 이미 있으면 no-op.
	 */
	void EnsureViewModel();

	/** 소유 VM 노출(테스트/Session용). 미바인드 시 null. */
	UTetrisHUDViewModel* GetViewModel() const { return ViewModel; }

	//~ UObject: 컬렉션 resolve를 위해 Outer(Session)의 월드를 노출 — 헤드리스(월드 부재) 시 null로 graceful no-op.
	virtual UWorld* GetWorld() const override;

private:
	// Randomizer만 dynamic 델리게이트(BlueprintAssignable) → AddDynamic + UFUNCTION 핸들러. 나머지는 non-dynamic 람다.
	UFUNCTION()
	void HandleNextQueueChanged(const TArray<EPieceType>& NextQueue);

	/** Model 델리게이트(non-dynamic 7 + dynamic 1) 해제 + 핸들 리셋 + Bound* null화. VM은 보존(재바인드 재진입용). */
	void DetachModel();

	/** MVVM 서브시스템(GameInstance) 부재 시 null 반환 → 컬렉션 등록은 graceful no-op(헤드리스 테스트 가드). */
	UMVVMViewModelCollectionObject* ResolveCollection() const;
	void RegisterToCollection();
	void UnregisterFromCollection();

	UPROPERTY(Transient)
	TObjectPtr<UTetrisHUDViewModel> ViewModel;

	// 구독 해제를 위해 바인드된 Model을 보관(Unbind에서 각 델리게이트 Remove에 필요).
	UPROPERTY(Transient)
	TObjectPtr<UTetrisGameCore> BoundCore;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisScoring> BoundScoring;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisRandomizer> BoundRandomizer;

	// non-dynamic 7종 구독 핸들(Scoring 5 + GameCore State/Hold 2).
	FDelegateHandle ScoreHandle;
	FDelegateHandle LevelHandle;
	FDelegateHandle LinesHandle;
	FDelegateHandle ComboHandle;
	FDelegateHandle B2BHandle;
	FDelegateHandle StateHandle;
	FDelegateHandle HoldHandle;
};
