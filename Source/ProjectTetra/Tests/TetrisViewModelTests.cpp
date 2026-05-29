// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UI/ViewModel/TetrisHUDViewModel.h"
#include "UI/ViewModel/TetrisHUDViewModelBinder.h"
#include "FSM/TetrisGameCore.h"
#include "Board/TetrisBoard.h"
#include "System/TetrisScoring.h"
#include "System/TetrisRandomizer.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// VM의 private setter를 구동하는 테스트 전용 seam(production 접근은 바인더 friend로 제한). 헤더의 friend 선언과 짝.
struct FTetrisHUDViewModelTestAccess
{
	static void SetScore(UTetrisHUDViewModel* VM, int64 V) { VM->SetScore(V); }
	static void SetLevel(UTetrisHUDViewModel* VM, int32 V) { VM->SetLevel(V); }
	static void SetCombo(UTetrisHUDViewModel* VM, int32 V) { VM->SetCombo(V); }
	static void SetHoldPiece(UTetrisHUDViewModel* VM, EPieceType V) { VM->SetHoldPiece(V); }
	static void SetCanHold(UTetrisHUDViewModel* VM, bool b) { VM->SetCanHold(b); }
	static void SetNextQueue(UTetrisHUDViewModel* VM, const TArray<EPieceType>& V) { VM->SetNextQueue(V); }
	static void SetGameState(UTetrisHUDViewModel* VM, EGameState V) { VM->SetGameState(V); }
};

namespace
{
	// 특정 FieldNotify 필드의 발행 횟수를 세는 구독 헬퍼(HANDOFF: 델리게이트 구독 카운트로 발행 검증).
	struct FFieldCounter
	{
		int32 Count = 0;

		void Subscribe(UTetrisHUDViewModel* VM, UE::FieldNotification::FFieldId FieldId)
		{
			VM->AddFieldValueChangedDelegate(FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
					[this](UObject*, UE::FieldNotification::FFieldId) { ++Count; }));
		}
	};
}

//~ setter 반영 + 발행 게이트(V1) ---------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHUDViewModelSetterNotifyTest,
	"Tetris.ViewModel.SetterReflectsAndNotifiesOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHUDViewModelSetterNotifyTest::RunTest(const FString&)
{
	UTetrisHUDViewModel* VM = NewObject<UTetrisHUDViewModel>();
	FFieldCounter ScoreN;
	ScoreN.Subscribe(VM, UTetrisHUDViewModel::FFieldNotificationClassDescriptor::Score);

	FTetrisHUDViewModelTestAccess::SetScore(VM, 1234);
	TestEqual(TEXT("getter가 새 값 반영"), VM->GetScore(), (int64)1234);
	TestEqual(TEXT("값 변경 → FieldNotify 1회"), ScoreN.Count, 1);

	// V1: 동일값 재설정 → 미발행
	FTetrisHUDViewModelTestAccess::SetScore(VM, 1234);
	TestEqual(TEXT("동일값 재설정 → 발행 없음(V1)"), ScoreN.Count, 1);

	// 다른 값 → 다시 발행
	FTetrisHUDViewModelTestAccess::SetScore(VM, 5678);
	TestEqual(TEXT("새 값 → 발행 +1"), ScoreN.Count, 2);
	return true;
}

//~ 계산 프로퍼티(computed FieldNotify) -----------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHUDViewModelScoreTextDerivedTest,
	"Tetris.ViewModel.ScoreTextDerivedNotify", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHUDViewModelScoreTextDerivedTest::RunTest(const FString&)
{
	UTetrisHUDViewModel* VM = NewObject<UTetrisHUDViewModel>();
	FFieldCounter ScoreTextN;
	ScoreTextN.Subscribe(VM, UTetrisHUDViewModel::FFieldNotificationClassDescriptor::GetScoreText);

	FTetrisHUDViewModelTestAccess::SetScore(VM, 999);
	TestEqual(TEXT("Score 변경 → GetScoreText 파생 통지 1회"), ScoreTextN.Count, 1);
	TestTrue(TEXT("GetScoreText == AsNumber(Score)"), VM->GetScoreText().EqualTo(FText::AsNumber((int64)999)));

	// 동일 Score면 파생 통지도 미발행(SetScore의 변경 게이트 안에서만 broadcast)
	FTetrisHUDViewModelTestAccess::SetScore(VM, 999);
	TestEqual(TEXT("동일 Score → 파생 통지 없음"), ScoreTextN.Count, 1);
	return true;
}

