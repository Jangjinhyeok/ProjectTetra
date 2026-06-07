# Common UI — 화면 스택 · 입력 라우팅 · 게임패드 (학습 + 설계)

> **Status**: In Design (Slice 1 = Pause 메뉴)
> **Author**: user + Claude (Architect)
> **Last Updated**: 2026-06-07
> **Implements Pillar**: Common UI 기반 화면 스택 + 입력 라우팅 (포트폴리오 핵심 차별화)
> **목적**: 이 문서는 **학습 문서**다. Builder가 구현하기 전에 사용자가 CommonUI의 개념을 먼저 이해하기 위한 것이며, 동시에 블로그 주제 "Common UI 도입 전후 비교"의 초안이 된다. 각 게이트가 건드리는 개념을 본문에서 설명한다.

---

## Overview

CommonUI는 **UMG 위에 얹는 Epic의 플러그인**이다(Fortnite에서 유래). raw UMG가 못 푸는 **크로스플랫폼 UI 조율 문제**를 표준화한다: ① 여러 화면을 쌓고 빼는 **화면 스택**, ② 게임 입력과 UI 입력을 중재하는 **입력 라우팅**, ③ 키보드/게임패드/마우스 **자동 전환과 focus 네비게이션**, ④ 장치별 **입력 힌트**, ⑤ 스타일/테마. 핵심은 CommonUI가 "위젯을 **어떻게 그리는가**"의 도구가 아니라 "**여러 화면과 여러 입력 장치를 어떻게 조율하는가**"의 프레임워크라는 점이다 — UMG가 그리기(View)라면 CommonUI는 그 위의 **화면·입력 오케스트레이션 계층**이다.

이 프로젝트에서 CommonUI를 쓰는 이유는 단순히 메뉴가 필요해서가 아니다. raw UMG로도 메뉴는 만든다. CommonUI를 쓰는 이유는 **포트폴리오로 "화면 스택 설계 + 입력 모드 중재 + 게임패드 완벽 지원"을 코드로 증명**하기 위해서다. 이 셋은 raw UMG에서 손으로 짜면 깨지기 쉽고 표준이 없는 부분이고, CommonUI는 바로 이걸 구조화한다.

**핵심 프레이밍 (불변 결정):**
1. **화면 = `UCommonActivatableWidget`** — UI를 "위젯 트리"가 아니라 **활성/비활성되는 화면들의 스택**으로 모델링한다. 한 화면이 곧 activatable 하나.
2. **입력 모드는 화면이 선언한다** — 각 화면이 `GetDesiredInputConfig()`로 "나는 Game/Menu/All 입력을 원함"을 선언하고, `CommonGameViewportClient`가 그걸 강제한다. PC가 `SetInputMode`를 수동으로 토글하지 않는다.
3. **레이어로 동시 표시를 분리한다** — HUD(Game)·Pause(GameMenu)·다이얼로그(Modal)는 서로 다른 **레이어(스택)**에 살아 독립적으로 push/pop된다.
4. **메뉴는 명령만 발행** — 버튼은 Session/Model을 모르고 델리게이트로 의도만 발행, 실행은 컨트롤러(PC)가 한다(MVVM 계층 격리 유지).
5. **경량 패턴** — Lyra의 `GameUIManagerSubsystem`/`GameUIPolicy` 풀스택 대신, PC가 `PrimaryGameLayout`를 직접 소유하는 단순 구조. 단일 로컬 플레이어 규모에 맞춘 의도적 단순화.

---

## Player Fantasy

(플레이어 직접 체감 시스템이 아닌 인프라 계층 — "느낌"은 **메뉴 조작감**과 **장치 전환의 매끄러움**에서 나온다.)

플레이어가 게임 중 Pause를 누르면 게임이 **즉시 멈추고** 메뉴가 뜨며, 이 순간 블록 이동 키는 더 이상 먹지 않는다(게임 입력 차단). 키보드만 쓰던 사람이 게임패드를 집어 D-pad를 움직이면 메뉴 focus가 **이음새 없이** 게임패드로 넘어가고, Resume을 고르거나 B/Esc를 누르면 정확히 게임으로 복귀한다 — 한 프레임의 입력 씹힘도, "지금 게임 입력인지 메뉴 입력인지" 모호함도 없다. 이 매끄러움은 우연이 아니라 CommonUI의 입력 라우팅이 만든다.

