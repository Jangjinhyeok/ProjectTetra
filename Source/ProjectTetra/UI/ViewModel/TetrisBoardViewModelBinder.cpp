// Copyright ProjectTetra. All Rights Reserved.

#include "UI/ViewModel/TetrisBoardViewModelBinder.h"
#include "UI/ViewModel/TetrisBoardViewModel.h"
#include "Board/TetrisBoard.h"
#include "FSM/TetrisGameCore.h"
#include "Block/TetrisPiece.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	// board-space에서 가시 영역(0<=X<Width, 0<=Y<VisibleHeight)만 남긴다. buffer 행(Y>=20)·범위 밖 제거.
	// 위젯은 dumb하게 두고(렌더 시 Y 플립만), 클립은 바인더 책임(HANDOFF §7).
	TArray<FIntPoint> ClipToVisible(const TArray<FIntPoint>& Coords)
	{
		TArray<FIntPoint> Out;
		Out.Reserve(Coords.Num());
		for (const FIntPoint& P : Coords)
		{
			if (P.X >= 0 && P.X < TetrisConstants::BoardWidth
			 && P.Y >= 0 && P.Y < TetrisConstants::BoardVisibleHeight)
			{
				Out.Add(P);
			}
		}
		return Out;
	}
}

void UTetrisBoardViewModelBinder::Bind(UTetrisBoard* Board, UTetrisGameCore* Core)
{
	DetachModel();     // 재진입 안전 — 이전 라운드 구독만 정리(VM은 보존해 View 연결 유지).
	EnsureViewModel(); // VM 없으면 생성+등록(StartGame 단독 경로 방어). 있으면 재사용.

	BoundBoard = Board;
	BoundCore = Core;

	// 초기 스냅샷(V2): 첫 델리게이트 발행 전에도 보드가 정확한 시작 상태를 들도록 현재값을 pull.
	RefreshLockedGrid();
	RefreshActive();
	if (Core)
	{
		ViewModel->SetGameState(Core->GetState());
	}

	// 델리게이트 구독(핸들 보관). 전부 non-dynamic → AddLambda. VM이 살아있을 때만 setter 호출.
	if (Board)
	{
		BoardChangedHandle = Board->OnBoardChanged.AddLambda([this](const TArray<FIntPoint>&) { RefreshLockedGrid(); });
		LinesClearedHandle = Board->OnLinesCleared.AddLambda([this](const FLineClearResult&) { RefreshLockedGrid(); });
	}
	if (Core)
	{
		ActivePieceHandle = Core->OnActivePieceUpdated.AddLambda([this](const FActivePiece&) { RefreshActive(); });
		// 상태 변경 시 GameState 갱신 + 활성 게이팅 재평가(예: GameOver 전이 시 활성/고스트 비움).
		StateHandle = Core->OnStateChanged.AddLambda([this](EGameState /*Old*/, EGameState New)
		{
			if (ViewModel)
			{
				ViewModel->SetGameState(New);
			}
			RefreshActive();
		});
	}
}

void UTetrisBoardViewModelBinder::EnsureViewModel()
{
	if (ViewModel)
	{
		return; // 멱등 — 이미 생성·등록됨.
	}
	ViewModel = NewObject<UTetrisBoardViewModel>(this);
	RegisterToCollection(); // 헤드리스(월드/서브시스템 부재)면 ResolveCollection null → no-op.
}

void UTetrisBoardViewModelBinder::DetachModel()
{
	if (BoundBoard)
	{
		BoundBoard->OnBoardChanged.Remove(BoardChangedHandle);
		BoundBoard->OnLinesCleared.Remove(LinesClearedHandle);
	}
	if (BoundCore)
	{
		BoundCore->OnActivePieceUpdated.Remove(ActivePieceHandle);
		BoundCore->OnStateChanged.Remove(StateHandle);
	}

	BoardChangedHandle.Reset();
	LinesClearedHandle.Reset();
	ActivePieceHandle.Reset();
	StateHandle.Reset();

	BoundBoard = nullptr;
	BoundCore = nullptr;
}

