// Copyright ProjectTetra. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TetrisSettingsSubsystem.generated.h"

/**
 * 플레이어 설정 보관소 (DAS/ARR/DCD/SDF) — SaveGame 영속.
 *
 * Why UGameInstanceSubsystem: 설정은 레벨/월드 전환을 넘어 한 번 로드되어 살아 있어야 한다.
 *   시뮬레이션 호스트(UTetrisSessionSubsystem)는 World subsystem이라 월드마다 재생성되므로 설정 보관엔 부적합.
 *   GameInstance 수명 = 앱 실행 동안 단일 인스턴스라 로드/캐시/저장의 단일 소유자가 된다.
 * Why 인메모리 캐시 + 변경 시에만 저장: 게터는 게임 시작마다 호출되므로 디스크 I/O 없이 캐시를 읽고,
 *   세터에서 clamp 후 값이 실제로 바뀐 경우에만 슬롯에 기록한다(불필요한 쓰기 회피).
 */
UCLASS()
class PROJECTTETRA_API UTetrisSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem — 앱 시작 시 슬롯에서 1회 로드(없으면 기본값 캐시 유지).
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	//~ 게터 — 핸들링 주입(Session::ResetHandling)이 매 StartGame/Retry에 읽는다. 디스크 접근 없음.
	int32 GetDASms() const { return DASms; }
	int32 GetARRms() const { return ARRms; }
	int32 GetDCDms() const { return DCDms; }
	int32 GetSDF() const { return SDF; }

	//~ 세터 — clamp 후 캐시 갱신 + 변경 시 SaveGame 기록. Settings 위젯의 슬라이더 조작이 호출.
	void SetDASms(int32 NewValue);
	void SetARRms(int32 NewValue);
	void SetDCDms(int32 NewValue);
	void SetSDF(int32 NewValue);

private:
	/** 현재 캐시 값을 SaveGame 페이로드로 직렬화해 슬롯에 기록. */
	void SaveToSlot() const;

	/** clamp 범위 — FHandlingConfig의 ClampMin/Max 메타와 일치(매직넘버 단일 출처). */
	static constexpr int32 DASMin = 0;
	static constexpr int32 DASMax = 500;
	static constexpr int32 ARRMin = 0;
	static constexpr int32 ARRMax = 200;
	// DCD: FHandlingConfig::DCDms 메타(ClampMin 0 / Max 500)와 일치. 런타임은 H5로 [0, DASSteps] 재클램프.
	static constexpr int32 DCDMin = 0;
	static constexpr int32 DCDMax = 500;
	// SDF: 정수 배수(×). 1=등속, 40=상한(tetr.io 41=instant 센티넬은 MVP 범위 밖 — 설계노트 4).
	static constexpr int32 SDFMin = 1;
	static constexpr int32 SDFMax = 40;

	/** 인메모리 캐시 — 기본값은 코어 기본과 일치(슬롯 부재 시 무변경 동작). */
	int32 DASms = 100;
	int32 ARRms = 17;
	int32 DCDms = 0;
	int32 SDF = 20;
};
