# FSM (Game State Machine / Game Loop)

> **Status**: In Design
> **Author**: user + Claude (Architect)
> **Last Updated**: 2026-05-27
> **Implements Pillar**: Deterministic Simulation + Event-Driven Core Loop

## Overview

FSM(Game State Machine)은 테트리스 한 판의 진행을 구동하는 **오케스트레이션 척추**다. 새 피스 스폰 → 낙하(중력) → 접지 → Lock → 라인 클리어 → 다음 스폰의 순환을 상태 기계로 표현하고, Top-out 시 게임을 종료한다. FSM은 **고정 타임스텝 `Step(FixedDeltaTime)`** 으로 전진하며, 입력은 직접 받지 않고 **추상 명령 큐**(MoveLeft/Right, Rotate, SoftDrop, HardDrop, Hold)를 소비한다. 매 스텝마다 명령을 처리하고 중력을 누적 적용한 뒤 상태 전이를 평가한다. Board·Piece/SRS·Randomizer·Lock Delay를 호출자로서 조율하되, 이들의 내부는 모르고 각자의 공개 인터페이스만 사용한다. **순수 C++**로 UI/Actor/Blueprint를 참조하지 않으며, 동일 시드 + 동일 명령 시퀀스 → 동일 결과를 보장하는 **결정적 시뮬레이션(Deterministic Simulation)**이다. UI 갱신은 ViewModel이 구독할 고수준 이벤트(상태 전이, 피스 스폰/이동, 게임오버)로 노출한다.

**핵심 프레이밍 (불변 결정):**
1. **입력 비결합** — FSM은 Enhanced Input을 모르고 추상 명령 큐만 안다. 결정성 + 단위 테스트 용이성을 확보하며, 이것이 Input 시스템과의 경계선이다.
2. **고정 타임스텝** — 프레임레이트 독립. 가변 `DeltaTime`을 그대로 중력에 쓰지 않고 고정 스텝으로 누적·소비한다.
3. **호출자 역할** — Board GDD가 명시한 대로 FSM이 `Clear()`/`LockPiece()`/`ClearLines()`/Top-out 판정을 호출한다. 그 계약을 그대로 존중한다.

## Player Fantasy

FSM은 플레이어가 그 존재를 인식하지 못해야 성공하는 시스템이다. 플레이어가 실제로 느끼는 것은 세 가지다:

- **즉응성(Responsiveness)** — 입력한 순간 피스가 정확히 그만큼 반응한다. 명령 큐를 매 스텝 결정적으로 소비하므로 "눌렀는데 안 먹었다" 같은 입력 유실이 없다.
- **공정성(Fairness)** — 접지 후 Lock Delay 동안 조정할 여유가 있고, 같은 상황은 항상 같게 동작한다(결정성). 무작위로 손해 보는 느낌이 없다.
- **흐름(Flow)** — 스폰 → 낙하 → Lock → 클리어 → 다음 스폰이 끊김 없이 이어져, 플레이어는 상태 전환을 의식하지 않고 "다음 한 수"에만 몰입한다.

레퍼런스는 **Tetr.io**의 스냅 있는 조작감과 결정적 동작이다. 이 신뢰감 위에서 finesse(최소 입력 배치), T-Spin, 속공 같은 고급 플레이가 성립한다. FSM이 흔들리면(중력 타이밍 들쭉, Lock 타이밍 불일치) 플레이어는 즉시 "조작이 이상하다"고 느낀다 — 즉 FSM의 품질이 곧 스킬 실링의 하한선이다.

이 시스템은 **"loved-engagement"가 아니라 "invisible-infrastructure"**로 자리매김한다(Board와 동일 성격). 직접 사랑받는 시스템이 아니라, 다른 모든 시스템의 신뢰를 떠받치는 기반이다.

## Detailed Design

### Core Rules

