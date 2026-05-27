# Randomizer (7-Bag)

> **Status**: In Design
> **Author**: user + Claude
> **Last Updated**: 2026-04-04
> **Implements Pillar**: Core Gameplay Foundation

## Overview

Randomizer는 7-Bag 알고리즘으로 테트로미노 출현 순서를 결정하는 시스템이다. 7종 피스를 한 세트(Bag)로 묶어 셔플 후 순서대로 배출하며, 한 Bag이 소진되면 새 Bag을 생성한다. 시드 고정을 지원하여 리플레이, 디버깅, 대전 동기화에 활용할 수 있다. Next Queue(최소 5개 미리보기)를 관리하며, FSM의 스폰 요청에 응답한다.

## Player Fantasy

7-Bag은 "공정함"의 보장이다. 순수 랜덤과 달리, 7개 이내에 반드시 모든 피스가 한 번씩 등장하므로 플레이어는 "I 피스가 영원히 안 온다"는 좌절을 경험하지 않는다. 경쟁형 테트리스에서 플레이어는 Bag 순서를 추적하며 전략을 세우는데, 이 예측 가능성이 스킬 시링을 만든다. Next Queue 미리보기와 결합하면 "지금 I가 나왔으니 이번 Bag에서는 끝, 다음 Bag 초반에 올 수 있다"는 계산이 가능해진다.

## Detailed Design

### Core Rules

**1. 7-Bag 알고리즘**
- Bag = { I, O, T, S, Z, J, L } 7종을 Fisher-Yates 셔플
- Bag에서 앞부터 하나씩 꺼냄 (FIFO)
- Bag이 비면 새 Bag을 생성하여 뒤에 연결
- 항상 최소 2개 Bag을 미리 생성 (Next Queue 5개 이상 보장)

**2. 시드 관리**
- `int64 Seed` — 난수 생성기 초기값
- 시드 미지정 시: 현재 시간 기반 자동 생성
- 시드 지정 시: 동일 시드 → 동일 Bag 순서 보장 (Deterministic)
- 난수 생성기: `FRandomStream` (UE5 내장, 시드 고정 지원)

**3. Next Queue**
- 다음에 나올 피스 목록을 유지 (최소 5개)
- `Dequeue()` 호출 시 선두 피스 반환 + Queue 보충
- Queue 내용은 ViewModel을 통해 UI에 노출

**4. 초기화**
- 게임 시작 시 `Initialize(Seed)` 호출
- 첫 Bag + 두 번째 Bag 즉시 생성
- Next Queue를 Bag에서 채움

### States and Transitions

Randomizer는 상태 머신이 아닌 순수 데이터 파이프라인이다.

```
[Bag Pool] → [Next Queue (≥5)] → Dequeue() → FSM이 피스 스폰
                                      ↓
                             Queue < 5개면 새 Bag 생성 + 보충
```

**내부 데이터:**
```
TArray<EPieceType> CurrentBag;    // 현재 Bag 잔여 피스
TArray<EPieceType> NextQueue;     // 미리보기 큐
FRandomStream      RandomStream;  // 시드 기반 난수 생성기
int64              Seed;          // 현재 시드
```

### Interactions with Other Systems

**Randomizer가 제공하는 인터페이스:**

| 호출자 | 함수 | 용도 |
|--------|------|------|
| FSM | `Dequeue()` → `EPieceType` | 다음 피스 꺼내기 (스폰 시) |
| FSM | `Initialize(int64 Seed)` | 게임 시작 시 초기화 |
| FSM | `Reset()` | 게임 재시작 시 리셋 |
| ViewModel | `GetNextQueue()` → `TArray<EPieceType>` | Next Queue 미리보기 (읽기 전용) |
| Hold | (간접) FSM을 통해 Dequeue 트리거 | Hold 교환 시 새 피스 필요할 때 |

**Randomizer가 발행하는 이벤트:**

| 이벤트 | 페이로드 | 구독자 |
|--------|---------|--------|
| `OnNextQueueChanged` | 현재 Next Queue 배열 | ViewModel → HUD (Next 미리보기) |

