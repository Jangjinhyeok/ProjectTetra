# Piece/SRS (Tetromino & Super Rotation System)

> **Status**: In Design
> **Author**: user + Claude
> **Last Updated**: 2026-04-04
> **Implements Pillar**: Core Gameplay Foundation

## Overview

Piece/SRS는 7종 테트로미노의 형태 정의, 회전 상태, 이동, 그리고 SRS(Super Rotation System) 기반 Wall Kick을 담당하는 시스템이다. 각 피스의 블록 좌표, 4방향 회전 데이터, Wall Kick 오프셋 테이블을 데이터 주도 방식으로 관리하며, Board의 `IsValidPosition()`을 활용해 충돌 검증을 수행한다. 하드코딩 없이 DataAsset으로 피스 데이터를 분리하여 확장성을 보장한다.

## Player Fantasy

Piece/SRS는 "내 의도대로 블록이 움직인다"는 조작 신뢰감의 핵심이다. 플레이어가 회전 버튼을 눌렀을 때, 벽이나 다른 블록 옆에서도 기대한 대로 회전이 성공하는 것 — 이것이 Wall Kick의 존재 이유다. SRS가 잘 작동하면 플레이어는 "이 게임 조작감 좋다"고 느끼고, 틀리면 "버그 아닌가?" 라고 느낀다. 경쟁형 테트리스에서 T-Spin 같은 고급 기술이 SRS Wall Kick 위에서 성립하므로, 정확한 구현이 게임의 스킬 시링을 결정한다.

## Detailed Design

### Core Rules

**1. 테트로미노 정의 (7종)**

각 피스는 4개의 블록으로 구성되며, 피봇(회전 중심) 기준 상대 좌표로 정의한다.

| Type | 스폰 방향(0°) 블록 좌표 (피봇 기준) | 바운딩 박스 | 색상 |
|------|--------------------------------------|-----------|------|
| I | (-1,0), (0,0), (1,0), (2,0) | 4x1 | Cyan |
| O | (0,0), (1,0), (0,1), (1,1) | 2x2 | Yellow |
| T | (-1,0), (0,0), (1,0), (0,1) | 3x2 | Purple |
| S | (-1,0), (0,0), (0,1), (1,1) | 3x2 | Green |
| Z | (0,0), (1,0), (-1,1), (0,1) | 3x2 | Red |
| J | (-1,1), (-1,0), (0,0), (1,0) | 3x2 | Blue |
| L | (-1,0), (0,0), (1,0), (1,1) | 3x2 | Orange |

**2. 회전 상태**
- 4방향: `0` (스폰), `R` (시계 1회), `2` (180°), `L` (반시계 1회)
- 각 피스 × 4방향 = 총 28개 블록 좌표 세트
- O 피스는 회전해도 형태가 동일하지만, SRS 일관성을 위해 4상태 모두 정의
- 모든 회전 데이터는 DataAsset에 저장 — 코드에 하드코딩하지 않음

**3. 회전 실행 (SRS)**
- 시계방향(CW): 0→R→2→L→0
- 반시계방향(CCW): 0→L→2→R→0
- 180° 회전: 향후 확장용 — 현재는 구현하지 않되 enum에 `Rotate180`을 예약

**회전 절차:**
1. 현재 상태에서 목표 상태의 블록 좌표 계산
2. `Board.IsValidPosition()` 으로 검증
3. 유효 → 회전 성공, 상태 전이
4. 무효 → Wall Kick 테이블의 오프셋을 순서대로 시도
5. 모든 Kick 실패 → 회전 취소 (현재 상태 유지)

**4. Wall Kick 테이블**

SRS 표준 Kick Table. 피스 타입별 2그룹으로 분리:

**JLSTZ Kick Table** (시계방향 CW):

| 전이 | Test 1 | Test 2 | Test 3 | Test 4 |
|------|--------|--------|--------|--------|
| 0→R | (-1, 0) | (-1,+1) | ( 0,-2) | (-1,-2) |
| R→2 | (+1, 0) | (+1,-1) | ( 0,+2) | (+1,+2) |
| 2→L | (+1, 0) | (+1,+1) | ( 0,-2) | (+1,-2) |
| L→0 | (-1, 0) | (-1,-1) | ( 0,+2) | (-1,+2) |

반시계(CCW)는 역방향 전이의 오프셋 부호를 반전하여 도출.

**I-Piece Kick Table** (시계방향 CW):

