// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Core/TetrisTypes.h"
#include "TetrisHUDViewModel.generated.h"

/**
 * HUD 전용 ViewModel (MVVM + FieldNotify).
 *
 * Why 순수 데이터 홀더: Model/Session/위젯을 컴파일 의존하지 않는다(include는 TetrisTypes + MVVM 베이스뿐).
 *   Model 델리게이트 구독은 바인더(UTetrisHUDViewModelBinder)의 책임이며, VM은 FieldNotify 필드 + setter만 노출한다.
 *   이 격리 덕에 위젯/월드 없이 단위 테스트가 가능하다(viewmodel.md 핵심 프레이밍 1).
 * Why 비-Blueprint setter + friend: UI가 임의로 상태를 쓰지 못하도록 차단 — 갱신 경로를 바인더 한 곳으로 격리한다
 *   (VERSION.md 5.7 friend class 규약). setter는 값이 달라질 때만 FieldNotify를 발행한다(V1 no-op 억제).
 */
UCLASS(BlueprintType)
class PROJECTTETRA_API UTetrisHUDViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	//~ Getters — FieldNotify Getter 바인딩 대상(네이티브 getter). View는 이 값을 읽기만 한다.
	int64 GetScore() const { return Score; }
	int32 GetLevel() const { return Level; }
	int32 GetLines() const { return Lines; }
	int32 GetCombo() const { return Combo; }
	int32 GetB2BCount() const { return B2BCount; }
	EPieceType GetHoldPiece() const { return HoldPiece; }
	bool GetCanHold() const { return bCanHold; }
	const TArray<EPieceType>& GetNextQueue() const { return NextQueue; }
	EGameState GetGameState() const { return GameState; }

	/**
	 * 계산 프로퍼티(FieldNotify computed 시연): Score를 로케일 천단위 포맷 텍스트로 변환.
	 * Score setter가 값 변경 시 이 함수의 파생 통지를 함께 발행한다(데이터+계산 프로퍼티 공존 패턴).
	 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Tetris|HUD")
	FText GetScoreText() const;

	/** 컬렉션 등록 키(viewmodel.md Tuning Knob). 바인더가 사용하며, View 바인딩과 일치해야 resolve 성공. */
	static const FName ContextName;

private:
	// 바인더만 상태를 쓴다 — 갱신 경로 격리(production).
	friend class UTetrisHUDViewModelBinder;
#if WITH_DEV_AUTOMATION_TESTS
	// 테스트 전용 seam: 단위 테스트가 private setter를 격리 구동하기 위함. 셰이핑 빌드엔 미존재(캡슐화 유지).
	friend struct FTetrisHUDViewModelTestAccess;
#endif

	void SetScore(int64 NewValue);
	void SetLevel(int32 NewValue);
	void SetLines(int32 NewValue);
	void SetCombo(int32 NewValue);
	void SetB2BCount(int32 NewValue);
	void SetHoldPiece(EPieceType NewValue);
	void SetCanHold(bool bNewValue);
	void SetNextQueue(const TArray<EPieceType>& NewValue);
	void SetGameState(EGameState NewValue);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	int64 Score = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	int32 Lines = 0;

	// -1 = 콤보 비활성. VM은 raw 값을 그대로 노출하고 의미 부여는 View 몫(viewmodel.md Edge 5).
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	int32 Combo = -1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	int32 B2BCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	EPieceType HoldPiece = EPieceType::None;

	// bool은 자동 getter 이름이 모호하므로 명시(GetCanHold). 나머지 필드는 Get<Name> 자동 추론.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = "GetCanHold", Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	bool bCanHold = true;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	TArray<EPieceType> NextQueue;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true"))
	EGameState GameState = EGameState::Idle;

	/** HUD가 노출할 Next 미리보기 개수 — SetNextQueue가 이 개수로 잘라 저장(viewmodel.md Tuning Knob, 기본 5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|HUD", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "5"))
	int32 NextQueueDisplayCount = 5;
};
