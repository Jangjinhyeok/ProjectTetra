// Copyright ProjectTetra. All Rights Reserved.

#include "Input/TetrisPlayerController.h"
#include "Input/TetrisHandlingTypes.h"
#include "Session/TetrisSessionSubsystem.h"
#include "FSM/TetrisGameCore.h"        // GetState()
#include "Core/TetrisTypes.h"          // EGameState
#include "UI/Foundation/TetrisPrimaryGameLayout.h"
#include "UI/Views/TetrisPauseWidget.h"
#include "UI/Views/TetrisMainMenuWidget.h"
#include "UI/Views/TetrisGameOverWidget.h"
#include "UI/Views/TetrisSettingsWidget.h"
#include "Misc/DateTime.h"            // MakeSeed: FDateTime::Now
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

	// GameOver 관찰: GameCore가 GameOver로 전이하면 PC가 결과 화면을 띄운다(이벤트 주도 §4 — 폴링 없음).
	//   GameCore 수명=World, PC도 동일하나 EndPlay에서 명시 해제(안전).
	if (CachedSession)
	{
		if (UTetrisGameCore* Core = CachedSession->GetGameCore())
		{
			Core->OnStateChanged.AddUObject(this, &ATetrisPlayerController::HandleGameStateChanged);
		}
	}

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

			// 게임 화면 위에 메인 메뉴를 띄운다 — 게임은 Idle로 메뉴 뒤에 대기, Start까지 입력 비활성.
			ShowMainMenu();
		}
	}
}

void ATetrisPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BeginPlay에서 건 OnStateChanged 구독을 해제 — GameCore가 PC보다 오래 살 경우의 dangling 콜백 방지.
	if (CachedSession)
	{
		if (UTetrisGameCore* Core = CachedSession->GetGameCore())
		{
			Core->OnStateChanged.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
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
		// Pause Quit = 메뉴 복귀(§2 예외, 설계노트 4). 먼저 ResumePause로 Pause 위젯을 닫고(시뮬 해제),
		//   이어 HandleReturnToMenu로 시뮬을 다시 정지한 뒤 메인 메뉴를 띄운다. (앱 종료는 메인 메뉴 Quit으로 일원화.)
		ActivePause->OnQuitRequested.AddUObject(this, &ATetrisPlayerController::ResumePause);
		ActivePause->OnQuitRequested.AddUObject(this, &ATetrisPlayerController::HandleReturnToMenu);
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
	// 메인 메뉴 Quit = 앱 종료. PIE/스탠드얼론·패키징 모두 quit 콘솔 커맨드가 종료를 수행.
	ConsoleCommand(TEXT("quit"));
}

void ATetrisPlayerController::ShowMainMenu()
{
	// 멱등: 이미 떠 있으면 중복 push 방지. 레이아웃/클래스 미구성 시 무동작(null-safe).
	if (ActiveMainMenu || !PrimaryLayout)
	{
		return;
	}
	if (!MainMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tetra] PlayerController: MainMenuWidgetClass 미지정 — 에디터에서 WBP_MainMenu를 할당하세요."));
		return;
	}

	// Menu 레이어에 push → 게임(Game 레이어) 위를 가린다. Menu 입력 모드가 게임 입력을 자동 억제.
	ActiveMainMenu = Cast<UTetrisMainMenuWidget>(PrimaryLayout->PushWidgetToLayer(EUILayer::Menu, MainMenuWidgetClass));
	if (ActiveMainMenu)
	{
		// 위젯은 명령만 발행(§3) — Start/Quit/Settings 실행은 PC가 수행.
		ActiveMainMenu->OnStartRequested.AddUObject(this, &ATetrisPlayerController::HandleStartRequested);
		ActiveMainMenu->OnQuitRequested.AddUObject(this, &ATetrisPlayerController::HandleQuitRequested);
		ActiveMainMenu->OnSettingsRequested.AddUObject(this, &ATetrisPlayerController::HandleSettingsRequested);
	}
}

void ATetrisPlayerController::HandleSettingsRequested()
{
	// 멱등: 이미 떠 있으면 무동작. 레이아웃/클래스 미구성 시 무동작(null-safe).
	if (ActiveSettings || !PrimaryLayout)
	{
		return;
	}
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tetra] PlayerController: SettingsWidgetClass 미지정 — 에디터에서 WBP_SettingsMenu를 할당하세요."));
		return;
	}

	// Menu 레이어에 push → 메인 메뉴 위에 쌓인다. 메인 메뉴는 스택에 남아 비활성 대기(Back pop 시 자동 복귀).
	ActiveSettings = Cast<UTetrisSettingsWidget>(PrimaryLayout->PushWidgetToLayer(EUILayer::Menu, SettingsWidgetClass));
	if (ActiveSettings)
	{
		ActiveSettings->OnBackRequested.AddUObject(this, &ATetrisPlayerController::HandleSettingsBack);
	}
}

