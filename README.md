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
| Input / Handling (DAS/ARR) | Core | ✅ 구현 + 테스트 | 고정스텝 순수 핸들링(DAS/ARR/DCD) + Enhanced Input |
| Session 와이어링 | Core | ✅ 구현 + 테스트 | WorldSubsystem 호스트 + 고정스텝 구동 (ADR-0001) |
| ViewModel (HUD) | UI | ✅ 구현 + 테스트 | HUD VM + 바인더, FieldNotify, MVVM Global Collection (위젯은 #12/#13) |
| Board Renderer | UI | ✅ 구현 + PIE | Board 전용 VM + 바인더 + UMG 보드 위젯(C++ base). PIE 육안 확인 완료 |
| HUD | UI | ✅ 구현 + PIE | 점수/레벨/줄 + Next 큐 + Hold, 이벤트 주도 갱신. PIE 육안 확인 완료 |
| Menu / Common UI | UI | 🔄 진행 중 (Slice 1 완료) | CommonUI 인프라(PrimaryGameLayout 4레이어 + 입력 라우팅) + Pause 메뉴 완료. 메인메뉴/GameOver는 Slice 2 |

> 모든 Model 시스템은 **UI 없이 단위/통합 테스트로 완결**되도록 설계됨 (`Source/ProjectTetra/Tests/`).

---

## UI 구현 구조

UI는 두 축으로 설계했다 — **MVVM(로직/UI 분리)** + **CommonUI(화면 스택·입력 라우팅·게임패드)**. 아래는 각 서브시스템이 *무엇을·왜·어떻게* 구현됐는지의 분해다.

> 📹 **데모 캡처 위치**: 각 항목의 `데모:` 줄에 GIF/스크린샷을 넣는다(에디터 PIE 캡처). 현재는 자리표시.

### 1. CommonUI 화면 스택 — PrimaryGameLayout + 4 레이어

UI를 "위젯 트리"가 아니라 **활성/비활성 화면들의 스택**으로 모델링한다. 루트 `UTetrisPrimaryGameLayout`이 4개 `CommonActivatableWidgetStack`(레이어)을 품고, `PushWidgetToLayer(EUILayer, …)`로 화면을 push/pop한다.

```
UTetrisPrimaryGameLayout (PC가 생성·viewport fill)
├ MenuStack      ← 메인 메뉴 (Slice 2)
├ GameStack      ← 인게임 화면 (보드 + HUD)
├ GameMenuStack  ← Pause 오버레이
└ ModalStack     ← GameOver / 다이얼로그
```

- 레이어 식별은 `EUILayer` enum(4레이어 고정 → GameplayTag 대비 보일러플레이트 회피).
- 모든 화면의 공통 베이스 `UTetrisActivatableWidget`이 입력 모드·Back 핸들러를 데이터로 선언.
- 📹 데모: *(Pause 오버레이 push/pop — 캡처 예정)*
- 소스: [`TetrisPrimaryGameLayout.h`](Source/ProjectTetra/UI/Foundation/TetrisPrimaryGameLayout.h) · [`TetrisActivatableWidget.h`](Source/ProjectTetra/UI/Foundation/TetrisActivatableWidget.h)

### 2. 입력 라우팅 — 게임 ↔ UI 자동 전환 (CommonUI 핵심)

raw UMG가 못 푸는 부분. 각 화면이 `GetDesiredInputConfig()`로 **자신이 원하는 입력 모드(Game/Menu)를 데이터로 선언**하면, 스택의 최상단 화면에 맞춰 입력 모드가 자동 전환된다(`CommonGameViewportClient` 경유). PlayerController가 `SetInputMode`를 수동 토글하지 않는다.

- 게임 화면 = `Game`(블록 입력 활성) / Pause·메뉴 = `Menu`(게임 입력 자동 차단, UI 네비만).
- 게임패드 ↔ 키보드 포커스 전환은 `CommonButtonBase`가 내장 처리.
- 📹 데모: *(Pause 시 블록 입력 차단 + 게임패드 메뉴 네비 — 캡처 예정)*
- 소스: [`TetrisActivatableWidget.cpp`](Source/ProjectTetra/UI/Foundation/TetrisActivatableWidget.cpp) (`GetDesiredInputConfig`) · [`TetrisPlayerController.cpp`](Source/ProjectTetra/Input/TetrisPlayerController.cpp)
- 개념 상세: [`docs/design/commonui.md`](docs/design/commonui.md)

### 3. MVVM + FieldNotify — 데이터 주도 HUD

게임 로직과 UI를 강하게 분리한다. **Model → Binder → ViewModel(FieldNotify) → View**. ViewModel은 순수 데이터 홀더로 Model/위젯을 컴파일 의존하지 않아 **위젯·월드 없이 단위 테스트**가 가능하다. 값이 실제로 바뀔 때만 통지(`Tick`/Property Binding polling 없음).

```
UTetrisGameCore/Scoring (델리게이트)
   → UTetrisHUDViewModelBinder (구독 + setter + 컬렉션 등록)
   → UTetrisHUDViewModel (FieldNotify 필드: Score/Level/Lines/Hold/Next/…)
   → View (Global VM Collection에서 "TetrisHUD" 키로 self-resolve)
```

- View는 ViewModel만 알고 Model을 직접 참조하지 않는다(컬렉션 resolve).
- 📹 데모: *(점수/레벨/줄 + Next/Hold 실시간 갱신 — 캡처 예정)*
- 소스: [`TetrisHUDViewModel.h`](Source/ProjectTetra/UI/ViewModel/TetrisHUDViewModel.h) · [`TetrisHUDViewModelBinder.h`](Source/ProjectTetra/UI/ViewModel/TetrisHUDViewModelBinder.h) · [`TetrisHUDWidget.h`](Source/ProjectTetra/UI/Views/TetrisHUDWidget.h)
- 설계 상세: [`docs/design/viewmodel.md`](docs/design/viewmodel.md)

### 4. 보드 렌더링 — Board 전용 ViewModel

보드 그리드도 View가 Model을 직접 읽지 않고 **Board 전용 VM(`UTetrisBoardViewModel`)을 경유**한다(MVVM 일관성). HUD VM과 동일한 패턴(전용 바인더 + Global VM Collection). 셀 렌더 MVP는 UMG `UniformGridPanel` + `Image`이며, 측정 기반 최적화(Retainer Box → 단일 머티리얼)는 후속 단계.

- 📹 데모: *(보드 렌더 + 라인 클리어 — 캡처 예정)*
- 소스: [`TetrisBoardViewModel.h`](Source/ProjectTetra/UI/ViewModel/TetrisBoardViewModel.h) · [`TetrisBoardWidget.h`](Source/ProjectTetra/UI/Views/TetrisBoardWidget.h)

### 5. 화면 플로우 — 메뉴 ↔ 게임 ↔ GameOver (🔄 Slice 2 진행 중)

단일 맵에서 **맵 로딩 없이 CommonUI 레이어 전환**으로 게임 플로우를 구성한다. 메인 메뉴(Menu 레이어) → Start → 게임(Game 레이어) → Top-out → GameOver(Modal 레이어) → Retry/메뉴 복귀. 화면 push/pop만으로 입력 모드가 자동 전환되는 구조를 플로우 전체로 확장한다.

- 메인 메뉴 비주얼은 Tetr.io 홈 메뉴(가로 컬러 바)를 레퍼런스로 꾸민다(구조=C++, 비주얼=WBP).
- 📹 데모: *(Slice 2 완료 후)*

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

## AI 활용 워크플로우

이 프로젝트는 AI 코딩 에이전트(Claude Code)를 **단순 코드 생성기가 아니라, 설계·검증·스코프 통제를 사람이 쥔 채 운용하는 도구**로 사용했다. AI 세션을 **Architect(설계·검토)와 Builder(구현)로 분리**하고, 모든 작업을 **게이트 단위로 분해 → 명세(HANDOFF) → 구현 → 검토(RESULT)**의 사이클로 진행했다. CommonUI처럼 학습이 목적인 영역은 개념 문서와 게이트별 학습 포인트를 먼저 만든 뒤 구현에 들어가는 **학습 통합 워크플로우**를 적용했다.

→ 방법론 상세, 사람이 직접 내린 결정들, 시스템별 진행 내역: **[`docs/ai-workflow.md`](docs/ai-workflow.md)**

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

1. **ViewModel** ✅ — HUD VM + 바인더 + FieldNotify로 게임 상태 노출 (첫 MVVM 적용, Global VM Collection)
2. **Board Renderer / HUD** ✅ — UMG 렌더링(보드 그리드 + 점수/레벨/줄 + Next/Hold), 이벤트 주도 갱신. 측정 기반 최적화(Retainer Box → SDF)는 후속
3. **Common UI** 🔄 — Slice 1 완료(인프라 + Pause: 화면 스택·입력 라우팅·게임패드 네비·Back). Slice 2 = 메인메뉴 + GameOver 화면
4. **Polish** — Widget Animation, Widget Pooling, 사운드, 랭킹(SaveGame)

> Session 와이어링 + Input/Handling + ViewModel + Board/HUD 렌더링 + CommonUI Slice 1(Pause)까지 완료됨 — `docs/architecture/adr-0001`, `docs/design/input-handling.md`, `docs/design/viewmodel.md`, `docs/design/commonui.md` 참조. AI 활용 방법론·진행 내역은 `docs/ai-workflow.md`.
