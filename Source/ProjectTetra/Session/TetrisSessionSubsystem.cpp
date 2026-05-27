// Copyright ProjectTetra. All Rights Reserved.

#include "Session/TetrisSessionSubsystem.h"
#include "Board/TetrisBoard.h"
#include "System/TetrisRandomizer.h"
#include "System/TetrisScoring.h"
#include "FSM/TetrisGameCore.h"
#include "Block/TetrisPiece.h"
#include "Core/TetrisTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

// PIE 육안 관찰용 디버그 토글. 켜면 매 프레임 보드 상태를 화면+로그에 출력한다(릴리스 영향 없음).
static TAutoConsoleVariable<int32> CVarTetraDebugBoard(
	TEXT("tetra.DebugBoard"),
	0,
	TEXT("0=off, >0=PIE에서 보드/활성 피스/상태를 ASCII로 화면+로그에 출력한다."),
	ECVF_Default);

// PIE에서 세션을 수동 시작하는 디버그 명령. 입력/UI 도입 전까지의 임시 시작 트리거.
// 사용법: tetra.StartGame [Seed]  (Seed 생략 시 0 = 시간 기반)
static void TetraStartGameCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	UTetrisSessionSubsystem* Session = World->GetSubsystem<UTetrisSessionSubsystem>();
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tetra] StartGame: 세션 서브시스템을 찾지 못함 (게임/PIE 월드인지 확인)."));
		return;
	}
	const int64 Seed = (Args.Num() > 0) ? FCString::Atoi64(*Args[0]) : 0;
	Session->StartGame(Seed);
	UE_LOG(LogTemp, Log, TEXT("[Tetra] StartGame(Seed=%lld)"), Seed);
}

static FAutoConsoleCommandWithWorldAndArgs CCmdTetraStartGame(
	TEXT("tetra.StartGame"),
	TEXT("Tetris 세션을 시작한다. 사용법: tetra.StartGame [Seed] (생략 시 0)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TetraStartGameCommand));

void UTetrisSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection); // 틱 등록 포함 — 반드시 호출
	CreateAndWireCore();
}

void UTetrisSessionSubsystem::Deinitialize()
{
	bRunning = false;
	GameCore = nullptr;
	Scoring = nullptr;
	Randomizer = nullptr;
	Board = nullptr;
	Super::Deinitialize();
}

bool UTetrisSessionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 실제 플레이 월드에서만 시뮬을 호스팅한다. 에디터 프리뷰/머티리얼 등 비게임 월드는 제외.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTetrisSessionSubsystem::CreateAndWireCore()
{
	if (GameCore)
	{
		return; // 멱등 — 이미 구성됨
	}
	Board = NewObject<UTetrisBoard>(this);
	Randomizer = NewObject<UTetrisRandomizer>(this);
	Scoring = NewObject<UTetrisScoring>(this);
	GameCore = NewObject<UTetrisGameCore>(this);
	GameCore->Initialize(Board, Randomizer, Scoring);
}

void UTetrisSessionSubsystem::StartGame(int64 Seed)
{
	CreateAndWireCore(); // Initialize 미경유(테스트) 안전 — 멱등
	GameCore->StartGame(Seed);
	TimeAccumulator = 0.0;
	bRunning = true;
}

void UTetrisSessionSubsystem::RestartGame()
{
	if (!GameCore)
	{
		return;
	}
	GameCore->RestartGame();
	TimeAccumulator = 0.0;
	bRunning = true;
}

void UTetrisSessionSubsystem::SetPaused(bool bPaused)
{
	// Pause 시 누적 중단(IsTickable=false). Resume 시 재개. 잔여 누적기는 보존.
	bRunning = !bPaused;
}

int32 UTetrisSessionSubsystem::AdvanceFixedSteps(float DeltaTime)
{
	if (!bRunning || !GameCore)
	{
		return 0;
	}
	// fsm.md §F1: 가변 dt를 SimDelta 단위로 쪼개 Step. MaxStepsPerFrame로 spiral-of-death 방지.
	const double SimDelta = 1.0 / (double)SimHz;
	TimeAccumulator += DeltaTime;
	int32 Steps = 0;
	while (TimeAccumulator >= SimDelta && Steps < MaxStepsPerFrame)
	{
		GameCore->Step();
		TimeAccumulator -= SimDelta;
		++Steps;
	}
	return Steps;
}

void UTetrisSessionSubsystem::Tick(float DeltaTime)
{
	AdvanceFixedSteps(DeltaTime);

	if (CVarTetraDebugBoard.GetValueOnGameThread() > 0)
	{
		DebugDrawBoard();
	}
}

void UTetrisSessionSubsystem::DebugDrawBoard()
{
	if (!GameCore || !Board)
	{
		return;
	}

	using namespace TetrisConstants;

	// 활성(미고정) 피스 좌표를 visible 영역에 한해 오버레이용으로 수집.
	TSet<int32> PieceCells;
	for (const FIntPoint& P : FTetrisPieceOps::GetAbsoluteBlockPositions(GameCore->GetActivePiece()))
	{
		if (P.X >= 0 && P.X < BoardWidth && P.Y >= 0 && P.Y < BoardVisibleHeight)
		{
			PieceCells.Add(P.Y * BoardWidth + P.X);
		}
	}

	// 위(Y=19)에서 아래(Y=0)로 ASCII 렌더. '#'=고정 셀, 'O'=활성 피스, '.'=빈 칸.
	const TArray<FCellState> Grid = Board->GetVisibleGrid();
	FString Out = TEXT("\n");
	for (int32 Y = BoardVisibleHeight - 1; Y >= 0; --Y)
	{
		for (int32 X = 0; X < BoardWidth; ++X)
		{
			const int32 Idx = Y * BoardWidth + X;
			TCHAR C = TEXT('.');
			if (PieceCells.Contains(Idx)) { C = TEXT('O'); }
			else if (Grid[Idx].IsFilled()) { C = TEXT('#'); }
			Out.AppendChar(C);
		}
		Out.AppendChar(TEXT('\n'));
	}
	Out += FString::Printf(TEXT("State=%d Score=%lld Level=%d Lines=%d"),
		(int32)GameCore->GetState(),
		Scoring ? Scoring->GetScore() : 0,
		Scoring ? Scoring->GetLevel() : 0,
		Scoring ? Scoring->GetTotalLinesCleared() : 0);

	// 화면: 고정 키로 매 프레임 덮어쓰기. 로그: 스팸 완화를 위해 N프레임마다.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage((uint64)0x7E74A, 0.0f, FColor::Green, Out);
	}
	if ((DebugLogThrottle++ % 15) == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Tetra]%s"), *Out);
	}
}

TStatId UTetrisSessionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTetrisSessionSubsystem, STATGROUP_Tickables);
}

bool UTetrisSessionSubsystem::IsTickable() const
{
	// 기본 게이트(IsAllowedToTick: 초기화됨 + 비-CDO)에 더해 게임 진행 중일 때만 틱.
	return bRunning && GameCore != nullptr;
}
