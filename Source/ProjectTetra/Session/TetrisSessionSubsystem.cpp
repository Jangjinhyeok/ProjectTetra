// Copyright ProjectTetra. All Rights Reserved.

#include "Session/TetrisSessionSubsystem.h"
#include "Board/TetrisBoard.h"
#include "System/TetrisRandomizer.h"
#include "System/TetrisScoring.h"
#include "FSM/TetrisGameCore.h"
#include "Block/TetrisPiece.h"
#include "Core/TetrisTypes.h"
#include "UI/ViewModel/TetrisHUDViewModelBinder.h"
#include "UI/ViewModel/TetrisBoardViewModelBinder.h"
#include "System/TetrisSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
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
	if (HUDBinder)
	{
		HUDBinder->Unbind(); // 구독 해제 + 컬렉션 제거
		HUDBinder = nullptr;
	}
	if (BoardBinder)
	{
		BoardBinder->Unbind(); // 구독 해제 + 컬렉션 제거
		BoardBinder = nullptr;
	}
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

	// Board/HUD VM을 월드 초기화 시점에 미리 생성·컬렉션 등록한다. 위젯은 NativeConstruct(PC BeginPlay,
	// 이 시점보다 나중)에서 컨텍스트명으로 안정적으로 resolve한다. StartGame은 이 VM들을 재사용(rebind)한다.
	if (!BoardBinder)
	{
		BoardBinder = NewObject<UTetrisBoardViewModelBinder>(this);
	}
	BoardBinder->EnsureViewModel();
	if (!HUDBinder)
	{
		HUDBinder = NewObject<UTetrisHUDViewModelBinder>(this);
	}
	HUDBinder->EnsureViewModel();
}

void UTetrisSessionSubsystem::StartGame(int64 Seed)
{
	CreateAndWireCore(); // Initialize 미경유(테스트) 안전 — 멱등
	ResetHandling();     // input-handling.md Edge 7: 시작 시 버퍼·핸들링 클리어(직전 잔여 입력 무시)
	// GameCore->StartGame 이전에 Bind — 시작 중 발행되는 델리게이트(상태 전이·큐 초기화)를 VM이 받아 시작 상태를 반영한다(viewmodel.md 생명주기).
	if (!HUDBinder)
	{
		HUDBinder = NewObject<UTetrisHUDViewModelBinder>(this);
	}
	HUDBinder->Bind(GameCore, Scoring, Randomizer); // Bind는 재진입 안전(내부에서 기존 바인딩 정리)
	// Board 바인더도 GameCore->StartGame 이전에 Bind — 시작 중 발행되는 OnActivePieceUpdated/OnStateChanged + 초기 스냅샷을 VM이 받아 시작 보드를 반영(HUD 바인더와 병렬).
	if (!BoardBinder)
	{
		BoardBinder = NewObject<UTetrisBoardViewModelBinder>(this);
	}
	BoardBinder->Bind(Board, GameCore);
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
	ResetHandling();
	// Unbind→Bind로 VM 재생성 + 스냅샷 재주입 → 이전 판 값 잔존 없음(viewmodel.md Edge 3).
	if (!HUDBinder)
	{
		HUDBinder = NewObject<UTetrisHUDViewModelBinder>(this);
	}
	HUDBinder->Bind(GameCore, Scoring, Randomizer);
	// Board 바인더도 Unbind→Bind(Bind 내부에서 재진입 정리) — 이전 판 잔존 없음.
	if (!BoardBinder)
	{
		BoardBinder = NewObject<UTetrisBoardViewModelBinder>(this);
	}
	BoardBinder->Bind(Board, GameCore);
	GameCore->RestartGame();
	TimeAccumulator = 0.0;
	bRunning = true;
}

UTetrisHUDViewModel* UTetrisSessionSubsystem::GetHUDViewModel() const
{
	return HUDBinder ? HUDBinder->GetViewModel() : nullptr;
}

UTetrisBoardViewModel* UTetrisSessionSubsystem::GetBoardViewModel() const
{
	return BoardBinder ? BoardBinder->GetViewModel() : nullptr;
}

void UTetrisSessionSubsystem::ResetHandling()
{
	// 세션 SimHz를 강제로 일치시켜 ms→스텝 환산이 루프 주파수와 결정적으로 맞물리게 한다(에디터 값 비파괴 — 로컬 복사).
	FHandlingConfig Cfg = HandlingConfig;
	Cfg.SimHz = SimHz;

	// 플레이어 설정(SaveGame 영속)이 있으면 DAS/ARR을 로컬 Cfg에만 덮어쓴다. 진입을 메인 메뉴로 한정했으므로
	// StartGame/RestartGame 시점 pull로 충분하다(런타임 라이브 재주입 불요 — 설계노트 2). 서브시스템 부재(테스트 월드 등) 시 에디터 기본값 유지.
	if (const UWorld* SessionWorld = GetWorld())
	{
		if (const UGameInstance* GI = SessionWorld->GetGameInstance())
		{
			if (const UTetrisSettingsSubsystem* Settings = GI->GetSubsystem<UTetrisSettingsSubsystem>())
			{
				Cfg.DASms = Settings->GetDASms();
				Cfg.ARRms = Settings->GetARRms();
				// DCD는 Cfg 경유(런타임 H5가 [0, DASSteps]로 재클램프). SDF는 GameCore 소유라 별도 경로지만
				// 같은 settings-pull 시점에 묶어 단일 주입점(StartGame/RestartGame/Retry 공통)을 유지한다.
				Cfg.DCDms = Settings->GetDCDms();
				if (GameCore)
				{
					GameCore->SoftDropFactor = Settings->GetSDF();
				}
			}
		}
	}

	Handling.SetConfig(Cfg);
	Handling.Reset();
	InputBuffer = FHandlingInput(); // held + edge 전부 클리어
}

void UTetrisSessionSubsystem::SetMoveHeld(bool bLeft, bool bHeld)
{
	if (bLeft) { InputBuffer.bLeftHeld = bHeld; }
	else { InputBuffer.bRightHeld = bHeld; }
}

void UTetrisSessionSubsystem::PushInputEdge(EInputEdge Edge)
{
	InputBuffer.Edges.Add(Edge);
}

void UTetrisSessionSubsystem::DriveHandlingForStep()
{
	// 버퍼 스냅샷(held + 누적 edge) → edge 드레인(1회 소비; held는 PC가 갱신하므로 유지) → 순수 핸들링 → 명령 적재.
	const FHandlingInput Snapshot = InputBuffer;
	InputBuffer.Edges.Reset();
	for (const EGameCommand Cmd : Handling.AdvanceStep(Snapshot))
	{
		GameCore->EnqueueCommand(Cmd);
	}
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
		DriveHandlingForStep(); // 매 Step 직전: 입력 → 명령 적재 (input-handling.md 통합 지점)
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
