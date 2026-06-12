// Copyright ProjectTetra. All Rights Reserved.

#include "UI/Components/TetrisHandlingRowWidget.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

namespace
{
	// 표시 전용 프레임 환산 주파수(Hz). Why named 상수: 매직넘버 금지 §7 — 결정적 SimHz(60)와 일치.
	//   실제 ms→스텝 환산은 Session SimHz가 담당하며, 여기 60은 "프레임 표시"용 cosmetic 값일 뿐이다.
	//   SimHz가 가변이 되면 그때 Configure로 Hz를 주입한다(설계노트 3).
	constexpr int32 GHandlingDisplayHz = 60;
}

void UTetrisHandlingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// USlider OnValueChanged는 DYNAMIC 멀티캐스트라 UFUNCTION 핸들러가 필요. 드래그 보간은 Slate가,
	// 정수 스냅·텍스트·발행은 이 엣지 콜백이 담당한다(이벤트 주도 §6).
	if (ValueSlider)
	{
		ValueSlider->OnValueChanged.AddDynamic(this, &UTetrisHandlingRowWidget::HandleSliderChanged);
	}

	// focus 하이라이트는 기본 숨김 — 슬라이더 focus 진입(NativeOnAddedToFocusPath) 시에만 표시.
	if (FocusHighlight)
	{
		FocusHighlight->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTetrisHandlingRowWidget::Configure(int32 InMin, int32 InMax, int32 InStep, EHandlingDisplayUnit InUnit, int32 InitialValue)
{
	Min = InMin;
	Max = InMax;
	Step = FMath::Max(1, InStep); // Step 0 방어(0 나눗셈/무한 스냅 회피).
	Unit = InUnit;

	const int32 Initial = FMath::Clamp(InitialValue, Min, Max);

	if (ValueSlider)
	{
		// 슬라이더 범위/스텝을 C++가 덮어쓴다 → WBP 에디터 기본(0–1)과 무관하게 행마다 올바른 범위.
		ValueSlider->SetMinValue(static_cast<float>(Min));
		ValueSlider->SetMaxValue(static_cast<float>(Max));
		ValueSlider->SetStepSize(static_cast<float>(Step));
		// SetValue는 OnValueChanged를 발행하지 않는다(유저 조작만 발행) → 재진입 루프 없음.
		ValueSlider->SetValue(static_cast<float>(Initial));
	}

	RefreshTexts(Initial); // 활성화 엣지 1회 표시(슬라이더 발행 없이 직접 갱신).
	UpdateFill(Initial);   // 채움 바도 초기값에 맞춰 동기화.
}

void UTetrisHandlingRowWidget::HandleSliderChanged(float RawValue)
{
	const int32 N = SnapToStep(RawValue);
	RefreshTexts(N);
	UpdateFill(N);
	OnValueChanged.ExecuteIfBound(N);
}

int32 UTetrisHandlingRowWidget::SnapToStep(float RawValue) const
{
	const int32 Snapped = Min + FMath::RoundToInt((RawValue - Min) / Step) * Step;
	return FMath::Clamp(Snapped, Min, Max);
}

void UTetrisHandlingRowWidget::UpdateFill(int32 N)
{
	if (!FillBar || Max <= Min)
	{
		return; // 채움 바 미배선 또는 범위 무효(0 나눗셈 방어) → no-op(null-safe 무회귀).
	}
	const float Percent = static_cast<float>(N - Min) / static_cast<float>(Max - Min);
	FillBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
}

void UTetrisHandlingRowWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	// 슬라이더(자식 focus 타깃)가 focus-path에 들어오면 행 하이라이트 표시. HitTestInvisible: 입력은 슬라이더가 받음.
	if (FocusHighlight)
	{
		FocusHighlight->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UTetrisHandlingRowWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	if (FocusHighlight)
	{
		FocusHighlight->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTetrisHandlingRowWidget::RefreshTexts(int32 N)
{
	switch (Unit)
	{
	case EHandlingDisplayUnit::TimeMs:
	{
		if (MsText)
		{
			MsText->SetText(FText::FromString(FString::Printf(TEXT("%dMS"), N)));
		}
		if (ValueText)
		{
			// 프레임 = ms × Hz / 1000 (표시 전용). 한 자리 소수로 "{N.N}F".
			const float Frames = static_cast<float>(N) * GHandlingDisplayHz / 1000.0f;
			ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1fF"), Frames)));
		}
		break;
	}
	case EHandlingDisplayUnit::Multiplier:
	{
		// 배수 행(SDF): ms/프레임 개념 없음 → MsText는 비우고 큰 값에 "{N}X"만.
		if (MsText)
		{
			MsText->SetText(FText::GetEmpty());
		}
		if (ValueText)
		{
			ValueText->SetText(FText::FromString(FString::Printf(TEXT("%dX"), N)));
		}
		break;
	}
	}
}
