// Copyright ProjectTetra. All Rights Reserved.

#include "Input/TetrisPlayerController.h"
#include "Input/TetrisHandlingTypes.h"
#include "Session/TetrisSessionSubsystem.h"
#include "FSM/TetrisGameCore.h"        // GetState()
#include "Core/TetrisTypes.h"          // EGameState
#include "UI/Foundation/TetrisPrimaryGameLayout.h"
#include "UI/Views/TetrisPauseWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

void ATetrisPlayerController::BeginPlay()
{
	Super::BeginPlay();

	GetSession(); // 캐시 워밍

	// IMC 등록 — LocalPlayer의 Enhanced Input 서브시스템에.
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (IMC_Gameplay)
			{
				InputSub->AddMappingContext(IMC_Gameplay, /*Priority=*/0);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Tetra] PlayerController: IMC_Gameplay 미지정 — 에디터에서 IMC 에셋을 할당하세요."));
			}
		}
	}

	// UI 루트(WBP_PrimaryGameLayout) 생성·viewport 부착 → Game 레이어에 게임 화면(WBP_GameScreen) push.
	// Why 직접 AddToViewport 제거: HUD를 PrimaryGameLayout의 Game 레이어로 옮겨 부착 방식을 일원화(HANDOFF §2).
	//   게임 화면 안의 보드/HUD 위젯이 각자 VM을 컬렉션 키로 self-resolve하므로 PC는 클래스만 알면 된다(View는 Session/바인더 비참조).
	if (PrimaryGameLayoutClass && !PrimaryLayout && IsLocalController())
	{
		PrimaryLayout = CreateWidget<UTetrisPrimaryGameLayout>(this, PrimaryGameLayoutClass);
		if (PrimaryLayout)
		{
			PrimaryLayout->AddToViewport();
			if (GameScreenClass)
			{
				PrimaryLayout->PushWidgetToLayer(EUILayer::Game, GameScreenClass);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Tetra] PlayerController: GameScreenClass 미지정 — 에디터에서 WBP_GameScreen을 할당하세요."));
			}
		}
	}
}

void ATetrisPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		// 프로젝트가 Enhanced Input을 기본 입력 컴포넌트로 쓰지 않을 때. 에셋 바인딩 불가.
		UE_LOG(LogTemp, Warning, TEXT("[Tetra] PlayerController: InputComponent가 EnhancedInputComponent가 아님 — 프로젝트 Input 설정을 확인하세요."));
		return;
	}

	// 좌우: Started→press(+held), Completed→held 해제. (release 엣지는 Handling이 held로 대체하므로 불필요)
	if (IA_MoveLeft)
	{
		EIC->BindAction(IA_MoveLeft, ETriggerEvent::Started, this, &ATetrisPlayerController::OnMoveLeftStarted);
		EIC->BindAction(IA_MoveLeft, ETriggerEvent::Completed, this, &ATetrisPlayerController::OnMoveLeftCompleted);
	}
	if (IA_MoveRight)
	{
		EIC->BindAction(IA_MoveRight, ETriggerEvent::Started, this, &ATetrisPlayerController::OnMoveRightStarted);
		EIC->BindAction(IA_MoveRight, ETriggerEvent::Completed, this, &ATetrisPlayerController::OnMoveRightCompleted);
	}

	// 회전/홀드/하드드롭: 이산 press만.
	if (IA_RotateCW)  { EIC->BindAction(IA_RotateCW, ETriggerEvent::Started, this, &ATetrisPlayerController::OnRotateCW); }
	if (IA_RotateCCW) { EIC->BindAction(IA_RotateCCW, ETriggerEvent::Started, this, &ATetrisPlayerController::OnRotateCCW); }
	if (IA_HardDrop)  { EIC->BindAction(IA_HardDrop, ETriggerEvent::Started, this, &ATetrisPlayerController::OnHardDrop); }
	if (IA_Hold)      { EIC->BindAction(IA_Hold, ETriggerEvent::Started, this, &ATetrisPlayerController::OnHold); }

	// 소프트드롭: Started→On, Completed→Off (지속 상태는 GameCore가 소유).
	if (IA_SoftDrop)
	{
		EIC->BindAction(IA_SoftDrop, ETriggerEvent::Started, this, &ATetrisPlayerController::OnSoftDropStarted);
		EIC->BindAction(IA_SoftDrop, ETriggerEvent::Completed, this, &ATetrisPlayerController::OnSoftDropCompleted);
	}

	// Pause: 이산 토글. 게임 입력과 달리 Session 시뮬이 아니라 UI 흐름을 제어한다.
	if (IA_Pause) { EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &ATetrisPlayerController::OnPauseToggle); }
}

