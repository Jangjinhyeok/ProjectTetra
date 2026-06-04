// Copyright ProjectTetra. All Rights Reserved.

#include "UI/Views/TetrisBoardWidget.h"
#include "UI/ViewModel/TetrisBoardViewModel.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UTetrisBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 색상 기본값 — WBP에서 미지정 시 사용(데이터 주도, 하드코딩 방지). None=빈 칸 배경.
	if (PieceColors.Num() == 0)
	{
		PieceColors.Add(EPieceType::None, FLinearColor(0.05f, 0.05f, 0.07f, 1.f));
		PieceColors.Add(EPieceType::I, FLinearColor(0.0f, 0.85f, 0.95f, 1.f));
		PieceColors.Add(EPieceType::O, FLinearColor(0.95f, 0.85f, 0.0f, 1.f));
		PieceColors.Add(EPieceType::T, FLinearColor(0.65f, 0.2f, 0.85f, 1.f));
		PieceColors.Add(EPieceType::S, FLinearColor(0.2f, 0.85f, 0.2f, 1.f));
		PieceColors.Add(EPieceType::Z, FLinearColor(0.9f, 0.2f, 0.2f, 1.f));
		PieceColors.Add(EPieceType::J, FLinearColor(0.2f, 0.35f, 0.9f, 1.f));
		PieceColors.Add(EPieceType::L, FLinearColor(0.95f, 0.5f, 0.0f, 1.f));
		PieceColors.Add(EPieceType::Garbage, FLinearColor(0.4f, 0.4f, 0.4f, 1.f));
	}

	BuildCellGrid();

	// VM 우선순위: 외부 주입(SetViewModel) > Collection resolve. 미주입 상태면 resolve 시도.
	if (!ViewModel)
	{
		ViewModel = ResolveViewModel();
	}
	SubscribeToViewModel();
	RepaintFromViewModel();
}

void UTetrisBoardWidget::NativeDestruct()
{
	UnsubscribeFromViewModel();
	Super::NativeDestruct();
}

void UTetrisBoardWidget::SetViewModel(UTetrisBoardViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		return;
	}
	UnsubscribeFromViewModel();
	ViewModel = InViewModel;
	SubscribeToViewModel();
	RepaintFromViewModel();
}

UTetrisBoardViewModel* UTetrisBoardWidget::ResolveViewModel() const
{
	// 바인더 등록 경로의 역방향: World→GameInstance→MVVM 서브시스템→Collection→"TetrisBoard" 조회.
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UMVVMGameSubsystem* Subsystem = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
			{
				if (UMVVMViewModelCollectionObject* Collection = Subsystem->GetViewModelCollection())
				{
					FMVVMViewModelContext Context;
					Context.ContextClass = UTetrisBoardViewModel::StaticClass();
					Context.ContextName = UTetrisBoardViewModel::ContextName;
					TScriptInterface<INotifyFieldValueChanged> Found = Collection->FindViewModelInstance(Context);
					return Cast<UTetrisBoardViewModel>(Found.GetObject());
				}
			}
		}
	}
	return nullptr;
}

void UTetrisBoardWidget::BuildCellGrid()
{
	// 멱등: 이미 만들었거나 패널이 없으면 건너뛴다(재진입/WBP 미구성 안전).
	if (CellImages.Num() > 0 || !CellGrid || !WidgetTree)
	{
		return;
	}

	const int32 Width = TetrisConstants::BoardWidth;
	const int32 Height = TetrisConstants::BoardVisibleHeight;
	CellImages.Reserve(Width * Height);

	// 흰 박스 브러시를 1회 설정하고, repaint에선 SetColorAndOpacity로 색만 바꾼다(셀당 브러시 재생성 회피).
	for (int32 Row = 0; Row < Height; ++Row)
	{
		for (int32 Col = 0; Col < Width; ++Col)
		{
			UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());

			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			Cell->SetBrush(Brush);

			if (UUniformGridSlot* GridSlot = CellGrid->AddChildToUniformGrid(Cell, Row, Col))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}

			CellImages.Add(Cell); // index = Row*Width + Col (row=top0)
		}
	}
}