**1. 구동 모델 (Fixed-Step Simulation)**
- FSM은 `Step(FixedDeltaTime)` 호출로만 전진한다. 기본 고정 스텝 = `1 / SimHz` (기본 `SimHz = 60`).
- 가변 프레임 환경에서는 외부 드라이버(Game Session)가 경과 시간을 고정 스텝 단위로 쪼개 `Step()`을 0회 이상 호출한다(accumulator pattern). **FSM 내부는 항상 고정 dt만 본다.**
- 결정성 계약: 동일 시드(Randomizer) + 동일 명령 시퀀스 + 동일 스텝 수 → 동일 최종 상태.

**2. 명령 큐 (Command Queue)**
- 입력은 추상 명령으로만 유입: `EGameCommand { MoveLeft, MoveRight, RotateCW, RotateCCW, SoftDropOn, SoftDropOff, HardDrop, Hold }`.
- Input 시스템이 `EnqueueCommand()`로 적재, FSM이 큐를 소유·소비. **DAS/ARR 자동 반복은 Input이 매 스텝 `MoveLeft/Right`를 적재하는 방식** — FSM은 1 명령 = 1칸으로만 처리한다.
- `SoftDropOn/Off`는 지속 상태 플래그(`bSoftDropHeld`)를 토글한다. 나머지는 이산 명령이다.

**3. 스텝 처리 순서 (결정성의 핵심 — 고정)**
매 `Step()`에서 현재 상태 기준으로:
1. **명령 큐 드레인** (FIFO) → 각 명령을 현재 상태 핸들러로 처리
2. **중력 적분** (Falling 상태에서만)
3. **접지/Lock 평가** (Lock Delay 타이머 갱신)
4. **상태 전이 평가**

이 순서(입력 → 중력 → Lock)를 고정하는 것이 결정성을 보장한다.

**4. 중력 (G-accumulator)**
- `GravityAccumulator += GravityPerStep` (단위: cells/step). `>= 1`인 동안 1칸씩 하강 시도(`TryMove(Down)`); 더 못 내려가면 누적기를 0으로 클램프하고 **Locking**으로 진입한다.
- `GravityPerStep = BaseG(Level) × (bSoftDropHeld ? SoftDropFactor : 1)`. `SoftDropFactor = ∞`(또는 20G 이상)이면 한 스텝에 바닥까지 하강.
- **소프트드롭은 자동 Lock하지 않는다** — 바닥에 닿아도 Lock Delay가 적용된다. 즉시 Lock은 **HardDrop만** 수행한다. (공정성 규칙)

**5. 게임 루프 (상태 순환)**
- **Spawn**: `Randomizer.Dequeue()` → `FTetrisPieceOps::Spawn()` → `Board.IsBlockOut()` 검사. 충돌 시 → GameOver(BlockOut). 아니면 `bHoldUsedThisPiece=false` 리셋 후 Falling 진입. (ARE=0이면 즉시 전이)
- **Falling**: 명령(이동/회전/소프트드롭/하드드롭/Hold) 처리 + 중력 적분. 피스가 더 못 내려가면 Locking 진입. HardDrop이면 즉시 최하단으로 이동한 뒤 곧장 Lock을 수행한다.
- **Locking**: 접지 상태에서 Lock Delay 타이머가 진행된다. 타이머 만료 또는 HardDrop → `Board.LockPiece()` → LineClear로. Move/Rotate로 다시 떠서 접지가 풀리면 → Falling 복귀(리셋 정책은 Lock Delay 시스템 관할).
- **LineClear**: `Board.ClearLines()` → `FLineClearResult`를 Score/Attack로 이벤트 발행한다. (클리어 지연=0이면 즉시) 직후 Lock된 좌표로 `Board.IsLockOut()` 검사 → LockOut이면 GameOver, 아니면 다음 Spawn.
- **GameOver**: 종료 상태. 입력을 무시한다. 외부 `RestartGame()` 시 `Board.Clear()` + `Randomizer.Reset()` + 상태 초기화.

