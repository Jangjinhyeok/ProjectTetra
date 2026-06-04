// Copyright ProjectTetra. All Rights Reserved.

#include "Input/TetrisPlayerController.h"
#include "Input/TetrisHandlingTypes.h"
#include "Session/TetrisSessionSubsystem.h"
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

	// in-game 레이아웃(WBP_GameLayout: 보드+HUD) 생성·viewport 부착. 클래스 미지정 시 무동작(null-safe) — #13에서 디폴트 지정.
	// 레이아웃 안의 보드/HUD 위젯이 각자 NativeConstruct에서 VM을 컬렉션 키로 self-resolve하므로 PC는 클래스만 알면 된다(View는 Session/바인더 비참조).
	if (InGameLayoutClass && !InGameLayout && IsLocalController())
	{
		InGameLayout = CreateWidget<UUserWidget>(this, InGameLayoutClass);
		if (InGameLayout)
		{
			InGameLayout->AddToViewport();
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
