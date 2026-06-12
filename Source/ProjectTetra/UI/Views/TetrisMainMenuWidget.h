// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/TetrisActivatableWidget.h"
#include "TetrisMainMenuWidget.generated.h"

class UCommonButtonBase;

/**
 * 메인 메뉴 화면 (View) — Start/Quit 버튼 메뉴. Menu 입력 모드, Back 핸들러 아님.
 *
 * Why 델리게이트로 명령만 발행: 위젯은 Session/Model을 모른다(HANDOFF §3). 버튼 클릭을
 *   네이티브 델리게이트로 외부(PC)에 위임하고, 실제 StartGame/quit은 PC가 수행한다.
 *   (MVVM "View는 명령 발행, 실행은 컨트롤러" 원칙 — PauseWidget과 동일 패턴.)
 * Why Back 핸들러 아님: 메인 메뉴는 스택 최하위 화면이라 Back으로 돌아갈 곳이 없다.
 *   앱 종료는 Back이 아니라 Quit 버튼으로만 명시한다(bIsBackHandler 기본 false 유지).
 * Why Menu 레이어가 Game 위를 가린다: 메뉴 활성 동안 게임은 Idle로 뒤에 대기하고,
 *   Menu 입력 모드가 게임 입력을 자동 억제한다(CommonGameViewportClient 중재).
 */
UCLASS()
class PROJECTTETRA_API UTetrisMainMenuWidget : public UTetrisActivatableWidget
{
	GENERATED_BODY()

public:
	UTetrisMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** 버튼이 발행하는 명령 — PC가 구독해 Session/앱 종료/화면 전환을 제어한다(파라미터 없음). */
	FSimpleMulticastDelegate OnStartRequested;
	FSimpleMulticastDelegate OnQuitRequested;
	FSimpleMulticastDelegate OnSettingsRequested;

protected:
	//~ UUserWidget
	virtual void NativeOnInitialized() override;

	//~ BindWidget — WBP가 이름 일치로 제공.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> StartButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> QuitButton;

	//~ BindWidgetOptional — 기존 author된 WBP_MainMenu를 깨지 않도록 선택 바인딩. G5에서 버튼 추가 시 자동 연결.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> SettingsButton;

private:
	/** CommonButton OnClicked(네이티브 이벤트) → 대응 명령 델리게이트로 중계. */
	void HandleStartClicked();
	void HandleQuitClicked();
	void HandleSettingsClicked();
};
