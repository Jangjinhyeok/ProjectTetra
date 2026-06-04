// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Core/TetrisTypes.h"
#include "TetrisBoardViewModel.generated.h"

/**
 * 보드 렌더 전용 ViewModel (MVVM + FieldNotify).
 *
 * Why 순수 데이터 홀더: HUD VM과 동일하게 Model/Session/위젯을 컴파일 의존하지 않는다
 *   (include는 TetrisTypes + MVVM 베이스뿐). Model 델리게이트 구독은 바인더(UTetrisBoardViewModelBinder)의
 *   책임이며, VM은 FieldNotify 필드 + setter만 노출한다 → 위젯/월드 없이 단위 테스트 가능.
 * Why 빈도 분리: 200칸 고정 그리드(LockedGrid)는 lock/clear 시에만, 4칸 활성/고스트는 매 스텝 통지된다.
 *   매 스텝 통지되는 건 소형 배열뿐이라 "200칸 매 프레임 갱신" 비용을 구조적으로 피한다(HANDOFF 빈도 분리).
 * 좌표계: 보드-space(좌하단 (0,0), Y-up, visible). 위젯이 렌더 시 row=BoardVisibleHeight-1-Y 플립을 담당한다.
 */
UCLASS(BlueprintType)
class PROJECTTETRA_API UTetrisBoardViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	//~ Getters — FieldNotify Getter 바인딩 대상. View는 이 값을 읽기만 한다.
	const TArray<EPieceType>& GetLockedGrid() const { return LockedGrid; }
	EPieceType GetActivePieceType() const { return ActivePieceType; }
	const TArray<FIntPoint>& GetActiveCells() const { return ActiveCells; }
	const TArray<FIntPoint>& GetGhostCells() const { return GhostCells; }
	EGameState GetGameState() const { return GameState; }

	/** 컬렉션 등록 키. View 바인딩(WBP_Board)의 VM 컨텍스트명과 일치해야 resolve 성공. */
	static const FName ContextName;

private:
	// 바인더만 상태를 쓴다 — 갱신 경로 격리(production).
	friend class UTetrisBoardViewModelBinder;
#if WITH_DEV_AUTOMATION_TESTS
	// 테스트 전용 seam: 단위 테스트가 private setter를 격리 구동하기 위함. 셰이핑 빌드엔 미존재(캡슐화 유지).
	friend struct FTetrisBoardViewModelTestAccess;
#endif

	void SetLockedGrid(const TArray<EPieceType>& NewValue);
	void SetActivePieceType(EPieceType NewValue);
	void SetActiveCells(const TArray<FIntPoint>& NewValue);
	void SetGhostCells(const TArray<FIntPoint>& NewValue);
	void SetGameState(EGameState NewValue);

	// 고정(lock된) 그리드. visible 200칸, row-major index=Y*Width+X. lock/clear 시에만 통지.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|Board", meta = (AllowPrivateAccess = "true"))
	TArray<EPieceType> LockedGrid;

	// 활성 피스 종류. None이면 비활성(스폰 전/게임오버 등) → 위젯이 활성/고스트 미렌더.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|Board", meta = (AllowPrivateAccess = "true"))
	EPieceType ActivePieceType = EPieceType::None;

	// 활성 피스 4칸의 board-space 좌표(가시 영역으로 클립됨). 매 이동/회전/중력 스텝 통지.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|Board", meta = (AllowPrivateAccess = "true"))
	TArray<FIntPoint> ActiveCells;

	// 고스트(착지 예측) 4칸의 board-space 좌표(가시 영역으로 클립됨). 활성과 동일 빈도 통지.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|Board", meta = (AllowPrivateAccess = "true"))
	TArray<FIntPoint> GhostCells;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|Board", meta = (AllowPrivateAccess = "true"))
	EGameState GameState = EGameState::Idle;
};
