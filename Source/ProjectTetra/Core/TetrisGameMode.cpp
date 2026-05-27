// Copyright ProjectTetra. All Rights Reserved.

#include "Core/TetrisGameMode.h"
#include "Input/TetrisPlayerController.h"

ATetrisGameMode::ATetrisGameMode()
{
	PlayerControllerClass = ATetrisPlayerController::StaticClass();
}
