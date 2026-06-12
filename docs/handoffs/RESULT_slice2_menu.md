# RESULT — Menu System #15 (Slice 2) : 메인 메뉴 + GameOver 화면

> **From**: Builder 세션
> **To**: Architect 세션
> **Date**: 2026-06-08
> **대상 HANDOFF**: `HANDOFF.md` (Slice 2, 단일 맵 레이어 전환)
> **엔진**: UE 5.7 / VS2022 14.44 toolchain
> **Status**: C++ 게이트 G1~G3 ✅ 완료. 빌드 성공 + 전체 테스트 **110/110, 0 실패**(무회귀). G4(WBP+PIE)는 사용자/에디터 작업 대기.

> ※ 이전 내용(Slice 1 RESULT)은 머지·종결되어 본 Slice 2 결과로 교체함.

---

## 게이트 완료 상태

| 게이트 | 내용 | 빌드 | 검증 | 상태 |
|---|---|---|---|---|
| **G1** | 메인 메뉴 화면 (C++) | ✅ | 격리 grep ✅(Model/VM/Session 0건) | ✅ completed |
| **G2** | GameOver 결과 화면 (C++, HUD VM self-resolve) | ✅ | 격리 grep ✅(Session/Model 0건) | ✅ completed |
| **G3** | PlayerController 플로우 배선 (C++) | ✅ | 110/110 + 기존 입력 핸들러 무변경 | ✅ completed |
| **G4** | WBP 제작 + Tetr.io 꾸미기 + PIE | — | — | ⏳ 사용자/에디터 작업 (가이드 하단) |

- **빌드**: `ProjectTetraEditor | Win64 | Development` → 각 게이트 `Result: Succeeded`. 신규/수정 cpp는 유니티 제외로 단독 컴파일 — 경고 0.
- **테스트**: `Automation RunTests Tetris` → **Started=110 / Success=110 / Failed=0** (`**** TEST COMPLETE. EXIT CODE: 0 ****`). 기준선 유지.

---

## 변경 파일 목록

### 신규 (4파일, BOM 없는 UTF-8)

| 파일 | 라인 | 내용 |
|---|---|---|
| `UI/Views/TetrisMainMenuWidget.h` | 48 | `UTetrisMainMenuWidget : UTetrisActivatableWidget`. `BindWidget` Start/Quit `UCommonButtonBase` + `FSimpleMulticastDelegate OnStartRequested/OnQuitRequested`. |
| `UI/Views/TetrisMainMenuWidget.cpp` | 38 | 생성자 `InputModeOverride=Menu`/`NoCapture`, `bIsBackHandler` 기본 false(메인 메뉴는 Back 무동작). `NativeOnInitialized` 버튼 OnClicked→델리게이트 중계(널 가드). PauseWidget 패턴. |
| `UI/Views/TetrisGameOverWidget.h` | 65 | `UTetrisGameOverWidget : UTetrisActivatableWidget`. `BindWidgetOptional` Score/Level/Lines TextBlock + `BindWidget` Retry/Menu 버튼 + `OnRetryRequested/OnMenuRequested`. |
| `UI/Views/TetrisGameOverWidget.cpp` | 87 | `NativeOnActivated`에서 HUD VM self-resolve→`GetScore/GetLevel/GetLines` 1회 read→`SetText`(VM null이면 no-op). `ResolveViewModel`은 `TetrisHUDWidget` 경로 복제(설계노트 1). 버튼 중계. |

### 수정 (2파일)

**`Input/TetrisPlayerController.h`** (148줄)
- forward decl: `UTetrisMainMenuWidget`, `UTetrisGameOverWidget`, `enum class EGameState : uint8`
- `EndPlay(const EEndPlayReason::Type)` 오버라이드 선언
- UI UPROPERTY(`EditDefaultsOnly, Tetris|UI`): `MainMenuWidgetClass`, `GameOverWidgetClass`
- Transient: `ActiveMainMenu`, `ActiveGameOver`
- 신규 함수 선언 7개: `ShowMainMenu` / `HandleStartRequested` / `HandleGameStateChanged` / `ShowGameOver` / `HandleRetryRequested` / `HandleReturnToMenu` / `MakeSeed`
- `HandleQuitRequested` 주석을 "메인 메뉴 Quit=앱 종료"로 명시

**`Input/TetrisPlayerController.cpp`** (397줄)
- include 3개: `TetrisMainMenuWidget.h`, `TetrisGameOverWidget.h`, `Misc/DateTime.h`
- `BeginPlay`: GetSession 직후 `GameCore->OnStateChanged` 구독 + UI 루트 생성 후 `ShowMainMenu()`
- `EndPlay` 구현: `OnStateChanged.RemoveAll(this)` (dangling 콜백 방지)
- `OnPauseToggle`: Pause `OnQuitRequested` 바인딩을 `HandleQuitRequested`(앱 종료)→`ResumePause`+`HandleReturnToMenu`(메뉴 복귀)로 변경 (§2 예외, PauseWidget C++ 무변경)
- 신규 함수 구현 7개

**수정 금지 영역 무변경**: Model 전 계층, HUD/Board VM·바인더, 기존 모든 위젯 C++(PauseWidget 포함), Build.cs/uproject/Config, 기존 테스트.

---

## 핸드오프 준수 평가

