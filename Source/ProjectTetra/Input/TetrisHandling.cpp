// Copyright ProjectTetra. All Rights Reserved.

#include "Input/TetrisHandling.h"

void FTetrisHandling::Reset()
{
	ActiveDir = EActiveDir::None;
	DASCharge = 0;
	ARRCounter = 0;
	bRepeating = false;
}

int32 FTetrisHandling::GetDASSteps() const
{
	// H1: round(ms / 1000 × SimHz). double 연산으로 결정적 반올림.
	return FMath::RoundToInt(Config.DASms / 1000.0 * Config.SimHz);
}

int32 FTetrisHandling::GetARRSteps() const
{
	return FMath::RoundToInt(Config.ARRms / 1000.0 * Config.SimHz);
}

int32 FTetrisHandling::GetDCDSteps() const
{
	const int32 Raw = FMath::RoundToInt(Config.DCDms / 1000.0 * Config.SimHz);
	return FMath::Clamp(Raw, 0, GetDASSteps());
}

EGameCommand FTetrisHandling::MoveCommandFor(EActiveDir Dir)
{
	// None으로는 호출되지 않는다(active일 때만 이동 명령 방출).
	return (Dir == EActiveDir::Left) ? EGameCommand::MoveLeft : EGameCommand::MoveRight;
}

void FTetrisHandling::StartDirection(EActiveDir NewDir, bool bApplyDCD, TArray<EGameCommand>& Out)
{
	const bool bSwitch = (ActiveDir != EActiveDir::None && ActiveDir != NewDir);

	Out.Add(MoveCommandFor(NewDir)); // 즉시 1칸 (press/전환/폴백 공통)
	ActiveDir = NewDir;
	ARRCounter = 0;
	bRepeating = false;

	// H5: 명시적 방향 전환만 DCD로 DAS를 부분 보존. 신규 press·재탭·폴백(반대 held)은 풀 재충전.
	//   DCD=0 → DASCharge=DASSteps(스냅 전환), DCD=DAS → DASCharge=0(풀 재충전).
	DASCharge = (bApplyDCD && bSwitch) ? (GetDASSteps() - GetDCDSteps()) : 0;
}

void FTetrisHandling::StopDirection()
{
	ActiveDir = EActiveDir::None;
	DASCharge = 0;
	ARRCounter = 0;
	bRepeating = false;
}

void FTetrisHandling::EmitRepeat(EGameCommand Cmd, int32 ARRSteps, TArray<EGameCommand>& Out) const
{
	if (ARRSteps == 0)
	{
		// H4: ARR=0 → 한 스텝에 벽까지. Handling은 보드 비참조이므로 BoardWidth개를 방출하고
		//   FSM의 TryMove가 벽에서 무해하게 클램프한다.
		for (int32 i = 0; i < TetrisConstants::BoardWidth; ++i)
		{
			Out.Add(Cmd);
		}
	}
	else
	{
		Out.Add(Cmd);
	}
}

void FTetrisHandling::AdvanceAutoShift(TArray<EGameCommand>& Out)
{
	const int32 DAS = GetDASSteps();
	const int32 ARR = GetARRSteps();
	const EGameCommand Cmd = MoveCommandFor(ActiveDir);

	if (!bRepeating)
	{
		// Charging (H2): 매 스텝 +1, DASSteps 도달 스텝에서 Repeating 진입 + 첫 자동반복 방출.
		++DASCharge;
		if (DASCharge >= DAS)
		{
			bRepeating = true;
			ARRCounter = 0;
			EmitRepeat(Cmd, ARR, Out);
		}
		return;
	}

	// Repeating
	if (ARR == 0)
	{
		EmitRepeat(Cmd, ARR, Out); // 매 스텝 벽까지 (H4)
		return;
	}

	// H3: 매 스텝 +1, ARRSteps마다 1칸.
	++ARRCounter;
	if (ARRCounter >= ARR)
	{
		Out.Add(Cmd);
		ARRCounter = 0;
	}
}

TArray<EGameCommand> FTetrisHandling::AdvanceStep(const FHandlingInput& In)
{
	TArray<EGameCommand> Out;

	// Phase 1: 이산 명령은 순서 보존 방출. 방향 press는 마지막 것만 채택(last-input priority).
	bool bHasPress = false;
	EActiveDir PressDir = EActiveDir::None;
	for (const EInputEdge Edge : In.Edges)
	{
		switch (Edge)
		{
		case EInputEdge::RotateCW:        Out.Add(EGameCommand::RotateCW); break;
		case EInputEdge::RotateCCW:       Out.Add(EGameCommand::RotateCCW); break;
		case EInputEdge::HardDrop:        Out.Add(EGameCommand::HardDrop); break;
		case EInputEdge::Hold:            Out.Add(EGameCommand::Hold); break;
		case EInputEdge::SoftDropOn:      Out.Add(EGameCommand::SoftDropOn); break;
		case EInputEdge::SoftDropOff:     Out.Add(EGameCommand::SoftDropOff); break;
		case EInputEdge::MoveLeftPress:   bHasPress = true; PressDir = EActiveDir::Left; break;
		case EInputEdge::MoveRightPress:  bHasPress = true; PressDir = EActiveDir::Right; break;
		case EInputEdge::MoveLeftRelease:
		case EInputEdge::MoveRightRelease:
			// held-state(bLeftHeld/bRightHeld)가 권위 — release 엣지는 별도 처리 불필요.
			break;
		}
	}

	// Phase 2: 방향 이동.
	bool bStartedThisStep = false;
	if (bHasPress)
	{
		StartDirection(PressDir, /*bApplyDCD=*/true, Out); // 명시적 press → 전환이면 DCD 적용
		bStartedThisStep = true;
	}

	// active 방향이 더 이상 held가 아니면: 반대가 held면 그쪽으로 전환(풀 DAS), 아니면 정지.
	if (ActiveDir == EActiveDir::Left && !In.bLeftHeld)
	{
		if (In.bRightHeld) { StartDirection(EActiveDir::Right, /*bApplyDCD=*/false, Out); bStartedThisStep = true; }
		else { StopDirection(); }
	}
	else if (ActiveDir == EActiveDir::Right && !In.bRightHeld)
	{
		if (In.bLeftHeld) { StartDirection(EActiveDir::Left, /*bApplyDCD=*/false, Out); bStartedThisStep = true; }
		else { StopDirection(); }
	}

	// 자동반복: active이고 이번 스텝에 새로 시작하지 않았을 때만(시작 스텝은 1칸만 방출).
	if (ActiveDir != EActiveDir::None && !bStartedThisStep)
	{
		AdvanceAutoShift(Out);
	}

	return Out;
}
