// Copyright ProjectTetra. All Rights Reserved.

#include "UI/ViewModel/TetrisHUDViewModelBinder.h"
#include "UI/ViewModel/TetrisHUDViewModel.h"
#include "FSM/TetrisGameCore.h"
#include "System/TetrisScoring.h"
#include "System/TetrisRandomizer.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UTetrisHUDViewModelBinder::Bind(UTetrisGameCore* Core, UTetrisScoring* Scoring, UTetrisRandomizer* Rand)
{
	if (ViewModel)
	{
		Unbind(); // 재진입 안전 — 이전 바인딩 정리 후 재구성.
	}

	ViewModel = NewObject<UTetrisHUDViewModel>(this);
	BoundCore = Core;
	BoundScoring = Scoring;
	BoundRandomizer = Rand;

	// 초기 스냅샷(V2): 첫 델리게이트 발행 전에도 HUD가 정확한 시작값을 표시하도록 현재 getter 값을 pull.
	if (Scoring)
	{
		ViewModel->SetScore(Scoring->GetScore());
		ViewModel->SetLevel(Scoring->GetLevel());
		ViewModel->SetLines(Scoring->GetTotalLinesCleared());
		ViewModel->SetCombo(Scoring->GetCombo());
		ViewModel->SetB2BCount(Scoring->GetB2BCount());
	}
	if (Core)
	{
		ViewModel->SetHoldPiece(Core->GetHoldSlot());
		ViewModel->SetCanHold(Core->IsHoldAvailable());
		ViewModel->SetGameState(Core->GetState());
	}
	if (Rand)
	{
		ViewModel->SetNextQueue(Rand->GetNextQueue());
	}

	// 델리게이트 구독(핸들 보관). VM이 살아있을 때만 setter 호출(null 가드).
	if (Scoring)
	{
		ScoreHandle = Scoring->OnScoreChanged.AddLambda([this](int64 V) { if (ViewModel) { ViewModel->SetScore(V); } });
		LevelHandle = Scoring->OnLevelChanged.AddLambda([this](int32 V) { if (ViewModel) { ViewModel->SetLevel(V); } });
		LinesHandle = Scoring->OnLinesChanged.AddLambda([this](int32 V) { if (ViewModel) { ViewModel->SetLines(V); } });
		ComboHandle = Scoring->OnComboChanged.AddLambda([this](int32 V) { if (ViewModel) { ViewModel->SetCombo(V); } });
		B2BHandle = Scoring->OnB2BChanged.AddLambda([this](int32 V) { if (ViewModel) { ViewModel->SetB2BCount(V); } });
	}
	if (Core)
	{
		StateHandle = Core->OnStateChanged.AddLambda([this](EGameState /*Old*/, EGameState New) { if (ViewModel) { ViewModel->SetGameState(New); } });
		// OnHoldChanged 1건이 HoldPiece + bCanHold 두 필드를 먹인다(델리게이트 7 ≠ 필드 9).
		HoldHandle = Core->OnHoldChanged.AddLambda([this](EPieceType Hold, bool bCanHold)
		{
			if (ViewModel)
			{
				ViewModel->SetHoldPiece(Hold);
				ViewModel->SetCanHold(bCanHold);
			}
		});
	}
	if (Rand)
	{
		// dynamic 델리게이트만 AddDynamic + UFUNCTION 경로(viewmodel.md Edge 4).
		Rand->OnNextQueueChanged.AddDynamic(this, &UTetrisHUDViewModelBinder::HandleNextQueueChanged);
	}

	RegisterToCollection();
}

void UTetrisHUDViewModelBinder::Unbind()
{
	if (BoundScoring)
	{
		BoundScoring->OnScoreChanged.Remove(ScoreHandle);
		BoundScoring->OnLevelChanged.Remove(LevelHandle);
		BoundScoring->OnLinesChanged.Remove(LinesHandle);
		BoundScoring->OnComboChanged.Remove(ComboHandle);
		BoundScoring->OnB2BChanged.Remove(B2BHandle);
	}
	if (BoundCore)
	{
		BoundCore->OnStateChanged.Remove(StateHandle);
		BoundCore->OnHoldChanged.Remove(HoldHandle);
	}
	if (BoundRandomizer)
	{
		BoundRandomizer->OnNextQueueChanged.RemoveDynamic(this, &UTetrisHUDViewModelBinder::HandleNextQueueChanged);
	}

	ScoreHandle.Reset();
	LevelHandle.Reset();
	LinesHandle.Reset();
	ComboHandle.Reset();
	B2BHandle.Reset();
	StateHandle.Reset();
	HoldHandle.Reset();

	UnregisterFromCollection();

	BoundScoring = nullptr;
	BoundCore = nullptr;
	BoundRandomizer = nullptr;
	ViewModel = nullptr;
}

void UTetrisHUDViewModelBinder::HandleNextQueueChanged(const TArray<EPieceType>& NextQueue)
{
	if (ViewModel)
	{
		ViewModel->SetNextQueue(NextQueue);
	}
}

UWorld* UTetrisHUDViewModelBinder::GetWorld() const
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

UMVVMViewModelCollectionObject* UTetrisHUDViewModelBinder::ResolveCollection() const
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

void UTetrisHUDViewModelBinder::RegisterToCollection()
{
	UMVVMViewModelCollectionObject* Collection = ResolveCollection();
	if (Collection && ViewModel)
	{
		// View가 컨텍스트명으로 resolve할 수 있도록 등록. ContextClass+ContextName이 바인딩 키.
		FMVVMViewModelContext Context;
		Context.ContextClass = UTetrisHUDViewModel::StaticClass();
		Context.ContextName = UTetrisHUDViewModel::ContextName;
		Collection->AddViewModelInstance(Context, ViewModel);
	}
}

void UTetrisHUDViewModelBinder::UnregisterFromCollection()
{
	UMVVMViewModelCollectionObject* Collection = ResolveCollection();
	if (Collection && ViewModel)
	{
		Collection->RemoveAllViewModelInstance(ViewModel);
	}
}
