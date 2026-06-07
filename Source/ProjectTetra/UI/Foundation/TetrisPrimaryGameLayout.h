// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TetrisPrimaryGameLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * UI 레이어 식별자(구조적 식별자) — 4레이어 고정이라 Lyra식 GameplayTag 대신 enum으로 단순화한다
 * (태그 에셋/등록 보일러플레이트 회피, HANDOFF 설계노트 2). 확장 폭주 시 태그 전환 검토.
 */
UENUM(BlueprintType)
enum class EUILayer : uint8
{
	Menu,       // 메인 메뉴/설정 (후속 슬라이스)
	Game,       // 인게임 화면(보드+HUD 호스트)
	GameMenu,   // Pause 등 게임 위 오버레이
	Modal       // 다이얼로그/팝업 (후속 슬라이스)
};

/**
 * PrimaryGameLayout — 4개 화면 스택 레이어를 가진 UI 루트. PC가 생성해 viewport에 fill로 부착한다.
 *
 * Why 위젯 트리가 아니라 스택들의 집합: UI를 "활성/비활성 화면들의 스택"으로 모델링한다.
 *   레이어별로 화면을 push/pop하면 입력 라우팅·Back·오버레이가 CommonUI 규칙으로 자동 처리된다.
 * Why Model/VM/Session 미참조: 계층 격리(HANDOFF §3). 루트는 레이어 관리만 담당한다.
 */
UCLASS()
class PROJECTTETRA_API UTetrisPrimaryGameLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 지정 레이어 스택에 위젯 클래스를 push하고 생성된 인스턴스를 반환한다(실패 시 null). */
	UCommonActivatableWidget* PushWidgetToLayer(EUILayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** 위젯을 자기 레이어 스택에서 제거한다(Back 자동 pop의 보조 경로 — 토글로 닫을 때 사용). */
	void RemoveWidget(UCommonActivatableWidget& Widget);

protected:
	//~ BindWidget — WBP가 이름 일치로 제공(겹쳐 배치, 전부 fill).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameStack;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> GameMenuStack;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> ModalStack;

private:
	/** 레이어 → 스택 매핑(switch). 미배선 레이어는 null. */
	UCommonActivatableWidgetStack* GetLayerStack(EUILayer Layer) const;
};
