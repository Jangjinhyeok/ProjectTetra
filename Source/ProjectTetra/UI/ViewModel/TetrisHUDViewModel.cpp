// Copyright ProjectTetra. All Rights Reserved.

#include "UI/ViewModel/TetrisHUDViewModel.h"

// 컬렉션 등록 키. View 바인딩(WBP_HUD)의 VM 컨텍스트명과 일치해야 resolve 성공(#13).
const FName UTetrisHUDViewModel::ContextName(TEXT("TetrisHUD"));

void UTetrisHUDViewModel::SetScore(int64 NewValue)
{
	// 값이 바뀐 경우에만 계산 프로퍼티(GetScoreText)도 파생 통지 — 동일값 재설정 시 불필요한 텍스트 갱신 방지.
	if (UE_MVVM_SET_PROPERTY_VALUE(Score, NewValue))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetScoreText);
	}
}

void UTetrisHUDViewModel::SetLevel(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(Level, NewValue);
}

void UTetrisHUDViewModel::SetLines(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(Lines, NewValue);
}

void UTetrisHUDViewModel::SetCombo(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(Combo, NewValue);
}

void UTetrisHUDViewModel::SetB2BCount(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(B2BCount, NewValue);
}

void UTetrisHUDViewModel::SetHoldPiece(EPieceType NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(HoldPiece, NewValue);
}

void UTetrisHUDViewModel::SetCanHold(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanHold, bNewValue);
}

void UTetrisHUDViewModel::SetNextQueue(const TArray<EPieceType>& NewValue)
{
	// HUD 표시 개수로 잘라 저장 — Randomizer 큐가 더 길어도 미리보기 칸 수만큼만 노출/통지(Tuning Knob).
	// TArray operator==는 요소 비교이므로 동일 시퀀스 재설정 시 FieldNotify 미발행(V1).
	const int32 Count = FMath::Min(NewValue.Num(), NextQueueDisplayCount);
	TArray<EPieceType> Truncated(NewValue.GetData(), Count);
	UE_MVVM_SET_PROPERTY_VALUE(NextQueue, Truncated);
}

void UTetrisHUDViewModel::SetGameState(EGameState NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(GameState, NewValue);
}

FText UTetrisHUDViewModel::GetScoreText() const
{
	// 로케일 기본 천단위 구분 포맷. 표시 변환은 VM이 담당하고 View는 텍스트만 소비.
	return FText::AsNumber(Score);
}
