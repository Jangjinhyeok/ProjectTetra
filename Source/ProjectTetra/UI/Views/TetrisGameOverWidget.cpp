// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisGameOverWidget.h"
#include "UI/ViewModel/TetrisHUDViewModel.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UTetrisGameOverWidget::UTetrisGameOverWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 결과 화면도 UI 입력 모드(게임 입력 자동 억제) + 마우스 캡처 없음. 기본값을 코드로 박아 안전화.
	InputModeOverride = ECommonInputMode::Menu;
	MouseCapture = EMouseCaptureMode::NoCapture;
	// 죽은 게임으로 Back 복귀를 막는다 — bIsBackHandler는 기본 false 유지(종료는 Retry/Menu로만).
}

void UTetrisGameOverWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼 클릭을 명령 델리게이트로 중계. 널 가드 — WBP BindWidget 누락 대비.
	if (RetryButton)
	{
		RetryButton->OnClicked().AddUObject(this, &UTetrisGameOverWidget::HandleRetryClicked);
	}
	if (MenuButton)
	{
		MenuButton->OnClicked().AddUObject(this, &UTetrisGameOverWidget::HandleMenuClicked);
	}
}

void UTetrisGameOverWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// GameOver 시점의 최종값은 정적 — 활성화 엣지에 1회만 read해 표시(구독 불필요, §4 이벤트 주도).
	// VM 미연결(월드/서브시스템 부재)이면 no-op — 텍스트는 WBP 디폴트 유지.
	const UTetrisHUDViewModel* ViewModel = ResolveViewModel();
	if (!ViewModel)
	{
		return;
	}

	// BindWidgetOptional — WBP가 일부만 배치해도 안전(개별 널 가드).
	if (ScoreText) { ScoreText->SetText(FText::AsNumber(ViewModel->GetScore())); }
	if (LevelText) { LevelText->SetText(FText::AsNumber(ViewModel->GetLevel())); }
	if (LinesText) { LinesText->SetText(FText::AsNumber(ViewModel->GetLines())); }
}

UTetrisHUDViewModel* UTetrisGameOverWidget::ResolveViewModel() const
{
	// 바인더 등록 경로의 역방향: World→GameInstance→MVVM 서브시스템→Collection→"TetrisHUD" 조회.
	// (HUD 위젯과 동일 경로 — 설계노트 1: 후속으로 공통 헬퍼 통합 검토 대상.)
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMVVMGameSubsystem* Subsystem = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
			{
				if (UMVVMViewModelCollectionObject* Collection = Subsystem->GetViewModelCollection())
				{
					FMVVMViewModelContext Context;
					Context.ContextClass = UTetrisHUDViewModel::StaticClass();
					Context.ContextName = UTetrisHUDViewModel::ContextName;
					TScriptInterface<INotifyFieldValueChanged> Found = Collection->FindViewModelInstance(Context);
					return Cast<UTetrisHUDViewModel>(Found.GetObject());
				}
			}
		}
	}
	return nullptr;
}

void UTetrisGameOverWidget::HandleRetryClicked()
{
	OnRetryRequested.Broadcast();
}

void UTetrisGameOverWidget::HandleMenuClicked()
{
	OnMenuRequested.Broadcast();
}
