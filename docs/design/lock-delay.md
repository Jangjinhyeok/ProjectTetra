# Lock Delay

> **Status**: In Design
> **Author**: user + Claude (Architect)
> **Last Updated**: 2026-05-27
> **Implements Pillar**: Deterministic Simulation + Fair Game Feel

## Overview

Lock Delay는 피스가 바닥(또는 블록 위)에 접지한 뒤 즉시 고정되지 않고 일정 유예 시간을 주는 시스템이다. 이 유예 동안 플레이어는 좌우 이동·회전으로 피스를 마지막으로 조정할 수 있다. FSM의 `Locking` 상태에 종속된 **타이머 + 리셋 정책**으로 구현되며, 고정 스텝 `Tick()`(정수 카운트)로 진행한다. 이동/회전이 성공하면 타이머를 리셋하되, **무한 지연(stalling)을 막기 위해 리셋 횟수 상한(Move Reset Limit)**을 둔다. 접지가 풀리면(다시 떠오르면) 타이머를 종료하고 FSM이 `Falling`으로 복귀시킨다. 순수 C++ 로직이며 UI를 모른다. Lock Delay의 정확성이 "마지막 순간 조정"이라는 테트리스 핵심 조작감을 좌우한다.

## Player Fantasy

Lock Delay는 "관용(forgiveness)"과 "통제감"을 만든다. 피스가 바닥에 닿는 순간 굳어버린다면 플레이어는 늘 쫓기지만, 짧은 유예가 있으면 "마지막 한 칸을 밀어 넣는다"는 정밀 조작이 가능해진다 — 틈새로 미끄러뜨리는 tuck, 회전으로 비집는 spin, T-Spin 셋업의 마무리가 모두 이 유예 위에서 성립한다. 동시에 이 시스템은 **긴장의 상한선**을 갖는다: Move Reset Limit 덕분에 무한히 버틸 수 없으므로, 플레이어는 "조정은 허용되지만 영원히는 아니다"라는 규칙을 체득한다. 레퍼런스는 Guideline 계열과 Tetr.io의 lock delay 감각이다. 평범한 플레이어에겐 거의 의식되지 않는 인프라지만, 숙련자에겐 finesse와 고급 테크닉을 떠받치는 **직접 활용 대상**이다.

## Detailed Design

### Core Rules

**1. 접지 판정 (Grounded)**
- `Grounded = !Board.IsValidPosition(블록을 (0,-1) 시프트)` — 한 칸 아래로 못 가면 접지.
- FSM이 Falling 스텝 종료 시 평가, 접지면 `Locking` 진입.

**2. 타이머 (정수 스텝 — 결정성)**
- `LockElapsedSteps`(int). `Locking` 진입 시 `0`. `Tick()`마다 `+= 1`. `LockElapsedSteps >= LockDelaySteps`이면 만료 → FSM에 `Lock` 신호.
- `LockDelayMs`는 ms로 노출하되 내부는 `LockDelaySteps = round(LockDelayMs/1000 × SimHz)`로 환산(기본 500ms → 30스텝 @60Hz). **float 초 누적 대신 정수 스텝 카운트**로 플랫폼 무관 결정성을 보장한다.

**3. 리셋 — Extended Placement Lock Down**
- 이동/회전 **성공** 시: `LockElapsedSteps = 0`, `MoveResetCounter++`.
- `MoveResetCounter >= MoveResetLimit(15)`이면 **더 이상 타이머 리셋 안 함** — 이동/회전은 여전히 허용되나 타이머는 계속 흘러 다음 만료에서 강제 Lock.
- **최저행 리프레시**: 피스의 최저 블록 Y가 기존 기록(`LowestRowRecord`)보다 낮아지면 `MoveResetCounter = 0`으로 리프레시. → 정당하게 내려가면 리셋 예산 회복.

**4. Ungrounded 처리**
- `Locking` 중 이동/회전으로 다시 아래로 갈 수 있게 되면, FSM이 `NotifyMoveOrRotate(bStillGrounded=false, …)`를 호출 → Lock Delay가 `bActive=false`로 타이머를 중지한다. FSM은 자신이 계산한 `bStillGrounded=false`로 `Falling` 복귀를 결정한다. (별도 `OnUngrounded` 호출 불필요)
- **`MoveResetCounter`는 피스당 유지** — 진동(접지↔비접지 반복)으로 무한 지연하는 우회를 차단한다. 단 최저행 갱신 시에만 리프레시.