void UTetrisBoardViewModelBinder::Unbind()
{
	DetachModel();
	UnregisterFromCollection();
	ViewModel = nullptr;
}

void UTetrisBoardViewModelBinder::RefreshLockedGrid()
{
	if (!ViewModel || !BoundBoard)
	{
		return;
	}

	// FCellState 그리드(visible 200, row-major index=Y*Width+X)를 EPieceType 배열로 변환.
	const TArray<FCellState> Grid = BoundBoard->GetVisibleGrid();
	TArray<EPieceType> Types;
	Types.Reserve(Grid.Num());
	for (const FCellState& Cell : Grid)
	{
		Types.Add(Cell.Type);
	}
	ViewModel->SetLockedGrid(Types);
}

void UTetrisBoardViewModelBinder::RefreshActive()
{
	if (!ViewModel || !BoundCore || !BoundBoard)
	{
		return;
	}

	// 활성 피스를 들고 있는 상태(낙하/락 카운트다운)에서만 활성/고스트를 노출한다.
	const EGameState State = BoundCore->GetState();
	const bool bPieceActive = (State == EGameState::Falling || State == EGameState::Locking);
	if (!bPieceActive)
	{
		ViewModel->SetActivePieceType(EPieceType::None);
		ViewModel->SetActiveCells(TArray<FIntPoint>());
		ViewModel->SetGhostCells(TArray<FIntPoint>());
		return;
	}

	const FActivePiece& Piece = BoundCore->GetActivePiece();
	const TArray<FIntPoint> Abs = FTetrisPieceOps::GetAbsoluteBlockPositions(Piece);

	// 고스트 = 활성에서 착지까지 내린 위치(각 좌표 Y -= DropDistance). 둘 다 가시 영역으로 클립.
	const int32 Dist = BoundBoard->GetDropDistance(Abs);
	TArray<FIntPoint> Ghost = Abs;
	for (FIntPoint& Pt : Ghost)
	{
		Pt.Y -= Dist;
	}

	ViewModel->SetActivePieceType(Piece.Type);
	ViewModel->SetActiveCells(ClipToVisible(Abs));
	ViewModel->SetGhostCells(ClipToVisible(Ghost));
}

UWorld* UTetrisBoardViewModelBinder::GetWorld() const
{
	// CDO/헤드리스(transient package outer)에서는 null → 컬렉션 등록이 no-op이 되도록.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	if (const UObject* MyOuter = GetOuter())
	{
		return MyOuter->GetWorld();
	}
	return nullptr;
}

UMVVMViewModelCollectionObject* UTetrisBoardViewModelBinder::ResolveCollection() const
{
	// 월드/게임인스턴스/MVVM 서브시스템이 모두 있어야 컬렉션을 얻는다. 헤드리스 테스트에선 단계적으로 null → no-op.
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMVVMGameSubsystem* Subsystem = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
			{
				return Subsystem->GetViewModelCollection();
			}
		}
	}
	return nullptr;
}

void UTetrisBoardViewModelBinder::RegisterToCollection()
{
	UMVVMViewModelCollectionObject* Collection = ResolveCollection();
	if (Collection && ViewModel)
	{
		// View가 컨텍스트명으로 resolve할 수 있도록 등록. ContextClass+ContextName이 바인딩 키.
		FMVVMViewModelContext Context;
		Context.ContextClass = UTetrisBoardViewModel::StaticClass();
		Context.ContextName = UTetrisBoardViewModel::ContextName;
		Collection->AddViewModelInstance(Context, ViewModel);
	}
}

void UTetrisBoardViewModelBinder::UnregisterFromCollection()
{
	UMVVMViewModelCollectionObject* Collection = ResolveCollection();
	if (Collection && ViewModel)
	{
		Collection->RemoveAllViewModelInstance(ViewModel);
	}
}