개발자(포트폴리오) 관점의 fantasy는 다른 층위다: **메인 메뉴·게임오버·설정 화면을 추가할 때 매번 입력 모드 코드를 새로 짜지 않고, "activatable 화면 하나를 만들어 레이어에 push"만 하면 입력 라우팅·게임패드 네비·Back이 공짜로 따라오는** 경험. 그리고 면접에서 "raw UMG 대비 CommonUI가 무엇을 표준화하는가"를 화면 스택·입력 모드·focus 세 축으로 설명할 수 있게 되는 것.

---

## Detailed Design

### 7개 핵심 개념 (이걸 알면 CommonUI를 안다)

#### 1. `UCommonActivatableWidget` — "화면" 한 장
raw UMG의 `UUserWidget`을 상속한 위젯으로, **활성/비활성 생명주기**를 추가한다.
- `NativeOnActivated()` / `NativeOnDeactivated()` — 화면이 스택의 맨 위로 와서 보이게/가려지게 될 때 호출. (위젯 `Construct/Destruct`와 **다르다** — 풀에서 재사용되는 위젯은 active만 토글될 수 있어 상태 초기화는 여기서.)
- **이 프로젝트**: 공통 베이스 `UTetrisActivatableWidget`를 두고, Game 화면·Pause 화면이 이를 상속. 베이스가 입력 모드·Back 핸들러를 데이터로 지정.

#### 2. `UCommonActivatableWidgetStack` — 화면을 쌓는 컨테이너
activatable들을 **스택(LIFO)**으로 관리한다. `AddWidget<T>()`로 push하면 그 화면이 맨 위로 와 active가 되고 아래는 deactivate된다. 맨 위를 pop하면 아래가 다시 active. **"화면 네비게이션 = 스택 push/pop"**이 CommonUI의 멘탈 모델이다.
- raw UMG엔 이 개념이 없다 — 직접 `AddToViewport`/`RemoveFromParent`로 z-order와 표시를 손으로 관리해야 한다.

#### 3. `PrimaryGameLayout` + 레이어 — 동시에 떠 있는 여러 스택
스택 하나로는 부족하다. HUD가 떠 있는 **동시에** Pause 메뉴가 그 위에 떠야 한다. 그래서 **여러 스택(=레이어)을 품은 루트 위젯**을 둔다.
```
PrimaryGameLayout (루트, viewport fill)
├ MenuStack     ← 메인 메뉴 (후속)
├ GameStack     ← in-game HUD (Game 화면)
├ GameMenuStack ← Pause 메뉴 (게임 위 오버레이)
└ ModalStack    ← 팝업/다이얼로그 (후속)
```
레이어는 z-order대로 쌓인다(Modal이 가장 위). 각 레이어는 독립 스택이라 서로 간섭 없이 push/pop.
- **이 프로젝트**: `UTetrisPrimaryGameLayout`가 4개 스택을 `BindWidget`으로 갖고, `PushWidgetToLayer(EUILayer, WidgetClass)` API 노출. 레이어 식별은 `EUILayer` enum(GameplayTag 대신 단순화).

#### 4. `FUIInputConfig` + `CommonGameViewportClient` — 입력 라우팅 (★핵심)
**이게 raw UMG가 못 하는, CommonUI를 쓰는 가장 큰 이유.** 각 activatable 화면이 `GetDesiredInputConfig()`로 자신이 원하는 입력 모드를 선언한다:

| `ECommonInputMode` | 의미 | 우리 용례 |
|--------------------|------|----------|
| `Game` | 게임 입력만 활성, UI 네비 비활성 | in-game HUD 화면 |
| `Menu` | UI 입력(네비/확인/취소)만, 게임 입력 차단 | Pause 메뉴 |
| `All` | 둘 다 | (혼합 HUD — 본 슬라이스 미사용) |