**Randomizer가 의존하는 시스템:** 없음 (EPieceType enum만 사용 — Core 공유 타입)

## Formulas

**1. Fisher-Yates 셔플**
```
for i = 6 downto 1:
    j = RandomStream.RandRange(0, i)
    Swap(Bag[i], Bag[j])
```
O(7) — 7개 원소 셔플, 성능 부담 없음

**2. Queue 보충 조건**
```
if NextQueue.Num() < NextQueueMinSize:
    GenerateNewBag()
    NextQueue.Append(NewBag)
```

---

## Edge Cases

**1. 게임 시작 시 첫 피스**
- 첫 Bag의 첫 피스가 곧 첫 스폰 피스 — 특별 처리 없음
- 일부 테트리스 구현은 첫 피스에서 S/Z/O를 배제하나, 표준 7-Bag에서는 제한 없음. 필요 시 Tuning Knob으로 추가

**2. Hold로 인한 추가 Dequeue**
- 첫 Hold 사용 시 FSM이 Dequeue를 한 번 더 호출 (Hold 슬롯이 비어있으므로)
- Randomizer 입장에서는 일반 Dequeue와 동일 — 구분 불필요

**3. 매우 긴 게임**
- Bag 생성 횟수에 상한 없음 — 메모리는 NextQueue 크기(5~14개)로 제한되므로 무한 게임에서도 문제 없음

**4. 시드 오버플로우**
- `FRandomStream`은 int32 시드 사용 — int64로 받되 내부적으로 캐스팅. 시드 범위 문서화 필요

---

## Dependencies

**Upstream:** 없음 (`EPieceType`은 Core 공유 타입)

**Downstream:**

| 시스템 | 의존 유형 | 사용하는 인터페이스 |
|--------|----------|-------------------|
| FSM | Hard | `Dequeue()`, `Initialize()`, `Reset()` |
| Hold | Soft | FSM을 통해 간접 사용 |
| ViewModel | Hard | `GetNextQueue()`, `OnNextQueueChanged` |

---

## Tuning Knobs

| 변수명 | 타입 | 기본값 | 범위 | 설명 |
|--------|------|--------|------|------|
| `NextQueueMinSize` | int32 | 5 | 1~14 | Next Queue 최소 유지 크기 (미리보기 개수) |
| `Seed` | int64 | 자동 | — | 0이면 시간 기반 자동, 양수면 고정 시드 |

**주의사항:**
- `NextQueueMinSize`가 7 이상이면 항상 2개 이상의 Bag이 미리 생성됨
- 14로 설정하면 2 Bag 전체가 미리보기로 노출 — 전략 깊이 증가하지만 UI 공간 필요

---

## Acceptance Criteria

**7-Bag 검증:**
- [ ] 한 Bag(7회 Dequeue) 내에 7종 피스가 정확히 1번씩 등장
- [ ] 연속 2 Bag(14회 Dequeue)에서 각 피스 정확히 2번 등장
- [ ] 1000 Bag 생성 후 각 피스 등장 횟수 = 1000 (±0, 7-Bag이므로 정확)

**시드 검증:**
- [ ] 동일 시드 → 동일 Bag 순서 (100 Bag까지 비교)
- [ ] 다른 시드 → 다른 Bag 순서
- [ ] 시드 미지정 시 매 게임 다른 순서

**Next Queue 검증:**
- [ ] 초기화 직후 NextQueue.Num() >= NextQueueMinSize
- [ ] Dequeue 후 자동 보충되어 항상 >= NextQueueMinSize 유지
- [ ] `OnNextQueueChanged` 이벤트가 Dequeue 시마다 발행

**Reset 검증:**
- [ ] Reset 후 동일 시드로 Initialize → 동일 순서 재현

---

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | 첫 피스 S/Z/O 배제 규칙 적용 여부 — Tetr.io는 미적용, 일부 가이드라인은 적용 | Game Designer | 구현 단계 체감 테스트 |
| 2 | `FRandomStream` int32 시드 한계 — 대전 동기화에 충분한지, 별도 PRNG 필요한지 | Lead Programmer | 넷코드 설계 시 |
