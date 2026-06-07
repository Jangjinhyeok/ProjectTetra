# AI 활용 워크플로우 & 진행 내역 (포트폴리오)

> 이 문서는 ProjectTetra를 **AI 코딩 에이전트(Claude Code)와 어떤 방법론으로 진행했는지**, 그리고 **무엇을 어디까지 만들었는지**를 정리한다. "AI에게 코드를 받아 붙였다"가 아니라, **설계·검증·스코프 통제를 사람이 쥔 채로 AI를 도구로 운용한 과정**을 보여주는 것이 목적이다.
>
> **Last Updated**: 2026-06-07

---

## 1. 한 줄 요약

게임 로직(Model)은 **테스트 우선 + 결정적 시뮬레이션**으로, UI는 **MVVM + CommonUI**로 분리해 만든 UE5.7 테트리스. AI는 **Architect/Builder 두 역할로 분리한 Two-CLI 워크플로우**로 운용했으며, 각 작업은 **게이트 단위로 분해 → HANDOFF 명세 → 구현 → RESULT 검토**의 사이클을 거쳤다.

---

## 2. AI 활용 방법론 — Two-CLI Architect/Builder

단순히 "AI에게 기능을 만들어 달라"고 하면, ① 요청 범위를 넘는 변경, ② 검증 없는 코드, ③ 설계 근거의 증발이 발생한다. 이를 막기 위해 AI 세션을 **두 역할로 분리**해 운용했다.

### 역할 분리

| 역할 | 책임 | 도구 제약 |
|------|------|----------|
| **Architect** | 설계, 영향 분석, 작업 명세(HANDOFF) 작성, 결과(RESULT) 검토 | 코드 Write 금지 — 읽기·분석·문서만 |
| **Builder** | HANDOFF를 spec으로 받아 구현, 게이트별 빌드·테스트 검증, RESULT 작성 | HANDOFF 수정 금지 — 명시된 파일만 변경 |

두 세션은 **파일로 통신**한다:
- `HANDOFF.md` (Architect → Builder): 목표, 제약, 수정 금지 영역, 게이트 분해, 게이트별 검증 방법.
- `RESULT.md` (Builder → Architect): 변경 파일, 핸드오프 준수 평가, 진행 중 발견·결정, 미해결 이슈.

이 분리가 만드는 효과:
- **설계와 구현의 분리** — 설계 세션은 "무엇을·왜"에 집중하고, 구현 세션은 "어떻게"에 집중한다. 한 세션이 둘을 섞으면 설계 근거가 코드 속에 묻힌다.
- **상호 검토** — Builder가 끝낸 코드를 Architect가 RESULT + 실제 diff로 다시 검토한다(같은 맥락에서 자기 코드를 보는 것보다 누락을 더 잘 잡는다).

### 게이트 분해

모든 비단순 작업은 **독립적으로 검증 가능한 1~3파일 단위의 게이트**로 쪼갰다. 좋은 게이트의 기준:
- 다른 게이트 없이도 빌드가 성공한다.
- 검증 기준이 명확하다(빌드 성공 / 특정 테스트 통과 / PIE 육안 항목).
- 다음 게이트의 전제를 명시한다.

> 예) Core Loop은 `G1 enum → G2 Lock Delay → G3 Score/Level → G4+G5 FSM 오케스트레이터`로 분해됐고, 각 게이트가 별도 커밋으로 남아 있다(`git log`에서 `Core Loop G1..G5` 확인 가능). Menu System(CommonUI)은 `G1 인프라 → G2 베이스/레이아웃 → G3 화면/Pause 위젯 → G4 PC 배선 → G5 WBP+PIE`로 분해됐다.

### 학습 통합 (CommonUI 사례)

CommonUI는 **개발자가 학습 목적으로 도입**했다. "AI가 다 만들면 모른 채 진행"되는 것을 막기 위해, 일반 구현 게이트와 다른 절차를 적용했다:
1. Architect가 **학습 문서**(`docs/design/commonui.md` — 7개 핵심 개념 + raw UMG 대비)를 먼저 작성.
2. HANDOFF의 각 게이트에 **"구현 전 읽을 개념"** 포인터를 박음.
3. 개발자가 개념 학습 → Builder 구현 → 개발자가 코드에서 그 개념이 어디 박혔는지 확인.

→ AI를 "코드 생성기"가 아니라 **"설계를 문서화하고 학습을 구조화하는 보조 도구"**로 쓴 사례.

### 사람이 쥔 결정들 (AI에 위임하지 않은 것)

방법론의 핵심은 **무엇을 AI에 맡기고 무엇을 안 맡겼는가**다. 다음은 개발자가 직접 결정했다:
- 아키텍처 방향(MVVM 채택, 결정적 시뮬레이션, Session을 WorldSubsystem으로 호스팅 — ADR-0001).
- 스코프 절단(Menu System을 "기반 + Pause 슬라이스"로 한정, 메인메뉴/게임오버는 후속).
- 트레이드오프 승인(예: UE 5.7에 `SetIsBackHandler()`가 없어 Builder가 제안한 적응안을 개발자가 승인).
- 커밋/푸시 시점(AI는 명시 지시 전까지 커밋하지 않음).

---

## 3. 이 방식이 만든 결과물 특성

