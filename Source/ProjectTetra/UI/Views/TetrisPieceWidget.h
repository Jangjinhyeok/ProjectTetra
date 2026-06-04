// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TetrisTypes.h"
#include "TetrisPieceWidget.generated.h"

class UUniformGridPanel;
class UImage;

/**
 * 미니 테트로미노 미리보기 위젯 (재사용 컴포넌트) — Next 큐/Hold 슬롯에서 한 피스를 작은 그리드로 그린다.
 *
 * Why dumb 컴포넌트: VM/Model을 모른다. 부모(HUD View)가 SetPiece(Type)로만 구동한다(단방향).
 * Why 셀 그리드 재사용: 보드와 동일한 데이터 주도 색상(PieceColors)으로 일관 표현 — 아트 에셋 불필요.
 * 미리보기 모양은 View-레이어 정적 룩업(GetPreviewCells)으로 둔다 — Model의 SRS 회전 데이터와
 *   분리(표시 전용). 캐논 모양 중복은 의도된 트레이드오프(HANDOFF 설계 노트 1).
 */
UCLASS()
class PROJECTTETRA_API UTetrisPieceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 피스 타입 설정. None이면 전체 빈칸(예: Hold 미보유). 값이 같으면 무동작. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|HUD")
	void SetPiece(EPieceType Type);

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;

	/** WBP가 제공하는 GridSize x GridSize UniformGridPanel(이름 PieceGrid). 셀 UImage를 1회 생성·배치. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> PieceGrid;

	/** 데이터 주도 셀 색상(생성자 기본값, WBP 오버라이드 가능). 빈칸은 투명으로 그린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|HUD")
	TMap<EPieceType, FLinearColor> PieceColors;

private:
	/** PieceGrid에 GridSize*GridSize개 UImage 셀을 1회 생성·배치(index = Row*GridSize + Col). */
	void BuildGrid();
	/** CurrentPiece의 모양 셀만 색칠, 나머지는 투명. */
	void Repaint();
	/** 타입별 색상 조회(미등록 시 투명). */
	FLinearColor GetPieceColor(EPieceType Type) const;

	/** 미리보기 표시 셀(4x4-local, X=col, Y=row). View 전용 캐논 모양(Model 회전 데이터 비참조). */
	static TArray<FIntPoint> GetPreviewCells(EPieceType Type);

	static constexpr int32 GridSize = 4;

	EPieceType CurrentPiece = EPieceType::None;

	// index = Row*GridSize + Col. BuildGrid에서 채우고 Repaint에서 색만 갱신.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CellImages;
};
