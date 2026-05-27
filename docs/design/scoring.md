# Score / Level

> **Status**: In Design
> **Author**: user + Claude (Architect)
> **Last Updated**: 2026-05-27
> **Implements Pillar**: Skill Expression + Progression Feedback

## Overview

Score/Level은 점수·삭제 줄 수·레벨·콤보·Back-to-Back을 추적하는 시스템이다. Board의 `OnLinesCleared` 이벤트와 피스의 T-Spin 판정 정보를 소비해 점수를 계산하고, 누적 삭제 줄 수에 따라 레벨을 올린다. **레벨은 FSM이 소비하는 `BaseG(Level)` 중력 커브의 입력**이 되어 난이도 곡선을 형성한다. 순수 C++ 이벤트 주도 로직으로 UI를 모르며, 변경 시 ViewModel이 구독할 통지(점수/레벨/콤보/B2B 변경)를 발행한다. 점수 규칙의 깊이(T-Spin, B2B, 콤보 보너스)가 곧 이 게임의 스킬 표현 폭을 결정한다.

## Player Fantasy

Score/Level은 "숙련이 보상받는다"는 성취 루프다. 한 줄씩 지우는 안전한 플레이도 점수를 주지만, 테트리스(4줄)·T-Spin·Back-to-Back 체인·콤보처럼 **위험하고 어려운 기술일수록 보상이 급격히 커진다**. 플레이어는 "그냥 지우기"에서 "더 크게 지우기"로 자연스럽게 욕망이 옮겨가고, 이 점수 곡선이 스킬 실링을 끌어올린다. 동시에 레벨 상승 → 낙하 속도 증가는 **점증하는 긴장**을 만든다: 잘할수록 빨라지고, 빨라질수록 실수의 대가가 커진다. 레퍼런스는 Guideline/Tetr.io의 점수 체계다. Board나 FSM 같은 invisible-infrastructure와 달리, 이 시스템은 플레이어가 **직접 좇는 "loved-engagement"** 대상이다 — 하이스코어와 화려한 클리어가 동기의 핵심이다.

## Detailed Design

### Core Rules

**1. 라인 클리어 기본 점수 (× Level, Guideline)**

| 클리어 | 기본 | 적용 |
|--------|------|------|
| Single (1줄) | 100 | × Level |
| Double (2줄) | 300 | × Level |
| Triple (3줄) | 500 | × Level |
| Tetris (4줄) | 800 | × Level |

- 0줄: 점수 없음.

**2. Back-to-Back (B2B)**
- **Difficult clear = Tetris(4줄)** (MVP). T-Spin은 VS에서 difficult로 편입 예정.
- 직전도 difficult였으면 B2B 활성 → 해당 클리어 점수 **×1.5** (예: B2B Tetris = 800×Level×1.5).
- B2B 체인은 **non-difficult 클리어(1~3줄)**로 끊김. **0줄 lock은 체인 유지**(끊지 않음).

**3. Combo**
- 매 lock에서 1줄 이상 클리어 시 `Combo++`, 0줄이면 `Combo = -1`로 리셋.
- 보너스: `50 × Combo × Level` (Combo ≥ 1일 때). 첫 클리어는 Combo 0(보너스 없음), 연속 두 번째부터 가산.

**4. 드롭 점수 (Guideline, 경량)**
- Soft drop: 1점/칸, Hard drop: 2점/칸. FSM이 드롭 칸 수를 전달. (Core 내 minor — FSM 연동 필요)

**5. Level 진행**
- `Level = 1 + floor(TotalLinesCleared / LinesPerLevel)`, `LinesPerLevel = 10`. Level 1부터 시작.
- `LevelCap`(기본 20)으로 상한 — 중력 발산 방지.

**6. BaseG(Level) — 중력 커브 (이 시스템 소유, FSM에 제공)**
```
SecPerRow(Level) = (0.8 − (Level−1)×0.007) ^ (Level−1)
BaseG(Level)     = 1 / (SecPerRow(Level) × SimHz)
```
- fsm.md F4의 잠정값을 여기서 **정식 확정**한다. FSM은 spawn/`OnLevelChanged` 시 조회.

**7. 이벤트 (ViewModel 구독)**
- `OnScoreChanged(int64)`, `OnLevelChanged(int32)`, `OnLinesChanged(int32)`, `OnComboChanged(int32)`, `OnB2BChanged(int32)`

### States and Transitions

Score/Level은 자체 FSM이 없는 이벤트 주도 누산기다. 내부 상태 + lock 이벤트 처리 파이프라인으로 기술한다.

**내부 상태 변수**

