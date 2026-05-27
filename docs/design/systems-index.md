# Systems Index: Tetra-UE

> **Status**: Approved
> **Created**: 2026-04-04
> **Last Updated**: 2026-04-04
> **Source Concept**: docs/TETRA-UE-PROJECT-CONTEXT.md

---

## Overview

Tetra-UE는 UE5 기반 경쟁형 테트리스로, 핵심 목표는 MVVM + FieldNotify 기반 UI 아키텍처 학습이다.
시스템은 순수 게임 로직(Model) → ViewModel → UMG View의 3계층으로 분리되며,
학습 단계별로 MVP(로직+기본UMG+MVVM) → VS(입력+CommonUI) → Alpha(연출) → Full(폴리시) 순서로 설계·구현한다.

---

## Systems Enumeration

| # | System Name | Category | Priority | Status | Design Doc | Depends On |
|---|-------------|----------|----------|--------|------------|------------|
| 1 | Board (Playfield) | Core | MVP | Designed | design/gdd/board.md | — |
| 2 | Piece/SRS | Core | MVP | Designed | design/gdd/piece-srs.md | — |
| 3 | Randomizer | Core | MVP | Designed | design/gdd/randomizer.md | Piece |
| 4 | Input/Handling (DAS/ARR) | Core | MVP | Not Started | — | — |
| 5 | Lock Delay | Gameplay | MVP | Approved | design/gdd/lock-delay.md | Board, Piece (FSM 경유) |
| 6 | FSM (Game State Machine) | Core | MVP | Approved | design/gdd/fsm.md | Board, Piece, Randomizer, Lock Delay, Input, Hold |
| 7 | Score/Level | Gameplay | MVP | Approved | design/gdd/scoring.md | FSM (OnPieceLocked) |
| 8 | Hold | Gameplay | MVP | Approved (in FSM, contains) | design/gdd/fsm.md | Piece, Randomizer |
| 9 | Ghost Piece | Gameplay | MVP | Not Started | — | Board, Piece |
| 10 | Attack/Garbage | Gameplay | VS | Not Started | — | Board, Score/Level |
| 11 | ViewModel | UI | MVP | Not Started | — | Board, Piece, Score/Level, FSM, Attack |
| 12 | Board Renderer | UI | MVP | Not Started | — | ViewModel, Board |
| 13 | HUD | UI | MVP | Not Started | — | ViewModel |
| 14 | Game Session/Flow (inferred) | Core | VS | Not Started | — | FSM |
| 15 | Menu System / CommonUI (inferred) | UI | VS | Not Started | — | Game Session/Flow |
| 16 | Settings/Options (inferred) | Persistence | VS | Not Started | — | Input/Handling, Menu System |
| 17 | Widget Animation (inferred) | UI | Alpha | Not Started | — | Board Renderer, HUD, ViewModel |
| 18 | Widget Pooling (inferred) | UI | Alpha | Not Started | — | Attack, ViewModel |
| 19 | Save/Ranking (inferred) | Persistence | Full | Not Started | — | Score/Level, Game Session/Flow |
| 20 | Audio (inferred) | Audio | Full | Not Started | ��� | FSM, Game Session/Flow |
| 21 | Accessibility (inferred) | Meta | Full | Not Started | — | Board Renderer, Settings |

---

## Categories

| Category | Description |
|----------|-------------|
| **Core** | 게임 루프의 기반 시스템 — Board, Piece, FSM, Input, Randomizer |
| **Gameplay** | 핵심 재미를 만드는 시스템 — Lock Delay, Score, Hold, Ghost, Attack |
| **UI** | ViewModel + UMG View 계층 — Board Renderer, HUD, CommonUI, Animation, Pooling |
| **Persistence** | 설정 저장, 랭킹 — Settings, Save/Ranking |
| **Audio** | 사운드 시스템 — BGM, SFX |
| **Meta** | 접근성, 튜토리얼 등 — Accessibility |

---

## Priority Tiers

| Tier | Definition | 학습 Phase | Target |
|------|------------|-----------|--------|
| **MVP** | 코어 루프 동작 + 기본 UI + MVVM 바��딩 | Phase 1~3 | First Playable |
| **Vertical Slice** | 완전한 한 판 경험 — 공격, 메뉴, 설정 | Phase 4~5 | Demo |
| **Alpha** | 연출, 최적화 — Widget Animation, Pooling | Phase 6 | Alpha |
| **Full Vision** | 폴리시 — 랭킹, 오디오, 접근성 | Phase 7 | Release |

---

## Dependency Map

### Foundation Layer (의존성 없음)

1. **Board** — 10x20 그리드 데이터 구조, 모든 게임 로직의 기반
2. **Piece/SRS** — 테트로미노 정의 + SRS 회전 + Wall Kick 데이터
3. **Input/Handling** — Enhanced Input + DAS/ARR/DCD/SDF 타이머 (독립 레이어)

