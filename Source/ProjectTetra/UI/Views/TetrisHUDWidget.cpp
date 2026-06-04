// Copyright ProjectTetra. All Rights Reserved.

#include "UI/Views/TetrisHUDWidget.h"
#include "UI/Views/TetrisPieceWidget.h"
#include "UI/ViewModel/TetrisHUDViewModel.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UTetrisHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildNextSlots();

	// VM 우선순위: 외부 주입(SetViewModel) > Collection resolve. 미주입 상태면 resolve 시도.
	if (!ViewModel)
	{
		ViewModel = ResolveViewModel();
	}
	SubscribeToViewModel();
	RefreshAll();
}

void UTetrisHUDWidget::NativeDestruct()
{
	UnsubscribeFromViewModel();
	Super::NativeDestruct();
}

void UTetrisHUDWidget::SetViewModel(UTetrisHUDViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		return;
	}
	UnsubscribeFromViewModel();
	ViewModel = InViewModel;
	SubscribeToViewModel();
	RefreshAll();
}

UTetrisHUDViewModel* UTetrisHUDWidget::ResolveViewModel() const
{
	// 바인더 등록 경로의 역방향: World→GameInstance→MVVM 서브시스템→Collection→"TetrisHUD" 조회.
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMVVMGameSubsystem* Subsystem = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
			{
				if (UMVVMViewModelCollectionObject* Collection = Subsystem->GetViewModelCollection())
				{
					FMVVMViewModelContext Context;
					Context.ContextClass = UTetrisHUDViewModel::StaticClass();
					Context.ContextName = UTetrisHUDViewModel::ContextName;
					TScriptInterface<INotifyFieldValueChanged> Found = Collection->FindViewModelInstance(Context);
					return Cast<UTetrisHUDViewModel>(Found.GetObject());
				}
			}
		}
	}
	return nullptr;
}

void UTetrisHUDWidget::BuildNextSlots()
{
	// 멱등: 이미 만들었거나 컨테이너/클래스가 없으면 건너뛴다(재진입/WBP 미구성 안전).
	if (NextSlots.Num() > 0 || !NextContainer || !NextPieceWidgetClass)
	{
		return;
	}

	NextSlots.Reserve(NextPreviewCount);
	for (int32 i = 0; i < NextPreviewCount; ++i)
	{
		UTetrisPieceWidget* NextSlot = CreateWidget<UTetrisPieceWidget>(this, NextPieceWidgetClass);
		if (NextSlot)
		{
			NextContainer->AddChild(NextSlot);
			NextSlots.Add(NextSlot);
		}
	}
}

void UTetrisHUDWidget::SubscribeToViewModel()
{
	if (!ViewModel)
	{
		return;
	}

	// 데이터 필드만 구독 — 어떤 게 바뀌든 RefreshAll. 핸들은 RemoveAll(this)로 일괄 해제하므로 개별 보관 불필요.
	using FDescriptor = UTetrisHUDViewModel::FFieldNotificationClassDescriptor;
	const UE::FieldNotification::FFieldId Fields[] = {
		FDescriptor::Score,
		FDescriptor::Level,
		FDescriptor::Lines,
		FDescriptor::HoldPiece,
		FDescriptor::NextQueue,
	};
	for (const UE::FieldNotification::FFieldId& FieldId : Fields)
	{
		ViewModel->AddFieldValueChangedDelegate(
			FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UTetrisHUDWidget::HandleFieldChanged));
	}
}

void UTetrisHUDWidget::UnsubscribeFromViewModel()
{
	if (ViewModel)
	{
		ViewModel->RemoveAllFieldValueChangedDelegates(this);
	}
}

void UTetrisHUDWidget::HandleFieldChanged(UObject* /*Object*/, UE::FieldNotification::FFieldId /*FieldId*/)
{
	RefreshAll();
}

void UTetrisHUDWidget::RefreshAll()
{
	if (!ViewModel)
	{
		// VM 미연결: 기본값으로(빈 슬롯). 텍스트는 손대지 않아도 무방하나 일관성 위해 비움 처리 생략.
		if (HoldPiece) { HoldPiece->SetPiece(EPieceType::None); }
		for (UTetrisPieceWidget* NextSlot : NextSlots) { if (NextSlot) { NextSlot->SetPiece(EPieceType::None); } }
		return;
	}

	// 점수는 computed 텍스트(로케일 포맷), 레벨/줄은 숫자 → 텍스트.
	if (ScoreText) { ScoreText->SetText(ViewModel->GetScoreText()); }
	if (LevelText) { LevelText->SetText(FText::AsNumber(ViewModel->GetLevel())); }
	if (LinesText) { LinesText->SetText(FText::AsNumber(ViewModel->GetLines())); }

	if (HoldPiece) { HoldPiece->SetPiece(ViewModel->GetHoldPiece()); }

	const TArray<EPieceType>& Queue = ViewModel->GetNextQueue();
	for (int32 i = 0; i < NextSlots.Num(); ++i)
	{
		if (NextSlots[i])
		{
			NextSlots[i]->SetPiece(Queue.IsValidIndex(i) ? Queue[i] : EPieceType::None);
		}
	}
}
