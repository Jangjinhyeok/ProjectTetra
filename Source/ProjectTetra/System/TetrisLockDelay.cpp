// Copyright ProjectTetra. All Rights Reserved.

#include "System/TetrisLockDelay.h"

int32 FTetrisLockDelay::GetLockDelaySteps() const
{
	// 정수 스텝 환산: round(LockDelayMs/1000 × SimHz).
	// Why: float 초 누적은 플랫폼/프레임 차이로 결정성이 깨질 수 있어 스텝 카운트로 고정한다.
	return FMath::RoundToInt(Config.LockDelayMs * Config.SimHz / 1000.0f);
}

void FTetrisLockDelay::OnNewPiece(int32 LowestBlockY)
{
	LockElapsedSteps = 0;
	MoveResetCounter = 0;
	LowestRowRecord = LowestBlockY;
	bActive = false;
}

void FTetrisLockDelay::OnGrounded()
{
	bActive = true;
	LockElapsedSteps = 0;
}

bool FTetrisLockDelay::Tick()
{
	if (!bActive)
	{
		return false;
	}
	LockElapsedSteps += 1;
	// LockDelaySteps=0(즉시 Lock 모드)이면 첫 Tick에서 1 >= 0 → 즉시 만료.
	return LockElapsedSteps >= GetLockDelaySteps();
}

void FTetrisLockDelay::NotifyMoveOrRotate(bool bStillGrounded, int32 NewLowestBlockY)
{
	// 정당한 하강(최저행 갱신) 여부. bLowestRowRefresh=false면 항상 false.
	const bool bLowered = Config.bLowestRowRefresh && (NewLowestBlockY < LowestRowRecord);

	if (!bStillGrounded)
	{
		// 다시 떠오름 → 타이머 중지. 카운터는 유지해 제자리 진동 우회를 차단한다.
		// 단 정당한 하강으로 최저행이 갱신됐다면 예산을 리필한다(Edge: 하강 = 리필).
		if (bLowered)
		{
			LowestRowRecord = NewLowestBlockY;
			MoveResetCounter = 0;
		}
		bActive = false;
		return;
	}

	if (bLowered)
	{
		// 정당한 하강: 예산 리필(=0) + 타이머 리셋. 한도 도달 상태보다 우선한다.
		// (증가는 하지 않는다 — "아래로 진행 = 예산 리필"이 일반 Move Reset을 대체)
		LowestRowRecord = NewLowestBlockY;
		MoveResetCounter = 0;
		LockElapsedSteps = 0;
	}
	else if (MoveResetCounter < Config.MoveResetLimit)
	{
		// 같은 행 내 이동/회전: Extended Placement 타이머 리셋.
		LockElapsedSteps = 0;
		MoveResetCounter += 1;
	}
	// 한도 도달 + 최저행 불변: 리셋 없음. 타이머는 계속 흘러 다음 만료에서 강제 Lock.
}
