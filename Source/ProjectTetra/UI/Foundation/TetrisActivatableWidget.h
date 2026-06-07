// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Engine/EngineBaseTypes.h"   // EMouseCaptureMode
#include "CommonInputModeTypes.h"     // ECommonInputMode
#include "TetrisActivatableWidget.generated.h"

/**
 * 프로젝트 공통 화면 베이스 (View) — 화면 한 장을 "활성/비활성"으로 모델링하는 CommonUI 베이스.
 *
 * Why 입력 모드를 데이터로 선언: PC가 SetInputMode를 수동 토글하지 않고, 각 화면이
 *   GetDesiredInputConfig로 자신이 원하는 입력 모드(Game/Menu)를 선언하면 CommonUI 스택이
 *   활성 화면에 맞춰 자동 중재한다(CommonGameViewportClient 경유). 게임 화면=Game, 메뉴=Menu.
 * Why Back 핸들러는 별도 프로퍼티를 두지 않음: 기반 UCommonActivatableWidget이 이미
 *   bIsBackHandler UPROPERTY를 제공하므로 WBP Class Defaults에서 직접 켠다(중복 선언 시 UHT 충돌).
 * Why Model/VM/Session 미참조: 계층 격리(HANDOFF §3). 이 베이스는 순수 화면 프레임이다.
 */
UCLASS()
class PROJECTTETRA_API UTetrisActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	//~ UCommonActivatableWidget — 이 화면이 활성일 때 적용할 입력 설정을 데이터로 구성해 반환.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	/** 이 화면 활성 시 입력 모드. 게임 화면=Game(게임 입력 활성), 메뉴=Menu(UI만 입력). */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|UI")
	ECommonInputMode InputModeOverride = ECommonInputMode::Menu;

	/** 이 화면 활성 시 마우스 캡처 모드. 메뉴는 캡처 불필요(NoCapture). */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|UI")
	EMouseCaptureMode MouseCapture = EMouseCaptureMode::NoCapture;
};
