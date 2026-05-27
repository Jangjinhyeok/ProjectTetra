# ProjectTetra (Tetra-UE)

> **Unreal Engine 5.7 기반 경쟁형 테트리스** — UMG / Common UI 아키텍처와 엔진 내부 원리 이해를 증명하기 위한 포트폴리오 프로젝트.

단순 기능 구현이 아니라 **UI 아키텍처 설계 능력**(MVVM + FieldNotify)과 **결정적 시뮬레이션(Deterministic Simulation)** 설계를 보여주는 것이 핵심 목표다. 레퍼런스는 Tetr.io / 뿌요뿌요 테트리스의 조작감.

---

## 왜 이 프로젝트인가 (목적)

- **루틴 습관화**: 설계 → 구현 → 측정(프로파일링) → 리팩터링 → 테스트 사이클을 매 작업 단위마다 반복.
- **MVVM + FieldNotify 기반 이벤트 주도 UI**: UMG를 "쓰는" 수준이 아니라, ViewModel 계층을 두고 **데이터가 바뀔 때만** UI가 갱신되는 구조를 체득. `Tick`/Property Binding 남발 금지.
- **수치 기반 성능 최적화 습관**: Widget Pooling / Retainer Box / 단일 머티리얼 렌더링 적용 시 `stat`·Unreal Insights로 전·후 비교.
- **입력 지연·조작감 정교화**: DAS / ARR / DCD / SDF를 정밀 설계하고, 결정적 시뮬레이션 구조를 익힘.

---

## 기술 스택

| 항목 | 내용 |
|------|------|
| 엔진 | Unreal Engine 5.7 |
| 언어 | C++ (로직 전부), Blueprint (View 바인딩·위젯 애니메이션에만 제한) |
| UI | UMG + Common UI Plugin + UMG ViewModel(MVVM) Plugin |
| 입력 | Enhanced Input |
| 테스트 | Unreal Automation Testing Framework |

---

## 아키텍처

### MVVM 3계층 — 로직과 UI의 강한 분리

```
[Model]                 [ViewModel]              [View]
순수 게임 로직 (C++)  →  FieldNotify 상태 노출  →  UMG 위젯
Board/Piece/FSM/Score    이벤트 발행                바인딩으로 표시만
```

- **Model**: 시뮬레이션·SRS·충돌·FSM·점수. UI를 절대 참조하지 않으며 콘솔에서도 동작 가능.
- **ViewModel**: Model 상태를 UI가 소비할 형태로 변환, 변경 시에만 통지.
- **View**: ViewModel만 알고 Model은 직접 참조하지 않음.

### Deterministic Simulation (결정적 시뮬레이션)

- 게임 로직은 **고정 타임스텝 `Step()`** 으로만 전진하며, 입력은 직접 받지 않고 **추상 명령 큐(`EGameCommand`)** 를 소비한다.
- 동일 시드 + 동일 명령 시퀀스 + 동일 스텝 수 → **동일 결과**를 보장 → 리플레이/넷코드 확장의 기반.
- 시간 기반 타이머(Lock Delay)는 float 누적 대신 **정수 스텝 카운트**로 플랫폼 무관 결정성 확보.

---

## 구현 현황

| 시스템 | 계층 | 상태 | 비고 |
|--------|------|------|------|
| Board (10×24 Playfield) | Model | ✅ 구현 + 테스트 | 충돌·라인클리어·가비지·Top-out |
| Piece / SRS | Model | ✅ 구현 + 테스트 | 7-피스, Wall Kick, 하드드롭 |
| Randomizer (7-Bag) | Model | ✅ 구현 + 테스트 | 시드 고정 결정성 |
| Lock Delay | Model | ✅ 구현 + 테스트 | Extended Placement, 정수 스텝 |
| Score / Level | Model | ✅ 구현 + 테스트 | lines/combo/B2B, 중력 커브 소유 |
| **FSM (GameCore)** | Model | ✅ 구현 + 테스트 | 오케스트레이터, 고정스텝, 명령 큐 |
| Input / Handling (DAS/ARR) | Model | ⏳ 예정 | Enhanced Input → 명령 큐 |
| Session 와이어링 | Core | ✅ 구현 + 테스트 | WorldSubsystem 호스트 + 고정스텝 구동 (ADR-0001) |
| ViewModel | UI | ⏳ 예정 | UMG ViewModel + FieldNotify |
| Board Renderer / HUD | UI | ⏳ 예정 | UMG, 이후 Retainer/SDF 최적화 |
| Menu / Common UI | UI | ⏳ 예정 | 화면 스택, 입력 라우팅 |