void ATetrisPlayerController::HandleSettingsBack()
{
	// 설정 화면만 pop → 아래 대기 중이던 메인 메뉴가 다시 최상단 활성이 된다(메인 메뉴 재push 불요).
	if (PrimaryLayout && ActiveSettings)
	{
		// Why RemoveAll: 스택 위젯 풀이 인스턴스를 재사용하면 재진입(Settings 다시 열기)마다 OnBackRequested에
		//   PC 바인딩이 누적된다. pop 전에 자기 바인딩만 끊어 중복 invoke를 막는다(non-UPROPERTY 델리게이트라 GC 자동 해제 없음).
		ActiveSettings->OnBackRequested.RemoveAll(this);
		PrimaryLayout->RemoveWidget(*ActiveSettings);
	}
	ActiveSettings = nullptr;
}

void ATetrisPlayerController::HandleStartRequested()
{
	// 새 시드로 게임 시작 후 메뉴를 pop → Game 레이어가 최상단 활성이 되어 게임 입력이 자동 복귀한다.
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->StartGame(MakeSeed());
	}
	if (PrimaryLayout && ActiveMainMenu)
	{
		PrimaryLayout->RemoveWidget(*ActiveMainMenu);
	}
	ActiveMainMenu = nullptr;
}

void ATetrisPlayerController::HandleGameStateChanged(EGameState /*OldState*/, EGameState NewState)
{
	// GameOver 진입 엣지에만 1회 결과 화면을 띄운다(중복 가드). 다른 전이는 무시.
	if (NewState == EGameState::GameOver && !ActiveGameOver)
	{
		ShowGameOver();
	}
}

void ATetrisPlayerController::ShowGameOver()
{
	if (!PrimaryLayout)
	{
		return;
	}
	if (!GameOverWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tetra] PlayerController: GameOverWidgetClass 미지정 — 에디터에서 WBP_GameOver를 할당하세요."));
		return;
	}

	// Modal 레이어(최상단)에 push — 게임/메뉴 위에 결과를 덮는다. 점수는 위젯이 VM에서 self-resolve.
	ActiveGameOver = Cast<UTetrisGameOverWidget>(PrimaryLayout->PushWidgetToLayer(EUILayer::Modal, GameOverWidgetClass));
	if (ActiveGameOver)
	{
		// 위젯은 명령만 발행(§3) — Retry/Menu 실행은 PC가 수행.
		ActiveGameOver->OnRetryRequested.AddUObject(this, &ATetrisPlayerController::HandleRetryRequested);
		ActiveGameOver->OnMenuRequested.AddUObject(this, &ATetrisPlayerController::HandleReturnToMenu);
	}
}

void ATetrisPlayerController::HandleRetryRequested()
{
	// 같은 자리(Game 레이어)에서 새 판: RestartGame 후 결과 화면만 pop. StateChanged(GameOver→Spawn)는 무시됨.
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->RestartGame();
	}
	if (PrimaryLayout && ActiveGameOver)
	{
		PrimaryLayout->RemoveWidget(*ActiveGameOver);
	}
	ActiveGameOver = nullptr;
}

void ATetrisPlayerController::HandleReturnToMenu()
{
	// GameOver Menu / Pause Quit 공통 경로: 결과 화면이 떠 있으면 정리 → 시뮬 정지 → 메인 메뉴 복귀.
	if (ActiveGameOver)
	{
		if (PrimaryLayout)
		{
			PrimaryLayout->RemoveWidget(*ActiveGameOver);
		}
		ActiveGameOver = nullptr;
	}

	// 뒤에 남은 게임 시뮬을 정지(메뉴 뒤에서 진행하지 않도록). GameOver 상태면 이미 비구동이라 무해.
	if (UTetrisSessionSubsystem* S = GetSession())
	{
		S->SetPaused(true);
	}

	ShowMainMenu();
}

int64 ATetrisPlayerController::MakeSeed() const
{
	// 플레이마다 변주(현재 시각 ticks). 결정성은 "시드 고정 시 동일 결과"라 시드 자체는 매판 달라도 됨(설계노트 2).
	return FDateTime::Now().GetTicks();
}