**6. Hold (액션, Falling/Locking 중)**
- `bHoldUsedThisPiece == true`면 무시한다(락 전 1회 제한).
- HoldSlot이 비어 있으면: 현재 피스 타입을 저장 → `Randomizer.Dequeue()`로 새 피스 스폰.
- HoldSlot이 차 있으면: 현재 피스 타입 ↔ HoldSlot 교환 → 교환된 타입을 스폰 위치/방향 `State0`으로 재배치.
- `bHoldUsedThisPiece=true`로 설정하고, 중력/Lock Delay를 리셋한 뒤 Falling으로 진입한다.

**7. Top-out 판정 (2종)**
- **Block Out**: Spawn 시 `IsBlockOut()` → GameOver(BlockOut)
- **Lock Out**: Lock 후 `IsLockOut()` → GameOver(LockOut)

**8. 드롭 칸 집계 (Score/Level 공급용)**
- FSM은 피스 생애 동안 `SoftDropCells`(소프트드롭으로 하강한 칸 수)와 `HardDropCells`(하드드롭 칸 수)를 누적한다. 스폰/Hold 교환 시 0으로 리셋, `OnPieceLocked` 발행 시 페이로드로 전달한다. (중력에 의한 자연 낙하는 집계하지 않음 — 입력 기반 드롭만)

> **Provisional**: Lock Delay의 타이머 값·리셋 정책(Move Reset Limit)은 **Lock Delay GDD에서 상세화**한다. 여기서는 "Locking 상태에서 타이머 만료 시 Lock, ungrounded 시 Falling 복귀"라는 계약만 고정한다.

### States and Transitions

**상태 집합** (컨텍스트 문서 `S_*` 매핑 포함)

| 상태 | 컨텍스트 매핑 | 의미 | 비고 |
|------|--------------|------|------|
| `Idle` | — | 게임 미시작/대기. FSM 생성 직후 기본 상태 | `StartGame()`으로 탈출 |
| `Spawn` | S_NEW | 다음 피스 꺼내 배치 + Block Out 검사 | ARE=0이면 즉시 통과 |
| `Falling` | S_MOVE/DOWN | 중력 적분 + 명령 처리 | 루프 본체 |
| `Locking` | S_LOCK | 접지 후 Lock Delay 카운트다운 | ungrounded 시 Falling 복귀 |
| `LineClear` | S_REMOVE | 라인 제거 + 결과 발행 + Lock Out 검사 | 클리어 지연=0이면 즉시 |
| `GameOver` | S_ISDIE | 종료(터미널). 입력 무시 | `RestartGame()`으로 탈출 |

**전이 표**

| From | 이벤트/조건 | Action | To |
|------|------------|--------|-----|
| `Idle` | `StartGame()` | `Board.Clear()`, `Randomizer.Initialize(seed)` | `Spawn` |
| `Spawn` | `IsBlockOut()==true` | `OnTopOut(BlockOut)` 발행 | `GameOver` |
| `Spawn` | 스폰 성공 | `bHoldUsedThisPiece=false`, 중력 누적기 0 | `Falling` |
| `Falling` | 중력/소프트드롭으로 접지(더 못 내려감) | Lock Delay 타이머 시작 | `Locking` |
| `Falling` | `HardDrop` 명령 | 최하단 이동 → `LockPiece()` | `LineClear` |
| `Falling` | `Hold` (사용 가능) | 피스 교환/스폰, 플래그/타이머 리셋 | `Falling` |
| `Falling` | 이동/회전 명령 | `TryMove`/`TryRotate` | `Falling` |
| `Locking` | Lock Delay 만료 | `LockPiece()` | `LineClear` |
| `Locking` | `HardDrop` 명령 | `LockPiece()` (즉시) | `LineClear` |
| `Locking` | 이동/회전으로 ungrounded | Lock Delay 리셋 정책 적용 | `Falling` |
| `Locking` | `Hold` (사용 가능) | 피스 교환/스폰, 리셋 | `Falling` |
| `LineClear` | `ClearLines()` 처리 + `IsLockOut()==true` | `OnTopOut(LockOut)` 발행 | `GameOver` |
| `LineClear` | 처리 완료 + LockOut 아님 | (ARE=0 즉시) | `Spawn` |
| `GameOver` | `RestartGame()` | `Board.Clear()`, `Randomizer.Reset()` | `Spawn` |

