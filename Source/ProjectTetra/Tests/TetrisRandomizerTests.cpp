// Copyright ProjectTetra. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "System/TetrisRandomizer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisRandomizerOneBagTest,
	"Tetris.Randomizer.OneBagContainsAllSeven", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisRandomizerOneBagTest::RunTest(const FString&)
{
	UTetrisRandomizer* R = NewObject<UTetrisRandomizer>();
	R->Initialize(12345);

	TSet<EPieceType> Seen;
	for (int32 i = 0; i < 7; ++i)
	{
		Seen.Add(R->Dequeue());
	}
	TestEqual(TEXT("한 Bag = 7종 모두"), Seen.Num(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisRandomizerThousandBagsTest,
	"Tetris.Randomizer.ThousandBagsBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisRandomizerThousandBagsTest::RunTest(const FString&)
{
	UTetrisRandomizer* R = NewObject<UTetrisRandomizer>();
	R->Initialize(42);

	TMap<EPieceType, int32> Counts;
	for (int32 i = 0; i < 7000; ++i)
	{
		++Counts.FindOrAdd(R->Dequeue());
	}
	const EPieceType Types[] = { EPieceType::I, EPieceType::O, EPieceType::T,
		EPieceType::S, EPieceType::Z, EPieceType::J, EPieceType::L };
	for (EPieceType T : Types)
	{
		TestEqual(TEXT("정확히 1000회 등장"), Counts[T], 1000);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisRandomizerSeedDeterminismTest,
	"Tetris.Randomizer.SeedDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisRandomizerSeedDeterminismTest::RunTest(const FString&)
{
	UTetrisRandomizer* A = NewObject<UTetrisRandomizer>();
	UTetrisRandomizer* B = NewObject<UTetrisRandomizer>();
	A->Initialize(99);
	B->Initialize(99);

	for (int32 i = 0; i < 100; ++i)
	{
		const EPieceType Pa = A->Dequeue();
		const EPieceType Pb = B->Dequeue();
		TestEqual(TEXT("동일 시드 → 동일 순서"), Pa, Pb);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisRandomizerNextQueueRefillTest,
	"Tetris.Randomizer.NextQueueRefill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisRandomizerNextQueueRefillTest::RunTest(const FString&)
{
	UTetrisRandomizer* R = NewObject<UTetrisRandomizer>();
	R->NextQueueMinSize = 5;
	R->Initialize(7);

	for (int32 i = 0; i < 50; ++i)
	{
		R->Dequeue();
		TestTrue(TEXT("Queue 항상 >= MinSize"), R->GetNextQueue().Num() >= 5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTetrisRandomizerResetTest,
	"Tetris.Randomizer.Reset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTetrisRandomizerResetTest::RunTest(const FString&)
{
	UTetrisRandomizer* R = NewObject<UTetrisRandomizer>();
	R->Initialize(123);

	TArray<EPieceType> First;
	for (int32 i = 0; i < 14; ++i) First.Add(R->Dequeue());

	R->Reset();

	TArray<EPieceType> Second;
	for (int32 i = 0; i < 14; ++i) Second.Add(R->Dequeue());

	for (int32 i = 0; i < 14; ++i)
	{
		TestEqual(TEXT("Reset 후 동일 시퀀스"), First[i], Second[i]);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
