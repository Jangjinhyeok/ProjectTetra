// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/TetrisActivatableWidget.h"
#include "TetrisGameOverWidget.generated.h"

class UCommonButtonBase;
class UTextBlock;
class UTetrisHUDViewModel;

/**
 * GameOver 결과 화면 (View) — 최종 점수/레벨/줄 표시 + Retry/Menu 버튼. Modal 레이어, Menu 입력 모드.
 *
 * Why VM을 참조하는가(메뉴 위젯과 다른 점): GameOver는 "명령 메뉴"가 아니라 "결과를 보여주는 View"다.
 *   HUD 위젯과 동일하게 HUD VM을 Collection(ContextName)에서 self-resolve해 최종값을 읽는다(HANDOFF §3).
 *   단 Model/Session은 여전히 미참조 — 표시는 VM, 명령은 델리게이트로 위임(PC가 실행).
 * Why NativeOnActivated에서 1회 read: GameOver 시점의 점수는 더 이상 변하지 않는 정적 최종값이라
 *   FieldNotify 구독이 불필요하다(이벤트 주도 §4 — 활성화 엣지에 1회만 read).
 * Why Back 핸들러 아님: 죽은 게임으로 Back 복귀를 막는다 — 종료는 Retry/Menu 버튼으로만(bIsBackHandler 기본 false).
 */
UCLASS()
class PROJECTTETRA_API UTetrisGameOverWidget : public UTetrisActivatableWidget
{
	GENERATED_BODY()

public:
	UTetrisGameOverWidget(const FObjectInitializer& ObjectInitializer);

	/** 버튼이 발행하는 명령 — PC가 구독해 Session을 제어한다(파라미터 없음). */
	FSimpleMulticastDelegate OnRetryRequested;
	FSimpleMulticastDelegate OnMenuRequested;

protected:
	//~ UUserWidget
	virtual void NativeOnInitialized() override;

	//~ UCommonActivatableWidget — 활성화 시 최종 결과를 VM에서 1회 read해 표시.
	virtual void NativeOnActivated() override;

	//~ BindWidgetOptional — WBP가 일부만 둬도 안전(결과 텍스트는 선택 표시).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LinesText;

	//~ BindWidget — 버튼은 필수(WBP가 이름 일치로 제공).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> RetryButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> MenuButton;

private:
	/** Collection에서 "TetrisHUD" VM을 resolve(월드/서브시스템 부재 시 null). HUD 위젯과 동일 경로(설계노트 1: 소폭 중복). */
	UTetrisHUDViewModel* ResolveViewModel() const;

	/** CommonButton OnClicked(네이티브 이벤트) → 대응 명령 델리게이트로 중계. */
	void HandleRetryClicked();
	void HandleMenuClicked();
};