| 제약 / 요구 | 준수 | 비고 |
|---|---|---|
| §1 Model 무변경 | ✅ | `EGameState` 읽기만(forward decl), 신규 타입 0 |
| §2 기존 위젯 C++ 무변경 | ✅ | Pause는 PC 측 바인딩만 재해석, 위젯 C++ 무변경 |
| §3 MainMenu = Model/VM/Session 미참조 | ✅ | grep 0건(매치는 주석 설명뿐) |
| §3 GameOver = VM만 참조 | ✅ | grep 0건, MVVM 컬렉션 경로만(HUD 위젯과 동일 자격) |
| §4 이벤트 주도 | ✅ | 전환=입력 엣지+OnStateChanged, GameOver 점수=활성화 1회 read, Tick/폴링 없음 |
| §5 데이터 주도 | ✅ | 위젯 클래스 TSubclassOf 주입, 레이어는 기존 EUILayer, 매직넘버 없음 |
| §6 신규 BOM 제거 | ✅ | 4파일 `2f 2f 20`(BOM 없음) |
| §7 컨벤션 | ✅ | 한국어 "왜" 주석, 영어 식별자, Epic 접두사, `Slot` 미사용 |
| 빌드/테스트 무회귀 | ✅ | 빌드 Succeeded, 110/110 |

> **빌드 노트**: `C4996 'FieldId.h' deprecated` 경고 2건은 기존 HUD 코드(`TetrisHUDWidget.h`)의 선행 경고. 본 슬라이스 신규 파일과 무관(GameOver는 VM 간접 접근, 해당 헤더 직접 include 없음).

---

## 발견된 이슈 / 설계 노트

1. **[ordering] Pause Quit의 2-바인딩 순서 의존성** — HANDOFF G3 명세대로 `OnQuitRequested`에 `ResumePause`→`HandleReturnToMenu`를 **순차 바인딩**. 정상 동작은 멀티캐스트가 등록 순서대로 invoke됨에 의존(ResumePause가 먼저 위젯 닫고 `SetPaused(false)`, 이어 HandleReturnToMenu가 `SetPaused(true)`+메뉴). UE `TMulticastDelegate`는 add 순서대로 호출하므로 실무상 안전하나, 순서 뒤바뀌면 "메뉴 뒤 시뮬이 계속 도는" 버그.
   → **제안**: 후속에서 Pause Quit 전용 단일 래퍼 핸들러로 명시화하면 fragility 제거(tech-debt, 동작 영향 없음).

2. **[중복] GameOver의 VM-resolve 코드 중복** — 설계노트 1대로 `ResolveViewModel` 로직이 HUD/GameOver 두 곳(MVP 허용). 후속 공통 베이스/헬퍼 통합 검토.

3. **[설계대로] `MakeSeed` 비결정 소스** — `FDateTime::Now().GetTicks()` 매판 변주(설계노트 2). "시드 고정 시 동일 결과" 결정성 정의 유지. 테스트 무영향.

4. **[관찰·정상] GameOver→Retry 시 StateChanged 재진입** — `HandleRetryRequested`의 `RestartGame()`이 `GameOver→Spawn` 전이를 발행하나, `HandleGameStateChanged`는 `New==GameOver`에만 반응 → 무시. `ActiveGameOver` 가드로 중복 push도 차단. 확인 완료, 조치 불필요.

---

## 미해결 질문 (Architect 판단 요청)

1. **GameOver 점수 포맷** — 현재 `FText::AsNumber(GetScore())`(HANDOFF G2 명세 `GetScore()` 준수). HUD 위젯은 점수에 로케일 천단위 포맷 `GetScoreText()` 사용. 결과 화면도 천단위로 통일할지? (통일 시 G2 cpp 1줄 변경.)
2. 이슈 #1(Pause Quit 래퍼 명시화)을 이번 슬라이스에서 정리할지 / 별도 tech-debt로 둘지.

---

## 다음 단계 — G4 에디터/사용자 작업 가이드 (C++ 무관)

1. **WBP_MainMenu** (`Content/UI/Widgets/`): 부모 `UTetrisMainMenuWidget`. `CommonButtonBase` 파생 버튼 이름 **정확히** `StartButton`/`QuitButton`. Tetr.io풍 가로 바·아이콘·hover 애니메이션 자유.
2. **WBP_GameOver**: 부모 `UTetrisGameOverWidget`. TextBlock `ScoreText`/`LevelText`/`LinesText`(일부만 둬도 `BindWidgetOptional` 안전), 버튼 `RetryButton`/`MenuButton`. 반투명 오버레이 권장.
3. **WBP_PrimaryGameLayout z-order**(뒤→앞): `GameStack → MenuStack → GameMenuStack → ModalStack`. 메뉴(Menu)·GameOver(Modal)가 게임(Game) **위**.
4. **BP_TetrisPlayerController** Class Defaults `Tetris|UI`: `MainMenuWidgetClass=WBP_MainMenu`, `GameOverWidgetClass=WBP_GameOver` (기존 3클래스 유지).
5. **PIE 검증**:
   - 시작 시 메인 메뉴(게임 입력 차단, 게임패드/키보드 네비) → **Start** → 메뉴 닫히고 플레이
   - Top-out → GameOver + 최종 점수/레벨/줄 → **Retry**(새 판) / **Menu**(메인 메뉴 복귀)
   - 플레이 중 **Pause→Quit** → 메인 메뉴 복귀(앱 종료 아님) / 메인 메뉴 **Quit** → PIE 종료
6. **미동작 체크**: BindWidget 이름 일치(Widget Reflector), z-order, PC 디폴트 2클래스, 버튼 Style 지정.

---

→ **Architect 세션에서 본 RESULT.md를 검토**하고, 미해결 질문 1·2 결정 + G4 진행 여부를 판단해 주세요.
