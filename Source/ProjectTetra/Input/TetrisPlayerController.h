// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TetrisPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UTetrisSessionSubsystem;
class UTetrisPrimaryGameLayout;
class UTetrisActivatableWidget;
class UTetrisPauseWidget;

/**
 * Tetris PlayerController — Enhanced Input(frame 도메인)을 "의도"로만 번역하는 어댑터.
 *
 * Why 이 경계: PC는 IMC/IA 바인딩 핸들러에서 타이밍 판단을 하지 않고, Session의 입력 버퍼에
 *   held-state/이산 엣지만 기록한다(번역 레이어). DAS/ARR 같은 자동반복은 고정스텝 FTetrisHandling이
 *   결정한다 → 프레임레이트 독립·결정성(input-handling.md §2). 게임 로직/UI는 참조하지 않는다.
 *   IA_ 및 IMC 에셋은 EditDefaultsOnly로 노출하고 에디터(BP/PC 디폴트)에서 지정한다.
 */
UCLASS()
class PROJECTTETRA_API ATetrisPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	//~ APlayerController
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

protected:
	/** 게임플레이 입력 매핑 컨텍스트. BeginPlay에서 AddMappingContext. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputMappingContext> IMC_Gameplay;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_MoveLeft;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_MoveRight;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_RotateCW;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_RotateCCW;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_SoftDrop;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_HardDrop;

	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_Hold;

	/** UI 루트(WBP_PrimaryGameLayout). BeginPlay에서 생성·viewport 부착. 미지정 시 무동작(null-safe). */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|UI")
	TSubclassOf<UTetrisPrimaryGameLayout> PrimaryGameLayoutClass;

	/** 게임 화면(WBP_GameScreen — 보드+HUD 호스트). BeginPlay에서 Game 레이어에 push. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|UI")
	TSubclassOf<UTetrisActivatableWidget> GameScreenClass;

	/** Pause 메뉴(WBP_PauseMenu). Pause 토글 시 GameMenu 레이어에 push. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|UI")
	TSubclassOf<UTetrisActivatableWidget> PauseWidgetClass;

	/** 일시정지 토글 입력. Started 엣지에서 OnPauseToggle. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|Input")
	TObjectPtr<UInputAction> IA_Pause;

private:
	/** 생성된 UI 루트(중복 생성 방지·수명 보유). */
	UPROPERTY(Transient)
	TObjectPtr<UTetrisPrimaryGameLayout> PrimaryLayout;

	/** 현재 떠 있는 Pause 위젯(없으면 null — 토글 상태 추적). */
	UPROPERTY(Transient)
	TObjectPtr<UTetrisPauseWidget> ActivePause;

	/** 세션 서브시스템 캐시(지연 조회). PC와 World 수명이 같아 단순 캐시로 충분. */
	UPROPERTY(Transient)
	TObjectPtr<UTetrisSessionSubsystem> CachedSession;

	UTetrisSessionSubsystem* GetSession();

	//~ Pause 흐름 — 입력/위젯 델리게이트가 트리거, Session 제어는 PC가 수행(§3 위젯은 명령만 발행).
	/** IA_Pause 엣지: 진행 중이면 Pause 진입, 이미 떠 있으면 닫기(토글). */
	void OnPauseToggle();
	/** Pause 닫기: 위젯 제거 + SetPaused(false). Resume/Back 공통 경로. */
	void ResumePause();
	/** Restart 요청: Session->RestartGame() 후 Pause 닫기. */
	void HandleRestartRequested();
	/** Quit 요청: PIE/스탠드얼론 종료. */
	void HandleQuitRequested();

	//~ 바인딩 핸들러 — Session 버퍼에 기록만(타이밍 판단 없음).
	void OnMoveLeftStarted();
	void OnMoveLeftCompleted();
	void OnMoveRightStarted();
	void OnMoveRightCompleted();
	void OnRotateCW();
	void OnRotateCCW();
	void OnSoftDropStarted();
	void OnSoftDropCompleted();
	void OnHardDrop();
	void OnHold();
};