| 변수 | 타입 | 의미 |
|------|------|------|
| `Score` | int64 | 누적 점수 |
| `TotalLinesCleared` | int32 | 누적 삭제 줄 수 (레벨 산출) |
| `Level` | int32 | 현재 레벨 (1부터) |
| `Combo` | int32 | 연속 클리어 카운트 (-1 = 비활성) |
| `B2BCount` | int32 | 연속 difficult 클리어 수 (0 = 비활성) |
| `bLastClearWasDifficult` | bool | 직전 클리어가 difficult였는지 |

**lock 처리 파이프라인** (입력: `FLineClearResult` + 드롭 정보)

| 단계 | N(=LinesCleared) > 0 | N == 0 |
|------|----------------------|--------|
| 1. Combo 갱신 | `Combo++` | `Combo = -1` |
| 2. Difficult 판정 | `bDifficult = (N == 4)` (MVP) | — |
| 3. B2B 갱신 | 직전 difficult & 현재 difficult → `B2BCount++`; 현재 non-difficult → `B2BCount=0` | **유지** |
| 4. 점수 가산 | 기본×Level (×1.5 if B2B 활성) + `50×Combo×Level` | 없음 |
| 5. 드롭 점수 | soft 1/칸 + hard 2/칸 | soft/hard 동일 적용 |
| 6. Level 재계산 | `1 + floor(TotalLines/10)`, cap | — |
| 7. 이벤트 발행 | 변경된 값마다 `On*Changed` | `OnComboChanged` |
| 8. `bLastClearWasDifficult` | `= bDifficult` | 변경 없음 |

**리셋(게임 시작/재시작)**: 모든 변수 초기화 — `Score=0, TotalLines=0, Level=1, Combo=-1, B2BCount=0, bLastClearWasDifficult=false`.

### Interactions with Other Systems

**Score가 구독 (Upstream)**

| 출처 | 신호 | 용도 |
|------|------|------|
| **FSM** | `OnPieceLocked(LinesCleared, ClearedRows, bWasLastActionRotation, LastKickIndex, SoftDropCells, HardDropCells)` | **매 lock마다(0줄 포함)** 콤보/B2B/점수/레벨 갱신 |

> **difficult 분류는 Score가 수행**: FSM은 원시 데이터만 전달하고, Score가 `bDifficult = (LinesCleared == 4)`로 판정(MVP). VS에서 `bWasLastActionRotation`/`LastKickIndex`로 T-Spin을 difficult에 편입. 점수 도메인 로직을 FSM에 누수시키지 않는다.

> **왜 Board.`OnLinesCleared` 직접 구독이 아닌가**: Board는 0줄에서 이벤트를 발행하지 않는다(board.md Edge 4) → **콤보 리셋 신호가 누락**된다. 따라서 매 lock을 아는 **FSM이 단일 소스**로 전달한다. (Board.`OnLinesCleared`는 그대로 Attack/ViewModel의 라인 연출용으로 사용)

**Score가 제공 (Downstream)**

| 대상 | 인터페이스 | 용도 |
|------|-----------|------|
| FSM | `GetBaseG(Level)`, `GetLevel()` 또는 `OnLevelChanged` 구독 | 중력 커브 |
| ViewModel | `On*Changed` 이벤트 + getters | HUD(점수/레벨/콤보/B2B) |
| Attack | `Combo`, `B2BCount`, 클리어 종류 | 공격력 계산 (VS) |

**FSM 연동 노트**: `fsm.md` 이벤트 목록에 `OnPieceLocked`가 추가되었다(매 lock마다 발행, 0줄 포함). Score/Level은 이 단일 이벤트만 구독하면 콤보/B2B/점수/레벨 전체를 갱신할 수 있다.

## Formulas

**S1. 라인 클리어 기본 점수**
```
LineBase[N] = {0, 100, 300, 500, 800}   // N = 0..4
LineScore   = LineBase[N] × Level
```

**S2. Back-to-Back 배수**
```
if (bLastClearWasDifficult AND bCurrentDifficult):
    LineScore ×= 1.5
```

**S3. Combo 보너스**
```
ComboBonus = (Combo >= 1) ? 50 × Combo × Level : 0
```

**S4. 드롭 점수**
```
DropScore = SoftDropCells × 1 + HardDropCells × 2
```

**S5. lock당 총 가산**
```
ScoreDelta = LineScore + ComboBonus + DropScore
Score += ScoreDelta
```

**S6. Level 진행**
```
Level = clamp(1 + floor(TotalLinesCleared / LinesPerLevel), 1, LevelCap)
```

