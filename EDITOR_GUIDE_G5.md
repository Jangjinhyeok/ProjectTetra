# EDITOR GUIDE — G5: 버튼 Hover Zoom + Settings 화면 WBP 제작 + PIE 검증

> **From**: Builder 세션 (RESULT.md G5 6줄 요약을 클릭 단위로 펼침)
> **For**: 사용자 (에디터 직접 작업 — C++ 무관, Builder 작업 아님)
> **Date**: 2026-06-12
> **선행**: C++ 게이트 G1~G4 완료 (빌드 ✅ / 110-110 테스트 ✅). 이 문서는 그 위에 얹는 **에디터 전용** 작업.
> **엔진**: UE 5.7
> **명세 출처**: `HANDOFF.md` G5 + `EDITOR_GUIDE_G4.md`(메뉴 슬라이스, 동일 프로젝트 관례 재사용).

이번 게이트는 **두 개의 독립 작업**이다:
- **A. Hover Zoom** — `WBP_CommonButton`을 새 C++ 베이스로 reparent하고 `HoverZoom` 애니메이션을 author. → 이 공통 버튼을 쓰는 **모든 버튼**(메인/Pause/GameOver/Settings)에 일괄 적용.
- **B. Settings 화면** — `WBP_SettingsMenu` 신규 + `WBP_MainMenu`에 진입 버튼 추가 + PC 디폴트 1칸.

A와 B는 서로 의존하지 않으니 순서는 자유. 단 PIE(5장)는 둘 다 끝낸 뒤.

---

## 0. 시작 전 — 정확한 사실 (코드에서 추출)

### 0.1 자산 경로 (G4 가이드와 동일)
```
Content/Blueprints/Widget/          ← 모든 WBP (WBP_CommonButton, WBP_MainMenu, WBP_PauseMenu …)
Content/Blueprints/Widget/Styles/   ← DA_ButtonStyle, DA_TextStyle
Content/Blueprints/BP_TetrisPlayerController
```
→ **신규 WBP_SettingsMenu도 `Content/Blueprints/Widget/`에 만든다**(기존과 일관).

### 0.2 가장 흔한 함정 — 부모 클래스 (Cast 의존성)
PC 코드는 push 후 `Cast<UTetrisSettingsWidget>`로 다운캐스트해 Back 델리게이트를 바인딩한다.
> **부모 클래스가 틀리면 화면은 뜨지만 Back 복귀/버튼이 무반응**(Cast 실패 → null → 바인딩 스킵).
- `WBP_SettingsMenu`의 부모 = 반드시 **`TetrisSettingsWidget`**
- `WBP_CommonButton`의 부모 = 반드시 **`TetrisButtonBase`** (reparent — 1장)

### 0.3 BindWidget / BindWidgetAnim 이름 — 코드에서 추출한 정확한 사양
WBP의 이름이 아래와 **글자 하나까지 일치**해야 C++가 잡는다 (대소문자 구분).

**WBP_CommonButton** (부모 `TetrisButtonBase`)
| 이름 | 종류 | 필수 여부 | 누락 시 |
|---|---|---|---|
| `HoverZoom` | **Widget Animation** | 선택 (BindWidgetAnimOptional) | 연출 없음(기능은 정상, 안전) |

**WBP_SettingsMenu** (부모 `TetrisSettingsWidget`)
| 위젯 이름 | 타입 | 필수 여부 | 누락 시 |
|---|---|---|---|
| `DASValueText` | TextBlock | 선택 (BindWidgetOptional) | DAS 값 미표시(안전) |
| `ARRValueText` | TextBlock | 선택 (BindWidgetOptional) | ARR 값 미표시(안전) |
| `DASMinusButton` | CommonButtonBase 파생(`WBP_CommonButton`) | **필수** (BindWidget) | 컴파일 에러 |
| `DASPlusButton` | CommonButtonBase 파생 | **필수** | 컴파일 에러 |
| `ARRMinusButton` | CommonButtonBase 파생 | **필수** | 컴파일 에러 |
| `ARRPlusButton` | CommonButtonBase 파생 | **필수** | 컴파일 에러 |
| `BackButton` | CommonButtonBase 파생 | **필수** | 컴파일 에러 |

