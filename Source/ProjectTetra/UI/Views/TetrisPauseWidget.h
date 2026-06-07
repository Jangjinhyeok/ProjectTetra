// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/TetrisActivatableWidget.h"
#include "TetrisPauseWidget.generated.h"

class UCommonButtonBase;

/**
 * Pause 오버레이 (View) — Resume/Restart/Quit 버튼 메뉴. Menu 입력 모드 + Back 핸들러.
 *
 * Why 델리게이트로 명령만 발행: 위젯은 Session/Model을 모른다(HANDOFF §3). 버튼 클릭을
 *   네이티브 델리게이트로 외부(PC)에 위임하고, 실제 SetPaused(false)/RestartGame/quit은 PC가 수행한다.
 *   (MVVM "View는 명령 발행, 실행은 컨트롤러" 원칙.)
 * Why Back=Resume: bIsBackHandler=true(생성자)로 Back 액션을 받아 NativeOnHandleBackAction에서
 *   Resume을 발행한 뒤 true를 반환 — Back이 상위 게임 화면까지 닫는 것을 막는다.
 */
UCLASS()
class PROJECTTETRA_API UTetrisPauseWidget : public UTetrisActivatableWidget
{
	GENERATED_BODY()

public:
	UTetrisPauseWidget(const FObjectInitializer& ObjectInitializer);

	/** 버튼/Back이 발행하는 명령 — PC가 구독해 Session을 제어한다(파라미터 없음). */
	FSimpleMulticastDelegate OnResumeRequested;
	FSimpleMulticastDelegate OnRestartRequested;
	FSimpleMulticastDelegate OnQuitRequested;

protected:
	//~ UUserWidget
	virtual void NativeOnInitialized() override;

	//~ UCommonActivatableWidget — Back 액션 처리(Back=Resume).
	virtual bool NativeOnHandleBackAction() override;

	//~ BindWidget — WBP가 이름 일치로 제공.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> ResumeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> RestartButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> QuitButton;

private:
	/** CommonButton OnClicked(네이티브 이벤트) → 대응 명령 델리게이트로 중계. */
	void HandleResumeClicked();
	void HandleRestartClicked();
	void HandleQuitClicked();
};