**S7. BaseG(Level) 중력 커브** (이 시스템 소유)
```
SecPerRow(Level) = (0.8 − (Level−1)×0.007) ^ (Level−1)
BaseG(Level)     = 1 / (SecPerRow(Level) × SimHz)
```

**예시 계산** — Level 2, B2B Tetris(직전도 Tetris), Combo 3, HardDrop 4칸:
```
LineScore = 800 × 2 = 1600 → B2B ×1.5 = 2400
ComboBonus = 50 × 3 × 2 = 300
DropScore  = 0 + 4×2 = 8
ScoreDelta = 2400 + 300 + 8 = 2708
```

**변수 정의 / 범위**

| 변수 | 의미 | 기본 | 범위 | 소유 |
|------|------|------|------|------|
| `LineBase[]` | 줄수별 기본점 | {0,100,300,500,800} | ≥0 | Score |
| `B2BMultiplier` | B2B 배수 | 1.5 | 1.0~2.0 | Score |
| `ComboBaseValue` | 콤보 단위점 | 50 | 0~200 | Score |
| `LinesPerLevel` | 레벨당 줄 수 | 10 | 1~100 | Score |
| `LevelCap` | 레벨 상한 | 20 | 1~99 | Score |
| `SimHz` | 시뮬 주파수 | 60 | 파생 | FSM 참조 |

## Edge Cases

**1. 첫 difficult 클리어 (B2B 시작)**
- 첫 Tetris: 직전이 difficult 아님 → ×1.5 미적용, `bLastClearWasDifficult=true`, `B2BCount=0`.

**2. 연속 difficult (B2B 활성)**
- 두 번째 연속 Tetris부터 ×1.5 적용, `B2BCount++`(1, 2, …).

**3. B2B 끊김**
- Tetris 후 Single/Double/Triple → B2B 종료(`B2BCount=0`, `bLastClearWasDifficult=false`), 해당 클리어는 일반 점수.

**4. 0줄 lock은 B2B 유지**
- Tetris 후 줄 없이 lock → `bLastClearWasDifficult`/`B2BCount` 그대로 유지. 다음 Tetris면 B2B 이어짐. (단 Combo는 리셋)

**5. Combo 진행/리셋**
- 첫 클리어 Combo 0(보너스 없음), 연속 둘째부터 Combo 1+ 보너스. 0줄 lock → `Combo=-1`.

**6. 점수 계산 시점의 Level**
- 점수는 **클리어 직전(현재) Level**로 계산 후, `TotalLines` 갱신 → Level 재산출. (순서 고정: 가산 → 레벨 갱신)

**7. 레벨 경계 다중 점프**
- `LinesPerLevel=10`에선 한 lock(최대 4줄)이 2단계를 못 넘음. `LinesPerLevel<4` 설정 시 다중 점프 가능 → `clamp`로 안전.

**8. LevelCap 도달**
- Level 고정, `BaseG` 고정. 점수는 계속 ×LevelCap로 가산.

**9. Score 오버플로우**
- `int64`로 사실상 무한대 — 정상 플레이 범위에서 오버플로우 불가.

## Dependencies

**Upstream (Score가 의존)**

| 시스템 | 유형 | 인터페이스 |
|--------|------|-----------|
| FSM | Hard | `OnPieceLocked(...)` 구독 — 매 lock 신호 |

**Downstream (Score에 의존)**

| 시스템 | 유형 | 사용 |
|--------|------|------|
| FSM | Soft | `GetBaseG(Level)` / `GetLevel()` (중력 커브) |
| ViewModel | Hard | `On*Changed` + getters (HUD) |
| Attack | Soft | `Combo`, `B2BCount` (VS) |

**FSM ↔ Score 상호 참조 (benign, 비순환 init)**
- Score는 FSM의 `OnPieceLocked`를 구독하고, FSM은 spawn 시 Score의 `GetBaseG`를 조회한다. 런타임 상호 참조지만 **생성 순환은 없다**: Session 계층이 Board/Randomizer/Score/LockDelay/FSM을 각각 생성한 뒤 와이어링하고, 이벤트(락)와 조회(스폰)는 시점이 달라 재진입이 없다.

**공유 타입**: 신규 없음. `FLineClearResult`(기존) 사용. 튜닝값은 `FScoringConfig` 구조체로 묶기 가능.

**양방향 일관성 노트:**
- `fsm.md` Downstream의 Score/Level 행을 "FSM.`OnPieceLocked` 구독"으로 정정함 ✅.
- `board.md` Downstream의 Score/Level은 "OnLinesCleared 직접 구독"으로 남아 있으나 실제로는 FSM 경유 → 각주 보강 권장(필수 아님).
- `systems-index.md` "Circular Dependencies: 없음"에 "FSM↔Score는 이벤트 기반 soft 양방향(순환 아님)" 각주 권장.