**WBP_MainMenu** (부모 `TetrisMainMenuWidget`, 기존 자산)
| 위젯 이름 | 타입 | 필수 여부 | 누락 시 |
|---|---|---|---|
| `SettingsButton` | CommonButtonBase 파생(`WBP_CommonButton`) | 선택 (BindWidgetOptional) | Settings 진입 불가(안전, 컴파일은 통과) |

> **입력 모드/Back 핸들러는 C++ 생성자에 이미 박혀 있다**(`InputModeOverride=Menu`, `bIsBackHandler=true`). WBP에서 건드릴 필요 없음 — 레이아웃/꾸미기만.

### 0.4 조정 사양 (참고 — C++가 강제)
| 항목 | 값 | 비고 |
|---|---|---|
| DAS 스텝 | ± **5 ms** | `DASValueText`에 `"%d ms"`로 표시 |
| ARR 스텝 | ± **1 ms** | `ARRValueText`에 `"%d ms"`로 표시 |
| DAS clamp | 0 – 500 ms | 서브시스템이 강제(버튼 계속 눌러도 범위 밖 안 감) |
| ARR clamp | 0 – 200 ms | 〃 |
| 기본값 | DAS 100 / ARR 17 | SaveGame 없을 때 |
> 값 텍스트는 **활성화 시 + ± 클릭 시**에만 갱신(이벤트 주도, Tick 아님). 디폴트 텍스트는 아무 값이어도 됨.

---

## A. Hover Zoom (WBP_CommonButton)

### 1. 부모 클래스 reparent
1. `Content/Blueprints/Widget/WBP_CommonButton` 열기.
2. 상단 메뉴 **File → Reparent Blueprint** (또는 Class Settings → Parent Class).
3. 새 부모 = **`TetrisButtonBase`** 검색·선택.
4. 컴파일 → 에러 없어야 함.
> reparent 후에도 기존 스타일(DA_ButtonStyle)·구조는 그대로 유지된다. `UTetrisButtonBase`는 `UCommonButtonBase`를 상속하므로 깨지는 것 없음.

### 2. HoverZoom 애니메이션 author
1. WBP_CommonButton 하단 **Animations** 패널 → **+ Animation** → 이름 **정확히 `HoverZoom`**.
2. 줌 대상 위젯 선택: 보통 버튼 **root 패널**(또는 라벨을 감싼 컨테이너). 그 위젯을 트랙에 추가.
3. **RenderTransform → Scale** 트랙 키프레임:
   | 시간 | Scale |
   |---|---|
   | 0.00s | (1.0, 1.0) |
   | ~0.12s | (1.1, 1.1) — 취향껏 1.05~1.15 |
4. **(중요) Pivot 중앙**: 줌이 좌상단 기준으로 튀지 않게, 대상 위젯 Details → **Render Transform → Pivot = (0.5, 0.5)**.
5. ease(보간)는 cubic 권장(딱딱한 linear 회피). 키 우클릭 → interpolation.
> **C++ 동작**: hover 진입 시 `PlayAnimationForward(HoverZoom)`, 이탈 시 `PlayAnimationReverse(HoverZoom)`. 따라서 애니메이션은 **0s=원본 → 끝=확대** 한 방향으로만 author하면 된다(원복은 C++가 reverse로 처리). 별도 "축소" 애니메이션 불필요.

### 3. 일괄 적용 확인
- WBP_CommonButton은 메인/Pause/GameOver/Settings 버튼의 베이스이므로 **한 번 author하면 전 버튼에 적용**된다(중복 작업 없음).
- 버튼별 차등 연출이 필요하면 그 버튼만 파생 WBP를 만들어 `HoverZoom`만 교체(이번 범위 밖).

---

## B. Settings 화면

### 4. WBP_SettingsMenu 제작
1. `Content/Blueprints/Widget/` → 우클릭 → **User Interface → Widget Blueprint**.
2. 부모 클래스 = **`TetrisSettingsWidget`** (← 0.2 함정. CommonActivatableWidget/UserWidget 아님).
3. 이름 = `WBP_SettingsMenu`.