| 전이 | Test 1 | Test 2 | Test 3 | Test 4 |
|------|--------|--------|--------|--------|
| 0→R | (-2, 0) | (+1, 0) | (-2,-1) | (+1,+2) |
| R→2 | (-1, 0) | (+2, 0) | (-1,+2) | (+2,-1) |
| 2→L | (+2, 0) | (-1, 0) | (+2,+1) | (-1,-2) |
| L→0 | (+1, 0) | (-2, 0) | (+1,-2) | (-2,+1) |

**O 피스**: Kick 없음 (회전 불변)

Kick Table은 DataAsset으로 분리하여 코드 변경 없이 수정 가능.

**5. 스폰 규칙**
- 스폰 위치: Column 3~6 범위 중앙 (피봇 기준 X=4 또는 5, 피스별 상이)
- 스폰 Row: 피스 하단이 Row 20 (버퍼존 최하단) — Visible 영역 바로 위
- 스폰 방향: 항상 `0` (기본 방향)
- 스폰 후 `Board.IsValidPosition()` 실패 시 → Block Out (Board가 판정)

**6. 이동**
- 좌/우 이동: 피봇 X좌표를 ±1, `Board.IsValidPosition()` 검증
- 소프트드롭: 피봇 Y좌표를 -1, 검증 (SDF에 따라 속도 결정 — Input 시스템 관할)
- 하드드롭: `Board.GetDropDistance()` 만큼 즉시 하강 → 즉시 Lock

### States and Transitions

피스 자체의 상태는 단순하다. 복잡한 생명주기는 FSM이 관리하고, 피스는 자신의 위치/회전 데이터만 갖는다.

**피스 인스턴스 데이터:**
```
EPieceType   Type;           // I, O, T, S, Z, J, L
FIntPoint    PivotPosition;  // 보드 상 피봇 절대 좌표
ERotationState RotationState; // 0, R, 2, L
```

**활성 피스 상태 전이:**

| 현재 상태 | 입력/이벤트 | 동작 | 결과 |
|----------|-----------|------|------|
| 활성 (보드 위) | 좌/우 이동 | PivotPosition.X ±1, 충돌 검증 | 성공: 이동 / 실패: 무시 |
| 활성 | 시계/반시계 회전 | SRS 회전 절차 (Kick 포함) | 성공: 상태 전이 / 실패: 유지 |
| 활성 | 소프트드롭 | PivotPosition.Y -1, 검증 | 성공: 하강 / 실패: 표면 접촉 |
| 활성 | 하드드롭 | DropDistance만큼 즉시 하강 | 즉시 Lock |
| 활성 | Lock 트리거 | FSM에서 호출 | Board.LockPiece() → 피스 소멸 |
| 없음 | 스폰 트리거 | Randomizer에서 다음 피스 타입 수신 | 새 피스 생성 (스폰 위치, 방향 0) |

**회전 상태 전이 다이어그램:**
```
    CW          CW          CW          CW
0 ----→ R ----→ 2 ----→ L ----→ 0
0 ←---- R ←---- 2 ←---- L ←---- 0
   CCW         CCW         CCW         CCW
```

### Interactions with Other Systems

**Piece가 사용하는 인터페이스 (Upstream):**

| 시스템 | 함수 | 용도 |
|--------|------|------|
| Board | `IsValidPosition(TArray<FIntPoint>)` → `bool` | 이동/회전/Kick/스폰 충돌 검증 |
| Board | `GetDropDistance(TArray<FIntPoint>)` → `int32` | 하드드롭 거리 계산 |

**Piece가 제공하는 인터페이스 (Downstream):**

| 호출자 | 함수/데이터 | 용도 |
|--------|-----------|------|
| FSM | `TryMove(EDirection)` → `bool` | 좌/우/하 이동 시도 |
| FSM | `TryRotate(ERotateDirection)` → `bool` | SRS 회전 시도 (Kick 포함) |
| FSM | `GetAbsoluteBlockPositions()` → `TArray<FIntPoint>` | 현재 블록의 보드 절대 좌표 |
| FSM | `GetPieceType()` → `EPieceType` | 현재 피스 타입 |
| FSM | `GetRotationState()` → `ERotationState` | 현재 회전 상태 |
| Ghost Piece | `GetAbsoluteBlockPositions()` | 고스트 위치 계산용 |
| Hold | `GetPieceType()` | Hold 교환 시 타입 확인 |
| Lock Delay | `GetAbsoluteBlockPositions()` | 표면 접촉 판정용 |
| ViewModel | 피스 위치/타입/회전 상태 전체 | UI 렌더링용 |

