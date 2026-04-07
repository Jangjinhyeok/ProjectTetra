// Copyright ProjectTetra. All Rights Reserved.

#include "System/TetrisRandomizer.h"

UTetrisRandomizer::UTetrisRandomizer()
{
}

void UTetrisRandomizer::Initialize(int64 InSeed)
{
	CurrentSeed = (InSeed != 0) ? InSeed : static_cast<int64>(FDateTime::Now().GetTicks());
	// Why: FRandomStream은 int32. 안정적 캐스팅 — 시드 자체는 리플레이를 위해 보존.
	RandomStream.Initialize(static_cast<int32>(CurrentSeed));
	NextQueue.Reset();
	RefillQueue();
	bInitialized = true;
	OnNextQueueChanged.Broadcast(NextQueue);
}

void UTetrisRandomizer::Reset()
{
	// 동일 시드로 재현 가능하도록 CurrentSeed를 그대로 사용.
	NextQueue.Reset();
	RandomStream.Initialize(static_cast<int32>(CurrentSeed));
	RefillQueue();
	OnNextQueueChanged.Broadcast(NextQueue);
}

EPieceType UTetrisRandomizer::Dequeue()
{
	if (!bInitialized)
	{
		Initialize(0);
	}
	if (NextQueue.Num() == 0)
	{
		RefillQueue();
	}
	const EPieceType Out = NextQueue[0];
	NextQueue.RemoveAt(0);
	RefillQueue();
	OnNextQueueChanged.Broadcast(NextQueue);
	return Out;
}

void UTetrisRandomizer::GenerateAndAppendBag()
{
	TArray<EPieceType> Bag = {
		EPieceType::I, EPieceType::O, EPieceType::T,
		EPieceType::S, EPieceType::Z, EPieceType::J, EPieceType::L
	};

	// Fisher-Yates 셔플 — Why: 균등 분포 보장, O(7) 비용.
	for (int32 i = Bag.Num() - 1; i > 0; --i)
	{
		const int32 j = RandomStream.RandRange(0, i);
		Bag.Swap(i, j);
	}
	NextQueue.Append(Bag);
}

void UTetrisRandomizer::RefillQueue()
{
	while (NextQueue.Num() < NextQueueMinSize)
	{
		GenerateAndAppendBag();
	}
}