**다이어그램**
```
                 StartGame()
   Idle ───────────────────────► Spawn ◄───────────────┐
                                   │                    │
                  IsBlockOut       │ 성공               │ LockOut 아님
              ┌────────────────────┤                    │
              ▼                    ▼                     │
          GameOver ◄──LockOut── LineClear ◄──Lock──┐  Spawn으로
              ▲                    ▲               │     ▲
              │                    │               │     │
              │              HardDrop/만료    Locking    │
              │                    │           ▲   │     │
              │ RestartGame()      │    접지   │   │ungrounded
              └──────────────  Falling ────────┘   └─────┘
                                 ▲ │
                                 └─┘ 이동/회전/Hold/소프트드롭
```

### Interactions with Other Systems

**FSM이 소유(own)하는 것**: 활성 피스 인스턴스(`FActivePiece`), HoldSlot, 중력 누적기, 현재 상태(`EGameState`), 명령 큐. Board/Randomizer는 **참조**하며 소유는 Game Session 계층이 결정한다.

**Upstream — FSM이 호출/구독 (코드 확인된 시그니처)**

| 시스템 | 인터페이스 | 용도 | 상태 |
|--------|-----------|------|------|
| Board | `Clear()`, `LockPiece(Coords,Type)`, `ClearLines()→FLineClearResult`, `IsBlockOut(Coords)`, `IsLockOut(Coords)` | 보드 변이·판정 | ✅ 구현됨 |
| Piece | `FTetrisPieceOps::{Spawn, TryMove, TryRotate, HardDrop, GetAbsoluteBlockPositions}` | 활성 피스 연산 | ✅ 구현됨 |
| Randomizer | `Initialize(Seed)`, `Reset()`, `Dequeue()→EPieceType`, `GetNextQueue()` | 다음 피스 공급 | ✅ 구현됨 |
| Lock Delay | `StartTimer()`, `Tick(dt)→bool 만료`, `NotifyMoveOrRotate()`(리셋), grounded 재평가 | 접지 후 락 타이밍 | ⚠️ provisional (다음 GDD) |
| Score/Level | `GetBaseG(Level)` 또는 `OnLevelChanged` 구독 → `BaseG` 갱신 | 중력 속도 커브 | ⚠️ provisional (다음 GDD) |

**Downstream — FSM에 의존**

| 시스템 | FSM에서 받는 것 | 방향 |
|--------|----------------|------|
| Input | `EnqueueCommand(EGameCommand)` (단방향, FSM을 읽지 않음) | Input → FSM |
| Game Session/Flow | `StartGame()`, `Step(dt)`, `RestartGame()` 구동 + 고정스텝 accumulator 소유 | Session → FSM |
| ViewModel | FSM 이벤트 구독 + 상태 읽기 (활성 피스, Hold, 현재 상태, Level) | FSM → ViewModel |
| Score/Level | `OnPieceLocked` 구독 (매 lock, 0줄 포함 — 콤보 리셋 위해 FSM 경유) | FSM → Score |
| Attack/Garbage | 라인 클리어/공격 트리거 (Board 이벤트 또는 FSM 이벤트) | ⚠️ provisional (VS) |

**FSM이 발행하는 이벤트** (non-dynamic multicast — 구독자 C++ ViewModel, Board 컨벤션과 일치)

| 이벤트 | 페이로드 | 구독자 |
|--------|---------|--------|
| `OnStateChanged` | `EGameState Old, New` | ViewModel |
| `OnActivePieceUpdated` | `const FActivePiece&` (스폰/이동/회전/드롭) | ViewModel(Ghost 포함) |
| `OnHoldChanged` | `EPieceType HoldType, bool bCanHold` | ViewModel |
| `OnGameOver` | `ETopOutType` | ViewModel, Session |
| `OnPieceLocked` | `int32 LinesCleared, TArray<int32> ClearedRows, bool bWasLastActionRotation, int32 LastKickIndex, int32 SoftDropCells, int32 HardDropCells` | Score/Level |

