# HANDOFF — Menu System #15 (Slice 2) : 메인 메뉴 + GameOver 화면 (단일 맵 레이어 전환)

> **From**: Architect 세션
> **To**: Builder 세션
> **Date**: 2026-06-07
> **선행**: Slice 1 완료·머지(`bc9307f` + 주석수정 `2e5ff90`). CommonUI 인프라(PrimaryGameLayout 4레이어 + 입력 라우팅 + 3대 전역 셋업) + Pause 메뉴 동작 중.
> **로드맵**: README #3 "Common UI" 두 번째 슬라이스 — 게임 플로우(메뉴↔게임↔게임오버)를 **맵 로딩 없이 CommonUI 레이어 전환**으로 완성.

---

## 배경 / 결정

- **사용자 결정 (2026-06-07)**:
  - ① 맵 구성 = **A. 단일 맵 레이어 전환**. `L_Main` 한 맵에서 메뉴(Menu 레이어)↔게임(Game 레이어)을 CommonUI 스택으로 전환. `OpenLevel` 없음.
  - ② 메뉴 **비주얼 레퍼런스 = Tetr.io 홈 메뉴**(가로 컬러 바). 단 **비주얼/레이아웃/애니메이션은 사용자가 WBP에서 직접 꾸민다**(G4). Builder는 **구조(C++)까지만**. → 기존 C++/WBP 분담과 동일.
  - ③ 메뉴 **아키텍처 = Lyra CommonUI 패턴**(화면 스택·포커스·입력 라우팅). 구조는 레퍼런스 비주얼과 무관(가로 바든 세로 버튼이든 C++ 동일).
- **인프라 선결 = 불필요**: Slice 1에서 CommonUI 3대 전역 셋업(ViewportClient/ButtonStyle/InputData) + PrimaryGameLayout 4레이어(Menu/Game/GameMenu/Modal 스택) + `PushWidgetToLayer(EUILayer, …)`가 이미 완성. **신규 인프라·플러그인·Build.cs 변경 없음.** 본 슬라이스는 화면 위젯 2개 + PC 플로우 배선만.
- **현 플로우 vs 목표 플로우**:
  - 현재: PC가 BeginPlay에서 GameScreen만 Game 레이어에 push → 게임은 `tetra.StartGame` 콘솔로만 시작.
  - 목표: BeginPlay에서 **메인 메뉴**를 Menu 레이어에 띄움 → **Start 버튼**으로 게임 시작 → **GameOver** 시 결과 화면 → Retry/메뉴 복귀.
- **확인된 배선점**:
  - `UTetrisGameCore::OnStateChanged(EGameState Old, New)` 델리게이트 존재 → PC가 구독해 `New==GameOver` 감지.
  - Session: `StartGame(int64 Seed)`, `RestartGame()`, `SetPaused(bool)`, `GetGameCore()`, `GetScoring()`.
  - HUD VM self-resolve 패턴(`UTetrisHUDViewModel::ContextName`, `TetrisHUDWidget::ResolveViewModel` 참조) → GameOver 화면이 동일 방식으로 최종 점수 읽음.

---

## 목표

