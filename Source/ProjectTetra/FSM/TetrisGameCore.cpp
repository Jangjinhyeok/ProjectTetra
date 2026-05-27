// Copyright ProjectTetra. All Rights Reserved.

#include "FSM/TetrisGameCore.h"
#include "Board/TetrisBoard.h"
#include "Block/TetrisPiece.h"
#include "System/TetrisRandomizer.h"
#include "System/TetrisScoring.h"

namespace
{
	// 부동소수 누적 오차 흡수용 epsilon. (예: 1/60을 60회 더하면 0.9999…가 될 수 있어 floor가 0이 되는 것을 방지)
	// 누적기 자체엔 더하지 않고 floor 비교에만 사용 → 드리프트 없음. BaseG(0.0167+)보다 훨씬 작아 오판 없음.
	constexpr double GravityFloorEpsilon = 1e-6;
}

UTetrisGameCore::UTetrisGameCore()
{
}

void UTetrisGameCore::Initialize(UTetrisBoard* InBoard, UTetrisRandomizer* InRandomizer, UTetrisScoring* InScoring)
{
	Board = InBoard;
	Randomizer = InRandomizer;
	Scoring = InScoring;
	LockDelay.SetConfig(LockDelayConfig);
	CurrentState = EGameState::Idle;
}

void UTetrisGameCore::StartGame(int64 Seed)
{
	if (!Board || !Randomizer)
	{
		return;
	}
	Board->Clear();
	Randomizer->Initialize(Seed);
	if (Scoring)
	{
		Scoring->ResetScoring();
	}

	HoldSlot = EPieceType::None;
	bSoftDropHeld = false;
	bHoldUsedThisPiece = false;
	GravityAccumulator = 0.0;
	SoftDropCells = 0;
	HardDropCells = 0;
	CommandQueue.Reset();
	LockDelay.SetConfig(LockDelayConfig);

	BeginSpawn();
}

void UTetrisGameCore::RestartGame()
{
	if (!Board || !Randomizer)
	{
		return;
	}
	Board->Clear();
	Randomizer->Reset();
	if (Scoring)
	{
		Scoring->ResetScoring();
	}

	HoldSlot = EPieceType::None;
	bSoftDropHeld = false;
	bHoldUsedThisPiece = false;
	GravityAccumulator = 0.0;
	SoftDropCells = 0;
	HardDropCells = 0;
	CommandQueue.Reset();
	LockDelay.SetConfig(LockDelayConfig); // StartGame과 대칭: 런타임 config 변경분 반영

	BeginSpawn();
}

void UTetrisGameCore::EnqueueCommand(EGameCommand Command)
{
	CommandQueue.Add(Command);
}

void UTetrisGameCore::Step()
{
	// Idle/GameOver: 명령·중력 모두 무시. 큐는 비워 누적을 막는다 (Edge 6).
	if (CurrentState == EGameState::Idle || CurrentState == EGameState::GameOver)
	{
		CommandQueue.Reset();
		return;
	}

	if (CurrentState == EGameState::Falling)
	{
		TickFalling();
	}
	else if (CurrentState == EGameState::Locking)
	{
		TickLocking();
	}
}

void UTetrisGameCore::TickFalling()
{
	// 1) 명령 드레인. HardDrop으로 lock되면 그 스텝은 여기서 종료(새 피스는 다음 스텝부터).
	if (DrainCommands())
	{
		return;
	}
	// Hold가 BlockOut을 유발하는 등 상태가 Falling을 벗어났으면 중력 적용 안 함.
	if (CurrentState != EGameState::Falling)
	{
		return;
	}
	// 2) 중력 적분 (+ 접지 시 Locking 진입)
	IntegrateGravity();
}

void UTetrisGameCore::TickLocking()
{
	if (DrainCommands())
	{
		return;
	}
	// 이동/회전으로 ungrounded → Falling 복귀했거나 GameOver면 Tick 안 함.
	if (CurrentState != EGameState::Locking)
	{
		return;
	}
	// Lock Delay 진행 → 만료 시 Lock.
	if (LockDelay.Tick())
	{
		DoLock();
	}
}

bool UTetrisGameCore::DrainCommands()
{
	while (CommandQueue.Num() > 0)
	{
		const EGameCommand Cmd = CommandQueue[0];
		CommandQueue.RemoveAt(0);

		if (Cmd == EGameCommand::HardDrop)
		{
			// 상태 전이 명령: 처리 후 드레인 중단. 잔여 명령은 큐에 남아 다음 스텝(새 피스)에서 처리된다.
			DoHardDrop();
			return true;
		}

		HandleNonHardDropCommand(Cmd);

		// Hold로 인한 GameOver 등 활성 상태를 벗어나면 더 이상 처리하지 않는다.
		if (CurrentState != EGameState::Falling && CurrentState != EGameState::Locking)
		{
			return false;
		}
	}
	return false;
}

