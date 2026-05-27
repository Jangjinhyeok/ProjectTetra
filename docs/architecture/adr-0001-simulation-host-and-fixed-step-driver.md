# ADR-0001: 시뮬레이션 호스트 & 고정스텝 구동 — Tickable World Subsystem

## Status
Accepted

## Date
2026-05-27

## Context

### Problem Statement
Core Loop(Board / Randomizer / Scoring / `UTetrisGameCore`)는 순수 로직 UObject로 구현됐고 단위/통합 테스트로 검증됐다. 하지만 이들을 **런타임에 생성·소유·초기화하고, 매 프레임 고정스텝으로 `GameCore::Step()`을 구동할 호스트가 없다.** 현재 프로젝트엔 게임플레이 프레임워크 클래스(GameMode/PlayerController/Subsystem)와 프로젝트 맵이 전혀 없다(순수 로직 모듈). 이 호스트를 무엇으로 둘지는 이후 ViewModel/UI가 시뮬레이션에 접근하는 방식까지 결정한다.

### Constraints
- `UTetrisGameCore::Step()`은 고정 타임스텝 가정(fsm.md §Formulas F1). 가변 `DeltaTime`을 직접 넣으면 안 됨.
- 결정성: 동일 시드 + 동일 명령 + 동일 스텝 수 → 동일 결과. 호스트는 이 계약을 깨면 안 됨.
- 소유 객체가 UObject이므로 GC 루팅 필요(`UPROPERTY`/`TObjectPtr`).
- 단일 플레이어 퍼즐. 네트워크 권위 모델 불필요.
- 에디터 PIE에서 관찰 가능해야 함(현재 UI 없음).

### Requirements
- 한 "매치"의 수명 동안 Core Loop 객체를 소유·초기화.
- 매 프레임 누적 시간을 `SimDelta` 단위로 쪼개 `Step()` 호출(spiral-of-death 방지 캡 포함).
- 향후 ViewModel/UI가 `UTetrisGameCore`와 그 이벤트에 접근 가능해야 함.
- `StartGame(Seed)` / `RestartGame()` / 일시정지 제어.

## Decision

Core Loop를 **`UTetrisSessionSubsystem : public UTickableWorldSubsystem`** 에 호스팅한다.

- **소유**: 서브시스템이 `UTetrisBoard` / `UTetrisRandomizer` / `UTetrisScoring` / `UTetrisGameCore`를 `UPROPERTY(TObjectPtr<>)`로 생성·보유(GC-safe). 와이어링은 `GameCore->Initialize(Board, Randomizer, Scoring)`.
- **구동**: `FTickableGameObject::Tick(DeltaTime)`에서 **고정스텝 accumulator**(fsm.md F1)로 `GameCore->Step()`을 0회 이상 호출. `MaxStepsPerFrame`로 캡.
- **수명**: `Initialize/Deinitialize`(서브시스템 표준)에서 객체 생성/해제. 게임 시작은 명시적 `StartGame(Seed)`.
- **접근**: `World->GetSubsystem<UTetrisSessionSubsystem>()` 전역 접근. ViewModel은 이를 통해 `GetGameCore()`로 이벤트 구독.
- **틱 가드**: `bRunning` 플래그 + `IsTickable()`로 Idle/Pause/비게임 월드에서 누적·구동 차단. `IsTickableInEditor()=false`.

### Architecture Diagram
```
UWorld
└── UTetrisSessionSubsystem (UTickableWorldSubsystem)   ← 전역 접근 지점
    │   Tick(dt): acc += dt; while(acc>=SimDelta && n<Max){ Core->Step(); acc-=SimDelta; }
    ├── owns UTetrisBoard
    ├── owns UTetrisRandomizer
    ├── owns UTetrisScoring
    └── owns UTetrisGameCore  ──Initialize(Board,Randomizer,Scoring)
                              ──events→ (향후) ViewModel → UMG View
```

### Key Interfaces
```cpp
UCLASS()
class UTetrisSessionSubsystem : public UTickableWorldSubsystem
{
    // 수명
    virtual void Initialize(FSubsystemCollectionBase&) override;  // 객체 생성+와이어링
    virtual void Deinitialize() override;

    // FTickableGameObject
    virtual void Tick(float DeltaTime) override;                  // 고정스텝 accumulator
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;                     // bRunning && 게임월드
    virtual bool IsTickableInEditor() const { return false; }

    // 세션 제어
    void StartGame(int64 Seed);
    void RestartGame();
    void SetPaused(bool bPaused);

    // 접근 (ViewModel용)
    UTetrisGameCore* GetGameCore() const;

    // 튜닝 (fsm.md F1)
    int32 SimHz = 60;
    int32 MaxStepsPerFrame = 5;
};
```