#### 4.1 위젯 트리 (최소 골격)
```
[Root]
└─ Overlay (fill)                         ← 정렬 컨테이너 (CanvasPanel 절대배치 쓰지 말 것)
   ├─ Image (배경 dim, fill)              ← 선택 (메뉴 위 오버레이)
   └─ SizeBox (WidthOverride ~600, 중앙)
      └─ VerticalBox
         ├─ [DAS 행] HorizontalBox
         │    ├─ WBP_CommonButton  → 이름: DASMinusButton   (라벨 "−")
         │    ├─ TextBlock         → 이름: DASValueText     ("100 ms")
         │    └─ WBP_CommonButton  → 이름: DASPlusButton    (라벨 "+")
         ├─ [ARR 행] HorizontalBox
         │    ├─ WBP_CommonButton  → 이름: ARRMinusButton
         │    ├─ TextBlock         → 이름: ARRValueText     ("17 ms")
         │    └─ WBP_CommonButton  → 이름: ARRPlusButton
         └─ WBP_CommonButton       → 이름: BackButton       (라벨 "BACK")
```
- 각 행에 "DAS" / "ARR" 정적 라벨 TextBlock을 앞에 둬도 됨(이름 무관, BindWidget 아님).
- 버튼 6개는 전부 **`WBP_CommonButton`**(User Created 섹션 드래그). 빈 CommonButton 쓰지 말 것 → hover zoom·스타일 자동 적용.
- 배치 후 Hierarchy에서 각 인스턴스 이름을 **0.3 표대로 정확히** rename.

#### 4.2 컴파일
- 컴파일 → **버튼 5개 바인딩 에러 없어야 함**. "X is not bound"이면 이름/타입 불일치(0.3 재확인). 값 텍스트는 Optional이라 빠져도 컴파일은 통과(표시만 생략).

### 5. WBP_MainMenu에 Settings 버튼 추가
1. 기존 `WBP_MainMenu` 열기.
2. Start/Quit 옆(또는 사이)에 **`WBP_CommonButton`** 하나 추가 → 이름 **`SettingsButton`**(라벨 "SETTINGS").
3. 컴파일·저장.
> Optional이라 안 넣어도 컴파일은 되지만, 안 넣으면 Settings 진입 경로가 없다(이번 게이트 목적).

### 6. BP_TetrisPlayerController — 디폴트 1칸
1. `Content/Blueprints/BP_TetrisPlayerController` 열기 → **Class Defaults**.
2. Details **Category: `Tetris|UI`**:

| 프로퍼티 | 값 | 상태 |
|---|---|---|
| `MainMenuWidgetClass` | `WBP_MainMenu` | 기존 (유지) |
| `GameOverWidgetClass` | `WBP_GameOver` | 기존 (유지) |
| **`SettingsWidgetClass`** | **`WBP_SettingsMenu`** | **← 신규 지정** |

3. 컴파일·저장.
> 드롭다운에 `WBP_SettingsMenu`가 안 보이면? → 부모가 `TetrisSettingsWidget`(=`UTetrisActivatableWidget` 파생)이 아니어서 `TSubclassOf` 필터에 안 걸린 것. 0.2 재확인.

---

## 7. PIE 검증 시나리오

`L_Main` 맵에서 Play (PIE).