**데이터 주도 에셋:**

| DataAsset | 내용 |
|-----------|------|
| `UPieceDefinitionDataAsset` | 7종 피스별 4방향 블록 좌표, 스폰 오프셋, 색상 |
| `UKickTableDataAsset` | JLSTZ / I-Piece 별 Kick 오프셋 테이블 |

## Formulas

**1. 블록 절대 좌표 계산**
```
AbsolutePosition[i] = PivotPosition + RelativeOffset[PieceType][RotationState][i]
(i = 0..3, 블록 4개)
```

**2. Wall Kick 후보 좌표 계산**
```
KickCandidate[k] = RotatedPosition[i] + KickOffset[KickGroup][FromState→ToState][k]
(k = 0..3, 최대 4회 시도)
```
KickGroup: I-Piece 또는 JLSTZ

**3. 하드드롭 최종 위치**
```
FinalPivotY = PivotPosition.Y - Board.GetDropDistance(GetAbsoluteBlockPositions())
```

**4. 스폰 피봇 좌표**
```
SpawnPivot.X = PieceDefinition[PieceType].SpawnOffsetX  // 피스별 정의 (보통 4 또는 5)
SpawnPivot.Y = BoardVisibleHeight  // = 20 (버퍼존 최하단)
```

## Edge Cases

**1. I 피스 벽 근처 회전**
- I 피스는 4칸 길이라 벽 근처에서 회전 시 2~3칸 Kick이 필요할 수 있음
- SRS Kick Table이 이를 처리하지만, 모든 전이에서 4번의 Kick 시도가 실패할 수 있음 → 회전 취소

**2. T-Spin 판정**
- T 피스가 Kick을 통해 회전 성공한 후, T의 4코너 중 3개 이상이 Filled/벽이면 T-Spin
- T-Spin Mini: Kick Test 1~2로 성공 + 코너 조건 미충족 시
- 이 판정은 Piece 시스템이 아닌 Score/Attack 시스템에서 수행 — Piece는 "마지막 회전이 Kick이었는지"와 "어떤 Kick Test로 성공했는지" 정보만 제공

**3. O 피스 회전 요청**
- 형태 불변이지만 회전 상태는 전이됨 (0→R→2→L)
- Kick 없이 항상 성공 — Board 검증도 불필요 (같은 위치이므로)

**4. 스폰 직후 회전**
- 스폰 위치에서 즉시 회전 가능 — 별도 제한 없음
- 버퍼존(Row 20~23)에서 Kick이 발생해 Visible 영역 밖으로 밀릴 수 있음 → 유효 (Y ≤ 23 범위 내)

**5. 바닥 접촉 상태에서 회전 성공**
- Kick으로 피스가 위로 올라갈 수 있음 (Y좌표 증가)
- 이 경우 Lock Delay가 리셋됨 (Lock Delay 시스템 관할)

**6. 이동과 회전 동시 입력**
- 입력 처리 순서는 FSM/Input 시스템이 결정
- Piece는 개별 TryMove/TryRotate만 처리, 동시성은 관여하지 않음

## Dependencies

**Upstream (Piece가 의존하는 시스템):**

| 시스템 | 의존 유형 | 인터페이스 |
|--------|----------|-----------|
| Board | Hard | `IsValidPosition()`, `GetDropDistance()` — 모든 이동/회전 검증 |

**Downstream (Piece에 의존하는 시스템):**

| 시스템 | 의존 유형 | 사용하는 데이터 |
|--------|----------|---------------|
| Randomizer | Hard | `EPieceType` — 다음 피스 타입 결정 |
| FSM | Hard | `TryMove()`, `TryRotate()`, `GetAbsoluteBlockPositions()` |
| Lock Delay | Hard | `GetAbsoluteBlockPositions()` — 표면 접촉 판정 |
| Hold | Hard | `GetPieceType()` — 피스 교환 |
| Ghost Piece | Hard | `GetAbsoluteBlockPositions()` — 드롭 미리보기 |
| Score/Attack | Soft | 마지막 Kick 정보 (`bWasLastActionRotation`, `LastKickIndex`) — T-Spin 판정용 |
| ViewModel | Hard | 피스 위치/타입/회전 상태 전체 |

