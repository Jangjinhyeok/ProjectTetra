# Board (Playfield)

> **Status**: In Design
> **Author**: user + Claude
> **Last Updated**: 2026-04-04
> **Implements Pillar**: Core Gameplay Foundation

## Overview

Board(Playfield)는 10x20 셀의 2D 그리드로, 테트로미노가 배치되는 게임 공간이다. 셀 상태 관리, 충돌 판정, 라인 클리어, Top-out 판정을 담당하며, UI를 참조하지 않는 순수 C++ 데이터 구조로 구현한다. 모든 게임플레이 시스템(FSM, Score, Lock Delay 등)이 이 시스템을 기반으로 동작한다.

## Player Fantasy

Board는 플레이어가 의식하지 않는 인프라 시스템이다. 플레이어가 느끼는 것은 "블록이 정확히 맞아 떨어지는 쾌감"과 "줄이 사라지는 시원함"인데, 이 경험이 성립하려면 Board의 충돌 판정이 정확하고, 라인 클리어 판정이 즉각적이어야 한다. Board가 잘 작동할수록 플레이어는 Board의 존재를 잊고 퍼즐에 몰입한다.

## Detailed Design

### Core Rules

**1. 그리드 구조**
- 너비 10열(Column 0~9), 높이 20행(Row 0~19) Visible 영역
- 상단 버퍼존 4행(Row 20~23) — 피스 스폰 및 회전 공간, 화면에 표시하지 않음
- 총 내부 배열: 10 x 24
- 좌표계: 좌하단 원점 (0,0), X축 오른쪽, Y축 위쪽

**2. 셀 상태**
- `Empty` — 비어 있음
- `Filled(EPieceType)` — 블록이 고정됨, 어떤 테트로미노에서 왔는지 색상 정보 포함
- 셀은 Filled 또는 Empty만 가능 (활성 피스는 Board에 기록하지 않음 — 피스가 Lock되는 순간에만 Board에 기록)

**3. 충돌 판정**
- 입력: 피스의 블록 좌표 목록 `TArray<FIntPoint>`
- 판정: 모든 블록이 (1) 그리드 범위 내 (0 ≤ X ≤ 9, 0 ≤ Y ≤ 23) 이고 (2) 해당 셀이 `Empty`이면 유효
- 반환: `bool` (유효/무효)
- 이 함수 하나로 이동, 회전, Wall Kick, 스폰 모두 검증

**4. 피스 고정 (Lock)**
- 입력: 피스의 블록 좌표 + EPieceType
- 동작: 각 좌표의 셀을 `Filled(EPieceType)`으로 설정
- Lock 후 즉시 라인 클리어 판정으로 전이

**5. 라인 클리어**
- 판정: Row 0부터 위로 순회, 한 행의 10셀이 모두 `Filled`이면 완성
- 제거: 완성된 행을 삭제하고, 위의 모든 행을 아래로 1칸씩 이동
- 동시 여러 줄 삭제 가능 (최대 4줄 = 테트리스)
- 반환: 삭제된 행 수 `int32` + 삭제된 행 인덱스 목록 (애니메이션용)

**6. Top-out 판정**
- **Block Out**: 새 피스 스폰 시 블록 좌표 중 하나라도 이미 `Filled`인 셀과 겹침
- **Lock Out**: 피스가 Lock된 후 모든 블록이 Visible 영역(Row 20+) 위에 있음
- 둘 중 하나라도 발생하면 게임 오버 이벤트 발행

### States and Transitions

Board는 자체 상태 머신을 갖지 않는다. FSM이 게임 흐름을 제어하고, Board는 호출되는 순수 데이터 레이어다.

**Board 데이터가 변경되는 시점:**

| 트리거 | Board 연산 | 호출자 |
|--------|-----------|--------|
| 게임 시작 | `Clear()` — 전체 그리드를 Empty로 초기화 | FSM (S_NEW 진입 전) |
| 피스 Lock | `LockPiece(Coords, PieceType)` — 셀을 Filled로 설정 | FSM (S_LOCK) |
| 라인 클리어 | `ClearLines()` — 완성 행 제거 + 위 행 낙하 | FSM (S_REMOVE) |
| 가비지 추가 | `AddGarbageLines(Count, GapColumn)` — 하단에 가비지 행 삽입, 기존 행 위로 밀어올림 | Attack 시스템 |
| Top-out 체크 | `IsBlockOut(SpawnCoords)` / `IsLockOut(LockCoords)` — 게임 오버 판정 | FSM (S_ISDIE) |

**Board가 발행하는 이벤트:**

| 이벤트 | 페이로드 | 구독자 |
|--------|---------|--------|
| `OnBoardChanged` | 변경된 셀 좌표 목록 | ViewModel → Board Renderer |
| `OnLinesCleared` | 삭제된 행 인덱스 + 행 수 | Score/Level, Attack, ViewModel |
| `OnTopOut` | Top-out 타입 (BlockOut/LockOut) | FSM |

