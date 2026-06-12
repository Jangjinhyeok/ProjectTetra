# EDITOR GUIDE — G4: WBP 제작 + Tetr.io 꾸미기 + PIE 검증

> **From**: Architect 세션 (RESULT.md Slice 2 검토 후 상세화)
> **For**: 사용자 (에디터 직접 작업 — C++ 무관, Builder 작업 아님)
> **Date**: 2026-06-09
> **선행**: C++ 게이트 G1~G3 완료 (빌드 ✅ / 110-110 테스트 ✅). 이 문서는 그 위에 얹는 **에디터 전용** 작업.
> **엔진**: UE 5.7

이 가이드는 RESULT.md의 G4 6줄 요약을 **클릭 단위로 따라할 수 있게** 펼친 것이다. C++는 이미 모든 배선이 끝났고, 사용자는 **WBP 2개 신규 + 기존 자산 2개 설정 확인 + PC 디폴트 2칸 채우기 + PIE 검증**만 하면 된다.

---

## 0. 시작 전 — 정확한 사실 (코드에서 추출, RESULT의 부정확분 정정)

### 0.1 실제 자산 경로 (RESULT 정정)
RESULT/HANDOFF는 `Content/UI/Widgets/`라 적었으나 **실제 프로젝트의 위젯 폴더는**:
```
Content/Blueprints/Widget/          ← 모든 WBP가 여기 있음
Content/Blueprints/Widget/Styles/   ← DA_ButtonStyle, DA_TextStyle
```
→ **신규 WBP_MainMenu / WBP_GameOver도 `Content/Blueprints/Widget/`에 만든다** (기존과 일관).

### 0.2 재사용할 기존 자산 (새로 만들지 말 것)
| 자산 | 위치 | G4에서 쓰임 |
|---|---|---|
| `WBP_CommonButton` | `Content/Blueprints/Widget/` | **버튼 4개의 베이스** — 빈 CommonButtonBase 대신 이걸 배치하면 DA_ButtonStyle 자동 적용 |
| `WBP_PauseMenu` | `Content/Blueprints/Widget/` | **구조 레퍼런스** — 메인 메뉴/게임오버는 PauseWidget과 동일 패턴. 열어서 구조를 참고/복제 |
| `WBP_PrimaryGameLayout` | `Content/Blueprints/Widget/` | z-order 확인 대상 (1.3) |
| `DA_ButtonStyle` / `DA_TextStyle` | `Content/Blueprints/Widget/Styles/` | 버튼/텍스트 스타일 |
| `BP_TetrisPlayerController` | `Content/Blueprints/` | 클래스 디폴트 2칸 채우기 (3장) |

### 0.3 가장 흔한 함정 — 부모 클래스 (Cast 의존성)
PC 코드는 push 후 `Cast<UTetrisMainMenuWidget>` / `Cast<UTetrisGameOverWidget>`로 다운캐스트해 버튼 델리게이트를 바인딩한다.
> **부모 클래스가 틀리면 화면은 뜨지만 버튼이 전부 무반응이 된다** (Cast 실패 → null → 바인딩 스킵, 로그도 안 남음).
- `WBP_MainMenu`의 부모 = 반드시 **`TetrisMainMenuWidget`**
- `WBP_GameOver`의 부모 = 반드시 **`TetrisGameOverWidget`**

### 0.4 BindWidget 이름 — 코드에서 추출한 정확한 사양
WBP의 위젯 이름이 아래와 **글자 하나까지 일치**해야 C++가 잡는다 (대소문자 구분).

**WBP_MainMenu** (부모 `TetrisMainMenuWidget`)
| 위젯 이름 | 타입 | 필수 여부 | 누락 시 |
|---|---|---|---|
| `StartButton` | CommonButtonBase 파생 (`WBP_CommonButton`) | **필수** (BindWidget) | 컴파일 에러 |
| `QuitButton` | CommonButtonBase 파생 (`WBP_CommonButton`) | **필수** (BindWidget) | 컴파일 에러 |

**WBP_GameOver** (부모 `TetrisGameOverWidget`)
| 위젯 이름 | 타입 | 필수 여부 | 누락 시 |
|---|---|---|---|
| `ScoreText` | TextBlock | 선택 (BindWidgetOptional) | 점수 미표시 (안전) |
| `LevelText` | TextBlock | 선택 (BindWidgetOptional) | 레벨 미표시 (안전) |
| `LinesText` | TextBlock | 선택 (BindWidgetOptional) | 줄수 미표시 (안전) |
| `RetryButton` | CommonButtonBase 파생 (`WBP_CommonButton`) | **필수** (BindWidget) | 컴파일 에러 |
| `MenuButton` | CommonButtonBase 파생 (`WBP_CommonButton`) | **필수** (BindWidget) | 컴파일 에러 |

