// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/TetrisActivatableWidget.h"
#include "TetrisSettingsWidget.generated.h"

class UCommonButtonBase;
class UTetrisHandlingRowWidget;
class UTetrisSettingsSubsystem;

/**
 * 설정 화면 (View) — tetr.io HANDLING 패널 스타일로 ARR/DAS/DCD/SDF 4행을 슬라이더로 조정. Menu 입력 모드 + Back 핸들러.
 *
 * Why 서브시스템 직접 read/write(VM 미사용): 4값은 생산자=각 행(슬라이더), 소비자=설정 서브시스템
 *   1:1이라 FieldNotify VM은 과설계다(HANDOFF §5 simplicity-first). 위젯이 GameInstance의 설정
 *   서브시스템을 직접 조회해 현재값을 행에 주입(Configure)하고, 행의 변경 발행을 즉시 영속 저장(SaveGame)에 위임한다.
 * Why 행은 표시·발행만, read/write는 위젯이 소유: 행 컴포넌트(UTetrisHandlingRowWidget)는 도메인 무지라
 *   값이 "무엇"인지 모른다. 위젯이 (Min/Max/Step/Unit/Initial)과 어느 세터로 쓸지를 결정한다(MVVM 격리).
 * Why Back=메뉴 복귀: bIsBackHandler=true로 Back을 받아 OnBackRequested를 발행 → PC가 Settings를 pop,
 *   아래 대기 중이던 메인 메뉴가 다시 최상단 활성이 된다(스택 모델). Model/Session/VM 미참조.
 */
UCLASS()
class PROJECTTETRA_API UTetrisSettingsWidget : public UTetrisActivatableWidget
{
	GENERATED_BODY()

public:
	UTetrisSettingsWidget(const FObjectInitializer& ObjectInitializer);

	/** Back/BackButton이 발행하는 명령 — PC가 구독해 Settings를 닫고 메인 메뉴로 복귀시킨다(파라미터 없음). */
	FSimpleMulticastDelegate OnBackRequested;

protected:
	//~ UUserWidget
	virtual void NativeOnInitialized() override;

	//~ UCommonActivatableWidget — 활성화 시 현재 설정값을 각 행에 1회 Configure(설정은 화면 밖에서 안 바뀜 → 구독 불요).
	virtual void NativeOnActivated() override;

	//~ UCommonActivatableWidget — Back 액션 처리(Back=메뉴 복귀).
	virtual bool NativeOnHandleBackAction() override;

	//~ BindWidget — 4행은 필수(WBP_SettingsMenu가 WBP_HandlingRow 인스턴스를 이름 일치로 제공).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTetrisHandlingRowWidget> ARRRow;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTetrisHandlingRowWidget> DASRow;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTetrisHandlingRowWidget> DCDRow;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTetrisHandlingRowWidget> SDFRow;

	//~ BindWidget — Back 버튼은 필수(WBP_SettingsMenu가 이름 일치로 제공).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> BackButton;

private:
	/** GameInstance에서 설정 서브시스템을 조회(부재 시 null). */
	UTetrisSettingsSubsystem* ResolveSettings() const;

	//~ 행 OnValueChanged → 서브시스템 세터에 위임(clamp·저장은 서브시스템 책임). 슬라이더 범위가 곧 clamp 범위라 표시-실제 불일치 없음.
	void HandleARRChanged(int32 NewValue);
	void HandleDASChanged(int32 NewValue);
	void HandleDCDChanged(int32 NewValue);
	void HandleSDFChanged(int32 NewValue);
	void HandleBackClicked();
};