### Core Layer (Foundation에 의존)

4. **Randomizer** — depends on: Piece
5. **Lock Delay** — depends on: Board, Piece
6. **Score/Level** — depends on: Board
7. **Hold** — depends on: Piece, Randomizer
8. **Ghost Piece** — depends on: Board, Piece
9. **FSM** �� depends on: Board, Piece, Randomizer, Lock Delay, Input, Hold (오케스트레이션 허브)
10. **Attack/Garbage** — depends on: Board, Score/Level

### Presentation Layer (Core + ViewModel에 의존)

11. **ViewModel** — depends on: Board, Piece, Score/Level, FSM, Attack (게임 상태 → UI 바인딩)
12. **Game Session/Flow** — depends on: FSM
13. **Board Renderer** — depends on: ViewModel, Board
14. **HUD** — depends on: ViewModel
15. **Menu System (CommonUI)** — depends on: Game Session/Flow
16. **Widget Animation** — depends on: Board Renderer, HUD, ViewModel
17. **Widget Pooling** �� depends on: Attack, ViewModel

### Polish Layer

18. **Settings/Options** — depends on: Input/Handling, Menu System
19. **Save/Ranking** — depends on: Score/Level, Game Session/Flow
20. **Audio** — depends on: FSM, Game Session/Flow
21. **Accessibility** — depends on: Board Renderer, Settings

---

## Recommended Design Order

| Order | System | Priority | Layer | Effort |
|-------|--------|----------|-------|--------|
| 1 | Board | MVP | Foundation | S |
| 2 | Piece/SRS | MVP | Foundation | M |
| 3 | Randomizer | MVP | Core | S |
| 4 | Input/Handling (DAS/ARR) | MVP | Core | M |
| 5 | Lock Delay | MVP | Core | S |
| 6 | FSM | MVP | Core | M |
| 7 | Score/Level | MVP | Core | S |
| 8 | Hold | MVP | Core | S |
| 9 | Ghost Piece | MVP | Core | S |
| 10 | ViewModel | MVP | Presentation | M |
| 11 | Board Renderer | MVP | Presentation | M |
| 12 | HUD | MVP | Presentation | S |
| 13 | Attack/Garbage | VS | Core | M |
| 14 | Game Session/Flow | VS | Presentation | M |
| 15 | Menu System (CommonUI) | VS | Presentation | L |
| 16 | Settings/Options | VS | Polish | M |
| 17 | Widget Animation | Alpha | Presentation | M |
| 18 | Widget Pooling | Alpha | Presentation | S |
| 19 | Save/Ranking | Full | Polish | S |
| 20 | Audio | Full | Polish | M |
| 21 | Accessibility | Full | Meta | S |

*(S = 1세션, M = 2~3세션, L = 4+세��)*

---

## Circular Dependencies

- 하드 순환: 없음
- **FSM ↔ Score/Level**: 이벤트 기반 soft 양방향 (FSM이 `OnPieceLocked` 발행 → Score 구독; FSM이 spawn 시 `Score.GetBaseG` 조회). 생성 순환 아님 — Session 계층이 각 객체 생성 후 와이어링하고, 락(이벤트)과 스폰(조회)의 시점이 달라 재진입 없음.

---

## High-Risk Systems

| System | Risk Type | 설�� | 대응 |
|--------|-----------|------|------|
| ViewModel | Technical | MVVM + FieldNotify 첫 적용. 바인딩 구조 실패 시 전체 UI 재작업 | Board→Score 바인딩으로 소규모 프로토타입 먼저 검증 |
| Board Renderer | Technical | 200칸 UImage 위젯 성능. Phase 1→3 최적화 경로 시작점 | 각 Phase 전환 시 `stat slate` 측정 필수 |
| Input/Handling | Design | DAS/ARR 0ms, SDF ∞ 등 극단값 처리. 조작감이 게임 퀄리티 직결 | Tetr.io 파라미터 벤치마크 + 체감 반복 테스트 |

---

## Progress Tracker

| Metric | Count |
|--------|-------|
| Total systems identified | 21 |
| Design docs started | 6 |
| Design docs reviewed | 3 (Core Loop: FSM, Lock Delay, Score/Level) |
| Design docs approved | 3 (Core Loop: FSM, Lock Delay, Score/Level) |
| MVP systems designed | 7/12 (Board, Piece/SRS, Randomizer, FSM, Hold-in-FSM, Lock Delay, Score/Level) |
| Vertical Slice systems designed | 0/4 |
| Alpha systems designed | 0/2 |
| Full Vision systems designed | 0/3 |

---

## Next Steps

- [ ] Design MVP-tier systems first (use `/design-system [system-name]`)
- [ ] Run `/design-review` on each completed GDD
- [ ] Run `/gate-check pre-production` when MVP systems are designed
- [ ] Prototype the highest-risk system early (`/prototype [system]`)
