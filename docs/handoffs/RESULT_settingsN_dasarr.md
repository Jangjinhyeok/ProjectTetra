# RESULT — Settings #N : DAS/ARR 설정 화면(SaveGame 영속) + 버튼 Hover Zoom

> **From**: Builder 세션
> **To**: Architect 세션
> **Date**: 2026-06-12
> **명세**: `HANDOFF.md`(동일 제목). C++ 게이트 G1~G4 전부 완료. G5는 에디터/PIE(사용자) — 별도 `EDITOR_GUIDE_G5.md` 작성.
> **빌드**: UBT 헤드리스(`ProjectTetraEditor Win64 Development`) **Result: Succeeded**. 경고는 기존 프로젝트 전역 `FieldNotification` C4996 deprecation 하나뿐(이번 변경 무관).

---

## 요약

| 게이트 | 범위 | 상태 | 검증 |
|---|---|---|---|
| G1 | 설정 영속화 백엔드(SaveGame + GameInstance 서브시스템) | ✅ 완료 | 빌드 ✅ / System 격리 ✅ |
| G2 | Session 핸들링 주입(`ResetHandling`만) | ✅ 완료 | 빌드 ✅ / 테스트 110-110 무회귀 ✅ |
| G3 | Settings 위젯 + 메뉴/PC 배선 | ✅ 완료 | 빌드 ✅ / 위젯 격리 ✅ / cpp-reviewer ✅ |
| G4 | Hover Zoom 버튼 베이스 | ✅ 완료 | 빌드 ✅ |
| G5 | WBP 제작 + PIE | ⏳ 사용자 | `EDITOR_GUIDE_G5.md` 참조 |

---

## 변경 파일 전체 목록

**신규 (6):**
- `System/TetrisSettingsSaveGame.h` — `USaveGame` payload(`DASms=100, ARRms=17`). 헤더 온리(아래 일탈 #1).
- `System/TetrisSettingsSubsystem.h`/`.cpp` — `UGameInstanceSubsystem`. Initialize(슬롯 로드+clamp), Get/Set(clamp+조건부 저장), SaveToSlot. 슬롯명 `"TetrisSettings"`/UserIndex 0 상수.
- `UI/Views/TetrisSettingsWidget.h`/`.cpp` — `UTetrisActivatableWidget` 파생. ± 핸들러(DAS ±5 / ARR ±1), Back 핸들러, 서브시스템 직접 read/write.
- `UI/Foundation/TetrisButtonBase.h`/`.cpp` — `UCommonButtonBase` 파생. `HoverZoom`(BindWidgetAnimOptional) Forward/Reverse.

**수정 (4):**
- `Session/TetrisSessionSubsystem.cpp` — `ResetHandling()` 내부 17줄만(설정 서브시스템 read → `Cfg.DASms/ARRms` 덮어쓰기). 헤더·그 외 로직 무변경.
- `UI/Views/TetrisMainMenuWidget.h`/`.cpp` — `SettingsButton`(BindWidgetOptional) + `OnSettingsRequested` + `HandleSettingsClicked` 중계.
- `Input/TetrisPlayerController.h`/`.cpp` — `SettingsWidgetClass`(EditDefaultsOnly) + `ActiveSettings`(Transient) + `HandleSettingsRequested`(Menu 레이어 push) / `HandleSettingsBack`(pop). `ShowMainMenu`에서 `OnSettingsRequested` 바인딩.

---

## 검증 상세

- **빌드**: G1·G2·G3·G4 각 게이트 종료 시 UBT 클린. 최종 G4 빌드 62s, `TetrisButtonBase.cpp` 컴파일 + `UnrealEditor-ProjectTetra.dll` 링크 확인.
- **테스트(G2)**: 전체 110/110 통과. 테스트 월드는 설정 서브시스템 미생성 → null-safe fallback 경로(기존 `FHandlingConfig` 기본값)로 무회귀 확인.
- **격리(MVVM, §4)**: Settings 위젯·System 서브시스템에 Model/Session/VM 심볼 grep 0건. 위젯→GameInstance 서브시스템 직결만.
- **컨벤션(§8)**: 신규 4파일 BOM 없음(`2f 2f 20`). Epic 접두사 / 한국어 Why 주석 / `Slot` 식별자 회피 / named 상수.
- **리뷰(G3)**: cpp-reviewer 별도 컨텍스트 검토 → 실질 이슈 1건(재진입 시 델리게이트 바인딩 누적) 수정, nit 1건(값 텍스트 Optional) 적용. 나머지는 기존 프로젝트 전역 패턴이라 스코프 밖으로 분류.

---

## HANDOFF 대비 일탈 (보고)

1. **`TetrisSettingsSaveGame` 헤더 온리** (HANDOFF §64는 `.h`/`.cpp` 명시) — payload 클래스에 구현부 없음. `.cpp` 생략. 기능 영향 없음.
2. **`SettingsButton`을 BindWidgetOptional** (HANDOFF §114는 required BindWidget) — 이미 author된 `WBP_MainMenu`를 G5 전까지 컴파일 에러로 깨지 않기 위해 Optional. G5에서 버튼 추가 후에도 Optional 유지 무해(null-guard 중계).
3. **Settings 값 텍스트 BindWidgetOptional** (HANDOFF §106은 BindWidget) — GameOver 위젯과 동일 컨벤션(없으면 표시 생략, 안전). cpp-reviewer nit 반영.
4. **`ResetHandling`에서 멤버 `HandlingConfig` 대신 로컬 `Cfg` 사용** — HANDOFF §82~91 의사코드는 `HandlingConfig.DASms` 직접 수정이나, 실제 코드는 로컬 `Cfg` 복사본에 주입 후 `SetConfig`. 멤버 비파괴(동등 결과, 더 안전).

---

## Architect 판단 요청 / 미해결

1. **게임패드 focus → hover 매핑** (HANDOFF §135): CommonUI가 gamepad focus를 `NativeOnHovered`로 매핑하는지는 **G5 PIE 육안 확인** 필요. 미동작 시 `NativeOnAddedToFocusPath/RemovedFromFocusPath` 추가가 G4 후속(이번 범위 밖). PIE 결과 보고 후 판정.
2. **델리게이트 해제 패턴 일관화**(tech-debt 후보): PC의 위젯 델리게이트 `RemoveAll`은 이번에 Settings만 보강. Pause/GameOver/MainMenu 포함 전역 일괄 정리는 별도 작업 권장.
3. **설정 VM 미도입**(HANDOFF §158 설계노트 1): DAS/ARR 2값 1:1이라 위젯↔서브시스템 직결. 설정 항목이 다수·다소비자로 늘면 `UTetrisSettingsViewModel` 도입(tech-debt 후보).

---

## 다음 단계

- **사용자**: `EDITOR_GUIDE_G5.md`를 따라 WBP 제작(WBP_CommonButton reparent + HoverZoom author, WBP_SettingsMenu 신규, WBP_MainMenu에 SettingsButton 추가) → PC 디폴트 `SettingsWidgetClass` 지정 → PIE 검증.
- **Architect**: 위 일탈 4건·미해결 3건 검토. PIE 결과(특히 게임패드 hover)에 따라 G4 후속 여부 판정.
