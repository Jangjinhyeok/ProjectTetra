// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"   // FUIInputConfig

TOptional<FUIInputConfig> UTetrisActivatableWidget::GetDesiredInputConfig() const
{
	// Why 데이터 주도: 입력 모드/마우스 캡처를 Class Defaults(에디터)에서 지정한 값으로 구성한다.
	//   게임 화면=Game이면 게임 입력 활성, 메뉴=Menu면 활성 화면 규칙에 따라 게임 입력이 자동 억제된다.
	return FUIInputConfig(InputModeOverride, MouseCapture);
}
