// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisButtonBase.h"

void UTetrisButtonBase::NativeOnHovered()
{
	Super::NativeOnHovered();

	// hover 진입 엣지에서 zoom in. 애니메이션 미author 시 null → no-op.
	if (HoverZoom)
	{
		PlayAnimationForward(HoverZoom);
	}
}

void UTetrisButtonBase::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	// hover 이탈 엣지에서 역재생으로 원복(현재 진행분에서 reverse — 중간 이탈도 자연스럽게).
	if (HoverZoom)
	{
		PlayAnimationReverse(HoverZoom);
	}
}