| # | 동작 | 기대 결과 |
|---|---|---|
| 1 | **PIE 시작** → 메인 메뉴 버튼에 **마우스 hover** | 버튼이 **zoom in**, 벗어나면 원복 (Hover Zoom A) |
| 2 | **게임패드/키보드로 버튼 focus 이동** | focus된 버튼이 zoom 되는지 **확인**(미동작 시 9번 메모) |
| 3 | 메인 메뉴 **Settings 클릭** | Settings 화면이 메인 메뉴 위로 전환. DAS `100 ms` / ARR `17 ms` 표시 |
| 4 | DAS **+ 5회 / − 2회**, ARR **+/−** 조정 | 값이 `±스텝`으로 갱신. 범위 끝(0/500, 0/200)에서 더 안 변함 |
| 5 | **Back 버튼** 또는 **게임패드 B / ESC** | Settings 닫히고 메인 메뉴 복귀(게임까지 닫히지 않음) |
| 6 | **PIE 종료 후 재실행** → Settings 재진입 | **조정값 유지**(SaveGame 영속). 예: DAS를 125로 바꿨으면 재실행 후에도 125 ms |
| 7 | Settings에서 값 조정 → Back → **Start** → 플레이 | 조정된 DAS/ARR로 좌우 이동 체감(DAS 크게/ARR 작게 하면 차이 뚜렷). 필요 시 로그로 확인 |
| 8 | (Retry도) 게임오버 → Retry | 새 판도 동일 DAS/ARR 반영(StartGame/RestartGame 공통 주입점) |

---

## 8. 완료 기준

- [ ] WBP_CommonButton 부모 = `TetrisButtonBase`로 reparent + `HoverZoom` 애니메이션 author (Pivot 0.5, Scale 1→1.1)
- [ ] WBP_SettingsMenu 생성 (부모 `TetrisSettingsWidget`, 버튼 5개 + 값 텍스트 2개 바인딩, 컴파일 통과)
- [ ] WBP_MainMenu에 `SettingsButton` 추가
- [ ] BP_TetrisPlayerController 디폴트 `SettingsWidgetClass = WBP_SettingsMenu`
- [ ] PIE 7장 시나리오 통과 (hover zoom / Settings 조정 / SaveGame 영속 / 게임 반영)

---

## 9. 트러블슈팅 (증상 → 원인)

| 증상 | 원인 / 조치 |
|---|---|
| **hover해도 zoom 안 됨** | (a) WBP_CommonButton 부모가 `TetrisButtonBase` 아님(1장). (b) 애니메이션 이름이 `HoverZoom` 아님(0.3) — Optional이라 조용히 스킵. (c) Scale 트랙이 RenderTransform가 아니거나 키 1개뿐 |
| zoom이 **좌상단으로 튐** | 대상 위젯 Render Transform **Pivot = (0.5,0.5)** 아님 (2.4) |
| **게임패드 focus 시 zoom 안 됨**(마우스는 됨) | CommonUI focus→hover 매핑 미동작 가능성. **RESULT 미해결 #1** — Builder에 보고하면 `NativeOnAddedToFocusPath` 후속 추가(C++) |
| Settings **버튼 클릭 무반응** | WBP_SettingsMenu 부모 클래스 틀림(Cast 실패). `TetrisSettingsWidget` 확인 (0.2) |
| 컴파일 **"X is not bound"** | 버튼 5개 이름/타입 불일치. 0.3 표와 글자 단위 대조. 버튼은 CommonButtonBase 파생이어야 함 |
| Settings **값 텍스트 안 보임** | `DASValueText`/`ARRValueText` 이름 불일치 — Optional이라 조용히 스킵. 이름 재확인 |
| **Back이 게임까지 닫음** | 정상 동작은 Settings만 pop. 메인 메뉴가 안 떠 있으면 push 순서 문제 — Output Log 확인 |
| **재실행 후 값 초기화**(영속 실패) | SaveGame 슬롯 쓰기 실패. Output Log에서 SaveGameToSlot 경고 확인. 슬롯명 `TetrisSettings` |
| 메인 메뉴에 **Settings 버튼 없음** | WBP_MainMenu에 `SettingsButton` 미추가(5장). Optional이라 컴파일은 통과 |
| PC 디폴트 드롭다운에 **WBP_SettingsMenu 안 보임** | 부모가 `UTetrisActivatableWidget` 파생 아님 (6장 노트) |

### 디버깅 도구
- **Widget Reflector** (Window → Developer Tools): 런타임 위젯 트리·이름 확인. BindWidget/Anim 이름 검증에 최적.
- **Output Log**: PC가 미지정 클래스(`SettingsWidgetClass`)에 대해 경고를 남김.

→ 완료 후 동작 이상(특히 게임패드 hover)이나 추가 요구가 있으면 Builder/Architect 세션에 보고.