**5. HardDrop**
- Lock Delay를 우회한다(FSM이 즉시 Lock). 이 시스템은 관여하지 않는다.

**6. 피스 교체 시 초기화**
- 새 피스 스폰 또는 Hold 교환 시 `LockElapsedSteps=0`, `MoveResetCounter=0`, `LowestRowRecord` 초기화.

### States and Transitions

Lock Delay는 자체 FSM이 없고 FSM `Locking` 상태에 종속된 헬퍼다. 내부 상태 변수와 FSM 이벤트별 동작으로 기술한다.

**내부 상태 변수**

| 변수 | 의미 |
|------|------|
| `LockElapsedSteps` | 접지 후 누적 스텝 (int) |
| `MoveResetCounter` | 피스당 리셋 사용 횟수 |
| `LowestRowRecord` | 이 피스가 도달한 최저 블록 Y |
| `bActive` | Locking 상태(타이머 진행) 여부 |

**FSM 이벤트 → Lock Delay 동작**

| FSM 이벤트 / 호출 | Lock Delay 동작 | FSM에 돌려주는 신호 |
|------------------|----------------|---------------------|
| `OnNewPiece` (스폰 / Hold 교환) | `LockElapsedSteps=0`, `MoveResetCounter=0`, `LowestRowRecord=현재 최저 Y`, `bActive=false` | — |
| `OnGrounded` (Falling→Locking) | `bActive=true`, `LockElapsedSteps=0` | — |
| `Tick()` (Locking 중) | `LockElapsedSteps += 1` | `LockElapsedSteps >= LockDelaySteps` → **만료(Lock)** |
| `NotifyMoveOrRotate(true, …)` (접지 유지) | `MoveResetCounter < Limit`이면 `LockElapsedSteps=0`, `MoveResetCounter++` | — |
| `NotifyMoveOrRotate` 중 최저행 갱신 | `LowestRowRecord` 갱신 + `MoveResetCounter=0` | — |
| `NotifyMoveOrRotate(false, …)` (ungrounded) | `bActive=false`, 타이머 중지 (`MoveResetCounter` 유지) | FSM이 `bStillGrounded=false`로 Falling 복귀 결정 |
| HardDrop | 무시 (관여 안 함) | — (FSM이 즉시 Lock) |

**리셋 우선순위**: 최저행 리프레시(`MoveResetCounter=0`)는 한도 초과 상태도 회복시킨다. 즉 "아래로 진행 = 예산 리필"이 한도 도달보다 우선한다.

### Interactions with Other Systems

**설계 결정: Lock Delay는 Board/Piece를 직접 참조하지 않는다 (순수 값 로직)**
- 접지 판정(`!IsValidPosition(shift down)`)과 최저 블록 Y 계산은 **FSM이 수행**한다(FSM은 이미 Board+Piece를 소유). 결과(`bGrounded`, `LowestBlockY`)를 Lock Delay에 **주입**한다.
- 결과: Lock Delay는 **의존성 0의 순수 타이머/카운터** → 단위 테스트가 Board/Piece 없이 가능하다. 프로젝트의 강한 분리 원칙과 정합한다.
- 대안(Lock Delay가 Board+Piece ref를 들고 직접 계산)은 self-contained지만 결합도↑·테스트 난이도↑ → 비채택.

**제안 API (HANDOFF용 구체화)**
```cpp
void OnNewPiece(int32 LowestBlockY);                 // 스폰/Hold 교환
void OnGrounded();                                   // Falling→Locking
bool Tick();                                         // 1 스텝 진행 → 만료 여부 (정수 카운트)
void NotifyMoveOrRotate(bool bStillGrounded, int32 NewLowestBlockY);  // !grounded면 내부 비활성화
// 읽기: GetElapsedSteps(), GetMoveResetCounter()    // ViewModel/디버그
```

**데이터 흐름**
```
FSM (Board/Piece 소유) ──grounded·lowestY 계산──► LockDelay.{OnGrounded, NotifyMoveOrRotate}
FSM.Step() ──► LockDelay.Tick() ──► bExpired ──► FSM이 Lock 수행
```

**의존 관계 요약**: Lock Delay는 직접 의존이 **없다**. 논리적으로 grounded/lowestY가 Board+Piece에서 파생되나, 계산은 FSM이 하고 값만 전달한다.

## Formulas