UTetrisSessionSubsystem* ATetrisPlayerController::GetSession()
{
	if (!CachedSession)
	{
		if (const UWorld* World = GetWorld())
		{
			CachedSession = World->GetSubsystem<UTetrisSessionSubsystem>();
		}
	}
	return CachedSession;
}

void ATetrisPlayerController::OnMoveLeftStarted()
{
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->SetMoveHeld(/*bLeft=*/true, /*bHeld=*/true);
		S->PushInputEdge(EInputEdge::MoveLeftPress);
	}
}

void ATetrisPlayerController::OnMoveLeftCompleted()
{
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->SetMoveHeld(/*bLeft=*/true, /*bHeld=*/false);
	}
}

void ATetrisPlayerController::OnMoveRightStarted()
{
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->SetMoveHeld(/*bLeft=*/false, /*bHeld=*/true);
		S->PushInputEdge(EInputEdge::MoveRightPress);
	}
}

void ATetrisPlayerController::OnMoveRightCompleted()
{
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->SetMoveHeld(/*bLeft=*/false, /*bHeld=*/false);
	}
}

void ATetrisPlayerController::OnRotateCW()
{
	if (UTetrisSessionSubsystem* S = GetSession()) { S->PushInputEdge(EInputEdge::RotateCW); }
}

void ATetrisPlayerController::OnRotateCCW()
{
	if (UTetrisSessionSubsystem* S = GetSession()) { S->PushInputEdge(EInputEdge::RotateCCW); }
}

void ATetrisPlayerController::OnSoftDropStarted()
{
	if (UTetrisSessionSubsystem* S = GetSession()) { S->PushInputEdge(EInputEdge::SoftDropOn); }
}

void ATetrisPlayerController::OnSoftDropCompleted()
{
	if (UTetrisSessionSubsystem* S = GetSession()) { S->PushInputEdge(EInputEdge::SoftDropOff); }
}

void ATetrisPlayerController::OnHardDrop()
{
	if (UTetrisSessionSubsystem* S = GetSession()) { S->PushInputEdge(EInputEdge::HardDrop); }
}

void ATetrisPlayerController::OnHold()
{
	if (UTetrisSessionSubsystem* S = GetSession()) { S->PushInputEdge(EInputEdge::Hold); }
}

void ATetrisPlayerController::OnPauseToggle()
{
	// 이미 떠 있으면 토글로 닫는다(상태 가드 무관 — 진행 중에만 진입했으므로).
	if (ActivePause)
	{
		ResumePause();
		return;
	}

	UTetrisSessionSubsystem* S = GetSession();
	if (!S || !PrimaryLayout)
	{
		return;
	}

	// 진행 중(Spawn/Falling/Locking/LineClear)일 때만 Pause 진입 — Idle/GameOver는 무동작.
	const UTetrisGameCore* Core = S->GetGameCore();
	if (!Core)
	{
		return;
	}
	const EGameState State = Core->GetState();
	if (State == EGameState::Idle || State == EGameState::GameOver)
	{
		return;
	}

	// 시뮬 정지 → Pause 메뉴를 GameMenu 레이어에 push. 게임 입력 차단은 PauseWidget의
	// GetDesiredInputConfig=Menu가 CommonGameViewportClient를 통해 자동 처리(PC가 IMC를 끄지 않음).
	S->SetPaused(true);
	ActivePause = Cast<UTetrisPauseWidget>(PrimaryLayout->PushWidgetToLayer(EUILayer::GameMenu, PauseWidgetClass));
	if (ActivePause)
	{
		// 위젯은 명령만 발행(§3) — 실제 Session 제어는 아래 핸들러(PC)가 수행.
		ActivePause->OnResumeRequested.AddUObject(this, &ATetrisPlayerController::ResumePause);
		ActivePause->OnRestartRequested.AddUObject(this, &ATetrisPlayerController::HandleRestartRequested);
		ActivePause->OnQuitRequested.AddUObject(this, &ATetrisPlayerController::HandleQuitRequested);
	}
	else
	{
		// push 실패 시 pause 상태 일관성 위해 즉시 복귀.
		S->SetPaused(false);
	}
}

void ATetrisPlayerController::ResumePause()
{
	if (!ActivePause)
	{
		return;
	}

	if (PrimaryLayout)
	{
		PrimaryLayout->RemoveWidget(*ActivePause);
	}
	ActivePause = nullptr;

	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->SetPaused(false);
	}
}

void ATetrisPlayerController::HandleRestartRequested()
{
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->RestartGame();
	}
	ResumePause();
}

void ATetrisPlayerController::HandleQuitRequested()
{
	// PIE/스탠드얼론 종료. 패키징 빌드에서도 quit 콘솔 커맨드가 종료를 수행.
	ConsoleCommand(TEXT("quit"));
}