## Alternatives Considered

### Alternative 1: 전용 Actor (`ATetrisGameSession`)
- **Description**: `PrimaryActorTick`로 Step() 구동, GameMode가 스폰하거나 레벨에 배치.
- **Pros**: 명시적 소유/수명, 배치·스폰으로 디버깅 직관적, 교과서적 gameplay 패턴.
- **Cons**: UI/ViewModel이 Actor 레퍼런스를 별도 경로(GameMode getter, `GetAllActorsOfClass`)로 찾아야 함. 보일러플레이트 증가.
- **Rejection Reason**: 전역 접근성이 떨어져 UI 중심 프로젝트에 불리. 단일 매치엔 Actor의 트랜스폼/배치 의미가 불필요.

### Alternative 2: `AGameModeBase` 서브클래스
- **Description**: `ATetrisGameMode`가 세션을 소유·Tick.
- **Pros**: "게임 룰"의 전통적 거처. GameMode getter로 접근.
- **Cons**: GameMode는 멀티플레이에서 server-only(향후 확장 시 제약). UI 접근성은 subsystem만 못함. GameMode에 시뮬 루프를 얹는 건 책임 과적재.
- **Rejection Reason**: 단일 매치 시뮬 호스트로는 subsystem이 더 깔끔하고 UI 친화적.

### Alternative 3: `UGameInstanceSubsystem`
- **Description**: 게임 인스턴스 수명(맵 전환에도 유지)에 세션을 둠.
- **Pros**: 맵 전환에도 상태 유지.
- **Cons**: 한 "매치"는 보통 맵(플레이) 수명과 일치 → 맵 전환 시 자동 리셋이 오히려 바람직. GameInstance 수명은 메뉴↔게임 전역 상태(설정 등)에 더 적합.
- **Rejection Reason**: 매치 단위 수명엔 World 범위가 의미상 정확. (전역 설정/오디오는 추후 GameInstanceSubsystem로 분리)

## Consequences

### Positive
- ViewModel/UI가 `World->GetSubsystem<>()`로 세션·GameCore에 전역 접근 → MVVM 바인딩 배선이 단순.
- 자동 생성/Tick으로 boilerplate 최소. 빈 맵 하나로 PIE 관찰 가능(별도 GameMode 불필요).
- 매치 수명 = 월드 수명 → 맵 재로드가 곧 깔끔한 리셋.
- `UTetrisGameCore` 무변경 — 호스트만 추가(기존 검증 자산 보존).

### Negative
- `FTickableGameObject` 틱은 기본적으로 항상 호출되므로 `IsTickable()`/`bRunning` 가드를 정확히 관리해야 함(누락 시 Idle/Pause에서도 누적).
- 전역 접근의 편의는 남용 시 결합도 증가 여지(ViewModel만 접근하도록 규율 필요).

### Risks
- **에디터/프리뷰 월드에서 틱**: `IsTickableInEditor()=false` + 월드 타입 체크로 차단.
- **프레임 히치 후 따라잡기 폭주**: `MaxStepsPerFrame` 캡(fsm.md F1)으로 완화.
- **일시정지 중 시간 누적**: Pause 시 accumulator 적재 중단(`IsTickable()=false`).

## Performance Implications
- **CPU**: Tick 오버헤드 무시 가능. `Step()`은 GameCore 예산 <5μs(일반)/<20μs(고G). 프레임당 최대 `MaxStepsPerFrame`회.
- **Memory**: UObject 4개 + accumulator. 수 KB.
- **Load Time**: 영향 없음(서브시스템 생성 비용 미미).
- **Network**: 해당 없음(단일 플레이어).

## Migration Plan
신규 추가. 기존 코드 변경 없음(`UTetrisGameCore` 등 무수정). 에디터에서 최소 빈 맵 1개 생성 + 기본맵 지정 필요(바이너리 에셋 → 사용자 에디터 작업).

## Validation Criteria
- 세션 레벨 Automation 테스트: 서브시스템 구동 로직(accumulator)이 N 프레임 누적 → 예상 스텝 수만큼 `Step()` 호출, 결정적 진행.
- PIE 디버그: on-screen/UE_LOG로 보드 상태가 중력에 따라 갱신되는 것 육안 확인.
- `MaxStepsPerFrame` 캡 동작(거대 dt 1회 → 캡만큼만 Step).

## Related Decisions
- 설계: `design/gdd/fsm.md` (F1 고정스텝 accumulator, `Step()`/`EnqueueCommand` 계약), `design/gdd/scoring.md` (`GetBaseG`), `design/gdd/lock-delay.md`.
- 후속(예정): Input/Handling(명령 큐 연결), ViewModel(세션 접근 + 이벤트 구독).