**LD1. ms → 스텝 환산 (결정성)**
```
LockDelaySteps = round(LockDelayMs / 1000 × SimHz)
예) 500ms @ 60Hz → 30스텝
```

**LD2. 만료 판정 (정수 스텝)**
```
on Tick(): LockElapsedSteps += 1
bExpired = (LockElapsedSteps >= LockDelaySteps)
```

**LD3. 리셋 (Extended Placement)**
```
on Move/Rotate success (grounded):
    if MoveResetCounter < MoveResetLimit:
        LockElapsedSteps = 0
        MoveResetCounter += 1
    // 한도 도달 시 타이머 리셋 없음 (계속 흐름)
```

**LD4. 최저행 리프레시 (한도보다 우선)**
```
on piece position change:
    if NewLowestBlockY < LowestRowRecord:
        LowestRowRecord = NewLowestBlockY
        MoveResetCounter = 0
```

**변수 정의 / 범위**

| 변수 | 의미 | 기본 | 범위 | 소유 |
|------|------|------|------|------|
| `LockDelayMs` | 락 지연 시간(ms) | 500 | 0~2000 | Lock Delay |
| `LockDelaySteps` | 환산 스텝 수 | 30 | 파생 | Lock Delay |
| `MoveResetLimit` | 피스당 리셋 상한 | 15 | 0~30 | Lock Delay |
| `LockElapsedSteps` | 누적 스텝(int) | 0 | 0~LockDelaySteps | Lock Delay |
| `MoveResetCounter` | 사용 리셋 횟수 | 0 | 0~Limit | Lock Delay |
| `LowestRowRecord` | 최저 도달 Y | 스폰 Y | 0~23 | Lock Delay |

## Edge Cases

**1. `LockDelayMs = 0`**
- 접지 즉시 만료 → 즉시 Lock. 클래식(무유예) 모드. 정상 허용.

**2. `MoveResetLimit = 0`**
- 리셋 불가. 접지 시 타이머 시작, 이동/회전해도 타이머 안 멈춤. 만료 시 Lock. (가장 엄격)

**3. 한도 도달 후 이동/회전**
- 이동/회전 자체는 여전히 허용된다. 단 `LockElapsedSteps` 리셋만 안 됨 → 곧 강제 Lock.

**4. 진동 우회 (접지↔비접지 반복)**
- `MoveResetCounter` 피스당 유지로 차단. 단 실제 하강으로 최저행 갱신 시에만 리프레시 → 제자리 진동으로는 예산 회복 불가.

**5. 20G / SoftDrop ∞ 착지**
- 스폰 즉시 접지: `MoveResetCounter=0`, `LowestRowRecord=착지 Y`, 타이머 정상 시작. SoftDrop이어도 즉시 Lock 아님(`bSoftDropLocks=false`).

**6. 회전 Kick으로 위로 떠오름**
- ungrounded → Falling 복귀. 다시 떨어져 접지 시 새 Locking(`LockElapsedSteps=0`), `MoveResetCounter`는 유지(최저행 안 바뀌었으면).

**7. 회전 Kick으로 더 내려감**
- 최저행 갱신 → `MoveResetCounter=0` 리프레시. 정당한 하강으로 인정.

**8. HardDrop 발생**
- Lock Delay 전체 우회. 타이머/카운터 무관하게 FSM이 즉시 Lock.

## Dependencies

**Upstream (Lock Delay가 직접 의존)**
- **없음.** Lock Delay는 의존성 0의 순수 타이머/카운터다.

**값 출처 (간접, FSM 경유)**

| 논리적 출처 | 파생 값 | 전달자 |
|------------|---------|--------|
| Board `IsValidPosition` | `bGrounded` | FSM이 계산 후 주입 |
| Piece `GetAbsoluteBlockPositions` | `LowestBlockY` | FSM이 계산 후 주입 |

**Downstream (Lock Delay에 의존)**

| 시스템 | 유형 | 사용 |
|--------|------|------|
| FSM | Hard | `OnNewPiece/OnGrounded/Tick/NotifyMoveOrRotate` (Locking 상태 구동) |
| ViewModel | Soft | `GetElapsedSteps/GetMoveResetCounter` (락 임박 시각 피드백, 선택적) |

**공유 타입**: 신규 타입 없음. 튜닝값은 `FLockDelayConfig { LockDelayMs, MoveResetLimit }` 구조체로 묶어 주입 가능(선택).

