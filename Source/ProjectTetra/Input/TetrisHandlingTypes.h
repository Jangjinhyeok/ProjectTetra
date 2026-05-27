// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetrisHandlingTypes.generated.h"

/**
 * 입력 엣지 — PlayerController(frame 도메인)가 캡처해 핸들링 버퍼에 쌓는 "이산 의도".
 *
 * Why: Enhanced Input은 타이밍 판단을 하지 않고 "무엇이 눌렸다/떼졌다"만 기록한다(번역 레이어).
 *   DAS/ARR 같은 자동반복 타이밍은 고정스텝 FTetrisHandling이 held-state로 결정한다.
 *   좌우 이동만 Press/Release 쌍을 갖는 이유: held 지속 여부로 자동반복을 제어하기 때문.
 *   회전/하드드롭/홀드/소프트드롭은 무상태 pass-through라 단일 엣지면 충분하다.
 */
UENUM(BlueprintType)
enum class EInputEdge : uint8
{
	MoveLeftPress     UMETA(DisplayName = "MoveLeftPress"),
	MoveRightPress    UMETA(DisplayName = "MoveRightPress"),
	MoveLeftRelease   UMETA(DisplayName = "MoveLeftRelease"),
	MoveRightRelease  UMETA(DisplayName = "MoveRightRelease"),
	RotateCW          UMETA(DisplayName = "RotateCW"),
	RotateCCW         UMETA(DisplayName = "RotateCCW"),
	HardDrop          UMETA(DisplayName = "HardDrop"),
	Hold              UMETA(DisplayName = "Hold"),
	SoftDropOn        UMETA(DisplayName = "SoftDropOn"),
	SoftDropOff       UMETA(DisplayName = "SoftDropOff"),
};

/**
 * 핸들링 튜닝값 묶음 (DAS/ARR/DCD).
 *
 * Why USTRUCT: Session(UObject)이 UPROPERTY(EditDefaultsOnly)로 에디터에 노출해 FTetrisHandling에
 *   주입하기 위함(FLockDelayConfig와 동일 패턴). 매직 넘버 금지 — 모든 손맛 튜닝값은 여기서 조정.
 *   ms→스텝 환산은 SimHz 해상도로 결정성을 보장한다(Formula H1).
 */
USTRUCT(BlueprintType)
struct FHandlingConfig
{
	GENERATED_BODY()

	/** Delayed Auto Shift — 자동 이동이 시작되기까지의 지연(ms). 0이면 press 즉시 자동반복. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Handling", meta = (ClampMin = "0", ClampMax = "500"))
	int32 DASms = 100;

	/** Auto Repeat Rate — 자동 반복 간격(ms). 0이면 한 스텝에 벽까지 즉시 이동(고수용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Handling", meta = (ClampMin = "0", ClampMax = "200"))
	int32 ARRms = 17;

	/** Direction Change DAS cut — 방향 전환 시 DAS를 일부만 남기고 깎는 양(ms). 런타임에 DASms 이하로 클램프(H5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Handling", meta = (ClampMin = "0", ClampMax = "500"))
	int32 DCDms = 0;

	/** 시뮬레이션 주파수(Hz). ms→스텝 환산에 사용. 결정성을 위해 Session/FSM의 SimHz와 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tetris|Handling", meta = (ClampMin = "1"))
	int32 SimHz = 60;
};

/**
 * 한 고정스텝 동안의 입력 스냅샷 — frame→fixed-step 핸드오프 단위.
 *
 * Why 비-USTRUCT 순수 struct: Session이 매 Step 채우고 드레인하는 내부 버퍼일 뿐, 에디터/BP 노출이
 *   불필요하다. held-state는 좌우만(소프트드롭은 edge pass-through라 held 추적은 GameCore 소유).
 *   프레임 사이 발생한 이산 엣지는 순서대로 누적되어 빠른 탭이 유실되지 않는다.
 */
struct FHandlingInput
{
	bool bLeftHeld = false;
	bool bRightHeld = false;
	TArray<EInputEdge> Edges;
};