스택의 맨 위 화면이 바뀔 때마다 CommonUI가 그 화면의 `GetDesiredInputConfig()`를 읽어 **현재 입력 모드를 자동 전환**한다. 이걸 실제로 강제하는 게 `CommonGameViewportClient` — 프로젝트의 Game Viewport Client Class를 이걸로 바꿔야(`DefaultEngine.ini`) 입력 가로채기가 동작한다. **이 설정이 없으면 `GetDesiredInputConfig`가 무효**가 되는 게 CommonUI 입문 최대 함정.
- raw UMG였다면: PC가 Pause 시 `SetInputMode(UIOnly)` + IMC 수동 제거, Resume 시 복원 — 상태가 어긋나기 쉽다. CommonUI는 "화면이 입력 모드를 데이터로 선언" → 스택이 자동 중재로 대체.

#### 5. `UCommonButtonBase` — 게임패드를 아는 버튼
raw UMG `UButton`과 달리 **focus 네비게이션·게임패드 입력·hold-to-confirm·장치별 스타일**이 내장. 키보드 방향키/게임패드 D-pad로 버튼 사이를 이동하고, 현재 focus된 버튼이 시각적으로 강조된다. 스타일은 `UCommonButtonStyle` 에셋으로 데이터 주도.
- raw UMG `UButton`은 게임패드 focus가 약해 손으로 네비 로직을 짜야 한다.

#### 6. Back 핸들링 — 일관된 "뒤로"
B(게임패드)/Esc(키보드)가 **어느 화면에서든 일관되게** 동작하도록 CommonUI가 표준화. 화면이 `SetIsBackHandler(true)`를 선언하면, Back 입력 시 그 화면의 `NativeOnHandleBackAction()`이 호출된다. 보통 "현재 화면 pop"이지만, 우리 Pause는 **Back = Resume**(메뉴 닫고 게임 복귀)으로 매핑.
- Back 입력 자체는 CommonUI Settings의 Default Back Action(`CommonInputActionDataBase` 에셋)으로 정의.

#### 7. `CommonInput` (장치 감지) — 힌트의 기반
`UCommonInputSubsystem`이 현재 입력 장치(KBM / Gamepad / Touch)를 실시간 감지하고, 장치가 바뀌면 델리게이트를 발행한다. 이걸 구독해 버튼 힌트 아이콘(Ⓐ vs Enter)을 자동 교체한다.
- **본 슬라이스**: 플러그인 활성·장치 감지 동작까지만(G1). 힌트 아이콘 위젯은 후속 슬라이스.

### 계층/소유 구조 (이 프로젝트)

```
ATetrisPlayerController (소유자)
└ UTetrisPrimaryGameLayout (PC가 생성·AddToViewport)
   ├ GameStack     ← UTetrisGameScreenWidget   (InputMode=Game)  ← 내부에 WBP_Board + WBP_HUD
   ├ GameMenuStack ← UTetrisPauseWidget         (InputMode=Menu, BackHandler) ← CommonButton ×3
   ├ MenuStack     (빈 레이어, 후속)
   └ ModalStack    (빈 레이어, 후속)

UTetrisActivatableWidget (공통 베이스: GetDesiredInputConfig + bIsBackHandler를 데이터로)
├ UTetrisGameScreenWidget
└ UTetrisPauseWidget
```

**계층 규칙**: 메뉴 위젯은 Model/VM/Session을 모른다. Pause 버튼 → 네이티브 델리게이트(OnResume/OnRestart/OnQuit) 발행 → **PC가 구독**해 `Session->SetPaused/RestartGame`/quit 실행. (View는 명령 발행, 실행은 컨트롤러 — MVVM 일관.)

---

## "Formulas" — 입력 모드 결정 표

CommonUI엔 수식이 없지만, 핵심 결정 로직은 **"맨 위 화면 → 입력 모드" 매핑**이다:

| 스택 최상단 화면 | InputMode | 게임 입력 | UI 네비 | 마우스 |
|------------------|-----------|----------|---------|--------|
| GameScreen (HUD) | `Game` | ✅ | ❌ | NoCapture |
| PauseWidget | `Menu` | ❌ | ✅ | NoCapture |

Pause push → 최상단이 PauseWidget → 자동 `Menu` 모드 → 게임 입력 차단. Pause pop → 최상단이 GameScreen → 자동 `Game` 모드 → 게임 입력 복귀. **PC는 입력 모드를 직접 만지지 않는다** — 스택 변화가 곧 입력 모드 변화.

