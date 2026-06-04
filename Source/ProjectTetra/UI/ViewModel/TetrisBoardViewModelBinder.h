// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/TetrisTypes.h"
#include "TetrisBoardViewModelBinder.generated.h"

class UTetrisBoardViewModel;
class UTetrisBoard;
class UTetrisGameCore;
class UMVVMViewModelCollectionObject;

/**
 * Board ViewModel 바인더 — Model(Board/GameCore) 델리게이트 → VM setter 연결과 컬렉션 등록/해제를 한 곳에 격리한다.
 *
 * Why 별도 UObject: VM을 순수 데이터 홀더로 유지하기 위해(viewmodel.md 핵심 프레이밍 1) Model 의존을 여기로 몰아넣는다.
 *   VM은 Model을 모르고, View는 Model/바인더를 모른다(Global View Model Collection으로 resolve). Session이 이 바인더를 소유한다.
 * HUD 바인더와 달리 Board/GameCore 델리게이트는 전부 non-dynamic이라 AddLambda만 쓰며 dynamic(AddDynamic/UFUNCTION) 경로가 없다.
 * 좌표 변환 책임: 바인더가 buffer 행(Y>=20)을 클립하고 board-space(Y-up, visible)로 VM에 적재한다(HANDOFF §7).
 * 단일 게임스레드 가정 — 락 불필요(기존 컨벤션 일치).
 */
UCLASS()
class PROJECTTETRA_API UTetrisBoardViewModelBinder : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * VM 확보(EnsureViewModel) → 초기 스냅샷(GetVisibleGrid 변환 + 활성/고스트) → Model 델리게이트 구독.
	 * 재진입 안전: 이미 바인드된 상태면 먼저 DetachModel한다(VM은 보존 — View 연결 유지).
	 */
	void Bind(UTetrisBoard* Board, UTetrisGameCore* Core);

	/** Model 구독 해제 + 컬렉션 제거 + VM 참조 해제(전체 teardown — Session Deinitialize용). */
	void Unbind();

	/**
	 * VM을 1회 생성하고 컬렉션에 등록(멱등). Session 초기화에서 위젯 construct 이전에 호출되어
	 * View가 "TetrisBoard"로 resolve할 대상을 미리 준비한다. 이미 있으면 no-op.
	 */
	void EnsureViewModel();

	/** 소유 VM 노출(테스트/Session용). 미바인드 시 null. */
	UTetrisBoardViewModel* GetViewModel() const { return ViewModel; }

	//~ UObject: 컬렉션 resolve를 위해 Outer(Session)의 월드를 노출 — 헤드리스(월드 부재) 시 null로 graceful no-op.
	virtual UWorld* GetWorld() const override;

private:
	//~ Model 상태 → VM 적재 (좌표 변환 포함).
	/** Board의 visible 그리드를 EPieceType 배열로 변환해 LockedGrid에 적재. */
	void RefreshLockedGrid();
	/** 활성/고스트 4칸을 board-space로 클립해 적재. 비활성 상태(Falling/Locking 외)면 비움. */
	void RefreshActive();

	/** Model 델리게이트 4종 해제 + 핸들 리셋 + BoundBoard/BoundCore null화. VM은 보존(재바인드 재진입용). */
	void DetachModel();

	/** MVVM 서브시스템(GameInstance) 부재 시 null 반환 → 컬렉션 등록은 graceful no-op(헤드리스 테스트 가드). */
	UMVVMViewModelCollectionObject* ResolveCollection() const;
	void RegisterToCollection();
	void UnregisterFromCollection();

	UPROPERTY(Transient)
	TObjectPtr<UTetrisBoardViewModel> ViewModel;

	// 구독 해제를 위해 바인드된 Model을 보관(Unbind에서 각 델리게이트 Remove에 필요).
	UPROPERTY(Transient)
	TObjectPtr<UTetrisBoard> BoundBoard;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisGameCore> BoundCore;

	// non-dynamic 4종 구독 핸들(Board 2 + GameCore 2).
	FDelegateHandle BoardChangedHandle;
	FDelegateHandle LinesClearedHandle;
	FDelegateHandle ActivePieceHandle;
	FDelegateHandle StateHandle;
};