PIE에서 맵 로딩 시:
1. **메인 메뉴**가 뜬다(Menu 레이어, Tetr.io풍으로 꾸밀 수 있는 구조). 게임은 Idle.
2. **Start** → `StartGame` + 메뉴 닫힘 → 게임 입력 활성·플레이 시작. **Quit** → 앱 종료.
3. 플레이 중 **Top-out → GameOver** → 결과 화면(Modal 레이어)에 **최종 점수/레벨/줄** 표시. **Retry** → 새 판. **Menu** → 메인 메뉴 복귀.
4. **Pause의 Quit**(Slice 1: 앱 종료) → **"메뉴로 나가기"**로 의미 변경(RESULT Slice 1 노트 #1 반영).

기존 빌드·전체 테스트(110/110) 무회귀.

```
[BeginPlay] → MainMenu(Menu layer) ── Start ──▶ StartGame + pop MainMenu ──▶ 게임 플레이(Game layer)
     ▲                                                                            │
     │                                                                       Top-out
   Menu                                                                          ▼
     │◀──────────────── GameOver(Modal layer) ◀── PC가 OnStateChanged 관찰 ── EGameState::GameOver
     │                       │
     │                     Retry ──▶ RestartGame + pop
     └── (Pause→Quit to Menu)
```

---

## 제약 (수정 금지 / 불변식)

1. **Model 무변경** — `Board/`,`Block/`,`FSM/`,`System/`,`Core/`(`TetrisTypes.h`는 읽기만, 신규 타입 금지). HUD/Board **VM·바인더 무변경**.
2. **기존 위젯 C++ 무변경** — `TetrisActivatableWidget`(베이스), `TetrisPrimaryGameLayout`, `TetrisGameScreenWidget`, `TetrisPauseWidget`, `TetrisHUDWidget`, `TetrisBoardWidget`, `TetrisPieceWidget` 손대지 않는다. (PrimaryGameLayout은 이미 4레이어 + push API 보유 → 신규 화면도 기존 API로 push.)
   - **단 예외**: Pause "Quit→메뉴" 의미 변경은 **PC 측 바인딩만** 바꾼다(PauseWidget은 그대로 `OnQuitRequested` 발행, PC가 재해석). PauseWidget C++ 무변경.
3. **MVVM·계층 격리**:
   - **MainMenu** 위젯 = Model/VM/Session 미참조. 버튼은 네이티브 델리게이트로 명령만 발행(실행은 PC). Pause 위젯과 동일 패턴.
   - **GameOver** 위젯 = **HUD VM은 참조 허용**(결과를 표시하는 View이므로 — HUD 위젯과 동일하게 Collection self-resolve). 단 **Model/Session 미참조**, 버튼은 델리게이트로 위임.
4. **이벤트 주도** — NativeTick/폴링 금지. 화면 전환은 입력 엣지(Start/Pause) 또는 `OnStateChanged` 델리게이트. GameOver 점수는 활성화 시 1회 read(최종값 정적).
5. **데이터 주도·매직넘버 금지** — 레이어는 `EUILayer`(기존). 위젯 클래스는 `TSubclassOf` UPROPERTY 주입. 하드코딩 금지.
6. **신규 C++ 파일 BOM 제거**.
7. **컨벤션** — 한국어 "왜" 주석 / 영어 식별자 / Epic 접두사 / `Slot` 식별자 회피.

**수정 허용 파일**(게이트 명시분만):
- `Source/ProjectTetra/Input/TetrisPlayerController.h`/`.cpp`
- 신규: `UI/Views/TetrisMainMenuWidget.*`, `UI/Views/TetrisGameOverWidget.*`

**수정 금지 영역**: Model 전 계층, HUD/Board VM·바인더, 기존 모든 위젯 C++, Build.cs/uproject/Config(인프라 변경 없음), 테스트(신규만, 기존 무수정).

---

## 게이트 분해

### G1 — 메인 메뉴 화면 (C++)

> **개념노트 (구현 전 읽기)**: commonui.md `Detailed Design` 1번(Activatable=화면) + 4번(입력 모드 선언). 메인 메뉴는 Pause와 **같은 패턴**(Menu 모드 activatable + CommonButton + 델리게이트)이라 Slice 1 복습 성격. 새 포인트는 "**Menu 레이어가 Game 레이어 위에 떠 게임을 가린다**"는 레이어 z-order 개념.

**파일(신규)**: `UI/Views/TetrisMainMenuWidget.h`/`.cpp`

- `UTetrisMainMenuWidget : UTetrisActivatableWidget`. 생성자 `InputModeOverride = Menu`. (`bIsBackHandler`는 false — 메인 메뉴에서 Back은 무동작, 앱 종료는 Quit 버튼으로 명시.)
- `BindWidget` 버튼: `UCommonButtonBase* StartButton; UCommonButtonBase* QuitButton;`
  - (Tetr.io는 5개 항목이지만 Slice 2 **액션은 Start/Quit 2개**. Settings 등 추가 항목은 사용자가 WBP에서 시각적으로 더 둘 수 있으나, 동작 배선은 후속. 구조는 N개 확장 가능.)
- 네이티브 델리게이트: `FSimpleMulticastDelegate OnStartRequested; OnQuitRequested;`(public).
- `NativeOnInitialized()`에서 버튼 `OnClicked().AddUObject` → 대응 델리게이트 Broadcast 중계(널 가드). PauseWidget과 동일 구조.
- Model/VM/Session 미참조.

**검증**: 빌드 성공 + 격리 grep(Model/VM/Session 심볼 0건). 동작은 G4 PIE.

---

### G2 — GameOver 결과 화면 (C++)

> **개념노트 (구현 전 읽기)**: commonui.md `Detailed Design` 3번(레이어 — Modal) + viewmodel.md(View가 VM을 self-resolve). GameOver는 **메뉴가 아니라 결과 View**라 HUD VM을 읽는다(HUD 위젯과 같은 자격). "메뉴 위젯은 VM 미참조 / 결과 View는 VM 참조"의 경계가 학습 포인트.

**파일(신규)**: `UI/Views/TetrisGameOverWidget.h`/`.cpp`

- `UTetrisGameOverWidget : UTetrisActivatableWidget`. 생성자 `InputModeOverride = Menu`. (`bIsBackHandler` false — 죽은 게임으로 Back 복귀 금지. 종료는 Retry/Menu 버튼으로만.)
- **결과 표시(VM self-resolve)**: HUD 위젯과 동일 경로로 `UTetrisHUDViewModel`을 Collection(`ContextName`)에서 resolve. `BindWidgetOptional` 텍스트:
  - `UTextBlock* ScoreText / LevelText / LinesText`(Optional — WBP가 일부만 둬도 안전).
  - `NativeOnActivated()`에서 VM resolve → `GetScore()/GetLevel()/GetLines()`를 1회 read → SetText. (GameOver 시점 최종값이라 구독 불필요. VM null이면 no-op.)
  - VM resolve 헬퍼는 `TetrisHUDWidget::ResolveViewModel` 로직을 참조해 구현(소폭 중복 — 설계노트 1).
- **버튼**: `BindWidget UCommonButtonBase* RetryButton / MenuButton;` + 네이티브 델리게이트 `OnRetryRequested; OnMenuRequested;` + `NativeOnInitialized` 중계(PauseWidget 패턴).
- Model/Session 미참조(VM만).

**검증**: 빌드 성공 + 격리 grep(Session/Model 심볼 0건, VM·MVVM 컬렉션 경로만). 동작은 G4 PIE.

---

### G3 — PlayerController 플로우 배선 (C++)

> **개념노트 (구현 전 읽기)**: commonui.md `Detailed Design`의 "계층/소유 구조"(PC가 PrimaryGameLayout 소유) + 4번 프레이밍(메뉴는 명령 발행, PC가 실행). 핵심 학습: "**화면 push/pop만으로 입력 모드가 자동 전환**된다 — 메뉴 pop 시 Game 레이어가 최상단 활성이 되어 게임 입력이 자동 복귀"(Slice 1 Pause와 동일 메커니즘을 메뉴↔게임 전환에 확장).

**파일**: `Input/TetrisPlayerController.h`/`.cpp`

- **신규 UPROPERTY**(`Tetris|UI`, EditDefaultsOnly):
  - `TSubclassOf<UTetrisActivatableWidget> MainMenuWidgetClass;`
  - `TSubclassOf<UTetrisActivatableWidget> GameOverWidgetClass;`
- **신규 Transient**: `TObjectPtr<UTetrisMainMenuWidget> ActiveMainMenu;`, `TObjectPtr<UTetrisGameOverWidget> ActiveGameOver;`
- **BeginPlay 변경**: 기존 GameScreen push(Game 레이어)는 유지하고, 그 위에 **메인 메뉴 진입** 추가 — `ShowMainMenu()` 호출. (게임은 Idle 상태로 메뉴 뒤에 대기.)
- **GameOver 관찰**: BeginPlay(또는 GetSession 직후)에서 `Session->GetGameCore()->OnStateChanged.AddUObject(this, &…::HandleGameStateChanged)` 구독. `Deinitialize`/`EndPlay`가 아니라 PC `EndPlay`에서 해제(GameCore 수명 = World, PC도 동일하나 안전 해제 권장).
- **신규 함수**:
  - `void ShowMainMenu()` — 이미 떠 있으면 무동작. `ActiveMainMenu = Cast<…>(PrimaryLayout->PushWidgetToLayer(EUILayer::Menu, MainMenuWidgetClass))` → `OnStartRequested → HandleStartRequested`, `OnQuitRequested → HandleQuitRequested` 바인딩.
  - `void HandleStartRequested()` — `Session->StartGame(MakeSeed())` → `PrimaryLayout->RemoveWidget(*ActiveMainMenu)` + `ActiveMainMenu=nullptr`. (메뉴 pop → Game 레이어 최상단 → 게임 입력 자동 활성.)
  - `void HandleQuitRequested()` — `ConsoleCommand(TEXT("quit"))`(앱 종료). *(메인 메뉴 Quit = 앱 종료. Pause Quit과 핸들러 분리.)*
  - `void HandleGameStateChanged(EGameState Old, EGameState New)` — `if (New==EGameState::GameOver && !ActiveGameOver) ShowGameOver();`
  - `void ShowGameOver()` — `ActiveGameOver = Cast<…>(PrimaryLayout->PushWidgetToLayer(EUILayer::Modal, GameOverWidgetClass))` → `OnRetryRequested → HandleRetryRequested`, `OnMenuRequested → HandleReturnToMenu` 바인딩.
  - `void HandleRetryRequested()` — `Session->RestartGame()` → GameOver pop + `ActiveGameOver=nullptr`. (게임 레이어 그대로, 새 판 진행.)
  - `void HandleReturnToMenu()` — GameOver pop(있으면) + `ActiveGameOver=nullptr` → `Session->SetPaused(true)`(뒤 시뮬 정지) → `ShowMainMenu()`. *(Retry/Menu 공통: 화면 정리 후 분기.)*
  - `int64 MakeSeed() const` — `FDateTime::Now().GetTicks()` 반환(플레이마다 변주, 시드당 결정성 유지). *(Why: 결정성은 "시드 고정 시 동일 결과"라 시드 자체는 매판 달라도 됨.)*
- **Pause Quit 의미 변경**(제약 §2 예외): Slice 1의 `HandleQuitRequested`(Pause용, 앱 종료)를 **메뉴 복귀로 변경**. Pause의 `OnQuitRequested` 바인딩을 **`ResumePause`로 닫은 뒤 `HandleReturnToMenu`로** 잇는다. 즉 Pause→Quit = "Pause 닫기 + 시뮬 정지 + 메인 메뉴". (메인 메뉴 Quit=앱 종료와 핸들러를 분리할 것 — 이름 충돌 주의: 메인 메뉴용은 `HandleQuitRequested`, Pause용은 `HandleReturnToMenu` 재사용.)

**검증**: 빌드 성공 + 전체 테스트 110/110(런타임 위젯·플로우는 테스트 무관 → 회귀 없음). 기존 입력 핸들러(Move/Rotate/Drop/Hold/Pause 토글) 로직 무변경 확인. 동작은 G4 PIE.

---

### G4 — WBP 제작 + Tetr.io 꾸미기 + PIE (사용자/에디터)

> **개념노트 (구현 전 읽기)**: commonui.md `Detailed Design` 5번(CommonButton 스타일) + `Edge Cases`. 이 게이트가 **사용자의 비주얼 작업** — 구조(G1~G3)가 받쳐주는 위에서 Tetr.io풍으로 꾸민다. CommonButton은 스타일 에셋 필요(Slice 1 `DA_ButtonStyle` 재사용/확장).

1. **WBP_MainMenu** (`Content/UI/Widgets/`): 부모 `UTetrisMainMenuWidget`. `CommonButtonBase` 파생 버튼 — 이름 정확히 `StartButton`/`QuitButton`. **Tetr.io 꾸미기**: 각 버튼을 가로 풀폭 바로, 내부에 픽셀 약어 아이콘 + 제목 + 부제 TextBlock + 항목별 AccentColor + 배경 이미지 + hover 확장 Widget Animation. (전부 WBP/디자이너 작업 — C++ 무관.)
2. **WBP_GameOver**: 부모 `UTetrisGameOverWidget`. 결과 TextBlock — 이름 정확히 `ScoreText`/`LevelText`/`LinesText`(일부만 둬도 BindWidgetOptional이라 안전). 버튼 `RetryButton`/`MenuButton`. 반투명 오버레이 + 결과 강조 연출 권장.
3. **WBP_PrimaryGameLayout z-order 확인/조정**: 스택 쌓는 순서(뒤→앞) = **GameStack → MenuStack → GameMenuStack → ModalStack**. 메인 메뉴(Menu)·GameOver(Modal)가 게임(Game) **위에** 보여야 한다. (Slice 1에서 GameMenu가 Game 위였으면, MenuStack을 그 사이/위로, ModalStack을 최상단으로.)
4. **BP_TetrisPlayerController**: Class Defaults `Tetris|UI` → `MainMenuWidgetClass=WBP_MainMenu`, `GameOverWidgetClass=WBP_GameOver`. (기존 PrimaryGameLayoutClass/GameScreenClass/PauseWidgetClass 유지.)
5. **PIE** 검증:
   - 시작 시 **메인 메뉴** 표시(게임 입력 안 먹음, 게임패드/키보드 네비). **Start** → 메뉴 닫히고 보드+HUD로 플레이 시작.
   - 일부러 Top-out → **GameOver 화면 + 최종 점수/레벨/줄** 표시. **Retry** → 새 판. **Menu** → 메인 메뉴 복귀.
   - 플레이 중 **Pause → Quit** → 메인 메뉴 복귀(앱 종료 아님). 메인 메뉴 **Quit** → 앱(PIE) 종료.
6. 미동작 체크: BindWidget 이름 일치(Widget Reflector), z-order(메뉴가 게임 뒤로 가지 않는지), PC 디폴트 2클래스 지정, 버튼 Style 지정.

---

## 설계 노트 / 트레이드오프 (Architect 의도)

1. **GameOver의 VM-resolve 소폭 중복** — `ResolveViewModel` 로직이 HUD 위젯과 GameOver 위젯 두 곳. MVP 허용. 후속으로 공통 베이스/헬퍼(예: VM-resolve를 `UTetrisActivatableWidget`나 별도 mixin으로)로 통합 검토(tech-debt).
2. **메인 메뉴 Start 시드 = 현재시각** — 매판 변주 + 시드당 결정성 유지. 리플레이/공유 기능이 생기면 "시드 입력" UI로 확장(후속).
3. **단일 맵 레이어 전환(맵 로딩 없음)** — 사용자 결정 A. 장점: 전환 즉시·끊김 없음, CommonUI 스택만으로 플로우 증명(아키텍처 포커스). 멀티/대형 씬으로 가면 별도 맵+로딩 화면이 다음 단계.
4. **Pause Quit = 메뉴 복귀로 변경** — Slice 1 RESULT 노트 #1 해소. 단일 플레이 + 메인 메뉴 존재 맥락에선 "앱 종료"보다 자연스럽다. 앱 종료는 메인 메뉴 Quit로 일원화.
5. **메뉴 버튼 C++ 클래스 미도입** — Tetr.io 항목 해부(아이콘/제목/부제/색)는 **순수 WBP**로 가능(고정 텍스트). 데이터 주도 엔트리 리스트가 필요해지기 전까진 C++ 버튼 파생을 두지 않는다(simplicity-first). 사용자 꾸미기 자유 + 구조 단순.

---

## 비기능 요건

- 주석 "왜" 한국어 / 식별자 영어 / Epic 접두사.
- 핫패스 무관(이벤트 주도). 화면 전환은 입력 엣지·상태 델리게이트 1회.
- 결정성 영향 없음(시드만 변주, 시뮬은 SetPaused로 정지).

---

## 게이트 전제 순서

G1(메인 메뉴) → G2(GameOver, VM resolve) → G3(PC 플로우, G1·G2 의존) → G4(WBP+꾸미기+PIE, 사용자). 각 게이트 독립 빌드 가능. **각 게이트 완료마다 사용자 보고 후 다음 진행**(일괄 금지).

→ 완료 후 `RESULT.md` 갱신하여 Architect 검토 요청.