> **입력 모드는 C++ 생성자에서 이미 `Menu`로 박혀 있다** — WBP에서 InputMode/Back 핸들러를 건드릴 필요 없음. 꾸미기만 하면 된다.

---

## 1. WBP_MainMenu 제작

### 1.1 위젯 생성
1. Content Browser → `Content/Blueprints/Widget/` 폴더.
2. 우클릭 → **User Interface → Widget Blueprint**.
3. 부모 클래스 선택 창에서 **`TetrisMainMenuWidget`** 검색·선택 (← 0.3 함정 주의. CommonActivatableWidget이나 UserWidget 아님).
4. 이름 = `WBP_MainMenu`.

### 1.2 위젯 트리 (최소 골격)
```
[Root]
└─ Overlay (fill)                       ← 정렬 컨테이너. CanvasPanel(절대배치) 쓰지 말 것
   ├─ Image (배경, fill)                ← 선택 (Tetr.io풍 배경)
   └─ SizeBox (WidthOverride ~600, 중앙)  ← 콘텐츠 최대폭 제한 (ultrawide에서 버튼이 화면 전체로 늘어나는 것 방지)
      └─ VerticalBox (Tetr.io풍 가로 바면 HorizontalBox)
         ├─ WBP_CommonButton  →  이름 변경: StartButton
         └─ WBP_CommonButton  →  이름 변경: QuitButton
```
- **root 컨테이너**: MainMenu는 런타임에 MenuStack 슬롯을 **fill로 채우는** 별도 애셋이다(PrimaryGameLayout의 디자인타임 자식 아님 — PC가 `PushWidgetToLayer`로 꽂음). 그래서 root는 풀스크린을 받으며, **CanvasPanel(절대좌표/앵커 배치) 대신 `Overlay`(정렬 슬롯)**를 둔다. 배경 없이 단순 세로 버튼만이면 `SizeBox`/`VerticalBox`를 바로 root에 둬도 무방.
- **SafeZone은 여기 넣지 않는다**: 이 프로젝트(PC 중심)는 SafeZone을 쓰지 않는다 — `SizeBox` 중앙 컬럼이라 콘텐츠가 가장자리에 안 붙는다. 콘솔/모바일로 갈 때만 이 콘텐츠 SizeBox를 국소 래핑한다(3.4).
- 팔레트에서 **`WBP_CommonButton`**(User Created 섹션)을 드래그해 배치. 빈 CommonButton/Button 쓰지 말 것.
- 배치 후 좌측 Hierarchy 패널에서 각 인스턴스 이름을 **정확히 `StartButton`, `QuitButton`**으로 변경 (더블클릭 rename).
- 버튼 라벨 텍스트(START / QUIT)는 각 WBP_CommonButton 내부 또는 위에 TextBlock으로. (CommonButton의 텍스트 노출 방식은 WBP_CommonButton 구현에 따름 — 기존 PauseMenu 버튼과 동일하게.)

### 1.3 Tetr.io 꾸미기 (자유 — C++ 무관)
구조만 맞으면 비주얼은 전부 자유. Tetr.io 홈 메뉴풍 권장 요소:
- 각 버튼을 **가로 풀폭 컬러 바**로 (HorizontalBox + Image 배경 + AccentColor).
- 좌측 픽셀 아이콘 + 제목(굵게) + 부제(작게) 2줄 TextBlock.
- **hover 시 바 확장** Widget Animation (CommonButton의 hover 상태에 연동).
- (선택) Settings/Records 등 시각 항목을 더 둬도 되지만 **동작 배선은 Start/Quit 2개뿐** — 추가 버튼은 클릭해도 무동작. 후속 슬라이스에서 배선.

### 1.4 컴파일
- 컴파일 → **에러 없어야 함**. "StartButton/QuitButton is not bound" 에러가 나면 이름 불일치 또는 타입 불일치 (0.4 표 재확인).

---

## 2. WBP_GameOver 제작

### 2.1 위젯 생성
1. `Content/Blueprints/Widget/` → Widget Blueprint 생성.
2. 부모 클래스 = **`TetrisGameOverWidget`** (← 0.3 함정).
3. 이름 = `WBP_GameOver`.

