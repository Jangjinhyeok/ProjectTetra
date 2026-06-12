// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TetrisHandlingRowWidget.generated.h"

class USlider;
class UTextBlock;
class UProgressBar;

/** 행 값의 표시 방식 — 시간(ms+frame) 또는 배수(×). Configure가 주입(데이터 주도). */
enum class EHandlingDisplayUnit : uint8
{
	TimeMs,     // "{N}MS" + 큰 "{N.N}F"(프레임).
	Multiplier, // 큰 "{N}X"(MS/프레임 없음).
};

/**
 * tetr.io HANDLING 패널의 한 행 — 슬라이더 + 값 텍스트의 재사용 표시 컴포넌트.
 *
 * Why 도메인 무지(설정 서브시스템/Model/Session/VM 미참조): 4행(ARR/DAS/DCD/SDF)이 같은 클래스를
 *   재사용하려면 행은 "어떤 값"인지 몰라야 한다. 행은 표시 + 변경 발행만 하고, 값의 read/write는
 *   부모(Settings 위젯)가 서브시스템에 위임한다(MVVM — View 컴포넌트는 도메인 결합 0).
 * Why 비-activatable(UCommonUserWidget): 화면 스택에 push되는 화면이 아니라 화면 안에 박히는 부품이다.
 *   ActivatableWidget의 생명주기/입력 라우팅이 불필요하므로 가벼운 CommonUserWidget을 베이스로 한다.
 * Why 이벤트 주도(Tick 없음): 슬라이더 드래그 보간은 Slate가 담당하고, C++는 값 변경 엣지에서만
 *   정수로 스냅해 텍스트를 갱신하고 OnValueChanged를 발행한다(폴링/NativeTick 금지 §6).
 */
UCLASS()
class PROJECTTETRA_API UTetrisHandlingRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 값 변경 발행 — 부모(Settings 위젯)가 구독해 서브시스템 세터에 위임(clamp·저장은 서브시스템). */
	DECLARE_DELEGATE_OneParam(FOnHandlingValueChanged, int32 /*NewValue*/);
	FOnHandlingValueChanged OnValueChanged;

	/**
	 * 행을 데이터 주도로 구성 — 슬라이더 범위/스텝/표시 단위/초기값을 1회 주입한다.
	 * 하드코딩 금지: 행은 자기 의미를 모르고, (Min/Max/Step/Unit/Initial)을 부모가 지정한다.
	 */
	void Configure(int32 InMin, int32 InMax, int32 InStep, EHandlingDisplayUnit InUnit, int32 InitialValue);

protected:
	//~ UUserWidget — 슬라이더 변경 델리게이트 바인딩(엣지에서만 값 처리).
	virtual void NativeOnInitialized() override;

	//~ UUserWidget — 자식(슬라이더=focus 타깃)의 focus-path 진입/이탈을 행이 받아 하이라이트를 토글.
	//   순수 스타일로 표현 불가한 "행 단위 focus 표시"만 C++가 담당(게임패드 행 이동 시각 피드백).
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	//~ BindWidget — 슬라이더는 필수(WBP_HandlingRow가 이름 일치로 제공).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> ValueSlider;

	//~ BindWidget — 단위 텍스트(TimeMs="{N}MS", Multiplier=빈 문자열).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MsText;

	//~ BindWidget — 큰 값 텍스트(TimeMs="{N.N}F", Multiplier="{N}X").
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	//~ BindWidgetOptional — 행 라벨("ARR/DAS/…")은 WBP 정적 텍스트로 author 가능하므로 선택.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	//~ BindWidgetOptional — 슬라이더 채움(시안→오렌지 flat fill). 2색 fill은 USlider 단일 바로 불가하므로
	//   ProgressBar를 슬라이더 정규화 값과 동기화해 오버레이한다(슬라이더=입력+노브, ProgressBar=채움).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> FillBar;

	//~ BindWidgetOptional — 행 focus 하이라이트(밝은 시안 테두리). 기본 숨김, 슬라이더 focus 시에만 표시.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FocusHighlight;

private:
	//~ USlider OnValueChanged(DYNAMIC) 핸들러 — raw float을 스텝 정수로 스냅 → 텍스트 갱신 → 발행.
	UFUNCTION()
	void HandleSliderChanged(float RawValue);

	/** Unit에 따라 MsText/ValueText를 갱신(TimeMs: ms+frame, Multiplier: ×배수). */
	void RefreshTexts(int32 N);

	/** raw 슬라이더 값을 [Min, Max] 안에서 Step 배수의 정수로 스냅. */
	int32 SnapToStep(float RawValue) const;

	/** FillBar를 정규화 값 (N-Min)/(Max-Min)으로 동기화(FillBar/범위 무효 시 no-op). */
	void UpdateFill(int32 N);

	//~ Configure가 주입한 구성 — 행은 자기 의미가 아니라 이 파라미터로만 동작.
	int32 Min = 0;
	int32 Max = 0;
	int32 Step = 1;
	EHandlingDisplayUnit Unit = EHandlingDisplayUnit::TimeMs;
};
