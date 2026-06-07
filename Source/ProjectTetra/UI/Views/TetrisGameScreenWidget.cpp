// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisGameScreenWidget.h"

UTetrisGameScreenWidget::UTetrisGameScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 게임 화면은 게임 입력 모드 — 블록 이동/회전 등 Enhanced Input이 활성이어야 한다.
	InputModeOverride = ECommonInputMode::Game;
}