**양방향 일관성 노트:**
- `fsm.md`는 Lock Delay를 Hard upstream으로 명시함 ✅ (계약 일치).
- `board.md`/`piece-srs.md`의 Downstream 표는 "Lock Delay가 `IsValidPosition`/`GetAbsoluteBlockPositions`를 직접 사용"으로 적혀 있으나, 본 설계에선 **FSM이 대신 호출**한다. 의미상 파생 관계는 유효하므로 두 GDD는 "Lock Delay (FSM 경유)"로 각주 보강을 권장(필수 아님).

## Tuning Knobs

| 변수 | 기본 | 안전 범위 | 영향 / 한계 시 증상 |
|------|------|----------|---------------------|
| `LockDelayMs` | 500 | 0~2000 | 접지 후 유예. 0=즉시 Lock(관용 없음). 너무 길면 스택 위에서 무한정 머무는 느낌 |
| `MoveResetLimit` | 15 | 0~30 | 피스당 리셋 상한. 0=리셋 불가(엄격). 너무 크면 사실상 무한 지연에 근접 |
| `bLowestRowRefresh` | true | bool | true=Extended Placement(하강 시 카운터 리프레시). false=단순 Move Reset(리프레시 없음) |

**상호작용 주의:**
- `LockDelayMs`와 FSM의 `BaseG(Level)`가 함께 고레벨 체감 난이도를 결정 — 고G에서 짧은 LockDelay는 매우 빡빡해진다.
- `bSoftDropLocks`(FSM 소유)가 true면 소프트드롭 착지 시 Lock Delay를 건너뛰므로 본 knob들의 체감이 줄어든다.
- `MoveResetLimit`을 매우 낮추면 T-Spin 셋업 마무리가 어려워질 수 있음 → 고급 플레이 영향, 플레이테스트 필요.

## Acceptance Criteria

**타이머 기본**
- [ ] `OnGrounded` 후 `LockDelaySteps`만큼 `Tick` → `Tick`이 만료(true) 반환
- [ ] 만료 전 `Tick` → false 반환, 상태 유지
- [ ] `LockDelayMs=0` → 접지 직후 첫 `Tick`에서 즉시 만료
- [ ] ms→스텝 환산: 500ms@60Hz → 정확히 30스텝에 만료

**리셋 (Extended Placement)**
- [ ] 접지 상태 이동/회전 → `LockElapsedSteps` 0으로 리셋, `MoveResetCounter` +1
- [ ] `MoveResetCounter == MoveResetLimit` 도달 후 이동/회전 → 리셋 안 됨(타이머 계속 진행)
- [ ] 한도 도달 후에도 만료까지 `Tick`하면 Lock 신호 발생
- [ ] `MoveResetLimit=0` → 첫 이동/회전부터 리셋 없음

**최저행 리프레시**
- [ ] `NewLowestBlockY < LowestRowRecord` → `MoveResetCounter=0`으로 리프레시
- [ ] 한도 초과 상태에서 최저행 갱신 → 카운터 0으로 회복(우선순위 검증)
- [ ] 같은 행 내 좌우 이동(최저행 불변) → 리프레시 안 됨

**Ungrounded / 피스 교체**
- [ ] `NotifyMoveOrRotate(false, …)` → `bActive=false`, 이후 `Tick`은 만료 안 시킴
- [ ] 재접지 후 `MoveResetCounter` 유지(진동 우회 차단 검증)
- [ ] `OnNewPiece` → 모든 상태 초기화(타이머/카운터/최저행)

**격리 / 결정성**
- [ ] Lock Delay 단위 테스트가 Board/Piece 인스턴스 없이 통과 (순수 격리 검증)
- [ ] 동일 호출 시퀀스 → 동일 만료 시점 (결정성)

**성능**
- [ ] `Tick()` 1회 < 1μs (단순 산술)

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | `LockDelayMs` / `MoveResetLimit` 최종값 | 플레이테스트 | 구현 후 |
| 2 | `bSoftDropLocks`(FSM) 기본 정책과의 체감 상호작용 | 플레이테스트 | 구현 후 |
| 3 | 최저행 기준 = 최저 블록 Y (채택) vs 피봇 Y — 구현 검증 | Lead Programmer | 구현 단계 |
| 4 | 고레벨/고G에서 LockDelay 가변 단축 도입 여부 | Game Designer | VS 단계 |
| 5 | `MoveResetLimit`과 T-Spin 셋업 난이도 상호작용 튜닝 | Score/Attack 연계 | VS 단계 |
