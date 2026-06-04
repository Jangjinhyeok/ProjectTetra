// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotification/FieldId.h"
#include "Core/TetrisTypes.h"
#include "TetrisBoardWidget.generated.h"

class UUniformGridPanel;
class UImage;
class UTetrisBoardViewModel;

/**
 * 보드 렌더 위젯 (View) — Board VM을 바인딩으로 소비해 10x20 셀 그리드를 그린다.
 *
 * Why View는 VM만 안다: Model(Board/GameCore)·Session·바인더를 직접 참조하지 않는다(HANDOFF §3).
 *   VM은 Global View Model Collection에서 "TetrisBoard" 키로 resolve하거나 SetViewModel로 외부 주입받는다.
 * Why 이벤트 주도: NativeTick/매 프레임 polling으로 다시 그리지 않는다. 갱신은 VM FieldNotify 구독으로만
 *   트리거되어 RepaintFromViewModel()을 호출한다(README Forbidden Patterns 준수).
 * Why dumb View: 좌표 변환(클립·고스트 드롭)은 바인더가 끝냈고, 위젯은 board-space→grid 행 플립
 *   (row = BoardVisibleHeight-1-Y)만 수행한다(HANDOFF §7).
 */
UCLASS()
class PROJECTTETRA_API UTetrisBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 외부 주입 경로(예: Session/테스트). Collection resolve와 양립 — null-safe. 구독을 재설정한다. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|Board")
	void SetViewModel(UTetrisBoardViewModel* InViewModel);

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** WBP가 제공하는 빈 UniformGridPanel(이름 CellGrid). 셀 UImage를 여기에 1회 생성·배치한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> CellGrid;

	/** 데이터 주도 셀 색상(생성자 기본값, WBP에서 오버라이드 가능). None은 빈 칸 배경색. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Board")
	TMap<EPieceType, FLinearColor> PieceColors;

	/** 고스트 셀 투명도(0~1). 활성 피스 색을 이 알파로 흐리게 그린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Board", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostAlpha = 0.25f;

private:
	/** Collection에서 "TetrisBoard" VM을 resolve(월드/서브시스템 부재 시 null). */
	UTetrisBoardViewModel* ResolveViewModel() const;

	/** CellGrid에 BoardWidth*BoardVisibleHeight개 UImage 셀을 1회 생성·배치(row=top0). */
	void BuildCellGrid();

	/** VM FieldNotify 5종 구독(핸들 보관). */
	void SubscribeToViewModel();
	/** 구독 전부 해제. */
	void UnsubscribeFromViewModel();

	/** FieldNotify 콜백 — 어떤 필드가 바뀌든 전체 repaint(MVP: 부분 갱신 미사용). */
	void HandleFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

	/** VM 상태로 셀 색을 다시 칠한다 — 레이어 합성(고정 그리드 → 고스트 → 활성). */
	void RepaintFromViewModel();

	/** 타입별 색상 조회(미등록 시 투명). */
	FLinearColor GetPieceColor(EPieceType Type) const;

	UPROPERTY(Transient)
	TObjectPtr<UTetrisBoardViewModel> ViewModel;

	// index = row*Width + col (row=top0). BuildCellGrid에서 채우고 repaint에서 색만 갱신.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CellImages;
};