**공유 타입 (Core 모듈):**
- `EPieceType` — Board GDD에서 정의 (I, O, T, S, Z, J, L, None, Garbage)
- `ERotationState` — { State0, StateR, State2, StateL }
- `ERotateDirection` — { CW, CCW }
- `EDirection` — { Left, Right, Down }

## Tuning Knobs

| 변수명 | 위치 | 타입 | 기본값 | 설명 |
|--------|------|------|--------|------|
| 블록 상대 좌표 | `UPieceDefinitionDataAsset` | `TArray<FIntPoint>` × 4방향 | SRS 표준 | 피스별 28개 좌표 세트 |
| 스폰 오프셋 X | `UPieceDefinitionDataAsset` | int32 | 피스별 상이 | 피스 타입별 스폰 Column |
| Kick 오프셋 | `UKickTableDataAsset` | `FIntPoint` × 4 × 8전이 × 2그룹 | SRS 표준 | Wall Kick 테이블 |
| 스폰 Row | 코드 상수 | int32 | `BoardVisibleHeight` (20) | Board Tuning Knob과 연동 |

**주의사항:**
- 블록 좌표와 Kick 테이블은 SRS 표준값이 기본 — 수정 시 전체 회전 테스트 필수
- DataAsset 분리의 핵심 이유: 향후 180° Kick 테이블 추가, 커스텀 피스(펜토미노 등) 확장이 코드 변경 없이 가능
- 스폰 Row는 Board의 `BoardVisibleHeight`와 반드시 동기화

## Acceptance Criteria

**회전 테스트 (SRS 핵심):**
- [ ] 7종 피스 × 4방향 회전 — 빈 보드에서 모든 CW/CCW 전이 성공
- [ ] 벽 근처 회전 — I 피스 Column 0에서 CW 회전 시 Kick으로 성공
- [ ] 천장 근처 회전 — 버퍼존 상단에서 회전 시 범위 내 Kick 성공
- [ ] 모든 Kick 실패 시 — 회전 취소, 피스 상태 변경 없음
- [ ] O 피스 회전 — 위치 변화 없이 상태만 전이

**Wall Kick 테이블 검증:**
- [ ] JLSTZ 8전이 × 4 Kick = 32개 오프셋이 SRS 표준과 일치
- [ ] I-Piece 8전이 × 4 Kick = 32개 오프셋이 SRS 표준과 일치
- [ ] 알려진 T-Spin 셋업에서 Kick이 정확히 동작 (T-Spin Triple, TST 등)

**이동 테스트:**
- [ ] 좌/우 이동 — 빈 공간에서 성공, 벽/블록에서 실패
- [ ] 소프트드롭 — Y-1 이동, 바닥에서 실패
- [ ] 하드드롭 — GetDropDistance만큼 즉시 하강

**스폰 테스트:**
- [ ] 7종 피스가 올바른 스폰 위치/방향으로 생성됨
- [ ] 스폰 좌표가 Board 범위 내
- [ ] 스폰 위치에 블록이 있으면 IsValidPosition 실패 (Block Out 전달)

**데이터 주도 검증:**
- [ ] DataAsset 값 변경 → 빌드 없이 피스 형태 변경 확인
- [ ] Kick Table DataAsset 변경 → 빌드 없이 Kick 동작 변경 확인

**Deterministic 검증:**
- [ ] 동일 피스 타입 + 동일 입력 시퀀스 → 동일 최종 위치/회전 상태

**T-Spin 정보 제공 검증:**
- [ ] 회전 성공 시 `bWasLastActionRotation = true` 설정
- [ ] Kick으로 성공 시 `LastKickIndex` 정확히 기록
- [ ] 이동 시 `bWasLastActionRotation = false`로 리셋

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | 180° 회전 Kick Table — SRS 표준에는 없으나 Tetr.io는 지원. 별도 DataAsset으로 추가할지? | Game Designer | VS 단계 (Attack/Garbage 설계 시) |
| 2 | T-Spin Mini vs T-Spin 판정의 정확한 기준 — Kick Index 기반 vs 코너 기반 중 어느 방식 채택? | Game Designer | Score/Level GDD 설계 시 |
| 3 | 피스별 스폰 오프셋 X의 정확한 값 — SRS 표준 참조하되, 소수점 피봇(I, O) 처리 방식 확정 필요 | Game Designer | 구현 단계 |
