// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TetrisGameMode.generated.h"

/**
 * Tetris GameMode — PlayerController 배선용 최소 GameMode.
 *
 * Why 로직 없음: ADR-0001대로 시뮬레이션은 UTetrisSessionSubsystem(WorldSubsystem)이 호스팅한다.
 *   GameMode는 ATetrisPlayerController를 기본 PC로 지정하는 역할만 한다(시뮬/룰을 넣지 않음).
 */
UCLASS()
class PROJECTTETRA_API ATetrisGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATetrisGameMode();
};
