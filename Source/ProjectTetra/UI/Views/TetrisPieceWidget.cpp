// Copyright ProjectTetra. All Rights Reserved.

#include "UI/Views/TetrisPieceWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"

void UTetrisPieceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 색상 기본값 — WBP 미지정 시 사용(보드 위젯과 동일 팔레트, 데이터 주도). 빈칸은 PieceColors가 아닌 투명으로 처리.
	if (PieceColors.Num() == 0)
	{
		PieceColors.Add(EPieceType::I, FLinearColor(0.0f, 0.85f, 0.95f, 1.f));
		PieceColors.Add(EPieceType::O, FLinearColor(0.95f, 0.85f, 0.0f, 1.f));
		PieceColors.Add(EPieceType::T, FLinearColor(0.65f, 0.2f, 0.85f, 1.f));
		PieceColors.Add(EPieceType::S, FLinearColor(0.2f, 0.85f, 0.2f, 1.f));
		PieceColors.Add(EPieceType::Z, FLinearColor(0.9f, 0.2f, 0.2f, 1.f));
		PieceColors.Add(EPieceType::J, FLinearColor(0.2f, 0.35f, 0.9f, 1.f));
		PieceColors.Add(EPieceType::L, FLinearColor(0.95f, 0.5f, 0.0f, 1.f));
	}

	BuildGrid();
	Repaint();
}

void UTetrisPieceWidget::SetPiece(EPieceType Type)
{
	if (CurrentPiece == Type)
	{
		return; // 동일값 무동작 — 불필요한 repaint 억제.
	}
	CurrentPiece = Type;
	Repaint();
}

void UTetrisPieceWidget::BuildGrid()
{
	// 멱등: 이미 만들었거나 패널이 없으면 건너뛴다(재진입/WBP 미구성 안전).
	if (CellImages.Num() > 0 || !PieceGrid || !WidgetTree)
	{
		return;
	}

	CellImages.Reserve(GridSize * GridSize);

	// 흰 박스 브러시를 1회 설정하고, Repaint에선 SetColorAndOpacity로 색만 바꾼다(보드 위젯과 동일 방식).
	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());

			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			Cell->SetBrush(Brush);

			if (UUniformGridSlot* GridSlot = PieceGrid->AddChildToUniformGrid(Cell, Row, Col))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}

			CellImages.Add(Cell); // index = Row*GridSize + Col
		}
	}
}

void UTetrisPieceWidget::Repaint()
{
	if (CellImages.Num() != GridSize * GridSize)
	{
		return; // 그리드 미구성(WBP 없음 등) — 안전 탈출.
	}

	// 전체 투명으로 초기화 후, 현재 피스 모양 셀만 색칠.
	for (UImage* Cell : CellImages)
	{
		if (Cell) { Cell->SetColorAndOpacity(FLinearColor::Transparent); }
	}

	if (CurrentPiece == EPieceType::None)
	{
		return; // 빈 슬롯(예: Hold 미보유) — 전부 투명.
	}

	const FLinearColor Color = GetPieceColor(CurrentPiece);
	for (const FIntPoint& P : GetPreviewCells(CurrentPiece))
	{
		if (P.X >= 0 && P.X < GridSize && P.Y >= 0 && P.Y < GridSize)
		{
			const int32 Idx = P.Y * GridSize + P.X; // X=col, Y=row
			if (CellImages[Idx]) { CellImages[Idx]->SetColorAndOpacity(Color); }
		}
	}
}

FLinearColor UTetrisPieceWidget::GetPieceColor(EPieceType Type) const
{
	if (const FLinearColor* Found = PieceColors.Find(Type))
	{
		return *Found;
	}
	return FLinearColor::Transparent;
}

TArray<FIntPoint> UTetrisPieceWidget::GetPreviewCells(EPieceType Type)
{
	// 4x4-local 캐논 미리보기 모양(X=col, Y=row, row 0=상단). 표시 전용 — 게임플레이 SRS 회전과 무관.
	switch (Type)
	{
	case EPieceType::I: return { {0,1},{1,1},{2,1},{3,1} };
	case EPieceType::O: return { {1,0},{2,0},{1,1},{2,1} };
	case EPieceType::T: return { {1,0},{0,1},{1,1},{2,1} };
	case EPieceType::S: return { {1,0},{2,0},{0,1},{1,1} };
	case EPieceType::Z: return { {0,0},{1,0},{1,1},{2,1} };
	case EPieceType::J: return { {0,0},{0,1},{1,1},{2,1} };
	case EPieceType::L: return { {2,0},{0,1},{1,1},{2,1} };
	default: return {};
	}
}