> Next Queue 변경은 `Randomizer.OnNextQueueChanged`를 ViewModel이 직접 구독한다(FSM 비경유).
> `OnPieceLocked`는 **매 lock마다(0줄 포함)** 발행 — Score/Level의 콤보 리셋이 0줄 lock을 알아야 하기 때문(board.md Edge 4: Board는 0줄에서 미발행). Score/Level GDD Interactions 참조.
> 페이로드는 **원시 데이터**(라인수 + 회전/킥 정보 + 드롭 칸)만 담는다. "difficult clear" 분류(Tetris/T-Spin)는 **Score/Level이 수행** — 점수 도메인 개념을 FSM에 누수시키지 않기 위함.

**데이터 흐름 요약**
```
Input → FSM.EnqueueCommand() ┐
                             ├─► FSM.Step(dt): 명령 처리 → 중력 → Lock 평가 → 상태 전이
Session → FSM.Step(dt) ──────┘        │
                                      ├─► Board.{LockPiece, ClearLines, Is*Out}
                                      ├─► Piece Ops (TryMove/Rotate/HardDrop)
                                      ├─► Randomizer.Dequeue
                                      ├─► OnPieceLocked ──► Score/Level (매 lock, 0줄 포함)
                                      └─► OnStateChanged / OnActivePieceUpdated / OnHoldChanged / OnGameOver
Board.OnLinesCleared ─────────────────► Attack, ViewModel(라인 연출) (Score는 FSM 경유)
```

## Formulas

**F1. 고정 스텝 누적 (Game Session 드라이버 — FSM 외부지만 계약상 명시)**
```
SimDelta = 1 / SimHz                       // 고정 스텝 길이(초)
TimeAccumulator += RealDeltaTime
steps = 0
while TimeAccumulator >= SimDelta AND steps < MaxStepsPerFrame:
    FSM.Step(SimDelta);  TimeAccumulator -= SimDelta;  steps++
```
- `MaxStepsPerFrame`(예: 5)로 spiral-of-death 방지(프레임 끊김 시 무한 따라잡기 차단).

**F2. 스텝당 중력**
```
GravityPerStep = BaseG(Level) × (bSoftDropHeld ? SoftDropFactor : 1)   [cells/step]
```

**F3. 중력 적용 (스텝 내 하강)**
```
GravityAccumulator += GravityPerStep
CellsToDrop = floor(GravityAccumulator)
repeat CellsToDrop:
    if TryMove(Down): GravityAccumulator -= 1
    else: GravityAccumulator = 0; → Locking; break
```

**F4. BaseG(Level) — ⚠️ 잠정 참조 커브 (Guideline), Score/Level GDD 소유**
```
SecPerRow(Level) = (0.8 − (Level−1)×0.007) ^ (Level−1)   [초/행], Level ∈ [1, 15]
BaseG(Level)     = 1 / (SecPerRow(Level) × SimHz)         [cells/step]
```
- Level 1 → SecPerRow=1.0초 → BaseG≈0.0167 (60스텝에 1칸)
- Level 10 → SecPerRow≈0.0646초 → BaseG≈0.258
- Level ≥ ~15 → 1G 이상(거의 즉시 낙하)

**F5. SoftDrop 즉시 바닥 (SDF=∞)**
```
SoftDropFactor = ∞ → GravityPerStep = ∞ → CellsToDrop = 바닥까지의 DropDistance
단, Lock은 즉시 수행하지 않음 (Lock Delay 적용). 즉시 Lock은 HardDrop만.
```

**변수 정의 / 범위**

| 변수 | 의미 | 기본 | 범위 | 소유 |
|------|------|------|------|------|
| `SimHz` | 고정 시뮬 주파수 | 60 | 30~240 | FSM |
| `SimDelta` | 스텝 길이 | 1/60 | 파생 | FSM |
| `MaxStepsPerFrame` | 프레임당 스텝 상한 | 5 | 1~10 | Session |
| `GravityAccumulator` | 중력 누적기(cells) | 0 | ≥0 | FSM |
| `BaseG(Level)` | 레벨별 기본 중력 | 위 커브 | >0 | ⚠️ Score/Level |
| `SoftDropFactor` | 소프트드롭 배수 | 20 (또는 ∞) | 1~∞ | ⚠️ Input |

