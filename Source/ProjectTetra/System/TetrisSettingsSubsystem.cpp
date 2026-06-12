// Copyright ProjectTetra. All Rights Reserved.

#include "System/TetrisSettingsSubsystem.h"

#include "System/TetrisSettingsSaveGame.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** 설정 전용 단일 슬롯 — 진행 저장과 분리(설계노트 4: 다중 슬롯/프로필 없음). */
	const FString SettingsSlotName = TEXT("TetrisSettings");
	constexpr int32 SettingsUserIndex = 0;
}

void UTetrisSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 슬롯이 있으면 로드해 캐시를 덮어쓴다. 없으면(최초 실행) 기본값 캐시 유지 — 저장은 최초 Set 시점에.
	if (const UTetrisSettingsSaveGame* Loaded = Cast<UTetrisSettingsSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SettingsSlotName, SettingsUserIndex)))
	{
		// 과거 슬롯이 현재 clamp 범위를 벗어나도 안전하게 보정. 구버전 슬롯에 DCD/SDF 필드가 없으면
		// 역직렬화가 UPROPERTY 기본값(0 / 20)을 유지하므로 clamp만으로 호환 처리된다.
		DASms = FMath::Clamp(Loaded->DASms, DASMin, DASMax);
		ARRms = FMath::Clamp(Loaded->ARRms, ARRMin, ARRMax);
		DCDms = FMath::Clamp(Loaded->DCDms, DCDMin, DCDMax);
		SDF = FMath::Clamp(Loaded->SDF, SDFMin, SDFMax);
	}
}

void UTetrisSettingsSubsystem::SetDASms(int32 NewValue)
{
	const int32 Clamped = FMath::Clamp(NewValue, DASMin, DASMax);
	if (Clamped == DASms)
	{
		return; // 무변경 — 불필요한 디스크 쓰기 회피.
	}
	DASms = Clamped;
	SaveToSlot();
}

void UTetrisSettingsSubsystem::SetARRms(int32 NewValue)
{
	const int32 Clamped = FMath::Clamp(NewValue, ARRMin, ARRMax);
	if (Clamped == ARRms)
	{
		return;
	}
	ARRms = Clamped;
	SaveToSlot();
}

void UTetrisSettingsSubsystem::SetDCDms(int32 NewValue)
{
	const int32 Clamped = FMath::Clamp(NewValue, DCDMin, DCDMax);
	if (Clamped == DCDms)
	{
		return;
	}
	DCDms = Clamped;
	SaveToSlot();
}

void UTetrisSettingsSubsystem::SetSDF(int32 NewValue)
{
	const int32 Clamped = FMath::Clamp(NewValue, SDFMin, SDFMax);
	if (Clamped == SDF)
	{
		return;
	}
	SDF = Clamped;
	SaveToSlot();
}

void UTetrisSettingsSubsystem::SaveToSlot() const
{
	UTetrisSettingsSaveGame* SaveObject = Cast<UTetrisSettingsSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UTetrisSettingsSaveGame::StaticClass()));
	if (!SaveObject)
	{
		return;
	}
	SaveObject->DASms = DASms;
	SaveObject->ARRms = ARRms;
	SaveObject->DCDms = DCDms;
	SaveObject->SDF = SDF;
	UGameplayStatics::SaveGameToSlot(SaveObject, SettingsSlotName, SettingsUserIndex);
}
