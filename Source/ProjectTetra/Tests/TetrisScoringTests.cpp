// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "System/TetrisScoring.h"

#if WITH_DEV_AUTOMATION_TESTS

// 모든 테스트는 HandlePieceLocked 호출만으로 동작한다 — Board/FSM 인스턴스 없이 Scoring 격리 검증.
// (scoring.md Acceptance "이벤트 / 격리")

namespace
{
	// 클리어 lock 헬퍼: 드롭 칸 0, 회전/Kick 정보는 MVP 미사용이므로 기본값.
	FORCEINLINE void Clear(UTetrisScoring* S, int32 Lines)
	{
		S->HandlePieceLocked(Lines, false, INDEX_NONE, 0, 0);
	}
}

//~ 기본 점수 (× Level) --------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringBaseScoresTest,
	"Tetris.Scoring.BaseLineScores", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringBaseScoresTest::RunTest(const FString&)
{
	// 각 클리어를 fresh 인스턴스에서 측정 → 콤보/B2B 간섭 제거. Level 1 기준.
	{ UTetrisScoring* S = NewObject<UTetrisScoring>(); Clear(S, 1); TestEqual(TEXT("Single +100"), S->GetScore(), (int64)100); }
	{ UTetrisScoring* S = NewObject<UTetrisScoring>(); Clear(S, 2); TestEqual(TEXT("Double +300"), S->GetScore(), (int64)300); }
	{ UTetrisScoring* S = NewObject<UTetrisScoring>(); Clear(S, 3); TestEqual(TEXT("Triple +500"), S->GetScore(), (int64)500); }
	{ UTetrisScoring* S = NewObject<UTetrisScoring>(); Clear(S, 4); TestEqual(TEXT("Tetris +800"), S->GetScore(), (int64)800); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringTetrisTimesLevelTest,
	"Tetris.Scoring.TetrisTimesLevel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringTetrisTimesLevelTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	// 20 싱글로 Level 3 도달 (TotalLines=20 → 1+20/10=3)
	for (int32 i = 0; i < 20; ++i) { Clear(S, 1); }
	TestEqual(TEXT("20줄 → Level 3"), S->GetLevel(), 3);

	// 콤보·B2B 중립화: 0줄 lock (Combo=-1, 직전 싱글들은 non-difficult → B2B 미적용)
	Clear(S, 0);
	const int64 Before = S->GetScore();
	Clear(S, 4); // Level 3 Tetris, 첫 difficult, Combo 0 → 순수 800×3
	TestEqual(TEXT("Level 3 Tetris → +2400"), S->GetScore() - Before, (int64)2400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringZeroLineNoScoreTest,
	"Tetris.Scoring.ZeroLineNoScoreChange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringZeroLineNoScoreTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 0); // 드롭 칸 없음
	TestEqual(TEXT("0줄 + 드롭 0 → 점수 불변"), S->GetScore(), (int64)0);
	return true;
}

//~ Back-to-Back ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringB2BFirstTetrisTest,
	"Tetris.Scoring.B2BFirstTetrisNoMultiplier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringB2BFirstTetrisTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 4); // 첫 Tetris
	TestEqual(TEXT("첫 Tetris ×1.5 미적용 (800×1)"), S->GetScore(), (int64)800);
	TestEqual(TEXT("첫 Tetris B2BCount 0"), S->GetB2BCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringB2BSecondTetrisTest,
	"Tetris.Scoring.B2BSecondTetrisMultiplier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringB2BSecondTetrisTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 4);          // Tetris1 (B2B 0, bLast difficult)
	Clear(S, 0);          // 0줄: 콤보 중립화, B2B 유지
	const int64 Before = S->GetScore();
	Clear(S, 4);          // Tetris2 (B2B 활성 → ×1.5)
	TestEqual(TEXT("연속 Tetris ×1.5 (800×1×1.5)"), S->GetScore() - Before, (int64)1200);
	TestEqual(TEXT("B2BCount 1"), S->GetB2BCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringB2BBrokenTest,
	"Tetris.Scoring.B2BBrokenBySingle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringB2BBrokenTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 4); Clear(S, 0); Clear(S, 4); // B2BCount 1로 진행
	TestEqual(TEXT("연속 Tetris → B2BCount 1"), S->GetB2BCount(), 1);

	Clear(S, 0);                            // 콤보 중립화 (B2B 유지)
	const int64 Before = S->GetScore();
	Clear(S, 1);                            // Single → B2B 종료
	TestEqual(TEXT("Single이 B2B 끊음 → B2BCount 0"), S->GetB2BCount(), 0);
	TestEqual(TEXT("Single은 일반 점수 (100×1, ×1.5 없음)"), S->GetScore() - Before, (int64)100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringB2BThroughZeroLineTest,
	"Tetris.Scoring.B2BMaintainedThroughZeroLine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringB2BThroughZeroLineTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 4); // Tetris1
	Clear(S, 0); // 0줄 lock — B2B 체인 유지
	const int64 Before = S->GetScore();
	Clear(S, 4); // Tetris2 → B2B 이어짐 ×1.5
	TestEqual(TEXT("0줄 거쳐도 B2B 이어짐 (×1.5)"), S->GetScore() - Before, (int64)1200);
	TestEqual(TEXT("B2BCount 1"), S->GetB2BCount(), 1);
	return true;
}

//~ Combo ----------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringComboProgressionTest,
	"Tetris.Scoring.ComboProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringComboProgressionTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 1); // 첫 클리어 Combo 0, 보너스 0
	TestEqual(TEXT("첫 클리어 Combo 0"), S->GetCombo(), 0);
	TestEqual(TEXT("첫 Single +100 (보너스 없음)"), S->GetScore(), (int64)100);

	const int64 Before = S->GetScore();
	Clear(S, 1); // 둘째 연속 Combo 1, 보너스 50×1×1
	TestEqual(TEXT("둘째 클리어 Combo 1"), S->GetCombo(), 1);
	TestEqual(TEXT("Single 100 + 콤보 50"), S->GetScore() - Before, (int64)150);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringComboResetTest,
	"Tetris.Scoring.ComboResetOnZeroLine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringComboResetTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 1); Clear(S, 1);
	TestEqual(TEXT("연속 클리어 Combo 1"), S->GetCombo(), 1);
	Clear(S, 0);
	TestEqual(TEXT("0줄 lock → Combo -1"), S->GetCombo(), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringComboBonusFormulaTest,
	"Tetris.Scoring.ComboBonusFormula", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringComboBonusFormulaTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	Clear(S, 1); Clear(S, 1); Clear(S, 1); // Combo 0,1,2
	const int64 Before = S->GetScore();
	Clear(S, 1); // 4번째 → Combo 3, 보너스 50×3×Level(1)
	TestEqual(TEXT("Combo 3"), S->GetCombo(), 3);
	TestEqual(TEXT("Single 100 + 콤보 150 (50×3×1)"), S->GetScore() - Before, (int64)250);
	return true;
}

//~ Level 진행 -----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringLevelProgressionTest,
	"Tetris.Scoring.LevelProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringLevelProgressionTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	for (int32 i = 0; i < 10; ++i) { Clear(S, 1); }
	TestEqual(TEXT("10줄 → Level 2"), S->GetLevel(), 2);
	for (int32 i = 0; i < 10; ++i) { Clear(S, 1); }
	TestEqual(TEXT("20줄 → Level 3"), S->GetLevel(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringLevelCapTest,
	"Tetris.Scoring.LevelCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringLevelCapTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	S->Config.LevelCap = 3; // 캡을 낮춰 검증
	for (int32 i = 0; i < 50; ++i) { Clear(S, 1); } // 50줄이면 캡 없으면 Level 6
	TestEqual(TEXT("LevelCap 도달 후 고정"), S->GetLevel(), 3);
	TestTrue(TEXT("BaseG도 캡 레벨로 고정 (50 == 3)"),
		FMath::IsNearlyEqual(S->GetBaseG(50), S->GetBaseG(3)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringBaseGCurveTest,
	"Tetris.Scoring.BaseGCurve", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringBaseGCurveTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	TestTrue(TEXT("BaseG(1) ≈ 0.0167"), FMath::IsNearlyEqual(S->GetBaseG(1), 0.01667f, 0.0005f));
	TestTrue(TEXT("BaseG(10) ≈ 0.258"), FMath::IsNearlyEqual(S->GetBaseG(10), 0.2596f, 0.01f));
	return true;
}

//~ 드롭 점수 ------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringDropScoreTest,
	"Tetris.Scoring.DropScores", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringDropScoreTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	S->HandlePieceLocked(0, false, INDEX_NONE, 3, 0); // Soft 3
	TestEqual(TEXT("Soft drop 3칸 → +3"), S->GetScore(), (int64)3);
	S->HandlePieceLocked(0, false, INDEX_NONE, 0, 4); // Hard 4
	TestEqual(TEXT("Hard drop 4칸 → +8 (누적 11)"), S->GetScore(), (int64)11);
	return true;
}

//~ 이벤트 / 격리 --------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringEventsOnlyChangedTest,
	"Tetris.Scoring.OnlyChangedEventsFire", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringEventsOnlyChangedTest::RunTest(const FString&)
{
	UTetrisScoring* S = NewObject<UTetrisScoring>();
	int32 ScoreN = 0, LevelN = 0, LinesN = 0, ComboN = 0, B2BN = 0;
	S->OnScoreChanged.AddLambda([&](int64) { ++ScoreN; });
	S->OnLevelChanged.AddLambda([&](int32) { ++LevelN; });
	S->OnLinesChanged.AddLambda([&](int32) { ++LinesN; });
	S->OnComboChanged.AddLambda([&](int32) { ++ComboN; });
	S->OnB2BChanged.AddLambda([&](int32) { ++B2BN; });

	Clear(S, 1); // Score↑, Lines↑, Combo(-1→0)↑ / Level(1→1) X, B2B(0→0) X
	TestEqual(TEXT("Score 이벤트 1"), ScoreN, 1);
	TestEqual(TEXT("Lines 이벤트 1"), LinesN, 1);
	TestEqual(TEXT("Combo 이벤트 1"), ComboN, 1);
	TestEqual(TEXT("Level 불변 → 미발행"), LevelN, 0);
	TestEqual(TEXT("B2B 불변 → 미발행"), B2BN, 0);

	Clear(S, 0); // Combo(0→-1)↑ 만, Score 불변(드롭 0)
	TestEqual(TEXT("0줄 후 Combo 이벤트 +1"), ComboN, 2);
	TestEqual(TEXT("0줄 후 Score 미발행"), ScoreN, 1);
	return true;
}

//~ 결정성 ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisScoringDeterminismTest,
	"Tetris.Scoring.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisScoringDeterminismTest::RunTest(const FString&)
{
	const int32 Seq[] = { 4, 0, 4, 1, 2, 0, 3, 4, 4, 1 };
	auto Run = [&](UTetrisScoring* S)
	{
		for (int32 N : Seq) { Clear(S, N); }
	};

	UTetrisScoring* A = NewObject<UTetrisScoring>();
	UTetrisScoring* B = NewObject<UTetrisScoring>();
	Run(A); Run(B);

	TestEqual(TEXT("동일 시퀀스 → 동일 점수"), A->GetScore(), B->GetScore());
	TestEqual(TEXT("동일 레벨"), A->GetLevel(), B->GetLevel());
	TestEqual(TEXT("동일 콤보"), A->GetCombo(), B->GetCombo());
	TestEqual(TEXT("동일 B2B"), A->GetB2BCount(), B->GetB2BCount());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