> **Provisional**: `BaseG(Level)` 커브와 `SoftDropFactor` 기본값은 각각 Score/Level, Input GDD에서 확정한다. FSM은 이 값을 외부에서 주입받아 사용할 뿐 정의하지 않는다.

## Edge Cases

**1. 같은 스텝에 여러 명령 (회전+이동+HardDrop 동시)**
- 큐를 FIFO로 드레인하되, **상태 전이를 유발하는 명령(HardDrop) 처리 시 드레인 루프를 break**하고 잔여 명령은 큐에 남긴다. 다음 `Step()`에서 새 상태(Spawn→Falling)가 처리 → 버퍼된 입력이 다음 피스에 자연 적용된다. 결정적.

**2. Hold 연타**
- `bHoldUsedThisPiece` 플래그로 락 전 1회만 허용한다. 두 번째 Hold 명령은 무시(소비만).

**3. Hold 교환 후 스폰 위치 충돌**
- 교환된 피스를 스폰 위치/`State0`에 재배치 후 `IsBlockOut()` 검사. 충돌이면 → GameOver(BlockOut). (높이 쌓인 상황의 드문 케이스)

**4. 20G / SoftDrop ∞ 스폰**
- 스폰 직후 첫 Falling 스텝에서 중력이 바닥까지 적용 → 즉시 Locking. Lock Delay는 여전히 적용(즉시 Lock 아님). 정상 동작.

**5. `Step(dt)`에 비정상 dt**
- FSM은 고정 `SimDelta`만 받는 계약. `dt <= 0`이면 no-op. 거대 dt는 Session이 `MaxStepsPerFrame`로 쪼개므로 FSM 단일 호출엔 도달하지 않는다.

**6. `Idle` / `GameOver`에서 `Step()` 호출**
- 중력·명령 모두 무시(no-op). 명령 큐는 비워 둔다(누적 방지). `GameOver` 이벤트는 진입 시 1회만 발행한다.

**7. Lock Delay 무한 리셋 악용 (무한 스핀)**
- Move Reset Limit(예: 15회) 초과 시 추가 리셋 무시 → 강제 Lock. ⚠️ 정확한 한계값·정책은 **Lock Delay GDD 소유**. FSM은 "리셋 허용 여부"를 Lock Delay에 질의만 한다.

**8. 동시성 (명령 enqueue ↔ Step)**
- FSM은 게임 스레드 단일 실행 가정. `EnqueueCommand`와 `Step`은 동일 스레드. 멀티스레드 보호 없음(설계상 불필요).

**9. LineClear 0줄 (Lock했지만 완성 줄 없음)**
- `ClearLines()`가 0 반환 + 이벤트 미발행(Board GDD Edge 4). FSM은 그대로 LockOut 검사 후 Spawn으로 진행한다.

**10. 빈 보드에서 즉시 HardDrop 반복**
- 매 피스가 바닥에 쌓인다. 정상. 결정성 테스트의 기본 시나리오로 활용한다.

## Dependencies

**Upstream (FSM이 의존)**

| 시스템 | 유형 | 인터페이스 | GDD |
|--------|------|-----------|-----|
| Board | Hard | `Clear/LockPiece/ClearLines/IsBlockOut/IsLockOut` | ✅ (board.md가 FSM을 dependent로 명시) |
| Piece/SRS | Hard | `FTetrisPieceOps::*` | ✅ (piece-srs.md가 FSM 명시) |
| Randomizer | Hard | `Initialize/Reset/Dequeue/GetNextQueue` | ✅ — ⚠️ randomizer.md에 FSM consumer 명시 추가 필요 |
| Lock Delay | Hard | 타이머 시작/Tick/리셋 질의 계약 | ⚠️ provisional (다음 GDD) |
| Input/Handling | Hard | `EnqueueCommand(EGameCommand)` (Input→FSM 단방향). `EGameCommand`는 FSM이 정의 | ⚠️ provisional |
| Score/Level | **Soft** | `BaseG(Level)` 주입 (없으면 고정 G로 동작 가능) | ⚠️ provisional |