void UTetrisGameCore::HandleNonHardDropCommand(EGameCommand Command)
{
	switch (Command)
	{
		case EGameCommand::MoveLeft:    TryMoveCommand(EMoveDirection::Left);  break;
		case EGameCommand::MoveRight:   TryMoveCommand(EMoveDirection::Right); break;
		case EGameCommand::RotateCW:    TryRotateCommand(ERotateDirection::CW);  break;
		case EGameCommand::RotateCCW:   TryRotateCommand(ERotateDirection::CCW); break;
		case EGameCommand::SoftDropOn:  bSoftDropHeld = true;  break;
		case EGameCommand::SoftDropOff: bSoftDropHeld = false; break;
		case EGameCommand::Hold:        DoHold(); break;
		default: break;
	}
}

void UTetrisGameCore::TryMoveCommand(EMoveDirection Direction)
{
	if (!FTetrisPieceOps::TryMove(ActivePiece, Direction, *Board))
	{
		return;
	}
	OnActivePieceUpdated.Broadcast(ActivePiece);

	// Locking 중 이동: 접지 재평가 → 떠오르면 Falling 복귀, 유지면 Lock Delay 리셋 질의.
	if (CurrentState == EGameState::Locking)
	{
		const bool bGrounded = IsGrounded(ActivePiece);
		LockDelay.NotifyMoveOrRotate(bGrounded, GetLowestBlockY(ActivePiece));
		if (!bGrounded)
		{
			EnterState(EGameState::Falling);
		}
	}
}

void UTetrisGameCore::TryRotateCommand(ERotateDirection Direction)
{
	if (!FTetrisPieceOps::TryRotate(ActivePiece, Direction, *Board))
	{
		return;
	}
	OnActivePieceUpdated.Broadcast(ActivePiece);

	if (CurrentState == EGameState::Locking)
	{
		const bool bGrounded = IsGrounded(ActivePiece);
		LockDelay.NotifyMoveOrRotate(bGrounded, GetLowestBlockY(ActivePiece));
		if (!bGrounded)
		{
			EnterState(EGameState::Falling);
		}
	}
}

void UTetrisGameCore::IntegrateGravity()
{
	GravityAccumulator += GetGravityPerStep();

	// floor에 epsilon을 더해 부동소수 오차로 칸 수가 하나 누락되는 것을 방지.
	const int32 CellsToDrop = FMath::FloorToInt(GravityAccumulator + GravityFloorEpsilon);
	for (int32 i = 0; i < CellsToDrop; ++i)
	{
		if (FTetrisPieceOps::TryMove(ActivePiece, EMoveDirection::Down, *Board))
		{
			GravityAccumulator -= 1.0;
			if (bSoftDropHeld)
			{
				// 소프트드롭 보유 중 하강한 칸만 입력 기반으로 집계 (자연 낙하 제외).
				++SoftDropCells;
			}
			OnActivePieceUpdated.Broadcast(ActivePiece);
		}
		else
		{
			GravityAccumulator = 0.0;
			break;
		}
	}

	// 더 못 내려가면 접지 → Locking. (소프트드롭이어도 즉시 Lock 아님: Lock Delay 적용)
	if (IsGrounded(ActivePiece))
	{
		EnterLocking();
	}
}

void UTetrisGameCore::EnterLocking()
{
	EnterState(EGameState::Locking);
	LockDelay.OnGrounded();
}

void UTetrisGameCore::BeginSpawn()
{
	EnterState(EGameState::Spawn);

	const EPieceType Next = Randomizer->Dequeue();
	ActivePiece = FTetrisPieceOps::Spawn(Next);

	const TArray<FIntPoint> Coords = FTetrisPieceOps::GetAbsoluteBlockPositions(ActivePiece);
	if (Board->IsBlockOut(Coords))
	{
		TriggerGameOver(ETopOutType::BlockOut);
		return;
	}

	bHoldUsedThisPiece = false;
	ResetPerPieceState(GetLowestBlockY(ActivePiece));

	EnterState(EGameState::Falling);
	OnActivePieceUpdated.Broadcast(ActivePiece);
}

void UTetrisGameCore::DoHold()
{
	if (!bHoldEnabled || bHoldUsedThisPiece)
	{
		return; // 락 전 1회 제한 / 기능 비활성
	}

	const EPieceType CurrentType = ActivePiece.Type;
	EPieceType TypeToSpawn;
	if (HoldSlot == EPieceType::None)
	{
		HoldSlot = CurrentType;
		TypeToSpawn = Randomizer->Dequeue();
	}
	else
	{
		TypeToSpawn = HoldSlot;
		HoldSlot = CurrentType;
	}

	ActivePiece = FTetrisPieceOps::Spawn(TypeToSpawn);
	bHoldUsedThisPiece = true;
	ResetPerPieceState(GetLowestBlockY(ActivePiece));

	OnHoldChanged.Broadcast(HoldSlot, /*bCanHold*/ false);

	// 교환된 피스의 스폰 위치 충돌 검사 (Edge 3).
	const TArray<FIntPoint> Coords = FTetrisPieceOps::GetAbsoluteBlockPositions(ActivePiece);
	if (Board->IsBlockOut(Coords))
	{
		OnActivePieceUpdated.Broadcast(ActivePiece);
		TriggerGameOver(ETopOutType::BlockOut);
		return;
	}

	EnterState(EGameState::Falling);
	OnActivePieceUpdated.Broadcast(ActivePiece);
}

