# RESULT — Settings #N2 : tetr.io 스타일 Handling 패널 (4값 슬라이더 + DCD/SDF 노출)

> **From**: Builder 세션
> **To**: Architect 세션
> **Date**: 2026-06-12
> **명세**: `HANDOFF.md`(동일 제목). C++ 게이트 G1~G4 전부 완료. G5는 에디터/PIE(사용자) — `EDITOR_GUIDE_N2_G5.md` 작성.
> **빌드**: UBT 헤드리스(`ProjectTetraEditor Win64 Development`) 각 게이트 **Result: Succeeded**. 경고는 기존 전역 `FieldNotification` C4996 deprecation 하나뿐(이번 변경 무관).
> **선행 아카이브**: 직전 #N(DAS/ARR) `RESULT.md` → `docs/handoffs/RESULT_settingsN_dasarr.md`로 이동(HANDOFF 권장).

---

## 요약

| 게이트 | 범위 | 상태 | 검증 |
|---|---|---|---|
| G1 | 백엔드 확장: DCD/SDF 영속(SaveGame + 서브시스템) | ✅ 완료 | 빌드 ✅ / System 격리 ✅ / 4 게터·세터 대칭 ✅ |
| G2 | Session 주입: DCD + SDF(`ResetHandling`만) | ✅ 완료 | 빌드 ✅ / 테스트 **110/110** 무회귀 ✅ |
| G3 | 재사용 Handling 행 컴포넌트(신규) | ✅ 완료 | 빌드 ✅ / 행 컴포넌트 격리 ✅ |
| G4 | Settings 위젯 재설계: 4행 호스트 | ✅ 완료 | 빌드 ✅ / 위젯 격리 ✅ |
| G5 | WBP 제작 + PIE | ⏳ 사용자 | `EDITOR_GUIDE_N2_G5.md` 참조 |

---

## 변경 파일 전체 목록

**신규 (2):**
- `UI/Components/TetrisHandlingRowWidget.h`/`.cpp` — `UCommonUserWidget` 파생, **도메인 무지** 재사용 행. `EHandlingDisplayUnit{TimeMs, Multiplier}`, `Configure(Min,Max,Step,Unit,Initial)`, `FOnHandlingValueChanged`(int32) 델리게이트. 내부: `NativeOnInitialized`에서 `USlider::OnValueChanged` 바인딩(UFUNCTION `HandleSliderChanged`) → `SnapToStep` → `RefreshTexts` → 발행. `GHandlingDisplayHz=60` named 상수(cpp anon ns).

**수정 (5):**
- `System/TetrisSettingsSaveGame.h` — payload에 `DCDms=0`, `SDF=20` 2필드 추가(코어 기본과 일치). 구버전 슬롯 호환 Why 주석.
- `System/TetrisSettingsSubsystem.h` — 캐시(`DCDms=0, SDF=20`) + clamp 상수(`DCDMin/Max=0/500`, `SDFMin/Max=1/40`) + 게터/세터 각 2개 추가(DAS/ARR과 대칭).
- `System/TetrisSettingsSubsystem.cpp` — `Initialize` 로드 clamp 2줄 + `SaveToSlot` 직렬화 2줄 + `SetDCDms`/`SetSDF` 세터 2개(조건부 저장).
- `Session/TetrisSessionSubsystem.cpp` — `ResetHandling()` settings-pull 블록 내부에만 `Cfg.DCDms` 주입 + `if(GameCore) GameCore->SoftDropFactor` 주입(4줄). 시그니처·헤더·그 외 로직 무변경.
- `UI/Views/TetrisSettingsWidget.h`/`.cpp` — **전면 재설계**. ± 버튼 4개·DAS/ARR 텍스트·`RefreshValueTexts`·4 ± 핸들러·step 상수 **제거**. ARRRow/DASRow/DCDRow/SDFRow(BindWidget) + 4 변경 핸들러 **추가**. `NativeOnActivated`에서 행별 `Configure`(서브시스템 read) + `OnValueChanged.BindUObject`. 생성자/Back/`ResolveSettings`/`HandleBackClicked` 유지.