- **검증 가능성** — Model 전 계층이 UI 없이 단위/통합 테스트로 완결(현재 110개 테스트 그린). 각 게이트가 빌드·테스트 게이트를 통과한 뒤에만 다음으로 진행.
- **스코프 통제** — HANDOFF가 "수정 금지 영역"을 명시하고 RESULT가 침범 여부를 자가 평가. "겸사겸사" 리팩토링이 끼어들지 않음.
- **설계 문서화** — 시스템마다 8섹션 GDD, 아키텍처 결정은 ADR로 남김. AI 작업의 근거가 코드 밖에 보존됨.

---

## 4. 진행 내역 (시스템별)

### Model 계층 — 순수 게임 로직 (테스트 우선, 결정적)
UI를 전혀 참조하지 않으며 콘솔에서도 동작 가능한 수준으로 분리. 전부 단위/통합 테스트 보유.

| 시스템 | 내용 | 문서 |
|--------|------|------|
| Board (10×24) | 충돌·라인클리어·가비지·Top-out | `docs/design/board.md` |
| Piece / SRS | 7-피스, Wall Kick, 하드드롭 | `docs/design/piece-srs.md` |
| Randomizer (7-Bag) | 시드 고정 결정성 | `docs/design/randomizer.md` |
| Lock Delay | Extended Placement, 정수 스텝 카운트(float 누적 회피 → 플랫폼 무관 결정성) | `docs/design/lock-delay.md` |
| Score / Level | lines/combo/B2B, 중력 커브 | `docs/design/scoring.md` |
| FSM (GameCore) | 게임 루프 오케스트레이터, 고정스텝, 추상 명령 큐 | `docs/design/fsm.md` |

> **결정적 시뮬레이션**: 게임 로직은 고정 타임스텝 `Step()`으로만 전진하고, 입력을 직접 받지 않고 추상 명령 큐(`EGameCommand`)를 소비한다. 동일 시드 + 동일 명령 + 동일 스텝 수 → 동일 결과 → 리플레이/넷코드 확장의 기반.

### Core / Session — 시뮬레이션 호스트
- `UTetrisSessionSubsystem`(Tickable World Subsystem)이 Core Loop을 소유하고 매 프레임 고정스텝 accumulator로 구동. 설계 근거는 `docs/architecture/adr-0001`.

### Input — 결정적 입력 핸들링
- DAS/ARR/DCD를 순수 `FTetrisHandling`에서 고정스텝으로 결정(프레임레이트 독립). PlayerController는 Enhanced Input을 "추상 명령"으로만 번역하는 어댑터. `docs/design/input-handling.md`.

### UI 계층 — MVVM + CommonUI
게임 로직과 UI를 완전 분리. View는 ViewModel만 알고 Model을 직접 참조하지 않는다.

| 시스템 | 내용 | 문서 |
|--------|------|------|
| HUD ViewModel | MVVM + FieldNotify, Global View Model Collection. **위젯 없이 단위 테스트 가능** | `docs/design/viewmodel.md` |
| Board Renderer | Board 전용 VM + 바인더 + UMG 보드 위젯 | (Board Renderer HANDOFF) |
| HUD | 점수/레벨/줄 + Next 큐 + Hold, 이벤트 주도 갱신(Tick polling 없음) | (HUD HANDOFF) |
| Menu System Slice 1 | **CommonUI 인프라**(PrimaryGameLayout 4레이어 + 입력 라우팅) + **Pause 메뉴**(게임↔UI 입력 전환, 게임패드 네비, Back 핸들링) | `docs/design/commonui.md` |

> **MVVM 분리의 증거**: ViewModel이 Model/Session/위젯을 컴파일 의존하지 않아, 위젯·월드 없이 테스트가 통과한다. CommonUI 메뉴 위젯도 Session/Model을 참조하지 않고 델리게이트로 명령만 발행한다(실행은 PlayerController).

### 진행 중 (다음)
- **Menu System Slice 2** — 메인 메뉴(Menu 레이어) + GameOver 결과 화면(VM `GetGameState()` 구독 → Modal 레이어). 동일 CommonUI 인프라 재사용.
- 측정 기반 UI 최적화(Retainer Box / 단일 머티리얼), Widget Animation, 사운드, 랭킹(SaveGame).

---

## 5. 문서 맵 (어디서 뭘 보는지)

| 보고 싶은 것 | 문서 |
|--------------|------|
| 프로젝트 개요·아키텍처·로드맵 | `README.md` |
| AI 활용 방법론·진행 내역(이 문서) | `docs/ai-workflow.md` |
| 시스템별 상세 설계(8섹션 GDD) | `docs/design/*.md`, 인덱스 `docs/design/systems-index.md` |
| 아키텍처 결정 기록 | `docs/architecture/adr-0001-*.md` |
| CommonUI 개념 학습 + 설계 | `docs/design/commonui.md` |

> **작업 통신 파일**(`HANDOFF.md`/`RESULT.md`)은 Architect↔Builder 세션 간 통신용이며 진행 중인 작업의 스냅샷이다(버전 관리 대상 아님 — 작업 단위로 덮어쓰임).

---

## 6. 사용한 도구

- **Claude Code** (Anthropic) — Architect/Builder 두 세션으로 운용.
- **mcp-unreal** — 에디터 연동 MCP 서버(레벨 액터 조회, 빌드/테스트 실행 등). 로컬 전용이라 버전 관리 제외.
- **UE5.7 Automation Testing** — Model 계층 검증(현재 110 테스트).
