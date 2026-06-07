// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/TetrisActivatableWidget.h"
#include "TetrisGameScreenWidget.generated.h"

/**
 * 인게임 화면 호스트 (View) — 보드+HUD를 품는 컨테이너 화면. 게임 입력 모드를 선언하는 지점.
 *
 * Why 별도 클래스: GameStack(CommonActivatableWidgetStack)은 UCommonActivatableWidget만 host 한다.
 *   기존 WBP_GameLayout(plain UUserWidget)은 못 넣으므로 activatable 베이스가 필요하다. 로직은 없고,
 *   디자이너에서 이 위젯 안에 기존 WBP_Board/WBP_HUD를 그대로 배치한다(VM self-resolve 경로 유지).
 * Why InputModeOverride=Game(생성자): 게임 중엔 게임 입력이 활성이어야 한다. 기본값을 코드로 박아
 *   에디터 Class Defaults 누락에도 안전하게 한다(WBP에서 재정의 가능).
 */
UCLASS()
class PROJECTTETRA_API UTetrisGameScreenWidget : public UTetrisActivatableWidget
{
	GENERATED_BODY()

public:
	UTetrisGameScreenWidget(const FObjectInitializer& ObjectInitializer);
};