> 모든 Model 시스템은 **UI 없이 단위/통합 테스트로 완결**되도록 설계됨 (`Source/ProjectTetra/Tests/`).

---

## 디렉토리 구조

```
ProjectTetra/
├── Source/ProjectTetra/
│   ├── Core/        # TetrisTypes.h — enum/struct/상수 (공통 어휘, 의존 0)
│   ├── Board/       # UTetrisBoard — Playfield 데이터·판정
│   ├── Block/       # PieceData, FActivePiece + FTetrisPieceOps (SRS)
│   ├── System/      # UTetrisRandomizer, FTetrisLockDelay, UTetrisScoring
│   ├── FSM/         # UTetrisGameCore — 게임 루프 오케스트레이터
│   └── Tests/       # Automation 단위/통합 테스트 (~50개)
├── docs/
│   └── design/      # 게임 디자인 문서(GDD) — 시스템별 설계 명세
└── README.md
```

---

## 설계 문서 (GDD)

각 시스템은 8개 필수 섹션(Overview / Player Fantasy / Detailed Design / Formulas / Edge Cases / Dependencies / Tuning Knobs / Acceptance Criteria)을 갖춘 GDD를 가진다.

- [`docs/design/systems-index.md`](docs/design/systems-index.md) — 전체 시스템 인덱스·의존성·설계 순서
- [`docs/design/board.md`](docs/design/board.md) · [`piece-srs.md`](docs/design/piece-srs.md) · [`randomizer.md`](docs/design/randomizer.md)
- [`docs/design/fsm.md`](docs/design/fsm.md) · [`lock-delay.md`](docs/design/lock-delay.md) · [`scoring.md`](docs/design/scoring.md)

**아키텍처 결정 기록(ADR)** — `docs/architecture/`
- [`adr-0001-simulation-host-and-fixed-step-driver.md`](docs/architecture/adr-0001-simulation-host-and-fixed-step-driver.md) — 시뮬레이션 호스트(Tickable World Subsystem) & 고정스텝 구동

---

## 빌드 & 테스트

**빌드**: `ProjectTetra.uproject`를 열거나 IDE(Rider/VS)에서 `ProjectTetraEditor | Development` 타깃 빌드.

**테스트** (Automation):
- 에디터 → `Tools > Session Frontend > Automation` → 필터 `Tetris` → 전체 실행
- 또는 헤드리스:
  ```
  UnrealEditor-Cmd.exe ProjectTetra.uproject -ExecCmds="Automation RunTests Tetris; Quit" -unattended -nullrhi -nosplash -log
  ```

테스트 네임스페이스: `Tetris.Board.* / Tetris.Piece.* / Tetris.Randomizer.* / Tetris.LockDelay.* / Tetris.Scoring.* / Tetris.GameCore.*`

---

## 로드맵

1. **Input / Handling** (다음) — Enhanced Input + DAS/ARR/DCD/SDF → 명령 큐 연결
2. **ViewModel** — UMG ViewModel + FieldNotify로 게임 상태 노출 (첫 MVVM 적용)
3. **Board Renderer / HUD** — UMG 렌더링 → 측정 기반 최적화(Retainer Box → SDF)
4. **Common UI** — 화면 스택·입력 라우팅·게임패드 지원
5. **Polish** — Widget Animation, Widget Pooling, 사운드, 랭킹(SaveGame)

> Session 와이어링(WorldSubsystem 호스트 + 고정스텝 구동)은 완료됨 — `docs/architecture/adr-0001` 참조.
