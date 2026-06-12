// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisMainMenuWidget.h"
#include "CommonButtonBase.h"

UTetrisMainMenuWidget::UTetrisMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 메뉴는 UI 입력 모드(활성 시 게임 입력 자동 억제) + 마우스 캡처 없음. 기본값을 코드로 박아 안전화.
	InputModeOverride = ECommonInputMode::Menu;
	MouseCapture = EMouseCaptureMode::NoCapture;
	// 메인 메뉴는 Back으로 닫지 않는다(최하위 화면) — bIsBackHandler는 기본 false 유지.
}

void UTetrisMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼 클릭을 명령 델리게이트로 중계. 널 가드 — WBP BindWidget 누락 대비.
	if (StartButton)
	{
		StartButton->OnClicked().AddUObject(this, &UTetrisMainMenuWidget::HandleStartClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked().AddUObject(this, &UTetrisMainMenuWidget::HandleQuitClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().AddUObject(this, &UTetrisMainMenuWidget::HandleSettingsClicked);
	}
}

void UTetrisMainMenuWidget::HandleStartClicked()
{
	OnStartRequested.Broadcast();
}

void UTetrisMainMenuWidget::HandleQuitClicked()
{
	OnQuitRequested.Broadcast();
}

void UTetrisMainMenuWidget::HandleSettingsClicked()
{
	OnSettingsRequested.Broadcast();
}
