// Copyright ProjectTetra. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/ViewModel/TetrisBoardViewModel.h"
#include "UI/ViewModel/TetrisBoardViewModelBinder.h"
#include "Core/TetrisTypes.h"
#include "Board/TetrisBoard.h"
#include "FSM/TetrisGameCore.h"
#include "Block/TetrisPiece.h"
#include "System/TetrisRandomizer.h"
#include "System/TetrisScoring.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 테스트 전용 setter 접근 seam. VM의 private setter를 호출하기 위해 friend로 선언됨(헤더 가드).
 * Why: production 캡슐화(바인더만 setter 호출)를 유지하면서 단위 테스트만 setter를 격리 구동.
 */
struct FTetrisBoardViewModelTestAccess
{
	static void SetLockedGrid(UTetrisBoardViewModel& VM, const TArray<EPieceType>& V) { VM.SetLockedGrid(V); }
	static void SetActivePieceType(UTetrisBoardViewModel& VM, EPieceType V) { VM.SetActivePieceType(V); }
	static void SetActiveCells(UTetrisBoardViewModel& VM, const TArray<FIntPoint>& V) { VM.SetActiveCells(V); }
	static void SetGhostCells(UTetrisBoardViewModel& VM, const TArray<FIntPoint>& V) { VM.SetGhostCells(V); }
	static void SetGameState(UTetrisBoardViewModel& VM, EGameState V) { VM.SetGameState(V); }
};

namespace
{
	// Why 고유 이름(Board 접두사): unity 빌드에서 HUD 테스트의 동명 헬퍼와 익명 네임스페이스 재정의 충돌 방지(commit 63fedfd 선례).
	/** FieldNotify 발행 횟수를 세는 헬퍼 — 구독 후 카운터 증가. */
	struct FBoardFieldBroadcastCounter
	{
		int32 Count = 0;

