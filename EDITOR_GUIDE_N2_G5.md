# EDITOR GUIDE — N2 / G5: tetr.io Handling 패널 WBP 제작 + PIE 검증

> **From**: Builder 세션 (RESULT.md G5 요약을 클릭 단위로 펼침)
> **For**: 사용자 (에디터 직접 작업 — C++ 무관, Builder 작업 아님)
> **Date**: 2026-06-12
> **선행**: C++ 게이트 G1~G4 완료 (빌드 ✅ / 테스트 110-110 ✅). 이 문서는 그 위에 얹는 **에디터 전용** 작업.
> **엔진**: UE 5.7
> **명세 출처**: `HANDOFF.md` G5.

이번 게이트의 핵심: 기존 Settings 화면(± 버튼)을 **tetr.io HANDLING 패널(4행 × 슬라이더)** 로 교체한다.
- **A. `WBP_HandlingRow` 신규** — 슬라이더 + 텍스트 한 행. 4행이 이 한 위젯을 재사용.
- **B. `WBP_SettingsMenu` 재구성** — 기존 ± 버튼/값 텍스트 제거 → `WBP_HandlingRow` 인스턴스 4개 배치.

A를 먼저(B가 A를 인스턴스로 씀). PIE(5장)는 둘 다 끝낸 뒤.

---

## 0. 시작 전 — 코드에서 추출한 정확한 사실

### 0.1 자산 경로 (기존과 일관)
```
Content/Blueprints/Widget/          ← 모든 WBP (WBP_SettingsMenu, WBP_MainMenu …)
Content/Blueprints/Widget/Styles/   ← DA_ButtonStyle, DA_TextStyle
```
→ **신규 `WBP_HandlingRow`도 `Content/Blueprints/Widget/`에 만든다.**

### 0.2 가장 흔한 함정 — 부모 클래스
| WBP | 반드시 이 부모 | 틀리면 |
|---|---|---|
| `WBP_HandlingRow` (신규) | **`TetrisHandlingRowWidget`** | 슬라이더/텍스트 BindWidget 미동작 |
| `WBP_SettingsMenu` (기존) | **`TetrisSettingsWidget`** (이미 그러함, 변경 없음) | Back 복귀/행 무반응 (PC Cast 실패) |

### 0.3 BindWidget 이름 — 글자 하나까지 일치 (대소문자 구분)

**`WBP_HandlingRow`** (부모 `TetrisHandlingRowWidget`)
| 위젯 이름 | 타입 | 필수 | 누락 시 |
|---|---|---|---|
| `ValueSlider` | **Slider** (USlider) | **필수** (BindWidget) | 컴파일 에러 |
| `MsText` | **TextBlock** | **필수** (BindWidget) | 컴파일 에러 |
| `ValueText` | **TextBlock** | **필수** (BindWidget) | 컴파일 에러 |
| `LabelText` | TextBlock | 선택 (BindWidgetOptional) | 라벨 미표시(안전) — WBP 정적 텍스트로 대체 가능 |

**`WBP_SettingsMenu`** (부모 `TetrisSettingsWidget`)
| 위젯 이름 | 타입 | 필수 | 누락 시 |
|---|---|---|---|
| `ARRRow` | `WBP_HandlingRow` 인스턴스 | **필수** (BindWidget) | 컴파일 에러 |
| `DASRow` | `WBP_HandlingRow` 인스턴스 | **필수** | 컴파일 에러 |
| `DCDRow` | `WBP_HandlingRow` 인스턴스 | **필수** | 컴파일 에러 |
| `SDFRow` | `WBP_HandlingRow` 인스턴스 | **필수** | 컴파일 에러 |
| `BackButton` | CommonButtonBase 파생(`WBP_CommonButton`) | **필수** | 컴파일 에러 |