**Downstream (FSM에 의존)**

| 시스템 | 유형 | 사용 |
|--------|------|------|
| Game Session/Flow | Hard | `StartGame/Step/RestartGame` 구동, 고정스텝 소유 |
| ViewModel | Hard | FSM 이벤트 구독 + 상태 읽기 |
| Attack/Garbage | Soft | 라인클리어/공격 트리거 (provisional, VS) |

**포함 관계 (contains, 의존 아님)**
- **Hold**: FSM 내부 액션 + `HoldSlot` 상태로 귀속된다. 별도 시스템이 아니라 FSM이 *포함*한다. systems-index의 Hold 항목은 "FSM 내 구현"으로 갱신한다. ViewModel은 FSM의 `OnHoldChanged`로 HoldSlot을 읽는다.

**공유 타입 (Core 모듈)**
- 신규: `EGameState { Idle, Spawn, Falling, Locking, LineClear, GameOver }`, `EGameCommand { MoveLeft, MoveRight, RotateCW, RotateCCW, SoftDropOn, SoftDropOff, HardDrop, Hold }`
- 기존 재사용: `EPieceType`, `ETopOutType`, `FActivePiece`, `FLineClearResult`

**양방향 일관성 점검 결과:**
1. `randomizer.md` — 이미 FSM을 Downstream(`Dequeue/Initialize/Reset` 소비자)으로 명시함. ✅ 일관성 충족.
2. `board.md`, `piece-srs.md` — 이미 FSM을 dependent로 명시함. ✅
3. `systems-index.md` Hold 행(order 8) → "FSM 내 구현(contains)"으로 표기 변경 완료.

## Tuning Knobs

**FSM 소유 knob**

| 변수 | 기본 | 안전 범위 | 영향 / 한계 시 증상 |
|------|------|----------|---------------------|
| `SimHz` | 60 | 30~240 | 시뮬 정밀도. 낮으면 고G에서 거친 낙하·입력 해상도↓. 높으면 CPU↑(스텝당 연산 동일하나 횟수↑) |
| `MaxStepsPerFrame` | 5 | 1~10 | 프레임 끊김 복구 상한. 낮으면 랙 후 슬로우모션, 높으면 따라잡기 스파이크 |
| `EntryDelaySteps` (ARE) | 0 | 0~30 | Spawn 전 대기 스텝. 0=즉시. 높이면 속공 리듬 둔화 |
| `LineClearDelaySteps` | 0 | 0~40 | LineClear 체류 스텝(연출 동기화용). 0=즉시. 로직 지연이므로 0 권장, 연출은 ViewModel 독립 |
| `bSoftDropLocks` | false | bool | true면 소프트드롭 바닥 도달 시 즉시 Lock. false=Lock Delay 적용(공정성 기본) |
| `bHoldEnabled` | true | bool | Hold 기능 on/off (게임 모드용) |

**외부 소유 — 여기서 정의하지 않고 참조만**

| 변수 | 소유 GDD |
|------|----------|
| `BaseG(Level)` 중력 커브 | Score/Level |
| `SoftDropFactor` (SDF), DAS, ARR, DCD | Input/Handling |
| `LockDelayDuration`, `MoveResetLimit` | Lock Delay |

**상호작용 주의:**
- `LineClearDelaySteps > 0` + 높은 레벨 → 클리어 대기 중 입력 버퍼링 정책 필요(Edge Case 1과 연계).
- `EntryDelaySteps`와 DAS는 함께 "초기 이동 타이밍"을 결정 → Input GDD에서 교차 검토.

## Acceptance Criteria

