// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisPauseWidget.h"
#include "CommonButtonBase.h"

UTetrisPauseWidget::UTetrisPauseWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 메뉴는 UI 입력 모드(활성 시 게임 입력 자동 억제) + 마우스 캡처 없음. 기본값을 코드로 박아 안전화.
	InputModeOverride = ECommonInputMode::Menu;
	MouseCapture = EMouseCaptureMode::NoCapture;
	// Back 액션을 받아 NativeOnHandleBackAction(Resume)으로 처리한다.
	bIsBackHandler = true;
}

void UTetrisPauseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼 클릭을 명령 델리게이트로 중계. 널 가드 — WBP BindWidget 누락 대비.
	if (ResumeButton)
	{
		ResumeButton->OnClicked().AddUObject(this, &UTetrisPauseWidget::HandleResumeClicked);
	}
	if (RestartButton)
	{
		RestartButton->OnClicked().AddUObject(this, &UTetrisPauseWidget::HandleRestartClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked().AddUObject(this, &UTetrisPauseWidget::HandleQuitClicked);
	}
}

bool UTetrisPauseWidget::NativeOnHandleBackAction()
{
	// Back=Resume: 메뉴 닫기 의미로 Resume을 발행하고 true를 반환해 Back 전파를 멈춘다
	//   (false면 상위 활성 화면=게임까지 Back이 전파될 수 있음).
	OnResumeRequested.Broadcast();
	return true;
}

void UTetrisPauseWidget::HandleResumeClicked()
{
	OnResumeRequested.Broadcast();
}

void UTetrisPauseWidget::HandleRestartClicked()
{
	OnRestartRequested.Broadcast();
}

void UTetrisPauseWidget::HandleQuitClicked()
{
	OnQuitRequested.Broadcast();
}