		FDelegateHandle Subscribe(UMVVMViewModelBase* VM, UE::FieldNotification::FFieldId FieldId)
		{
			return VM->AddFieldValueChangedDelegate(
				FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
					[this](UObject*, UE::FieldNotification::FFieldId) { ++Count; }));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetrisBoardViewModelLockedGridTest,
	"Tetris.BoardViewModel.LockedGridSetterNotifiesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardViewModelLockedGridTest::RunTest(const FString& Parameters)
{
	UTetrisBoardViewModel* VM = NewObject<UTetrisBoardViewModel>();
	TestNotNull(TEXT("VM created"), VM);

	FBoardFieldBroadcastCounter Counter;
	Counter.Subscribe(VM, UTetrisBoardViewModel::FFieldNotificationClassDescriptor::LockedGrid);

	// 일부 칸을 채운 그리드 — 최초 설정 시 1회 발행 기대.
	TArray<EPieceType> Grid;
	Grid.SetNum(TetrisConstants::BoardWidth * TetrisConstants::BoardVisibleHeight);
	for (EPieceType& Cell : Grid) { Cell = EPieceType::None; }
	Grid[0] = EPieceType::T;

	FTetrisBoardViewModelTestAccess::SetLockedGrid(*VM, Grid);
	TestEqual(TEXT("LockedGrid broadcast once after change"), Counter.Count, 1);
	TestEqual(TEXT("LockedGrid size is visible 200"), VM->GetLockedGrid().Num(), 200);
	TestEqual(TEXT("LockedGrid getter reflects written cell"), (int32)VM->GetLockedGrid()[0], (int32)EPieceType::T);

	// 동일 그리드 재설정: 발행 없음(V1 게이트, TArray 요소 비교).
	FTetrisBoardViewModelTestAccess::SetLockedGrid(*VM, Grid);
	TestEqual(TEXT("No broadcast on same grid"), Counter.Count, 1);

	// 한 칸만 달라진 그리드: 발행됨.
	Grid[5] = EPieceType::I;
	FTetrisBoardViewModelTestAccess::SetLockedGrid(*VM, Grid);
	TestEqual(TEXT("Broadcast on changed grid"), Counter.Count, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTetrisBoardViewModelActivePieceTest,
	"Tetris.BoardViewModel.ActivePieceFieldsNotify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardViewModelActivePieceTest::RunTest(const FString& Parameters)
{
	UTetrisBoardViewModel* VM = NewObject<UTetrisBoardViewModel>();
	TestNotNull(TEXT("VM created"), VM);

	// ActivePieceType: 변경 1회 발행, 동일값 미발행.
	FBoardFieldBroadcastCounter TypeCounter;
	TypeCounter.Subscribe(VM, UTetrisBoardViewModel::FFieldNotificationClassDescriptor::ActivePieceType);
	FTetrisBoardViewModelTestAccess::SetActivePieceType(*VM, EPieceType::S);
	TestEqual(TEXT("ActivePieceType notify once"), TypeCounter.Count, 1);
	TestEqual(TEXT("ActivePieceType getter"), (int32)VM->GetActivePieceType(), (int32)EPieceType::S);
	FTetrisBoardViewModelTestAccess::SetActivePieceType(*VM, EPieceType::S);
	TestEqual(TEXT("ActivePieceType no notify on same"), TypeCounter.Count, 1);

	// ActiveCells: 동일 시퀀스 미발행, 다른 시퀀스 발행.
	FBoardFieldBroadcastCounter ActiveCounter;
	ActiveCounter.Subscribe(VM, UTetrisBoardViewModel::FFieldNotificationClassDescriptor::ActiveCells);
	TArray<FIntPoint> Cells = { FIntPoint(4, 19), FIntPoint(5, 19), FIntPoint(4, 18), FIntPoint(5, 18) };
	FTetrisBoardViewModelTestAccess::SetActiveCells(*VM, Cells);
	TestEqual(TEXT("ActiveCells notify on first set"), ActiveCounter.Count, 1);
	TestEqual(TEXT("ActiveCells getter size"), VM->GetActiveCells().Num(), 4);
	FTetrisBoardViewModelTestAccess::SetActiveCells(*VM, Cells);
	TestEqual(TEXT("ActiveCells no notify same sequence"), ActiveCounter.Count, 1);

	// GhostCells: 독립 필드로 통지.
	FBoardFieldBroadcastCounter GhostCounter;
	GhostCounter.Subscribe(VM, UTetrisBoardViewModel::FFieldNotificationClassDescriptor::GhostCells);
	TArray<FIntPoint> Ghost = { FIntPoint(4, 1), FIntPoint(5, 1), FIntPoint(4, 0), FIntPoint(5, 0) };
	FTetrisBoardViewModelTestAccess::SetGhostCells(*VM, Ghost);
	TestEqual(TEXT("GhostCells notify once"), GhostCounter.Count, 1);

	// GameState: 변경 1회 발행, 동일값 미발행.
	FBoardFieldBroadcastCounter StateCounter;
	StateCounter.Subscribe(VM, UTetrisBoardViewModel::FFieldNotificationClassDescriptor::GameState);
	FTetrisBoardViewModelTestAccess::SetGameState(*VM, EGameState::Falling);
	TestEqual(TEXT("GameState notify once"), StateCounter.Count, 1);
	FTetrisBoardViewModelTestAccess::SetGameState(*VM, EGameState::Falling);
	TestEqual(TEXT("GameState no notify on same"), StateCounter.Count, 1);

	return true;
}

//~ G3: 바인더 통합 (NewObject Model, PIE/위젯 불필요) -------------------------

namespace
{
	// Board/Randomizer/Scoring/GameCore를 NewObject로 와이어링·시작한 묶음(Board VM 통합용).
	// Why 고유 이름(Board 접두사): unity 빌드에서 HUD 테스트의 FModelFixture와 충돌 방지.
	struct FBoardModelFixture
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
			Core->StartGame(Seed); // 큐 채움 + 첫 피스 스폰(OnActivePieceUpdated) + Falling 전이
		}
	};

	// 두 좌표 배열이 같은 집합인지(순서 무관) 비교 — 클립/변환 결과 검증용.
	bool SamePointSet(const TArray<FIntPoint>& A, const TArray<FIntPoint>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (const FIntPoint& P : A) { if (!B.Contains(P)) { return false; } }
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardBinderInitialSnapshotTest,
	"Tetris.BoardViewModel.BinderInitialSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardBinderInitialSnapshotTest::RunTest(const FString&)
{
	FBoardModelFixture M; M.StartAt(123);
	UTetrisBoardViewModelBinder* Binder = NewObject<UTetrisBoardViewModelBinder>();
	Binder->Bind(M.Board, M.Core);
	UTetrisBoardViewModel* VM = Binder->GetViewModel();
	TestNotNull(TEXT("Bind 후 VM 생성됨"), VM);

	// V2: LockedGrid == ConvertGrid(GetVisibleGrid()).
	const TArray<FCellState> Grid = M.Board->GetVisibleGrid();
	TestEqual(TEXT("LockedGrid 크기 == visible 그리드"), VM->GetLockedGrid().Num(), Grid.Num());
	TestEqual(TEXT("LockedGrid 크기 200"), VM->GetLockedGrid().Num(), 200);
	bool bGridMatch = (VM->GetLockedGrid().Num() == Grid.Num());
	for (int32 i = 0; bGridMatch && i < Grid.Num(); ++i) { bGridMatch &= (VM->GetLockedGrid()[i] == Grid[i].Type); }
	TestTrue(TEXT("LockedGrid가 변환된 visible 그리드와 일치"), bGridMatch);

	// 시작 직후 상태 = Falling, 활성 타입 = 스폰 피스 타입.
	TestEqual(TEXT("GameState 스냅샷"), VM->GetGameState(), M.Core->GetState());
	TestEqual(TEXT("시작 직후 Falling"), M.Core->GetState(), EGameState::Falling);
	TestEqual(TEXT("ActivePieceType == 활성 피스 타입"), VM->GetActivePieceType(), M.Core->GetActivePiece().Type);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardBinderActiveGhostTest,
	"Tetris.BoardViewModel.BinderActiveGhostTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardBinderActiveGhostTest::RunTest(const FString&)
{
	FBoardModelFixture M; M.StartAt(123);
	UTetrisBoardViewModelBinder* Binder = NewObject<UTetrisBoardViewModelBinder>();
	Binder->Bind(M.Board, M.Core);
	UTetrisBoardViewModel* VM = Binder->GetViewModel();

	// 바인더와 동일 변환을 독립 재현해 비교(클립 + 고스트 드롭).
	const TArray<FIntPoint> Abs = FTetrisPieceOps::GetAbsoluteBlockPositions(M.Core->GetActivePiece());
	const int32 Dist = M.Board->GetDropDistance(Abs);

	TArray<FIntPoint> ExpectedActive;
	for (const FIntPoint& P : Abs)
	{
		if (P.X >= 0 && P.X < 10 && P.Y >= 0 && P.Y < 20) { ExpectedActive.Add(P); }
	}
	TArray<FIntPoint> ExpectedGhost;
	for (const FIntPoint& P : Abs)
	{
		const FIntPoint G(P.X, P.Y - Dist);
		if (G.X >= 0 && G.X < 10 && G.Y >= 0 && G.Y < 20) { ExpectedGhost.Add(G); }
	}

	TestTrue(TEXT("ActiveCells == 기대(클립)"), SamePointSet(VM->GetActiveCells(), ExpectedActive));
	TestTrue(TEXT("GhostCells == 기대(클립+드롭)"), SamePointSet(VM->GetGhostCells(), ExpectedGhost));

	// 클립 불변식: 활성 셀은 모두 가시 영역(Y<20). buffer 누수 없음.
	bool bAllVisible = true;
	for (const FIntPoint& P : VM->GetActiveCells()) { bAllVisible &= (P.Y >= 0 && P.Y < 20); }
	TestTrue(TEXT("ActiveCells 전부 가시 영역"), bAllVisible);

	// 빈 보드에서 고스트는 바닥까지 떨어져 4칸 모두 가시.
	TestEqual(TEXT("GhostCells 4칸(빈 보드 바닥)"), VM->GetGhostCells().Num(), 4);
	TestTrue(TEXT("DropDistance > 0(빈 보드)"), Dist > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardBinderLockPropagationTest,
	"Tetris.BoardViewModel.BinderLockPropagation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardBinderLockPropagationTest::RunTest(const FString&)
{
	FBoardModelFixture M; M.StartAt(123);
	UTetrisBoardViewModelBinder* Binder = NewObject<UTetrisBoardViewModelBinder>();
	Binder->Bind(M.Board, M.Core);
	UTetrisBoardViewModel* VM = Binder->GetViewModel();

	// 가시 영역 바닥 두 칸 lock → OnBoardChanged → LockedGrid 갱신(index = Y*Width + X).
	const TArray<FIntPoint> Coords = { FIntPoint(0, 0), FIntPoint(1, 0) };
	M.Board->LockPiece(Coords, EPieceType::I);
	TestEqual(TEXT("LockedGrid[(0,0)] == I"), (int32)VM->GetLockedGrid()[0 * 10 + 0], (int32)EPieceType::I);
	TestEqual(TEXT("LockedGrid[(1,0)] == I"), (int32)VM->GetLockedGrid()[0 * 10 + 1], (int32)EPieceType::I);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisBoardBinderUnbindTest,
	"Tetris.BoardViewModel.BinderUnbindStopsUpdates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisBoardBinderUnbindTest::RunTest(const FString&)
{
	FBoardModelFixture M; M.StartAt(123);
	UTetrisBoardViewModelBinder* Binder = NewObject<UTetrisBoardViewModelBinder>();
	Binder->Bind(M.Board, M.Core);

	// Unbind 후에도 살아있도록 강참조 유지(GC 방지) — 구독 해제 효과만 검증.
	TStrongObjectPtr<UTetrisBoardViewModel> VM(Binder->GetViewModel());

	Binder->Unbind();
	TestNull(TEXT("Unbind 후 바인더 VM 참조 해제"), Binder->GetViewModel());

	// 구독이 끊겼으므로 이후 발행은 VM에 반영되지 않아야 한다.
	M.Board->LockPiece({ FIntPoint(5, 5) }, EPieceType::Z);
	int32 NonEmpty = 0;
	for (EPieceType T : VM->GetLockedGrid()) { if (T != EPieceType::None) { ++NonEmpty; } }
	TestEqual(TEXT("Unbind 후 LockedGrid 미갱신(여전히 빈 보드)"), NonEmpty, 0);

	M.Core->OnStateChanged.Broadcast(EGameState::Falling, EGameState::GameOver);
	TestNotEqual(TEXT("Unbind 후 GameState 미갱신"), VM->GetGameState(), EGameState::GameOver);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