**상태 머신 / 전이**
- [ ] `StartGame()` → Idle에서 Spawn으로 전이, 첫 피스 활성화
- [ ] Spawn 성공 → Falling 전이 + `OnActivePieceUpdated` 발행, `bHoldUsedThisPiece=false`
- [ ] Falling에서 접지 → Locking 전이
- [ ] Locking에서 Lock Delay 만료 → `LockPiece()` 호출 + LineClear 전이
- [ ] Locking에서 이동/회전으로 ungrounded → Falling 복귀
- [ ] LineClear 처리 후 LockOut 아니면 Spawn 복귀

**고정 스텝 / 중력**
- [ ] Level 1, 60스텝 경과 → 피스 정확히 1칸 하강 (BaseG≈0.0167 검증)
- [ ] 누적기 floor 동작 — 부분 누적이 다음 스텝으로 이월
- [ ] SoftDropOn → GravityPerStep이 SDF배 증가, SoftDropOff → 복귀
- [ ] SDF=∞ → 한 스텝에 바닥까지 하강하되 Lock은 미수행(Lock Delay 적용)
- [ ] `dt<=0` Step 호출 → 상태 불변(no-op)

**HardDrop / Lock**
- [ ] HardDrop → 최하단 이동 + 즉시 Lock + LineClear 전이
- [ ] HardDrop으로 같은 스텝 잔여 명령은 다음 Step으로 이월(드레인 break)

**Hold**
- [ ] 빈 HoldSlot에서 Hold → 현재 피스 저장 + 다음 피스 스폰 + `OnHoldChanged`
- [ ] 채워진 HoldSlot에서 Hold → 피스 교환, State0 재배치
- [ ] 같은 피스에서 Hold 2회 → 두 번째 무시
- [ ] `bHoldEnabled=false` → Hold 명령 완전 무시

**Top-out**
- [ ] Spawn 위치 충돌 → `OnGameOver(BlockOut)` 발행, GameOver 전이
- [ ] Lock 후 전 블록이 버퍼존 → `OnGameOver(LockOut)`
- [ ] GameOver에서 입력/Step → no-op, 이벤트 중복 발행 없음

**결정성 (핵심)**
- [ ] 동일 시드 + 동일 명령 시퀀스 + 동일 스텝 수 → 동일 최종 보드/피스 상태 (재현 테스트)
- [ ] 두 FSM 인스턴스에 같은 입력 → 바이트 단위 동일 상태

**이벤트**
- [ ] 상태 전이 시에만 `OnStateChanged` 발행 (동일 상태 재진입 시 미발행)
- [ ] 피스 이동/회전/스폰/드롭 시 `OnActivePieceUpdated` 발행

**성능 예산**
- [ ] `Step()` 1회 < 5μs (일반 케이스, 명령 큐 비었을 때)
- [ ] `Step()` 1회 < 20μs (20G + 명령 다수 케이스)
- [ ] FSM 인스턴스 메모리 < 1KB (보드/랜덤마이저 제외)
- [ ] Tick 기반 UI 갱신 0회 — 모든 UI 통지는 이벤트 기반 (forbidden pattern 위반 없음)

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | Lock Delay 지속시간 · 리셋 정책 · Move Reset Limit 정확값 | Lock Delay GDD | 다음 문서 |
| 2 | `BaseG(Level)` 최종 커브 (Guideline vs 커스텀) | Score/Level GDD | 그다음 문서 |
| 3 | `SoftDropFactor`/DAS/ARR/DCD 기본값 | Input GDD | VS 단계 |
| 4 | ARE/LineClearDelay 실제 활성화 여부 (게임필 튜닝) | 구현 + 플레이테스트 | 구현 후 |
| 5 | LineClear 연출 동기화: 로직 0지연 유지 vs 연출 중 입력 버퍼링 정책 | ViewModel/UI 단계 | Presentation |
| 6 | Attack/Garbage가 FSM 이벤트 vs Board 이벤트 중 무엇을 구독할지 | Attack GDD | VS 단계 |
| 7 | 180° 회전 추가 시 `EGameCommand`·Piece 회전 확장 | Game Designer | VS 단계 |
| 8 | 결정성 상태 직렬화 포맷 (리플레이/넷코드) | Lead Programmer | 향후 |