//~ 다양한 타입 필드(enum/bool) -------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHUDViewModelEnumBoolFieldsTest,
	"Tetris.ViewModel.EnumAndBoolFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHUDViewModelEnumBoolFieldsTest::RunTest(const FString&)
{
	UTetrisHUDViewModel* VM = NewObject<UTetrisHUDViewModel>();
	FFieldCounter HoldN, CanHoldN, StateN;
	HoldN.Subscribe(VM, UTetrisHUDViewModel::FFieldNotificationClassDescriptor::HoldPiece);
	CanHoldN.Subscribe(VM, UTetrisHUDViewModel::FFieldNotificationClassDescriptor::bCanHold);
	StateN.Subscribe(VM, UTetrisHUDViewModel::FFieldNotificationClassDescriptor::GameState);

	FTetrisHUDViewModelTestAccess::SetHoldPiece(VM, EPieceType::T);
	FTetrisHUDViewModelTestAccess::SetCanHold(VM, false);
	FTetrisHUDViewModelTestAccess::SetGameState(VM, EGameState::Falling);

	TestEqual(TEXT("HoldPiece 반영"), VM->GetHoldPiece(), EPieceType::T);
	TestEqual(TEXT("bCanHold 반영"), VM->GetCanHold(), false);
	TestEqual(TEXT("GameState 반영"), VM->GetGameState(), EGameState::Falling);
	TestEqual(TEXT("HoldPiece 발행 1"), HoldN.Count, 1);
	TestEqual(TEXT("bCanHold 발행 1"), CanHoldN.Count, 1);
	TestEqual(TEXT("GameState 발행 1"), StateN.Count, 1);

	// 동일값 재설정 → 미발행(V1)
	FTetrisHUDViewModelTestAccess::SetCanHold(VM, false);
	TestEqual(TEXT("bCanHold 동일값 → 발행 없음"), CanHoldN.Count, 1);
	return true;
}

//~ NextQueue: 표시 개수 절단 + V1 ----------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHUDViewModelNextQueueTruncateTest,
	"Tetris.ViewModel.NextQueueTruncatesToDisplayCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHUDViewModelNextQueueTruncateTest::RunTest(const FString&)
{
	UTetrisHUDViewModel* VM = NewObject<UTetrisHUDViewModel>();
	FFieldCounter QueueN;
	QueueN.Subscribe(VM, UTetrisHUDViewModel::FFieldNotificationClassDescriptor::NextQueue);

	// 7종 큐 → 표시 개수(기본 5)로 절단
	const TArray<EPieceType> Seven = { EPieceType::I, EPieceType::O, EPieceType::T, EPieceType::S, EPieceType::Z, EPieceType::J, EPieceType::L };
	FTetrisHUDViewModelTestAccess::SetNextQueue(VM, Seven);
	TestEqual(TEXT("표시 개수 5로 절단"), VM->GetNextQueue().Num(), 5);
	TestEqual(TEXT("절단된 첫 요소 유지"), VM->GetNextQueue()[0], EPieceType::I);
	TestEqual(TEXT("NextQueue 발행 1"), QueueN.Count, 1);

	// 동일 앞 5요소를 다시 → 절단 후 동일 시퀀스 → V1 미발행
	const TArray<EPieceType> SameFirstFive = { EPieceType::I, EPieceType::O, EPieceType::T, EPieceType::S, EPieceType::Z, EPieceType::I };
	FTetrisHUDViewModelTestAccess::SetNextQueue(VM, SameFirstFive);
	TestEqual(TEXT("동일 앞5 시퀀스 → 발행 없음(V1)"), QueueN.Count, 1);

	// 앞 5요소가 달라지면 발행
	const TArray<EPieceType> Different = { EPieceType::O, EPieceType::O, EPieceType::T, EPieceType::S, EPieceType::Z };
	FTetrisHUDViewModelTestAccess::SetNextQueue(VM, Different);
	TestEqual(TEXT("앞5 변경 → 발행 +1"), QueueN.Count, 2);
	return true;
}