void UTetrisGameCore::DoHardDrop()
{
	const int32 Distance = FTetrisPieceOps::HardDrop(ActivePiece, *Board);
	HardDropCells += Distance;
	if (Distance > 0)
	{
		OnActivePieceUpdated.Broadcast(ActivePiece);
	}
	DoLock();
}

void UTetrisGameCore::DoLock()
{
	// 락 시점의 좌표·메타데이터를 캡처 (라인 클리어로 보드가 바뀌기 전).
	const TArray<FIntPoint> Coords = FTetrisPieceOps::GetAbsoluteBlockPositions(ActivePiece);
	const EPieceType LockedType = ActivePiece.Type;
	const bool bRot = ActivePiece.bWasLastActionRotation;
	const int32 Kick = ActivePiece.LastKickIndex;
	const int32 LockedSoftCells = SoftDropCells;
	const int32 LockedHardCells = HardDropCells;

	Board->LockPiece(Coords, LockedType);

	EnterState(EGameState::LineClear);
	const FLineClearResult Result = Board->ClearLines();

	// Score/Level 갱신 — GameCore가 Scoring을 직접 보유하므로 lock 시점에 직접 호출(pull/push 분리).
	//   ClearedRows는 점수 계산에 불필요하므로 전달하지 않는다. difficult 분류는 Scoring 내부에서 수행.
	//   레벨이 오르면 다음 스텝의 GetGravityPerStep()이 갱신된 Level의 BaseG를 즉시 반영한다.
	if (Scoring)
	{
		Scoring->HandlePieceLocked(Result.LinesCleared, bRot, Kick, LockedSoftCells, LockedHardCells);
	}

	// 매 lock마다(0줄 포함) 발행 — ViewModel(라인 연출 등)이 구독. ClearedRows 포함.
	// 의도적으로 LockOut 검사 *전*에 발행: fsm.md 전이표가 "ClearLines() 처리 → IsLockOut 검사" 순서이며,
	//   배치된 피스의 lock/클리어는 (게임오버를 유발하더라도) 점수에 반영되는 것이 정상이다.
	OnPieceLocked.Broadcast(Result.LinesCleared, Result.ClearedRows, bRot, Kick, LockedSoftCells, LockedHardCells);

	// Lock Out: 락된 좌표가 모두 버퍼존(Y >= Visible)이면 게임 오버.
	if (Board->IsLockOut(Coords))
	{
		TriggerGameOver(ETopOutType::LockOut);
		return;
	}

	BeginSpawn();
}

void UTetrisGameCore::TriggerGameOver(ETopOutType Type)
{
	EnterState(EGameState::GameOver);
	OnGameOver.Broadcast(Type);
}

void UTetrisGameCore::EnterState(EGameState NewState)
{
	if (CurrentState == NewState)
	{
		return; // 동일 상태 재진입은 이벤트 미발행
	}
	const EGameState Old = CurrentState;
	CurrentState = NewState;
	OnStateChanged.Broadcast(Old, NewState);
}

bool UTetrisGameCore::IsGrounded(const FActivePiece& Piece) const
{
	const TArray<FIntPoint> Shifted = FTetrisPieceOps::GetAbsoluteBlockPositionsAt(
		Piece.Type, Piece.PivotPosition + FIntPoint(0, -1), Piece.RotationState);
	return !Board->IsValidPosition(Shifted);
}

int32 UTetrisGameCore::GetLowestBlockY(const FActivePiece& Piece) const
{
	const TArray<FIntPoint> Coords = FTetrisPieceOps::GetAbsoluteBlockPositions(Piece);
	int32 MinY = MAX_int32;
	for (const FIntPoint& P : Coords)
	{
		MinY = FMath::Min(MinY, P.Y);
	}
	return MinY;
}

double UTetrisGameCore::GetGravityPerStep() const
{
	// BaseG는 Scoring 소유 커브. 없으면 Level 1 등가(1칸/SimHz)로 폴백.
	const double BaseG = Scoring ? (double)Scoring->GetBaseG(GetCurrentLevel()) : (1.0 / (double)SimHz);
	const double Factor = bSoftDropHeld ? (double)SoftDropFactor : 1.0;
	return BaseG * Factor;
}

int32 UTetrisGameCore::GetCurrentLevel() const
{
	return Scoring ? Scoring->GetLevel() : 1;
}

void UTetrisGameCore::ResetPerPieceState(int32 LowestBlockY)
{
	GravityAccumulator = 0.0;
	SoftDropCells = 0;
	HardDropCells = 0;
	LockDelay.OnNewPiece(LowestBlockY);
}
