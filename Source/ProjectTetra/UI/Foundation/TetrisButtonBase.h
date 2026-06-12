// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "TetrisButtonBase.generated.h"

class UWidgetAnimation;

/**
 * 프로젝트 공통 버튼 베이스 — hover(또는 게임패드 focus) 시 zoom 연출.
 *
 * Why C++ 베이스 + Widget Animation: 연출의 보간(RenderTransform Scale)은 Slate가 도는
 *   Widget Animation이 담당하고(C++ Tick 아님), C++는 hover in/out 엣지에서 forward/reverse
 *   재생만 트리거한다. 생명주기(NativeOnHovered/Unhovered)는 C++로, 비주얼은 WBP author로
 *   분리 — 한 곳(공통 버튼)만 reparent하면 이를 쓰는 모든 버튼에 일괄 적용된다.
 * Why BindWidgetAnimOptional: WBP가 같은 이름("HoverZoom")의 애니메이션을 author하면 자동
 *   바인딩, 없으면 null로 안전(연출 없이 기능만 유지).
 */
UCLASS()
class PROJECTTETRA_API UTetrisButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

protected:
	//~ UCommonButtonBase — hover 진입: zoom in.
	virtual void NativeOnHovered() override;

	//~ UCommonButtonBase — hover 이탈: zoom 원복.
	virtual void NativeOnUnhovered() override;

	//~ WBP의 동명 애니메이션 자동 바인딩(없으면 null). Transient — 직렬화 대상 아님.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HoverZoom;
};
