# ViewModel (HUD) — MVVM + FieldNotify

> **Status**: In Design
> **Author**: user + Claude (Architect)
> **Last Updated**: 2026-05-29
> **Implements Pillar**: MVVM + FieldNotify 기반 이벤트 주도 UI (포트폴리오 핵심)

## Overview

ViewModel은 검증된 순수 Model 계층(GameCore/Scoring/Randomizer)의 상태를 **UMG View가 바인딩으로 소비할 형태로 변환**하는 MVVM의 중간 계층이다. 이번 단계는 **HUD 전용 ViewModel**(점수/레벨/줄/콤보/B2B + Next 큐 + Hold + 게임 상태)에 한정하며, 보드 그리드 렌더링은 별도 시스템(#12 Board Renderer)으로 분리한다. Model은 이미 이벤트 주도 델리게이트(`On*Changed`)를 노출하므로, ViewModel은 그것을 구독해 **값이 실제로 바뀔 때만** `FieldNotify`로 View에 통지한다 — `Tick`/Property Binding 매 프레임 polling 금지. 소유·와이어링은 UE5.7 표준 경로(**전용 UI 바인더 + Global View Model Collection**)를 따른다: VM은 순수 데이터 홀더로 두고, 별도 바인더(UObject)가 Model 델리게이트 → VM setter 연결과 컬렉션 등록/해제를 책임진다. 이 분리 덕에 ViewModel은 **위젯·월드 없이 단위 테스트**가 가능하며(Model 계층과 동일한 검증 가능성), MVVM 분리 원칙을 코드로 증명한다.

**핵심 프레이밍 (불변 결정):**
1. **VM은 순수 데이터 홀더** — Model 델리게이트를 직접 구독하지 않는다. 구독은 바인더의 책임(VM은 setter만 노출).
2. **이벤트 주도 갱신** — Model이 값 변경을 통지할 때만 VM이 `UE_MVVM_SET_PROPERTY_VALUE`로 FieldNotify 발행. polling 없음.
3. **View는 VM만 안다** — Model/Session/바인더를 직접 참조하지 않는다. Global View Model Collection으로 해소(resolve).
4. **HUD 한정** — 보드 그리드·활성 피스는 이번 범위 밖(#12). VM 인터페이스를 작게 유지해 첫 MVVM 적용 리스크를 최소화.

## Player Fantasy

(플레이어 직접 체감 시스템이 아닌 인프라 계층 — "느낌"보다 "정확성/반응성"이 가치다.)

ViewModel은 플레이어가 의식하지 못하는 사이 "화면이 항상 진실을 말한다"는 신뢰를 만든다. 줄을 지운 순간 점수가 정확히 그 값으로, 레벨업하는 순간 레벨이, Hold를 쓴 순간 Hold 슬롯이 — 지연도, 한 프레임 깜빡임도, 어긋난 숫자도 없이 갱신된다. 이 정확성·즉응성은 polling이 아니라 "변경 시에만 통지"하는 구조에서 나온다. 개발자(포트폴리오) 관점의 fantasy는 다른 층위다: **기획이 "콤보 표시를 바꾸자"고 할 때 Model·로직을 한 줄도 건드리지 않고 VM/View만 수정해 대응**하는 경험, 그리고 UI가 존재하기 전에 통과한 Model 테스트에 더해 **위젯 없이 통과하는 ViewModel 테스트**로 "분리가 실제로 됐음"을 증명하는 경험. 즉 이 시스템의 loved-engagement 대상은 플레이어가 아니라 유지보수자다.

## Detailed Design

### 구성요소 & 소유

```
[Model]                          [Binder]                         [ViewModel]            [View]
UTetrisGameCore (델리게이트)   →  UTetrisHUDViewModelBinder     →  UTetrisHUDViewModel  →  UMG 위젯
UTetrisScoring  (델리게이트)      (구독 + setter + 컬렉션 등록)     (FieldNotify 필드)      (바인딩 소비)
UTetrisRandomizer (델리게이트)
```

| 요소 | 클래스 | 소유 | 역할 |
|------|--------|------|------|
| ViewModel | `UTetrisHUDViewModel : UMVVMViewModelBase` | 바인더 | 순수 데이터 홀더. FieldNotify 필드 + setter만. Model/델리게이트 비참조. |
| 바인더 | `UTetrisHUDViewModelBinder : UObject` | Session | Model 델리게이트 구독 → VM setter. 컬렉션 등록/해제. 초기 스냅샷 주입. |
| Collection | UE5.7 `GlobalViewModelCollection` (MVVM 서브시스템) | (엔진) | View가 VM을 컨텍스트명으로 resolve. |

**계층 규칙**: VM은 Model을 모르고(setter만), View는 Model/바인더/Session을 모른다(컬렉션 resolve). Model→VM 단방향 의존은 바인더 한 곳에 격리된다. 바인더·VM 코드는 `Source/ProjectTetra/UI/ViewModel/`(UI 계층)에 둔다.

### `UTetrisHUDViewModel : UMVVMViewModelBase`

데이터 필드(모두 `UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)` + 비-Blueprint setter):

| 필드 | 타입 | 기본 | 소스 델리게이트 |
|------|------|------|----------------|
| `Score` | `int64` | 0 | `Scoring.OnScoreChanged` |
| `Level` | `int32` | 1 | `Scoring.OnLevelChanged` |
| `Lines` | `int32` | 0 | `Scoring.OnLinesChanged` |
| `Combo` | `int32` | -1 | `Scoring.OnComboChanged` |
| `B2BCount` | `int32` | 0 | `Scoring.OnB2BChanged` |
| `HoldPiece` | `EPieceType` | None | `GameCore.OnHoldChanged` (1st param) |
| `bCanHold` | `bool` | true | `GameCore.OnHoldChanged` (2nd param) |
| `NextQueue` | `TArray<EPieceType>` | {} | `Randomizer.OnNextQueueChanged` |
| `GameState` | `EGameState` | Idle | `GameCore.OnStateChanged` (New) |

- setter는 `UE_MVVM_SET_PROPERTY_VALUE(Field, NewValue)`로 **값이 달라질 때만** 변경+통지(아래 Formulas V1). 바인더만 호출 → `friend class UTetrisHUDViewModelBinder` 패턴(VERSION.md 5.7 규약).
- **계산 프로퍼티(시연용)**: `GetScoreText() : FText` — `UFUNCTION(BlueprintPure, FieldNotify)`. 포맷은 `FText::AsNumber(Score)`(로케일 기본 천단위 구분). `Score` setter가 값 변경 시 `UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetScoreText)`로 파생 통지. FieldNotify computed 패턴을 1건 시연(데이터 프로퍼티 + 계산 프로퍼티 둘 다 보유).

### `UTetrisHUDViewModelBinder : UObject`

```
void Bind(UTetrisGameCore* Core, UTetrisScoring* Scoring, UTetrisRandomizer* Rand)
  1) VM = NewObject<UTetrisHUDViewModel>(this)
  2) 초기 스냅샷: getter로 현재값 pull → VM setter (Score=Scoring->GetScore(), Level, Lines,
     Combo, B2B, Hold=Core->GetHoldSlot()/IsHoldAvailable(), Next=Rand->GetNextQueue(), State=Core->GetState())
  3) 델리게이트 구독(핸들 보관): non-dynamic 7종(Scoring 5 + GameCore OnStateChanged/OnHoldChanged)은
     AddLambda/AddUObject, OnNextQueueChanged(dynamic 1종)만 AddDynamic + UFUNCTION 핸들러
     (OnHoldChanged 하나가 HoldPiece+bCanHold 두 필드를 먹임 → 델리게이트 7 ≠ 필드 9)
  4) GlobalViewModelCollection 에 VM을 컨텍스트명 "TetrisHUD" 로 등록

void Unbind()
  - 보관한 FDelegateHandle 전부 해제(dynamic은 RemoveDynamic)
  - 컬렉션에서 "TetrisHUD" VM 제거, VM 참조 해제
```

- `OnPieceLocked`/`OnGameOver`는 HUD VM이 직접 쓰지 않음(점수 변화는 Scoring 델리게이트가 이미 통지). 이번 범위에선 구독 안 함.
- 단일 게임스레드 가정 — 락 불필요(기존 컨벤션 일치).

### 생명주기 (Session 소유)

| 시점 | 동작 |
|------|------|
| `Session::StartGame` | `CreateAndWireCore` 후 `Binder->Bind(GameCore, Scoring, Randomizer)`. (`ResetHandling` 다음, `GameCore->StartGame` **이전**에 호출해 초기 스냅샷이 시작 상태를 담도록) |
| `Session::RestartGame` | 기존 바인더 `Unbind` → 재`Bind`(또는 VM 재사용 + 스냅샷 재주입). MVP는 단순화 위해 Unbind→Bind. |
| `Session::Deinitialize` | `Binder->Unbind()` + 바인더 참조 해제. |

- Session은 바인더를 `TObjectPtr<UObject>` 불투명 참조 + `Bind/Unbind`만 호출하는 게 이상적이나, 구현 단순성을 위해 `TObjectPtr<UTetrisHUDViewModelBinder>` 구체 타입 보유 허용(UI 계층 헤더 1개 include). VM 내부는 여전히 모름.

### Model 접근 경로 보강 (필수 선행)

- Session에 `UTetrisScoring* GetScoring() const`, `UTetrisRandomizer* GetRandomizer() const` getter 추가(현재 `GetGameCore()`만 존재). 바인더가 델리게이트를 구독하기 위함.

### 플러그인 / 모듈 셋업 (게이트)

- `ModelViewViewModel` 플러그인 활성: `.uproject` Plugins에 등록.
- `Build.cs` `PublicDependencyModuleNames`에 `"ModelViewViewModel"`, `"FieldNotification"` 추가.

### View(위젯) — 이번 범위 밖, 명시만

- `WBP_HUD`(UMG)가 MVVM 확장으로 `"TetrisHUD"` VM을 resolve해 `Score`/`Level`/… 를 TextBlock 등에 바인딩. 위젯 제작·바인딩은 Board Renderer/HUD 단계(#12/#13)에서. 본 단계는 **VM이 올바른 값을 들고 통지함**까지만 책임(테스트로 검증).

## Formulas

이 시스템은 수치 시뮬레이션이 아니라 **상태 매핑 계층**이라 산술 공식이 없다. 대신 동작을 규정하는 두 규칙을 형식화한다.

**V1. 변경 통지 게이트 (no-op 억제)**
```
setter(Field, New):
    if (Field == New) → 통지 없음 (FieldNotify 미발행)
    else              → Field = New; FieldNotify 발행
```
- `UE_MVVM_SET_PROPERTY_VALUE`의 기본 동작. 동일값 재설정 시 위젯 무효화/리바인딩이 일어나지 않아 불필요한 UI 갱신을 막는다. (TArray `NextQueue`는 요소 비교 — 동일 시퀀스면 미발행.)

**V2. 초기 스냅샷 일관성**
```
Bind 직후 VM의 모든 필드 == 해당 Model getter의 현재 반환값
```
- 첫 델리게이트가 발행되기 전에도 HUD가 정확한 시작값을 표시하기 위함(빈 화면/0점 깜빡임 방지).

**변수 정의 / 범위**

| 변수 | 의미 | 기본 | 범위 | 소유 |
|------|------|------|------|------|
| `NextQueueDisplayCount` | HUD가 노출할 Next 미리보기 개수 | 5 | 1~`Randomizer.NextQueueMinSize`(=5) | ViewModel(표시) |
| `ContextName` | 컬렉션 등록 키 | `"TetrisHUD"` | 고정 문자열 | ViewModel |

## Edge Cases

**1. Bind 전 위젯이 VM을 resolve**
- 컬렉션에 아직 미등록 → resolve 실패(null). 위젯은 null-safe 바인딩(빈 값 표시). Session::StartGame에서 Bind되면 정상화. (위젯 단계에서 처리; VM 책임 아님)

**2. 게임 중 동일값 재통지** (예: 0줄 lock으로 점수 불변)
- V1 게이트로 FieldNotify 미발행 → UI 갱신 없음. 정상.

**3. RestartGame 시 잔여 VM 상태**
- Unbind→Bind로 VM 재생성 + 초기 스냅샷 재주입 → 이전 판 값 잔존 없음. (VM 재사용 택할 경우 모든 필드 스냅샷 재주입 필수.)

**4. `OnNextQueueChanged`(dynamic) vs 나머지(non-dynamic) 혼재**
- 바인더가 dynamic 1건만 `AddDynamic`+`UFUNCTION`으로, 나머지 non-dynamic 7건(Scoring 5 + GameCore 2)은 람다/`AddUObject`로 분기 처리. Unbind 시 각각 `RemoveDynamic`/핸들 해제.

**5. Combo 비활성 표현** (`Combo == -1`)
- VM은 raw `-1`을 그대로 노출. "콤보 없음→숨김, ≥1→표시" 판단은 View(또는 계산 프로퍼티 확장)의 몫. VM은 의미 부여 안 함.

**6. Session 없이 VM/바인더 테스트**
- `NewObject`로 GameCore/Scoring/Randomizer를 만들어 `Bind` → Model 함수 직접 호출(예: `Scoring->HandlePieceLocked`)로 델리게이트 유발 → VM 필드 검증. 월드/위젯/PIE 불필요.

**7. 게임오버**
- `GameState`가 `GameOver`로 전이됨을 VM이 통지. 게임오버 사유(`ETopOutType`)·게임오버 화면 전환은 본 범위 밖(#15 Menu/CommonUI) — 필요 시 VM 필드 추가로 확장.

**8. View가 매 프레임 polling 시도**
- 금지(README Forbidden Patterns). VM은 polling용 Tick getter를 제공하지 않으며, 모든 갱신은 FieldNotify 이벤트로만. (computed 프로퍼티도 이벤트 기반 통지.)

## Dependencies

**Upstream (ViewModel이 의존)**

| 대상 | 유형 | 인터페이스 | 양방향 확인 |
|------|------|-----------|------------|
| `UTetrisScoring` | Hard | `On{Score,Level,Lines,Combo,B2B}Changed` + getter | ✅ scoring.md가 ViewModel을 구독자로 명시(델리게이트 주석) |
| `UTetrisGameCore` | Hard | `On{State,Hold}Changed` + `GetState/GetHoldSlot/IsHoldAvailable` | ✅ fsm.md/헤더가 "ViewModel이 구독" 명시 |
| `UTetrisRandomizer` | Hard | `OnNextQueueChanged`(dynamic) + `GetNextQueue` | ✅ randomizer.md Downstream에 ViewModel 명시됨(L127) |
| Session | Hard | `GetGameCore/GetScoring/GetRandomizer` + 바인더 생명주기 | ⚙️ Session에 getter 2종 추가 필요 |
| ModelViewViewModel / FieldNotification | Hard | `UMVVMViewModelBase`, `GlobalViewModelCollection`, FieldNotify 매크로 | ⚙️ 플러그인 활성 + Build.cs 모듈 |

**Downstream (ViewModel에 의존)**

| 시스템 | 유형 | 사용 |
|--------|------|------|
| Board Renderer (#12) | — | 보드 그리드는 별도 경로(VM 미사용) — 본 HUD VM과 무관 |
| HUD (#13) | Hard | `WBP_HUD`가 `"TetrisHUD"` VM 바인딩 |
| Menu/CommonUI (#15) | Soft | 게임오버 화면이 `GameState`/(향후 `ETopOutType`) 소비 |

**호스팅 관계 (composition)**
- Session이 `UTetrisHUDViewModelBinder`를 소유·구동, 바인더가 `UTetrisHUDViewModel`을 소유. (LockDelay가 GameCore에, Handling이 Session에 소유되는 것과 동일 패턴)

**공유 타입 (재사용)**
- `EPieceType`, `EGameState`(TetrisTypes.h) — VM 필드 타입으로 재사용. 신규 타입 없음.

**양방향 일관성 (확인/후속)**
- ✅ 업스트림 양방향 이미 충족: `fsm.md`(L150,158-161)·`scoring.md`(L104,211)·`randomizer.md`(L127) 모두 ViewModel을 downstream 소비자로 명시. 추가 작업 불필요.
- (TODO) `systems-index.md` #11 ViewModel: Status `Not Started`→`Approved`, Design Doc `design/gdd/viewmodel.md` 링크.
- (TODO) README 구현 현황 ViewModel 행 갱신(구현 완료 후).

## Tuning Knobs

| 변수 | 기본 | 안전 범위 | 영향 / 한계 시 증상 |
|------|------|----------|---------------------|
| `NextQueueDisplayCount` | 5 | 1~5 | HUD Next 미리보기 개수. Randomizer가 보장하는 최소 큐(5) 초과 설정 시 표시 부족 |
| `ContextName` | `"TetrisHUD"` | — | 컬렉션 등록 키. View 바인딩과 일치해야 resolve 성공 |
| 계산 프로퍼티 포함 | `GetScoreText` 1건 | — | FieldNotify computed 시연. 늘리면 파생 통지 비용↑(MVP는 1건) |

**외부 소유 — 참조만**

| 변수 | 소유 |
|------|------|
| 점수/레벨/콤보 산식·기본값 | Scoring (`FScoringConfig`) |
| Next 큐 최소 크기 | Randomizer (`NextQueueMinSize`) |

## Acceptance Criteria

**ViewModel 단독 (위젯/월드 없이)**
- [ ] 각 setter 호출 → 대응 getter가 새 값 반환 + FieldNotify 1회 발행
- [ ] 동일값 재설정 → FieldNotify 미발행(V1 게이트)
- [ ] `GetScoreText`가 `Score` 변경 시 파생 통지(computed FieldNotify)

**바인더 통합 (NewObject Model, PIE 불필요)**
- [ ] `Bind` 직후 VM 모든 필드 == Model getter 현재값(V2 초기 스냅샷)
- [ ] `Scoring->HandlePieceLocked(...)`로 점수/레벨/줄/콤보/B2B 변경 → VM 해당 필드 갱신
- [ ] `GameCore` 상태 전이 → VM `GameState` 갱신, Hold 사용 → `HoldPiece`/`bCanHold` 갱신
- [ ] `Randomizer` 큐 변경(dynamic 델리게이트) → VM `NextQueue` 갱신
- [ ] `Unbind` 후 Model 델리게이트 발행 → VM 미갱신(구독 해제 확인) + 컬렉션에서 제거됨
- [ ] `RestartGame` → 이전 판 값 잔존 없음(스냅샷 재주입)

**격리 / 회귀**
- [ ] VM이 Model/Session/위젯을 컴파일 의존하지 않음(헤더 grep으로 확인 — VM은 TetrisTypes만 include)
- [ ] 기존 `Tetris.*` 전부 그린(회귀 없음). 신규 `Tetris.ViewModel.*` 추가
- [ ] `Tick`/매 프레임 polling getter 부재(README Forbidden Patterns 준수)

**통합 (PIE 육안 — 위젯 단계 이후)**
- [ ] (#13 HUD 완성 후) `tetra.StartGame` → 플레이 중 점수/레벨/Next/Hold가 화면에 실시간 반영. ← 본 단계 코드의 최종 소비 검증(위젯 제작 후)