---

## Edge Cases

- **Pause 중복 진입** — Pause 키 연타 시 두 번 push 금지. PC가 `ActivePause` 추적해 토글(있으면 닫고, 없으면 연다).
- **Idle/GameOver에서 Pause** — 게임이 진행 중이 아닐 때 Pause는 무동작. `GameCore->GetState()` 가드.
- **Viewport Client 미설정** — `GameViewportClientClassName` 누락 시 입력 모드 전환이 조용히 무효(에러 없이 게임 입력이 안 꺼짐). G1 검증 필수.
- **CommonButton 스타일 누락** — Style 미지정 버튼은 회색/무반응. WBP 제작 시 `DA_ButtonStyle` 필수.
- **Widget 풀 재사용 상태** — activatable이 재사용될 때 `NativeOnActivated`에서 상태 초기화(본 슬라이스는 Pause가 단순해 미해당, 후속 메뉴에서 주의).
- **Back이 게임까지 닫음** — Pause의 `NativeOnHandleBackAction`이 `true`를 반환해 Back 전파를 막아야(GameScreen까지 pop되지 않게).

---

## Dependencies

| 의존 | 방향 | 비고 |
|------|------|------|
| CommonUI / CommonInput 플러그인 | 빌드 | `.uproject` 활성 + Build.cs 모듈 |
| `CommonGameViewportClient` | 런타임 | `DefaultEngine.ini` Game Viewport Client Class |
| `UTetrisSessionSubsystem` | PC→Session | Pause/Restart 실행(메뉴 위젯은 미참조, PC만) |
| 기존 WBP_Board / WBP_HUD | 디자이너 | GameScreen 내부에 재배치(C++ 무변경) |
| HUD VM `GetGameState()` | (후속) | GameOver 화면 전환에 사용 — 본 슬라이스 미사용 |

**무의존**: Model 전 계층, HUD/Board VM·바인더, 기존 위젯 C++. CommonUI 도입이 게임 로직에 영향 0.

---

## Tuning Knobs

- `EUILayer` — 레이어 종류(Menu/Game/GameMenu/Modal). 확장 시 여기.
- 화면별 `InputModeOverride`(Game/Menu/All) + `MouseCapture` — activatable 베이스의 EditDefaultsOnly.
- `bIsBackHandler` — 화면이 Back을 가로챌지.
- `DA_ButtonStyle` / `DA_TextStyle` — CommonButton/Text 룩. 데이터 주도.
- CommonUI Settings: `CommonButtonAcceptKeyHandling`(이미 `TriggerClick`), Default Back Action.
- IA_Pause 매핑(IMC_Gameplay) — Pause 키 바인딩.

---

## Acceptance Criteria

- [ ] CommonUI/CommonInput 활성 + 빌드 성공(G1).
- [ ] `UTetrisActivatableWidget`/`UTetrisPrimaryGameLayout` 빌드 + 계층 격리(Model/VM/Session 미참조)(G2).
- [ ] `UTetrisGameScreenWidget`/`UTetrisPauseWidget` 빌드 + Pause 위젯 Session 심볼 0건(G3).
- [ ] PC가 PrimaryGameLayout 생성·Game 화면 push, IA_Pause 토글 배선, 전체 테스트 110/110(G4).
- [ ] **PIE**: Pause 시 게임 정지 + 메뉴 오버레이 + **게임 입력 차단** + 키보드/게임패드 네비 + Resume/Back 복귀 + Restart/Quit 동작(G5).
- [ ] (학습) 사용자가 "raw UMG 대비 CommonUI가 표준화하는 3축(화면 스택·입력 모드·focus)"을 설명할 수 있다.

---

## 참고

- HANDOFF: `HANDOFF.md` (Menu System #15 Slice 1) — 게이트별 구현 명세 + 개념노트.
- 레퍼런스: Lyra `UIManagerSubsystem`/`PrimaryGameLayout`/`CommonActivatableWidget` 사용 패턴, Epic CommonUI 공식 문서.
- 후속 슬라이스: 메인 메뉴(MenuStack), GameOver 결과 화면(VM `GetGameState()` 구독 → Modal), 입력 힌트 위젯(CommonInput 장치 델리게이트).