> **제거 대상**(C++에서 이미 삭제됨 → WBP에 남아도 무해한 orphan이지만 정리 권장): `DASMinusButton`, `DASPlusButton`, `ARRMinusButton`, `ARRPlusButton`, `DASValueText`, `ARRValueText`.

> **입력 모드/Back 핸들러는 C++ 생성자에 박혀 있다**(`InputModeOverride=Menu`, `bIsBackHandler=true`). WBP에서 건드릴 필요 없음 — 레이아웃/꾸미기만.

### 0.4 행별 조정 사양 (참고 — C++ `Configure`가 강제, WBP 슬라이더 기본 0–1은 덮어써짐)
| 행 (위젯 이름) | Min | Max | Step | 표시 단위 | `MsText` | `ValueText` |
|---|---|---|---|---|---|---|
| `ARRRow` | 0 | 200 | 1 | TimeMs | `"{N}MS"` | `"{N.N}F"` |
| `DASRow` | 0 | 500 | 1 | TimeMs | `"{N}MS"` | `"{N.N}F"` |
| `DCDRow` | 0 | 500 | 1 | TimeMs | `"{N}MS"` | `"{N.N}F"` |
| `SDFRow` | 1 | 40 | 1 | Multiplier | **빈 문자열**(C++가 비움) | `"{N}X"` |

> 슬라이더 Min/Max를 WBP에서 직접 맞출 필요 없다 — C++ `Configure`가 활성화 시 덮어쓴다. 다만 디자인 시 미리 맞춰두면 에디터 프리뷰가 자연스럽다.

---

## 1. `WBP_HandlingRow` 만들기 (신규)

1. `Content/Blueprints/Widget/` 우클릭 → **User Widget** → 부모 선택 창에서 **`TetrisHandlingRowWidget`** 선택. 이름 `WBP_HandlingRow`.
2. 디자이너에서 가로 레이아웃(예: **Horizontal Box** 또는 **Grid**) 구성:
   - (선택) **TextBlock** 이름 `LabelText` — 또는 정적 텍스트로 "ARR" 등 직접 author(행마다 다르므로 §2에서 인스턴스별로 덮어쓸 거면 `LabelText` BindWidget이 편함).
   - **Slider** 이름 **`ValueSlider`** — Orientation Horizontal. (Min/Max는 C++가 덮어쓰니 기본값 둬도 됨.)
   - **TextBlock** 이름 **`MsText`** — 작은 글씨(예: "100MS").
   - **TextBlock** 이름 **`ValueText`** — 큰 글씨(예: "6.0F" / "20X").
3. tetr.io 다크 톤으로 스타일링(슬라이더 바 색/핸들, 텍스트 폰트). **비주얼 자유**.
4. 컴파일 → 저장. (BindWidget 3개가 잡혔는지 컴파일 경고 0 확인.)

> **SDF 행의 `MsText`**: Multiplier 단위라 C++가 `MsText`를 **빈 문자열**로 만든다. 레이아웃은 공통 유지하되 빈칸이 보여도 무방, 또는 §2에서 SDF 인스턴스만 `MsText`를 Collapsed로.

---

## 2. `WBP_SettingsMenu` 재구성 (기존 자산)

1. `WBP_SettingsMenu` 열기 (부모가 `TetrisSettingsWidget`인지 확인 — 이미 그러함).
2. **기존 ± 버튼/값 텍스트 제거**: `DASMinusButton`/`DASPlusButton`/`ARRMinusButton`/`ARRPlusButton`/`DASValueText`/`ARRValueText` 6개 위젯 삭제.
3. **세로 레이아웃**(Vertical Box 권장)에 `WBP_HandlingRow` 인스턴스 **4개** 드래그 배치. 각 인스턴스 이름(좌측 위젯 트리에서 rename)을 **정확히**:
   - 1행: **`ARRRow`** — (라벨 "ARR")
   - 2행: **`DASRow`** — (라벨 "DAS")
   - 3행: **`DCDRow`** — (라벨 "DCD")
   - 4행: **`SDFRow`** — (라벨 "SDF")