void UTetrisBoardWidget::SubscribeToViewModel()
{
	if (!ViewModel)
	{
		return;
	}

	// 5종 필드 — 어떤 게 바뀌든 전체 repaint(MVP). 핸들은 RemoveAll(this)로 일괄 해제하므로 개별 보관 불필요.
	using FDescriptor = UTetrisBoardViewModel::FFieldNotificationClassDescriptor;
	const UE::FieldNotification::FFieldId Fields[] = {
		FDescriptor::LockedGrid,
		FDescriptor::ActivePieceType,
		FDescriptor::ActiveCells,
		FDescriptor::GhostCells,
		FDescriptor::GameState,
	};
	for (const UE::FieldNotification::FFieldId& FieldId : Fields)
	{
		ViewModel->AddFieldValueChangedDelegate(
			FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UTetrisBoardWidget::HandleFieldChanged));
	}
}

void UTetrisBoardWidget::UnsubscribeFromViewModel()
{
	if (ViewModel)
	{
		// 이 위젯이 바인딩한 모든 필드 콜백 제거(CreateUObject의 user object == this).
		ViewModel->RemoveAllFieldValueChangedDelegates(this);
	}
}

void UTetrisBoardWidget::HandleFieldChanged(UObject* /*Object*/, UE::FieldNotification::FFieldId /*FieldId*/)
{
	RepaintFromViewModel();
}

FLinearColor UTetrisBoardWidget::GetPieceColor(EPieceType Type) const
{
	if (const FLinearColor* Found = PieceColors.Find(Type))
	{
		return *Found;
	}
	return FLinearColor::Transparent;
}

void UTetrisBoardWidget::RepaintFromViewModel()
{
	const int32 Width = TetrisConstants::BoardWidth;
	const int32 Height = TetrisConstants::BoardVisibleHeight;
	if (CellImages.Num() != Width * Height)
	{
		return; // 그리드 미구성(WBP 없음 등) — 안전 탈출.
	}

	// VM 미연결: 전체 배경색으로.
	if (!ViewModel)
	{
		const FLinearColor Bg = GetPieceColor(EPieceType::None);
		for (UImage* Cell : CellImages)
		{
			if (Cell) { Cell->SetColorAndOpacity(Bg); }
		}
		return;
	}

	// 레이어 합성용 색 버퍼(index = row*Width + col, row=top0). ① 고정 그리드 베이스.
	TArray<FLinearColor> Colors;
	Colors.SetNum(Width * Height);
	const TArray<EPieceType>& Locked = ViewModel->GetLockedGrid();
	for (int32 Y = 0; Y < Height; ++Y)
	{
		const int32 Row = Height - 1 - Y; // board-space(Y-up) → grid row(top0) 플립
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 BoardIdx = Y * Width + X;
			const EPieceType Type = Locked.IsValidIndex(BoardIdx) ? Locked[BoardIdx] : EPieceType::None;
			Colors[Row * Width + X] = GetPieceColor(Type);
		}
	}

	const EPieceType ActiveType = ViewModel->GetActivePieceType();

	// ② 고스트 — 비점유(None) 칸에만, 활성 색을 GhostAlpha로 흐리게.
	if (ActiveType != EPieceType::None)
	{
		FLinearColor GhostColor = GetPieceColor(ActiveType);
		GhostColor.A *= GhostAlpha;
		for (const FIntPoint& P : ViewModel->GetGhostCells())
		{
			if (P.X >= 0 && P.X < Width && P.Y >= 0 && P.Y < Height)
			{
				const int32 Row = Height - 1 - P.Y;
				const int32 BoardIdx = P.Y * Width + P.X;
				const bool bEmpty = !Locked.IsValidIndex(BoardIdx) || Locked[BoardIdx] == EPieceType::None;
				if (bEmpty)
				{
					Colors[Row * Width + P.X] = GhostColor;
				}
			}
		}
	}

	// ③ 활성 — 최상위(고스트/고정 위에 덮어쓰기).
	if (ActiveType != EPieceType::None)
	{
		const FLinearColor ActiveColor = GetPieceColor(ActiveType);
		for (const FIntPoint& P : ViewModel->GetActiveCells())
		{
			if (P.X >= 0 && P.X < Width && P.Y >= 0 && P.Y < Height)
			{
				const int32 Row = Height - 1 - P.Y;
				Colors[Row * Width + P.X] = ActiveColor;
			}
		}
	}

	// 합성 결과를 셀 이미지에 적용(색만 변경).
	for (int32 i = 0; i < CellImages.Num(); ++i)
	{
		if (CellImages[i])
		{
			CellImages[i]->SetColorAndOpacity(Colors[i]);
		}
	}
}