### 2.2 위젯 트리 (최소 골격)
```
[Root]
└─ Overlay (fill)                       ← 정렬 컨테이너 (CanvasPanel 아님)
   ├─ Image (dim 배경, fill, alpha ~0.6)  ← 죽은 게임 위를 덮는 반투명. 풀블리드(인셋 금지)
   └─ SizeBox (WidthOverride ~600, 중앙)  ← 결과 콘텐츠 폭 제한
      └─ VerticalBox (중앙 정렬)
         ├─ TextBlock         → 이름: ScoreText   ("Score")
         ├─ TextBlock         → 이름: LevelText   ("Level")
         ├─ TextBlock         → 이름: LinesText   ("Lines")
         ├─ WBP_CommonButton  → 이름: RetryButton
         └─ WBP_CommonButton  → 이름: MenuButton
```
- **MainMenu와 동일 원칙**: root는 `Overlay`(정렬), SafeZone 없음(3.4). 콘텐츠는 `SizeBox`로 폭 제한.
- **dim 배경 Image는 풀블리드**(fill, SizeBox 밖): 화면 전체를 덮어야 하므로 SizeBox 안에 넣지 않는다 — 인셋되면 가장자리에 게임 화면이 비친다. (콘솔/모바일에서 콘텐츠에 SafeZone을 국소로 걸 때도 이 dim은 SafeZone 밖 풀블리드로 둔다, 3.4.)
- 결과 TextBlock 3개 이름 = **`ScoreText` / `LevelText` / `LinesText`** (Optional이라 일부만 둬도 안전하지만, 결과 화면이므로 3개 다 권장).
- 버튼 2개 이름 = **`RetryButton` / `MenuButton`** (필수).
- TextBlock 디폴트 텍스트는 아무 값(예: "0") — 활성화 시 C++가 `NativeOnActivated`에서 실제 최종값으로 덮어쓴다.

### 2.3 점수 표시 형식 (참고 — 현재 동작)
- 현재 C++는 `FText::AsNumber(GetScore())` — 로케일 기본 숫자 표시 (천단위 구분 적용됨).
- HUD 위젯과 완전 동일 포맷으로 통일할지는 **RESULT 미해결 질문 #1** (C++ 1줄 후속) — 에디터 작업과 독립. 지금은 그대로 두면 됨.

### 2.4 꾸미기 + 컴파일
- 반투명 오버레이 + 결과 강조(큰 폰트, 점수 팝업 애니메이션) 권장.
- 컴파일 → RetryButton/MenuButton 바인딩 에러 없어야 함.

---

## 3. WBP_PrimaryGameLayout — z-order 확인 (신규 제작 아님, 점검만)

> 이미 Slice 1에서 만들어진 자산. C++ 헤더가 **4개 스택을 BindWidget으로 요구**하므로, 네 스택이 전부 존재하고 **그리는 순서(z-order)**가 맞는지 확인한다.

### 3.1 필수 스택 4개 (BindWidget 이름)
`WBP_PrimaryGameLayout`을 열어 Hierarchy에 아래 4개 `CommonActivatableWidgetStack`이 있는지 확인:
- `MenuStack`, `GameStack`, `GameMenuStack`, `ModalStack`
- 하나라도 없으면 컴파일 에러가 났을 것 (Slice 1이 PIE까지 됐다면 4개 다 있음). 없으면 추가.

### 3.2 z-order (핵심 — 메뉴/결과가 게임 위에 보이려면)
네 스택은 같은 **Overlay/CanvasPanel 안에 전부 fill로 겹쳐** 배치된다. UMG는 **Hierarchy에서 아래쪽 자식이 위에 그려진다**. 따라서 위→아래(=뒤→앞) 순서:
```
Overlay
├─ GameStack       (맨 뒤 — 게임 화면)
├─ MenuStack       (그 위 — 메인 메뉴가 게임을 가림)
├─ GameMenuStack   (그 위 — Pause가 게임 위에)
└─ ModalStack      (맨 앞 — GameOver가 전부 위에)
```
- **확인 포인트**: Slice 1에서 Pause(GameMenu)가 게임 위에 잘 떴다면 `GameMenu > Game`은 이미 OK. 이번엔 `MenuStack`(메인 메뉴)과 `ModalStack`(게임오버)이 위 순서에 맞게 있는지만 확인/조정.
- `enum EUILayer`의 선언 순서(Menu, Game, GameMenu, Modal)는 **스택 식별자일 뿐 z-order와 무관** — z-order는 오직 위 Hierarchy 배치 순서가 결정한다. 헷갈리지 말 것.

