// Copyright ProjectTetra. All Rights Reserved.

#include "System/TetrisScoring.h"

UTetrisScoring::UTetrisScoring()
{
	ResetScoring();
}

void UTetrisScoring::ResetScoring()
{
	Score = 0;
	TotalLinesCleared = 0;
	Level = 1;
	Combo = -1;
	B2BCount = 0;
	bLastClearWasDifficult = false;
}

void UTetrisScoring::HandlePieceLocked(int32 LinesCleared, bool /*bWasLastActionRotation*/, int32 /*LastKickIndex*/, int32 SoftDropCells, int32 HardDropCells)
{
	// 변경 감지를 위한 스냅샷 — 변경된 값만 이벤트를 발행한다.
	const int64 OldScore = Score;
	const int32 OldLevel = Level;
	const int32 OldLines = TotalLinesCleared;
	const int32 OldCombo = Combo;
	const int32 OldB2B = B2BCount;

	const int32 N = LinesCleared;

	// 드롭 점수(S4)는 0줄 lock에도 동일하게 가산된다.
	const int64 DropScore = (int64)SoftDropCells * 1 + (int64)HardDropCells * 2;

	if (N > 0)
	{
		// 1) Combo 갱신 (클리어 성공 → 진행)
		Combo += 1;

		// 2) Difficult 분류 (MVP: Tetris만). VS에서 회전/Kick 정보로 T-Spin 편입.
		const bool bDifficult = (N == 4);

		// 3) B2B — ×1.5는 "직전도 difficult"일 때만. (갱신 전 bLastClearWasDifficult 기준)
		const bool bB2BActive = (bLastClearWasDifficult && bDifficult);
		if (bB2BActive)
		{
			B2BCount += 1;
		}
		else if (!bDifficult)
		{
			// non-difficult 클리어는 B2B 체인을 끊는다. (difficult 첫 클리어는 0 유지)
			B2BCount = 0;
		}

		// 4) 점수 — 클리어 직전(현재) Level로 계산 (Edge 6: 가산 → 레벨 갱신 순서 고정)
		const int32 BaseIndex = FMath::Clamp(N, 0, Config.LineBase.Num() - 1);
		int64 LineScore = (int64)Config.LineBase[BaseIndex] * Level;
		if (bB2BActive)
		{
			LineScore = (int64)((double)LineScore * (double)Config.B2BMultiplier);
		}
		const int64 ComboBonus = (Combo >= 1) ? (int64)Config.ComboBaseValue * Combo * Level : 0;
		Score += LineScore + ComboBonus + DropScore;

		// 5) Level 재계산 (가산 이후)
		TotalLinesCleared += N;
		Level = FMath::Clamp(1 + TotalLinesCleared / Config.LinesPerLevel, 1, Config.LevelCap);

		// 6) difficult 상태 기록 (다음 lock의 B2B 판정 기준)
		bLastClearWasDifficult = bDifficult;
	}
	else
	{
		// 0줄 lock: Combo 리셋. B2B/bLastClearWasDifficult는 유지(Edge 4). 드롭 점수만 가산.
		Combo = -1;
		Score += DropScore;
	}

	// 7) 변경된 값마다 이벤트 발행
	if (Score != OldScore) { OnScoreChanged.Broadcast(Score); }
	if (Level != OldLevel) { OnLevelChanged.Broadcast(Level); }
	if (TotalLinesCleared != OldLines) { OnLinesChanged.Broadcast(TotalLinesCleared); }
	if (Combo != OldCombo) { OnComboChanged.Broadcast(Combo); }
	if (B2BCount != OldB2B) { OnB2BChanged.Broadcast(B2BCount); }
}

float UTetrisScoring::GetBaseG(int32 InLevel) const
{
	// BaseG(Level) = 1 / (SecPerRow × SimHz), SecPerRow = (GravityBase − (L−1)×GravityDecay)^(L−1)
	// L은 [1, LevelCap]로 클램프 — 커브 발산/음수 밑 방지.
	const int32 L = FMath::Clamp(InLevel, 1, Config.LevelCap);
	const double Base = (double)Config.GravityBase - (double)(L - 1) * (double)Config.GravityDecay;
	const double SecPerRow = FMath::Pow(Base, (double)(L - 1));
	return (float)(1.0 / (SecPerRow * (double)Config.SimHz));
}
