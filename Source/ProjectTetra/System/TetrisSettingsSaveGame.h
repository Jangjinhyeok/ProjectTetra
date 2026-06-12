// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TetrisSettingsSaveGame.generated.h"

/**
 * 플레이어 설정 영속 페이로드 (DAS/ARR/DCD/SDF).
 *
 * Why USaveGame: 핸들링 손맛(DAS/ARR/DCD/SDF)은 플레이어별 취향이라 게임을 껐다 켜도 유지되어야 한다.
 *   에디터 EditDefaultsOnly(클래스 기본값)는 런타임 변경·영속이 불가하므로 SaveGame 슬롯에 보관한다.
 * Why 디스크 스키마를 좁게: 진행 저장과 분리된 "설정 전용" 페이로드 — 값만 담아 마이그레이션 부담을 줄인다.
 *   기본값은 코어 기본(DAS 100ms / ARR 17ms / DCD 0ms / SDF 20×)과 일치시켜 슬롯 부재 시 동작이 동일하게 한다.
 * Why 구버전 슬롯 호환: 과거 슬롯에 DCD/SDF 필드가 없어도 역직렬화 시 UPROPERTY 기본값이 유지되므로
 *   로드 측 clamp만으로 안전하다(별도 버전 게이트 불필요).
 */
UCLASS()
class PROJECTTETRA_API UTetrisSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Delayed Auto Shift (ms). FHandlingConfig::DASms 기본과 일치. */
	UPROPERTY()
	int32 DASms = 100;

	/** Auto Repeat Rate (ms). FHandlingConfig::ARRms 기본과 일치. */
	UPROPERTY()
	int32 ARRms = 17;

	/** DAS Cut Delay (ms). FHandlingConfig::DCDms 기본과 일치. */
	UPROPERTY()
	int32 DCDms = 0;

	/** Soft Drop Factor (배수). UTetrisGameCore::SoftDropFactor 기본과 일치. */
	UPROPERTY()
	int32 SDF = 20;
};