### 3.3 각 스택 슬롯 = Fill / Fill (풀스크린 점검) ★ 실전 함정
네 스택은 같은 Overlay에 겹쳐 두는데, **각 `OverlaySlot`의 Horizontal/Vertical Alignment가 둘 다 `Fill`**이어야 화면 전체를 채운다.

> **하나라도 `Fill`이 아니면**(예: 기본값 `Left`/`Top`으로 남으면) 그 스택에 띄운 화면은 **좌상단에 콘텐츠 desired size만큼만**(예: 내부 SizeBox가 600×300이면 딱 그 크기로) 뜬다. → "메인 메뉴가 왼쪽 위 구석에 작은 박스로만 보이고 게임을 안 가림"의 **전형적 원인**.
> 스택을 나중에 추가하면 정렬을 깜빡하기 쉽다(Overlay에 위젯을 드롭하면 항상 Fill로 들어오지 않음). **4개 전부** 확인할 것.

**점검/수정**: Hierarchy에서 각 Stack(`GameStack`/`MenuStack`/`GameMenuStack`/`ModalStack`) 선택 → Details → **Slot (Overlay Slot)** → `Horizontal Alignment = Fill`, `Vertical Alignment = Fill`.

### 3.4 SafeZone — 이 프로젝트는 생략 (필요 시 화면별 콘텐츠에 국소 적용)
> SafeZone을 레이아웃에서 일괄로 거는 패턴은 **UMG 제약상 성립하지 않는다.**

- **단일 자식 제약**: `SafeZone`(`USafeZone`)은 자식을 **1개만** 받는 ContentWidget이다(Overlay 같은 PanelWidget과 다름). 그래서 MenuStack/GameMenuStack/ModalStack **묶음을 통째로 감쌀 수 없다** — 안에 패널을 하나 더 끼워야 하는데, 그래도 다음 문제가 남는다.
- **dim 풀블리드 충돌**: GameOver dim 배경은 화면 끝까지 덮어야 하므로 SafeZone 안에 넣으면 가장자리가 인셋되어 비친다. dim과 콘텐츠가 **한 Modal 위젯 안에** 있어 스택 단위로 안/밖을 가를 수도 없다.

→ 결론: **안전영역은 "필요한 화면의 콘텐츠"에 국소로** 거는 것이지, 레이아웃에서 모든 스택에 일괄로 거는 게 아니다.

| 타겟 | SafeZone 처리 |
|---|---|
| **이 프로젝트 (PC 중심)** | **생략.** 각 화면이 `SizeBox` 중앙 컬럼이라 콘텐츠가 가장자리에 붙지 않음 → 오버스캔/노치 우려 없으면 둘 이유 없음 |
| 콘솔/모바일로 확장 시 | 안전영역이 필요한 **그 화면의 콘텐츠 SizeBox만 SafeZone으로 국소 래핑**(자식 1개라 제약과 안 부딪힘). dim 배경은 SafeZone 밖 풀블리드 유지 |

- 점검(콘솔/모바일만 해당): 뷰포트의 **Safe Zone 디버그 오버레이** 또는 디바이스 프리뷰로 가장자리 콘텐츠가 잘리는지 확인.

---

## 4. BP_TetrisPlayerController — 클래스 디폴트 2칸 채우기

1. `Content/Blueprints/BP_TetrisPlayerController` 열기 → **Class Defaults**.
2. Details 패널 **Category: `Tetris|UI`** 섹션에서:

| 프로퍼티 | 값 | 상태 |
|---|---|---|
| `PrimaryGameLayoutClass` | `WBP_PrimaryGameLayout` | 기존 (유지) |
| `GameScreenClass` | `WBP_GameScreen` | 기존 (유지) |
| `PauseWidgetClass` | `WBP_PauseMenu` | 기존 (유지) |
| **`MainMenuWidgetClass`** | **`WBP_MainMenu`** | **← 신규 지정** |
| **`GameOverWidgetClass`** | **`WBP_GameOver`** | **← 신규 지정** |

3. 컴파일·저장.
> 드롭다운에 `WBP_MainMenu`가 안 보이면? → WBP 부모 클래스가 `TetrisMainMenuWidget`(=`UTetrisActivatableWidget` 파생)이 아니어서 TSubclassOf 필터에 안 걸린 것. 0.3 재확인.

