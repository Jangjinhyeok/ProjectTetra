// Copyright ProjectTetra. All Rights Reserved.

#include "UI/ViewModel/TetrisBoardViewModel.h"

// 컬렉션 등록 키. View 바인딩(WBP_Board)의 VM 컨텍스트명과 일치해야 resolve 성공(#12 G6).
const FName UTetrisBoardViewModel::ContextName(TEXT("TetrisBoard"));

void UTetrisBoardViewModel::SetLockedGrid(const TArray<EPieceType>& NewValue)
{
	// TArray operator==는 요소 비교 → 동일 그리드 재설정 시 FieldNotify 미발행(V1 no-op 게이트).
	UE_MVVM_SET_PROPERTY_VALUE(LockedGrid, NewValue);
}

void UTetrisBoardViewModel::SetActivePieceType(EPieceType NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActivePieceType, NewValue);
}

void UTetrisBoardViewModel::SetActiveCells(const TArray<FIntPoint>& NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ActiveCells, NewValue);
}

void UTetrisBoardViewModel::SetGhostCells(const TArray<FIntPoint>& NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(GhostCells, NewValue);
}

void UTetrisBoardViewModel::SetGameState(EGameState NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(GameState, NewValue);
}