//~ 기본값(생성 직후 스냅샷 전 상태) -------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisHUDViewModelDefaultsTest,
	"Tetris.ViewModel.DefaultFieldValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisHUDViewModelDefaultsTest::RunTest(const FString&)
{
	UTetrisHUDViewModel* VM = NewObject<UTetrisHUDViewModel>();
	TestEqual(TEXT("Score 기본 0"), VM->GetScore(), (int64)0);
	TestEqual(TEXT("Level 기본 1"), VM->GetLevel(), 1);
	TestEqual(TEXT("Lines 기본 0"), VM->GetLines(), 0);
	TestEqual(TEXT("Combo 기본 -1"), VM->GetCombo(), -1);
	TestEqual(TEXT("B2BCount 기본 0"), VM->GetB2BCount(), 0);
	TestEqual(TEXT("HoldPiece 기본 None"), VM->GetHoldPiece(), EPieceType::None);
	TestEqual(TEXT("bCanHold 기본 true"), VM->GetCanHold(), true);
	TestEqual(TEXT("NextQueue 기본 비어있음"), VM->GetNextQueue().Num(), 0);
	TestEqual(TEXT("GameState 기본 Idle"), VM->GetGameState(), EGameState::Idle);
	return true;
}

//~ G3: 바인더 통합 (NewObject Model, PIE/위젯 불필요) -------------------------

namespace
{
	// Board/Randomizer/Scoring/GameCore를 NewObject로 만들고 와이어링·시작한 묶음.
	struct FModelFixture
	{
		UTetrisBoard* Board = nullptr;
		UTetrisRandomizer* Randomizer = nullptr;
		UTetrisScoring* Scoring = nullptr;
		UTetrisGameCore* Core = nullptr;