---

## 5. PIE 검증 시나리오

`L_Main` 맵에서 Play (PIE). 아래 6개 플로우를 순서대로 확인:

| # | 동작 | 기대 결과 |
|---|---|---|
| 1 | **PIE 시작** | 메인 메뉴가 게임 위에 표시. 게임 입력(이동/회전) 안 먹음. 게임패드/키보드로 버튼 네비게이션 됨 |
| 2 | **Start 클릭** | 메뉴 닫히고 보드+HUD로 플레이 시작. 게임 입력 정상 동작 |
| 3 | **일부러 Top-out** (블록 쌓아 게임오버) | GameOver 화면 + **최종 점수/레벨/줄** 표시 |
| 4 | GameOver에서 **Retry** | 결과 화면 닫히고 같은 자리에서 새 판 시작 |
| 5 | GameOver에서 **Menu** (다시 죽인 뒤) | 메인 메뉴로 복귀, 뒤 게임 시뮬 정지 |
| 6a | 플레이 중 **Pause → Quit** | **메인 메뉴 복귀** (앱 종료 아님!) |
| 6b | 메인 메뉴 **Quit** | PIE 종료 |

---

## 6. 트러블슈팅 (증상 → 원인)

| 증상 | 원인 / 조치 |
|---|---|
| 메뉴는 뜨는데 **버튼 클릭 무반응** | WBP 부모 클래스 틀림 (Cast 실패). `WBP_MainMenu`→`TetrisMainMenuWidget`, `WBP_GameOver`→`TetrisGameOverWidget` 확인 (0.3) |
| 컴파일 시 **"X is not bound"** | BindWidget 이름 불일치/타입 불일치. 0.4 표와 글자 단위 대조. 버튼은 CommonButtonBase 파생이어야 함 |
| 메뉴가 **왼쪽 위 구석에 작은 박스로만** 뜨고 게임을 안 가림 | WBP_PrimaryGameLayout에서 **해당 Stack의 OverlaySlot이 Fill/Fill이 아님**(Left/Top 등). 콘텐츠 desired size만큼만 표시됨. 4개 스택 전부 Fill로 (3.3) |
| 메뉴가 **게임 뒤에 숨음** | WBP_PrimaryGameLayout z-order. MenuStack이 GameStack보다 아래(Hierarchy)에 와야 함 (3.2) |
| **GameOver 점수가 안 보임** | (a) ScoreText 등 이름 불일치 — Optional이라 조용히 스킵됨, 이름 재확인. (b) VM 미연결 — HUD에서 점수가 정상 보였는지 먼저 확인 (같은 VM 경로 사용) |
| PC 디폴트 드롭다운에 **WBP 안 보임** | 부모가 UTetrisActivatableWidget 파생 아님 (4장 노트) |
| 메인 메뉴 Start 했는데 **게임 입력 여전히 안 먹음** | 메뉴 pop 실패 — MainMenuWidgetClass 지정 확인 + Output Log에 push 경고 확인 |
| **메뉴 안 뜸** (게임 바로 시작) | MainMenuWidgetClass 미지정. Output Log: `MainMenuWidgetClass 미지정` 경고 확인 (4장) |

### 디버깅 도구
- **Widget Reflector** (Window → Developer Tools): 런타임 위젯 트리·이름 확인. BindWidget 이름 검증에 최적.
- **Output Log**: PC가 미지정 클래스에 대해 `[Tetra] PlayerController: ... 미지정` 경고를 남김.

---

## 7. 완료 기준

- [ ] WBP_MainMenu 생성 (부모 `TetrisMainMenuWidget`, StartButton/QuitButton 바인딩, 컴파일 통과)
- [ ] WBP_GameOver 생성 (부모 `TetrisGameOverWidget`, Score/Level/Lines + Retry/Menu 바인딩, 컴파일 통과)
- [ ] WBP_PrimaryGameLayout z-order 확인 (Game < Menu < GameMenu < Modal)
- [ ] BP_TetrisPlayerController 디폴트에 MainMenuWidgetClass / GameOverWidgetClass 지정
- [ ] PIE 5장 시나리오 6개 전부 통과
- [ ] (선택) Tetr.io풍 꾸미기 + hover/전환 Widget Animation

→ 완료 후 동작 이상이나 추가 요구가 있으면 Architect 세션에 보고. 이상 없으면 슬라이스 2 종결.
