// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotification/FieldId.h"
#include "Core/TetrisTypes.h"
#include "TetrisHUDWidget.generated.h"

class UTextBlock;
class UPanelWidget;
class UTetrisPieceWidget;
class UTetrisHUDViewModel;

/**
 * HUD 렌더 위젯 (View) — HUD VM을 바인딩으로 소비해 점수/레벨/줄 + Next 큐 + Hold를 표시한다.
 *
 * Why View는 VM만 안다: Model/Session/바인더를 직접 참조하지 않는다(HANDOFF §2).
 *   VM은 Global View Model Collection에서 "TetrisHUD" 키로 resolve하거나 SetViewModel로 외부 주입받는다.
 * Why 이벤트 주도: NativeTick/polling 없음. 갱신은 VM FieldNotify 구독 → RefreshAll()로만 트리거된다.
 * Next 미리보기는 NextPieceWidgetClass(WBP_PieceWidget)를 NextContainer에 동적 생성해 재사용한다.
 */
UCLASS()
class PROJECTTETRA_API UTetrisHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 외부 주입 경로(예: Session/테스트). Collection resolve와 양립 — null-safe. 구독을 재설정한다. */
	UFUNCTION(BlueprintCallable, Category = "Tetris|HUD")
	void SetViewModel(UTetrisHUDViewModel* InViewModel);

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//~ BindWidget — WBP가 이름 일치로 제공.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LinesText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTetrisPieceWidget> HoldPiece;

	/** Next 미리보기들이 추가될 컨테이너(VerticalBox 등). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> NextContainer;

	/** Next 미리보기 슬롯 위젯 클래스(WBP_PieceWidget). NextContainer에 동적 생성. */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|HUD")
	TSubclassOf<UTetrisPieceWidget> NextPieceWidgetClass;

	/** 표시할 Next 미리보기 최대 개수(VM NextQueueDisplayCount 이하 권장). */
	UPROPERTY(EditDefaultsOnly, Category = "Tetris|HUD", meta = (ClampMin = "1", ClampMax = "5"))
	int32 NextPreviewCount = 5;

private:
	/** Collection에서 "TetrisHUD" VM을 resolve(월드/서브시스템 부재 시 null). */
	UTetrisHUDViewModel* ResolveViewModel() const;

	/** NextContainer에 NextPreviewCount개 미리보기 위젯을 1회 생성·보관. */
	void BuildNextSlots();

	/** VM FieldNotify(Score/Level/Lines/HoldPiece/NextQueue) 구독. */
	void SubscribeToViewModel();
	/** 구독 전부 해제. */
	void UnsubscribeFromViewModel();

	/** FieldNotify 콜백 — 어떤 필드가 바뀌든 전체 갱신(MVP: HUD는 가벼워 부분 갱신 불필요). */
	void HandleFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

	/** VM 상태로 텍스트/Hold/Next를 다시 채운다. VM 미연결 시 기본값. */
	void RefreshAll();

	UPROPERTY(Transient)
	TObjectPtr<UTetrisHUDViewModel> ViewModel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTetrisPieceWidget>> NextSlots;
};