## Tuning Knobs

| 변수 | 기본 | 안전 범위 | 영향 / 한계 시 증상 |
|------|------|----------|---------------------|
| `LineBase[1..4]` | 100/300/500/800 | ≥0, 단조증가 | 클리어 종류별 보상비. Tetris/Single 비율이 위험 보상을 결정 |
| `B2BMultiplier` | 1.5 | 1.0~2.0 | difficult 연속 보너스. 1.0=B2B 무의미, 2.0+=과보상 |
| `ComboBaseValue` | 50 | 0~200 | 콤보 단위점. 높으면 콤보 위주 메타, 0=콤보 무의미 |
| `LinesPerLevel` | 10 | 1~100 | 레벨업 속도. 낮으면 급가속(난이도 폭발), 높으면 정체 |
| `LevelCap` | 20 | 1~99 | 최대 레벨. 중력 커브 발산 방지. 너무 높으면 사실상 20G 도달 |
| `GravityBase` (0.8) | 0.8 | 0.5~0.99 | BaseG 커브 시작점. 낮으면 초반부터 빠름 |
| `GravityDecay` (0.007) | 0.007 | 0.001~0.02 | 레벨당 가속률. 크면 후반 급가속 |

**상호작용 주의:**
- `LinesPerLevel`(Score 소유)와 `GravityDecay`가 함께 난이도 곡선의 기울기를 결정 → 둘을 동시에 키우면 체감 난이도 급증.
- `B2BMultiplier`×`LineBase[4]`(Tetris)가 단일 최대 점수원 → 밸런스 시 이 곱을 기준점으로.
- `LevelCap`이 `BaseG` 상한을 정함 → Lock Delay의 `LockDelayMs`와 함께 고레벨 플레이 가능성을 좌우(교차 튜닝).

## Acceptance Criteria

**기본 점수 (× Level)**
- [ ] Level 1 Single → +100, Double → +300, Triple → +500, Tetris → +800
- [ ] Level 3 Tetris → +2400 (800×3) 검증
- [ ] 0줄 lock → 점수 변화 없음

**B2B**
- [ ] 첫 Tetris → ×1.5 미적용 (800×Level), `bLastClearWasDifficult=true`
- [ ] 연속 두 번째 Tetris → ×1.5 적용, `B2BCount=1`
- [ ] Tetris 후 Single → B2B 종료(`B2BCount=0`), Single 일반 점수
- [ ] Tetris 후 0줄 lock 후 Tetris → B2B 이어짐(×1.5)

**Combo**
- [ ] 첫 클리어 Combo 0(보너스 0), 둘째 연속 클리어 Combo 1 → +50×Level
- [ ] 0줄 lock → Combo -1 리셋
- [ ] Combo 보너스 = 50×Combo×Level 정확 계산

**Level 진행**
- [ ] 10줄 누적 → Level 2, 20줄 → Level 3
- [ ] `LevelCap=20` 도달 후 추가 줄 → Level 고정, BaseG 고정
- [ ] BaseG(Level 1) ≈ 0.0167, BaseG(Level 10) ≈ 0.258 (커브 검증)

**드롭 점수**
- [ ] Hard drop 4칸 → +8, Soft drop 3칸 → +3

**이벤트 / 격리**
- [ ] 점수/레벨/콤보/B2B 변경 시 해당 `On*Changed`만 발행 (불변 값은 미발행)
- [ ] Score/Level 단위 테스트가 `OnPieceLocked` 모킹만으로 통과 (Board/FSM 인스턴스 없이)

**결정성**
- [ ] 동일 lock 시퀀스 → 동일 최종 점수/레벨/콤보 (재현)

**성능**
- [ ] lock 1회 처리 < 2μs (산술 + 이벤트 발행)

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | T-Spin / T-Spin Mini 점수표 + B2B difficult 편입 | Game Designer | VS 단계 |
| 2 | Perfect Clear(All Clear) 보너스 도입 여부 | Game Designer | VS 단계 |
| 3 | 드롭 점수(soft/hard) 최종 포함 여부 + FSM 드롭 칸 전달 방식 | Lead Programmer | 구현 단계 |
| 4 | `BaseG` 커브 최종값 (Guideline vs 체감 커스텀) | 플레이테스트 | 구현 후 |
| 5 | Combo 보너스 상한/캡 필요 여부 | 플레이테스트 | 구현 후 |
| 6 | 점수 롤링 애니메이션·표시 포맷 | ViewModel/UI | Presentation |