### Interactions with Other Systems

**Board가 제공하는 인터페이스 (다른 시스템이 호출):**

| 호출자 | 함수 | 용도 |
|--------|------|------|
| Piece/SRS, FSM | `IsValidPosition(TArray<FIntPoint>)` → `bool` | 이동/회전/Wall Kick 충돌 판정 |
| FSM | `LockPiece(TArray<FIntPoint>, EPieceType)` | 피스 고정 |
| FSM | `ClearLines()` → `FLineClearResult` | 라인 클리어 실행, 결과 반환 |
| FSM | `IsBlockOut(TArray<FIntPoint>)` → `bool` | 스폰 시 Block Out 판정 |
| FSM | `IsLockOut(TArray<FIntPoint>)` → `bool` | Lock 후 Lock Out 판정 |
| FSM | `Clear()` | 게임 시작/재시작 시 보드 초기화 |
| Attack | `AddGarbageLines(int32 Count, int32 GapColumn)` | 가비지 라인 삽입 |
| Ghost Piece | `GetDropDistance(TArray<FIntPoint>)` → `int32` | 하드드롭 거리 계산 (충돌까지 아래로 몇 칸) |
| ViewModel | `GetCell(int32 X, int32 Y)` → `FCellState` | 개별 셀 읽기 |
| ViewModel | `GetVisibleGrid()` → `TArray<FCellState>` | 전체 Visible 영역 스냅샷 |

**Board가 의존하는 시스템:** 없음 (Foundation Layer)

**데이터 흐름 요약:**
```
FSM/Piece → Board.IsValidPosition() → bool
FSM       → Board.LockPiece()      → OnBoardChanged 이벤트
FSM       → Board.ClearLines()     → OnLinesCleared 이벤트 → Score/Level, Attack
Attack    → Board.AddGarbageLines() → OnBoardChanged 이벤트
ViewModel ← Board (이벤트 구독)    → Board Renderer (UI 갱신)
```

## Formulas

**1. 라인 클리어 판정**
```
IsLineComplete(Row) = ∀ Col ∈ [0, 9] : Grid[Col][Row] ≠ Empty
```

**2. 하드드롭 거리 (Ghost Piece용)**
```
DropDistance(Coords) = max d where IsValidPosition(Coords shifted by (0, -d)) == true
                      d ∈ [0, 23]
```
구현: d=0부터 1씩 증가하며 충돌할 때까지 반복, 최대 24회 반복이므로 성능 부담 없음

**3. 가비지 라인 삽입 후 오버플로우**
```
NewRowY(OldRow) = OldRow + GarbageCount
if NewRowY >= 24: 해당 행의 블록은 소멸 (Top-out은 별도 판정)
```

## Edge Cases

**1. 버퍼존에 블록이 남아있는 상태에서 라인 클리어**
- Row 0~19에서 완성된 줄을 제거하면, Row 20+ 버퍼존의 블록도 아래로 내려옴
- 버퍼존 블록이 Visible 영역으로 진입하는 것은 정상 동작

**2. 동시 4줄 클리어 (테트리스) 시 행 이동 순서**
- 아래 행부터 제거하고 위 행을 내림 — 한 번에 처리하지 않으면 인덱스가 꼬임
- 구현: 완성 행 인덱스를 수집 → 아래부터 역순 제거 → 한꺼번에 행 재정렬

**3. 가비지 삽입으로 기존 블록이 버퍼존 초과**
- 가비지 삽입 시 기존 행이 위로 밀려 Row 24 이상이 되면 해당 블록 소멸
- 소멸 자체는 Top-out이 아님 — Top-out은 다음 피스 스폰 시 별도 판정

**4. 빈 보드에서 라인 클리어 호출**
- `ClearLines()`가 완성 행 0개를 반환 — 정상 동작, 이벤트 발행하지 않음

**5. 가비지 Gap Column 범위**
- GapColumn은 반드시 0~9 범위 — 범위 밖 값은 assert/clamp 처리

**6. IsValidPosition에 빈 좌표 배열 전달**
- 빈 배열은 `true` 반환 (블록이 없으면 충돌 없음) — 방어적 처리

## Dependencies

**Upstream (Board가 의존하는 시스템):** 없음

Board는 Foundation Layer로, 외부 시스템에 대한 의존성이 전혀 없다. `EPieceType` enum만 공유 타입으로 사용하며, 이는 `Core/` 모듈의 공용 타입 헤더에 정의한다.

**Downstream (Board에 의존하는 시스템):**