---

## 검증 상세

- **빌드**: G1·G2·G3·G4 각 게이트 종료 시 UBT 클린(`Result: Succeeded`). UHT 매크로 파싱 통과.
- **테스트(G2)**: 헤드리스 자동화(`Automation RunTests Tetris`) **110/110 통과, 실패 0**. 리포트 JSON 파싱 확인(succeeded=110, failed=0, notRun=0). 테스트 월드는 설정 서브시스템 미생성 → DCD/SDF 주입 스킵 = 무회귀.
- **격리(MVVM, §4)**: 행 컴포넌트·Settings 위젯·System 서브시스템에 Model/Session/VM/GameCore 심볼 grep → **실코드 의존 0**(매치는 모두 "왜" 주석 내 개념 설명). 행은 서브시스템 미참조, 값 read/write는 Settings 위젯이 소유.
- **컨벤션(§8)**: 신규 2파일 BOM 없음(`head -c3` 확인). Epic 접두사 / 한국어 Why 주석 / `Slot` 식별자 회피 / 매직넘버 named 상수화(행 범위·표시 Hz).
- **스코프**: 수정 허용 6파일(신규 2 + 수정 5 중 SaveGame.h 포함)만 변경. `FHandlingConfig`/`UTetrisGameCore` 코드 무변경, `TetrisPlayerController.*`·`TetrisSessionSubsystem.h` 무변경(PC는 헤더 의존 재컴파일만), Build.cs/테스트 무변경.

---

## HANDOFF 대비 일탈 (보고)

1. **G5 가이드 파일명** — HANDOFF는 G5를 본 RESULT에서 펼치라 했으나, 루트에 직전 #N 작업의 `EDITOR_GUIDE_G5.md`가 이미 존재(미커밋)하여 충돌 방지를 위해 이번 가이드는 **`EDITOR_GUIDE_N2_G5.md`**로 분리 작성. 내용·스코프 동일.
2. **직전 RESULT.md 아카이브 실행** — HANDOFF는 "권장"이었으나, 같은 루트에 #N2 RESULT를 쓰기 위해 직전 #N RESULT를 `docs/handoffs/`로 이동(파괴 아님, 보존).
3. 그 외 일탈 없음 — 백엔드/주입/행/위젯 모두 HANDOFF 의사코드·표대로 구현.

---

## Architect 판단 요청 / 미해결

1. **게임패드 슬라이더 조정**(HANDOFF §G3·설계노트 5): `USlider`가 focus 시 dpad/아날로그 좌우로 값이 바뀌는지는 **G5 PIE 육안 확인** 필요. 미동작 시 행 컴포넌트에 dpad-left/right(또는 analog) 핸들러 추가가 G3 후속(이번 범위 밖). PIE 결과 보고 후 판정.
2. **SDF instant(∞) 센티넬 생략**(설계노트 4): tetr.io 41=instant는 MVP 범위 밖. 정수 배수 1–40(`{N}X`)만. 폴리시 후속(tech-debt 후보).
3. **표시용 Hz 고정(60)**(설계노트 3): 프레임 표시는 cosmetic `ms×60/1000`. SimHz가 가변이 되면 `Configure`로 Hz 주입하도록 확장(현재는 named 상수).

---

## 다음 단계

- **사용자**: `EDITOR_GUIDE_N2_G5.md`를 따라 WBP 제작(`WBP_HandlingRow` 신규 → `WBP_SettingsMenu` 4행 재구성) → PIE 검증(슬라이더 조정 → `{N}MS`/`{N.N}F`/`{N}X` 갱신 → 재실행 영속 → Start 반영 → 게임패드 슬라이더).
- **Architect**: 위 미해결 3건 검토. PIE 결과(특히 게임패드 슬라이더)에 따라 G3 후속 여부 판정.