		void StartAt(int64 Seed)
		{
			Board = NewObject<UTetrisBoard>();
			Randomizer = NewObject<UTetrisRandomizer>();
			Scoring = NewObject<UTetrisScoring>();
			Core = NewObject<UTetrisGameCore>();
			Core->Initialize(Board, Randomizer, Scoring);
			Core->StartGame(Seed); // 큐 채움 + 첫 피스 스폰 + 상태 전이
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBinderInitialSnapshotTest,
	"Tetris.ViewModel.BinderInitialSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBinderInitialSnapshotTest::RunTest(const FString&)
{
	FModelFixture M; M.StartAt(123);
	UTetrisHUDViewModelBinder* Binder = NewObject<UTetrisHUDViewModelBinder>();
	Binder->Bind(M.Core, M.Scoring, M.Randomizer);

	UTetrisHUDViewModel* VM = Binder->GetViewModel();
	TestNotNull(TEXT("Bind 후 VM 생성됨"), VM);

	// V2: 모든 필드 == Model getter 현재값.
	TestEqual(TEXT("Score 스냅샷"), VM->GetScore(), M.Scoring->GetScore());
	TestEqual(TEXT("Level 스냅샷"), VM->GetLevel(), M.Scoring->GetLevel());
	TestEqual(TEXT("Lines 스냅샷"), VM->GetLines(), M.Scoring->GetTotalLinesCleared());
	TestEqual(TEXT("Combo 스냅샷"), VM->GetCombo(), M.Scoring->GetCombo());
	TestEqual(TEXT("B2B 스냅샷"), VM->GetB2BCount(), M.Scoring->GetB2BCount());
	TestEqual(TEXT("Hold 스냅샷"), VM->GetHoldPiece(), M.Core->GetHoldSlot());
	TestEqual(TEXT("CanHold 스냅샷"), VM->GetCanHold(), M.Core->IsHoldAvailable());
	TestEqual(TEXT("GameState 스냅샷"), VM->GetGameState(), M.Core->GetState());

	// NextQueue는 표시 개수(5)로 절단되므로 앞 5요소 일치로 검증.
	const TArray<EPieceType>& RandQueue = M.Randomizer->GetNextQueue();
	TestTrue(TEXT("Next 큐 최소 5"), RandQueue.Num() >= 5);
	TestEqual(TEXT("VM NextQueue 5개"), VM->GetNextQueue().Num(), 5);
	bool bFirstFiveMatch = true;
	for (int32 i = 0; i < 5; ++i) { bFirstFiveMatch &= (VM->GetNextQueue()[i] == RandQueue[i]); }
	TestTrue(TEXT("Next 큐 앞5 일치"), bFirstFiveMatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBinderScoringPropagationTest,
	"Tetris.ViewModel.BinderScoringPropagation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBinderScoringPropagationTest::RunTest(const FString&)
{
	FModelFixture M; M.StartAt(123);
	UTetrisHUDViewModelBinder* Binder = NewObject<UTetrisHUDViewModelBinder>();
	Binder->Bind(M.Core, M.Scoring, M.Randomizer);
	UTetrisHUDViewModel* VM = Binder->GetViewModel();

	// 실제 진입점으로 점수/줄/콤보/B2B 변경 → 델리게이트 통해 VM 갱신.
	M.Scoring->HandlePieceLocked(4, false, INDEX_NONE, 0, 0); // Tetris
	TestEqual(TEXT("Score 전파"), VM->GetScore(), M.Scoring->GetScore());
	TestEqual(TEXT("Lines 전파"), VM->GetLines(), M.Scoring->GetTotalLinesCleared());
	TestEqual(TEXT("Combo 전파"), VM->GetCombo(), M.Scoring->GetCombo());
	TestEqual(TEXT("B2B 전파"), VM->GetB2BCount(), M.Scoring->GetB2BCount());
	TestTrue(TEXT("점수가 실제로 증가"), VM->GetScore() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBinderStateHoldQueuePropagationTest,
	"Tetris.ViewModel.BinderStateHoldQueuePropagation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBinderStateHoldQueuePropagationTest::RunTest(const FString&)
{
	FModelFixture M; M.StartAt(123);
	UTetrisHUDViewModelBinder* Binder = NewObject<UTetrisHUDViewModelBinder>();
	Binder->Bind(M.Core, M.Scoring, M.Randomizer);
	UTetrisHUDViewModel* VM = Binder->GetViewModel();

	// 델리게이트 직접 발행으로 바인더 구독 매핑을 결정적으로 검증(FSM 구동 불필요).
	M.Core->OnStateChanged.Broadcast(EGameState::Falling, EGameState::GameOver);
	TestEqual(TEXT("GameState 전파(GameOver)"), VM->GetGameState(), EGameState::GameOver);

	// OnHoldChanged 1건 → HoldPiece + bCanHold 둘 다 갱신.
	M.Core->OnHoldChanged.Broadcast(EPieceType::T, false);
	TestEqual(TEXT("HoldPiece 전파"), VM->GetHoldPiece(), EPieceType::T);
	TestEqual(TEXT("bCanHold 전파"), VM->GetCanHold(), false);

	// dynamic 델리게이트(OnNextQueueChanged) → NextQueue 갱신(절단 적용).
	const TArray<EPieceType> NewQ = { EPieceType::O, EPieceType::O, EPieceType::O, EPieceType::O, EPieceType::O, EPieceType::O };
	M.Randomizer->OnNextQueueChanged.Broadcast(NewQ);
	TestEqual(TEXT("NextQueue 5개로 갱신"), VM->GetNextQueue().Num(), 5);
	TestEqual(TEXT("NextQueue 첫 요소 O"), VM->GetNextQueue()[0], EPieceType::O);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBinderUnbindStopsUpdatesTest,
	"Tetris.ViewModel.BinderUnbindStopsUpdates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBinderUnbindStopsUpdatesTest::RunTest(const FString&)
{
	FModelFixture M; M.StartAt(123);
	UTetrisHUDViewModelBinder* Binder = NewObject<UTetrisHUDViewModelBinder>();
	Binder->Bind(M.Core, M.Scoring, M.Randomizer);

	// Unbind 후에도 살아있도록 VM에 강참조 유지(GC 방지) — 구독 해제 효과만 검증.
	TStrongObjectPtr<UTetrisHUDViewModel> VM(Binder->GetViewModel());
	const int64 ScoreBefore = VM->GetScore();

	Binder->Unbind();
	TestNull(TEXT("Unbind 후 바인더 VM 참조 해제"), Binder->GetViewModel());

	// 구독이 끊겼으므로 이후 발행은 VM에 반영되지 않아야 한다.
	M.Scoring->OnScoreChanged.Broadcast(999999);
	M.Core->OnHoldChanged.Broadcast(EPieceType::I, false);
	M.Randomizer->OnNextQueueChanged.Broadcast({ EPieceType::Z, EPieceType::Z, EPieceType::Z, EPieceType::Z, EPieceType::Z });

	TestEqual(TEXT("Unbind 후 Score 불변"), VM->GetScore(), ScoreBefore);
	TestNotEqual(TEXT("Unbind 후 Hold 미갱신(I 아님)"), VM->GetHoldPiece(), EPieceType::I);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