| 시스템 | 의존 유형 | 사용하는 인터페이스 |
|--------|----------|-------------------|
| Piece/SRS | Hard | `IsValidPosition()` — 회전/Wall Kick 검증 |
| Lock Delay | Hard | `IsValidPosition()` — 표면 접촉 판정 |
| FSM | Hard | `LockPiece()`, `ClearLines()`, `Clear()`, Top-out 판정 |
| Score/Level | Hard | `OnLinesCleared` 이벤트 구독 |
| Ghost Piece | Hard | `GetDropDistance()` |
| Attack/Garbage | Hard | `AddGarbageLines()`, `OnLinesCleared` 구독 |
| ViewModel | Hard | `GetCell()`, `GetVisibleGrid()`, 이벤트 구독 |
| Board Renderer | Soft | ViewModel을 통해 간접 참조 (Board 직접 참조 안 함) |

**공유 타입 (Core 모듈에 정의):**
- `EPieceType` — I, O, T, S, Z, J, L, None(Empty), Garbage
- `FCellState` — { EPieceType Type; } (확장 가능한 구조체)
- `FLineClearResult` — { int32 LinesCleared; TArray\<int32\> ClearedRows; }

## Tuning Knobs

| 변수명 | 타입 | 기본값 | 범위 | 설명 |
|--------|------|--------|------|------|
| `BoardWidth` | int32 | 10 | 4~20 | 그리드 너비 (표준: 10) |
| `BoardVisibleHeight` | int32 | 20 | 10~30 | Visible 영역 높이 (표준: 20) |
| `BoardBufferHeight` | int32 | 4 | 2~8 | 상단 버퍼존 높이 |

**주의사항:**
- 표준 테트리스 규격(10x20)을 기본값으로 고정하되, `constexpr` 또는 `EditDefaultsOnly`로 노출하여 향후 커스텀 모드(미니 보드, 와이드 보드 등) 확장 가능하게 설계
- `BoardWidth` 변경 시 라인 클리어 판정, 스폰 위치, 가비지 Gap 범위 모두 영향받음 — 변경 시 전체 테스트 필수
- 현재 스코프에서는 10x20 고정으로 개발하고, 커스텀 모드는 Full Vision 단계에서 검토

## Acceptance Criteria

**기능 테스트:**
- [ ] 빈 보드 초기화 — 240셀(10x24) 모두 Empty
- [ ] `IsValidPosition` — 그리드 내 빈 셀 좌표 → true
- [ ] `IsValidPosition` — 그리드 밖 좌표 (X<0, X>9, Y<0, Y>23) → false
- [ ] `IsValidPosition` — Filled 셀과 겹치는 좌표 → false
- [ ] `LockPiece` — 지정 좌표가 Filled(PieceType)으로 변경됨
- [ ] `LockPiece` 후 `OnBoardChanged` 이벤트 발행됨
- [ ] 1줄 완성 시 `ClearLines()` → LinesCleared=1, 해당 행 제거, 위 행 낙하
- [ ] 4줄 동시 완성 시 정확히 4줄 제거, 나머지 행 올바르게 재정렬
- [ ] `ClearLines()` 후 `OnLinesCleared` 이벤트에 정확한 행 인덱스 포함
- [ ] 빈 보드에서 `ClearLines()` → LinesCleared=0, 이벤트 미발행
- [ ] `AddGarbageLines(2, 5)` → 하단에 2행 추가, Column 5만 Empty, 기존 행 2칸 위로 이동
- [ ] 가비지 삽입으로 Row 24 초과 블록 소멸
- [ ] Block Out 판정 — 스폰 좌표가 Filled 셀과 겹치면 true
- [ ] Lock Out 판정 — Lock된 블록이 모두 Row 20+ 이면 true
- [ ] `GetDropDistance` — 빈 보드에서 Row 20의 피스 → 거리 20 반환
- [ ] `GetDropDistance` — 바닥에 블록 있을 때 정확한 거리 반환

**성능 테스트:**
- [ ] `IsValidPosition` 호출 1회당 1μs 이내 (4블록 기준)
- [ ] `ClearLines` 호출 1회당 10μs 이내 (4줄 동시 클리어 기준)
- [ ] Board 전체 메모리: 240 * sizeof(FCellState) 이내 — 수 KB 수준

**Deterministic 검증:**
- [ ] 동일 입력 시퀀스 → 동일 보드 상태 (시드 고정 테스트)

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | 가비지 라인의 Gap이 매 행마다 랜덤인지 동일 Column인지? | Game Designer | Attack/Garbage GDD 설계 시 |
| 2 | Board Renderer Phase 3 (SDF 셰이더) 전환 시 Board 데이터 인터페이스 변경 필요 여부 | Technical Artist | Board Renderer GDD 설계 시 |
| 3 | Deterministic Simulation에서 Board 상태 직렬화 포맷 (리플레이/넷코드용) | Lead Programmer | 구현 단계 |
