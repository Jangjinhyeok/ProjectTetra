// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisSettingsWidget.h"
#include "CommonButtonBase.h"
#include "UI/Components/TetrisHandlingRowWidget.h"
#include "System/TetrisSettingsSubsystem.h"
#include "Engine/GameInstance.h"

namespace
{
	// 행별 슬라이더 범위/스텝 — 매직넘버 금지 §7. Min/Max는 설정 서브시스템 clamp 범위를 그대로 미러한다
	// (슬라이더가 곧 clamp라 범위 밖 값을 못 만들어 표시-실제 불일치가 없다). Step=1로 1ms/1× 단위 조정.
	constexpr int32 ARRMin = 0,  ARRMax = 200;
	constexpr int32 DASMin = 0,  DASMax = 500;
	constexpr int32 DCDMin = 0,  DCDMax = 500;
	constexpr int32 SDFMin = 1,  SDFMax = 40;
	constexpr int32 RowStep = 1;
}

UTetrisSettingsWidget::UTetrisSettingsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 메뉴는 UI 입력 모드(활성 시 게임 입력 자동 억제) + 마우스 캡처 없음. 기본값을 코드로 박아 안전화.
	InputModeOverride = ECommonInputMode::Menu;
	MouseCapture = EMouseCaptureMode::NoCapture;
	// Back으로 닫고 메인 메뉴로 복귀(NativeOnHandleBackAction).
	bIsBackHandler = true;
}

void UTetrisSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Back 버튼 클릭을 핸들러로 중계. 널 가드 — WBP BindWidget 누락 대비. (행 슬라이더 바인딩은 NativeOnActivated에서.)
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UTetrisSettingsWidget::HandleBackClicked);
	}
}

void UTetrisSettingsWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 활성화 엣지에 현재 영속값을 각 행에 1회 Configure(범위/스텝/단위/초기값 주입) + 변경 발행 바인딩.
	// Configure의 슬라이더 SetValue는 발행하지 않으므로 초기 주입이 세터로 새지 않는다.
	// BindUObject는 1:1 델리게이트라 재활성화 시 재바인딩이 덮어써 누적되지 않는다.
	UTetrisSettingsSubsystem* Settings = ResolveSettings();
	if (!Settings)
	{
		return;
	}

	if (ARRRow)
	{
		ARRRow->Configure(ARRMin, ARRMax, RowStep, EHandlingDisplayUnit::TimeMs, Settings->GetARRms());
		ARRRow->OnValueChanged.BindUObject(this, &UTetrisSettingsWidget::HandleARRChanged);
	}
	if (DASRow)
	{
		DASRow->Configure(DASMin, DASMax, RowStep, EHandlingDisplayUnit::TimeMs, Settings->GetDASms());
		DASRow->OnValueChanged.BindUObject(this, &UTetrisSettingsWidget::HandleDASChanged);
	}
	if (DCDRow)
	{
		DCDRow->Configure(DCDMin, DCDMax, RowStep, EHandlingDisplayUnit::TimeMs, Settings->GetDCDms());
		DCDRow->OnValueChanged.BindUObject(this, &UTetrisSettingsWidget::HandleDCDChanged);
	}
	if (SDFRow)
	{
		SDFRow->Configure(SDFMin, SDFMax, RowStep, EHandlingDisplayUnit::Multiplier, Settings->GetSDF());
		SDFRow->OnValueChanged.BindUObject(this, &UTetrisSettingsWidget::HandleSDFChanged);
	}
}

bool UTetrisSettingsWidget::NativeOnHandleBackAction()
{
	// Back=메뉴 복귀: OnBackRequested를 발행하고 true를 반환해 Back 전파를 멈춘다(아래 게임 화면까지 닫히는 것 방지).
	OnBackRequested.Broadcast();
	return true;
}

UTetrisSettingsSubsystem* UTetrisSettingsWidget::ResolveSettings() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UTetrisSettingsSubsystem>();
	}
	return nullptr;
}

void UTetrisSettingsWidget::HandleARRChanged(int32 NewValue)
{
	if (UTetrisSettingsSubsystem* Settings = ResolveSettings())
	{
		Settings->SetARRms(NewValue);
	}
}

void UTetrisSettingsWidget::HandleDASChanged(int32 NewValue)
{
	if (UTetrisSettingsSubsystem* Settings = ResolveSettings())
	{
		Settings->SetDASms(NewValue);
	}
}

void UTetrisSettingsWidget::HandleDCDChanged(int32 NewValue)
{
	if (UTetrisSettingsSubsystem* Settings = ResolveSettings())
	{
		Settings->SetDCDms(NewValue);
	}
}

void UTetrisSettingsWidget::HandleSDFChanged(int32 NewValue)
{
	if (UTetrisSettingsSubsystem* Settings = ResolveSettings())
	{
		Settings->SetSDF(NewValue);
	}
}

void UTetrisSettingsWidget::HandleBackClicked()
{
	OnBackRequested.Broadcast();
}