4. `LabelText`를 BindWidget으로 author했다면 각 인스턴스의 `LabelText` 기본 텍스트를 행 이름에 맞게 설정. (정적 텍스트 방식이면 `WBP_HandlingRow` 안에서 직접.)
5. `BackButton`(기존 `WBP_CommonButton`) 유지.
6. 컴파일 → **BindWidget 5개(ARRRow/DASRow/DCDRow/SDFRow/BackButton)가 전부 잡혀 컴파일 에러 0** 확인 → 저장.

> 컴파일 에러 "변수 ARRRow를 찾을 수 없음" 류가 뜨면 → 인스턴스 이름 오타 또는 인스턴스 타입이 `WBP_HandlingRow`가 아님.

---

## 3. PIE 검증

1. **메인 메뉴 → Settings** 진입 (기존 `SettingsButton` 경로 유지).
2. **4행 슬라이더 조정**:
   - ARR/DAS/DCD: 슬라이더 드래그 → `MsText`가 `"{N}MS"`, `ValueText`가 `"{N.N}F"`로 실시간 갱신.
   - SDF: 슬라이더 드래그 → `ValueText`가 `"{N}X"`, `MsText`는 빈칸.
3. **Back** → 메인 메뉴 복귀.
4. **영속 확인**: PIE 종료 → 다시 PIE 실행 → Settings 재진입 → **4값이 유지**되는지(SaveGame). 슬라이더 위치 + 텍스트 모두.
5. **반영 확인**: 값 조정 후 **Start** → 플레이.
   - DAS/ARR/DCD: 좌우 이동 손맛 체감 또는 콘솔 `tetra.DebugBoard 1`로 관찰.
   - SDF: 소프트드롭 키 유지 시 낙하 속도가 배수만큼 빨라지는지 체감.
6. **게임패드**(HANDOFF 미해결 1): 슬라이더에 focus 후 **dpad/아날로그 좌우로 값이 바뀌는지** 확인.
   - **동작하면**: OK, 추가 작업 없음.
   - **미동작이면**: RESULT.md "미해결 1"대로 행 컴포넌트에 dpad/analog 핸들러 추가가 G3 후속(Architect 판정). 관찰 결과를 기록.

---

## 4. 미동작 체크리스트 (증상 → 원인)

| 증상 | 가장 흔한 원인 |
|---|---|
| `WBP_SettingsMenu` 컴파일 에러(변수 못 찾음) | 행 인스턴스 이름이 `ARRRow/DASRow/DCDRow/SDFRow`와 불일치 |
| 행은 보이는데 슬라이더 옆 텍스트 안 변함 | `WBP_HandlingRow` 안 `ValueSlider/MsText/ValueText` 이름 불일치(Widget Reflector로 실제 이름 확인) |
| 슬라이더 범위가 0–1로 좁음 | 정상 — C++ `Configure`가 활성화 시 덮어씀(PIE에서 확인). 디자인 프리뷰만 어색할 뿐 |
| 화면은 뜨는데 Back/값 저장 무반응 | `WBP_SettingsMenu` 부모가 `TetrisSettingsWidget` 아님(PC Cast 실패) |
| 값이 PIE 재실행에 안 남음 | 세터 경로 문제 아님(C++ 검증됨) — Settings 진입이 실제로 SaveGame 세터를 타는지 슬라이더 변경 발생 여부 확인 |
| SDF 행에 "MS"가 남음 | 정상 동작은 빈칸 — `MsText` BindWidget 이름 확인(C++가 비움). 안 비면 이름 불일치 |

---

## 완료 후

PIE 검증 결과(특히 **게임패드 슬라이더 조정 여부**)를 사용자가 Architect 세션에 전달 → `RESULT.md` "미해결 1"의 G3 후속 필요 여부 판정.
