# Input / Handling (DAS / ARR / DCD / SDF)

> **Status**: In Design
> **Author**: user + Claude (Architect)
> **Last Updated**: 2026-05-27
> **Implements Pillar**: 입력 조작감 정교화 + Deterministic Simulation

## Overview

Input/Handling은 플레이어의 raw 입력(Enhanced Input)을 FSM이 소비하는 **추상 명령 스트림(`EGameCommand`)으로 번역**하고, 그 과정에 **핸들링 파라미터(DAS/ARR/DCD/SDF)**를 적용해 "조작감"을 만드는 레이어다. 좌우 이동의 **초기 1칸 → DAS 지연 → ARR 자동 반복** 타이밍, 소프트드롭 속도, 방향 전환 시 DAS 리셋(DCD)을 관장한다. 회전·하드드롭·Hold는 이산 입력으로 즉시 매핑한다. **결정성을 위해 DAS/ARR 타이머는 고정스텝 C++ 레이어에서 진행**하며, Enhanced Input은 "버튼 held 상태 + 이산 press"만 캡처해 넘긴다(프레임 도메인 ↔ 고정스텝 도메인 분리). 생성된 명령은 `UTetrisSessionSubsystem::GetGameCore()->EnqueueCommand()`로 적재된다. 이 시스템의 정밀도가 곧 경쟁형 테트리스의 조작 신뢰감과 스킬 실링을 결정한다.

**핵심 프레이밍 (불변 결정):**
1. **번역 레이어** — Input은 "무엇을 할지"(이동/회전/드롭)를 명령으로만 표현한다. 게임 로직은 모른다.
2. **DAS/ARR는 고정스텝** — 프레임레이트와 무관하게 동일 입력 → 동일 명령 시퀀스(결정적).
3. **Enhanced Input = 의도 캡처만** — held-state + press 이벤트. 자동반복 타이밍은 핸들링 프로세서가 결정한다.

## Player Fantasy

Input/Handling은 "손과 게임이 하나가 된" 감각을 만든다. 키를 누른 만큼, 누른 순간, 정확히 그대로 피스가 반응한다 — 입력 지연도, 씹힘도, 의도치 않은 과잉 이동도 없다. 낮은 DAS/ARR로 세팅한 숙련자는 피스를 **쏘듯이** 원하는 열에 꽂아 넣고, 이 즉응성이 빠른 스태킹과 finesse(최소 입력 배치)를 가능하게 한다. 경쟁형 테트리스에서 플레이어는 자신의 핸들링 값을 **강박적으로 튜닝**한다 — DAS 100ms가 빠른지 90ms가 맞는지, ARR 0이 좋은지. 즉 이 시스템은 invisible-infrastructure가 아니라 플레이어가 **직접 만지고 자기 것으로 만드는 "loved-engagement"** 대상이다. 레퍼런스는 Tetr.io의 핸들링 커스터마이즈 문화. 핸들링이 정밀하면 "이 게임 손맛 좋다"가 되고, 어긋나면 "컨트롤이 안 먹는다"가 된다.

## Detailed Design

### Core Rules

**1. 파이프라인 (3계층)**
```
Enhanced Input (frame)         →  Input Buffer          →  FTetrisHandling (fixed-step, 순수)  →  GameCore
ATetrisPlayerController            held-state + edge 큐     AdvanceStep() → TArray<EGameCommand>    EnqueueCommand → Step()
(IMC/IA 바인딩, 번역만)
```

**2. Enhanced Input 매핑 (frame 도메인, `ATetrisPlayerController`)**
- IMC_Gameplay + Input Actions → 의도만 캡처(타이밍 판단 안 함):
  - `IA_MoveLeft`/`IA_MoveRight`: Started→press edge + held=true, Completed→held=false
  - `IA_RotateCW`/`IA_RotateCCW`: Started→이산 press
  - `IA_SoftDrop`: Started→`SoftDropOn` edge, Completed→`SoftDropOff` edge
  - `IA_HardDrop`: Started→이산 press
  - `IA_Hold`: Started→이산 press
- 매 프레임 held-state 갱신 + edge를 입력 버퍼에 push.

**3. 입력 버퍼 (frame→fixed-step 핸드오프)**
- `FHandlingInput`: held-state(`bLeftHeld/bRightHeld` — 좌우만; 소프트드롭은 edge pass-through라 held 불필요) + **이산 edge 큐**.
- 프레임 사이 발생한 edge는 누락 없이 쌓이고 다음 고정스텝에서 드레인 → 빠른 탭 유실 방지.

**4. `FTetrisHandling::AdvanceStep(const FHandlingInput&) → TArray<EGameCommand>` (순수, 매 Step)**
1. **이산 즉시 명령**: 큐의 Rotate/HardDrop/Hold/SoftDropOn/Off를 순서 보존해 방출.
2. **방향 이동 (DAS/ARR)**:
   - 새 방향 press → **즉시 1칸** 방출 + DAS 타이머 시작(`ActiveDir` 설정).
   - held 지속: DAS 경과 전 반복 없음 → 경과 후 ARR 간격 반복.
     - `ARR>0`: ARR스텝마다 1칸.
     - `ARR=0`: **즉시 벽까지** — 한 스텝에 `BoardWidth(=10)`개 방출(FSM이 벽에서 무해하게 클램프; Handling은 보드 비참조).
   - **방향 전환**(반대 키 press): `ActiveDir` 교체 + DCD 적용(DAS를 DCD만큼만 남기고 리셋).
   - held 해제: `ActiveDir` 해제, 타이머 정지.
3. SoftDrop은 `SoftDropOn/Off`만 방출(속도 배수 SDF는 GameCore 소유).

**5. 동시 좌우 입력**: **last-input priority** — 마지막 누른 방향 우선. 그 키를 떼면 여전히 눌린 반대 키로 폴백.

**6. 극단값**: `DAS=0`→press 즉시 ARR 반복 시작. `ARR=0`→벽까지 즉시. 둘 다 0→press 즉시 벽까지.

**7. 결정성**: `AdvanceStep`은 입력 버퍼 스냅샷 + 내부 타이머만으로 결정. Board/Piece 비참조(벽 클램프는 FSM). 동일 (스텝단위) 입력 → 동일 명령.

### States and Transitions

`FTetrisHandling`의 방향 이동은 **DAS/ARR 상태기계**다 (이산 입력은 무상태 pass-through).

**내부 상태 변수**

| 변수 | 의미 |
|------|------|
| `ActiveDir` | 현재 자동반복 대상 방향 (None/Left/Right) |
| `DASCharge` | press 후 경과 스텝 (DAS 충전) |
| `ARRCounter` | 직전 반복 후 경과 스텝 |
| `bLeftHeld` / `bRightHeld` | held 상태(입력 버퍼에서 갱신) |

**상태**: `Idle` → `Charging`(DAS 충전) → `Repeating`(ARR 반복)

| From | 이벤트/조건 | Action | To |
|------|------------|--------|-----|
| `Idle` | 방향 press | **1칸 방출**, `ActiveDir=dir`, `DASCharge=0` | `Charging` |
| `Charging` | held 지속, `DASCharge < DASSteps` | `DASCharge++` | `Charging` |
| `Charging` | `DASCharge >= DASSteps` | **첫 자동반복 즉시 방출** 1칸 (`ARR=0`→벽까지), `ARRCounter=0` | `Repeating` |
| `Repeating` | `ARRCounter < ARRSteps` | `ARRCounter++` | `Repeating` |
| `Repeating` | `ARRCounter >= ARRSteps` | **1칸 방출**, `ARRCounter=0` (`ARR=0`이면 매 스텝 벽까지) | `Repeating` |
| `Charging`/`Repeating` | 반대 방향 press (전환) | **1칸(새 dir) 방출**, `ActiveDir=new`, DAS 재설정(DCD 반영) | `Charging` |
| `Charging`/`Repeating` | `ActiveDir` 해제 & 반대 held | 반대 방향으로 전환(press처럼: 1칸+DAS 시작) | `Charging` |
| `Charging`/`Repeating` | 모든 방향 해제 | 정지 | `Idle` |

**리셋**: 피스 스폰/게임 시작 시 `ActiveDir=None`, 타이머 0 (held는 유지 — 누르고 있으면 다음 피스도 이어서 이동, Tetr.io류). ← "스폰 시 held 유지" 여부는 Open Question 참조.

### Interactions with Other Systems

**구성요소 & 소유**

| 요소 | 도메인 | 소유 | 역할 |
|------|--------|------|------|
| `ATetrisPlayerController` | frame | (엔진) | IMC/IA 바인딩 → Session 입력 버퍼에 held/edge 기록. 번역만. |
| `FTetrisHandling` | fixed-step | **Session** | 순수 DAS/ARR 상태기계 + `FHandlingConfig`. |
| `FHandlingInput` 버퍼 | 핸드오프 | **Session** | PC가 쓰고 Session이 매 Step 스냅샷·드레인. |

**Upstream (Input이 의존)**

| 대상 | 인터페이스 | 상태 |
|------|-----------|------|
| GameCore (FSM) | `EnqueueCommand(EGameCommand)` | ✅ 구현 |
| Session | `GetGameCore()` + 고정스텝 루프 | ✅ 구현(확장 예정) |
| Enhanced Input Plugin | `IMC_Gameplay`, `IA_*` (에셋) | 플러그인 활성 필요 |

**Downstream (Input에 의존)**

| 시스템 | 사용 | 상태 |
|--------|------|------|
| Settings/Options | `FHandlingConfig`(DAS/ARR/DCD) 런타임 쓰기 | ⚠️ VS, 미설계 |

**통합 — `UTetrisSessionSubsystem` 확장 (이번 Phase의 정당한 수정 지점)**
세션의 `AdvanceFixedSteps` 루프가 각 Step **직전**에 핸들링을 구동하도록 확장:
```
// 매 고정 스텝:
const FHandlingInput Snap = ConsumeInputBuffer();           // PC가 채운 버퍼 스냅샷+드레인
for (EGameCommand C : Handling.AdvanceStep(Snap))           // 순수 핸들링 → 명령
    GameCore->EnqueueCommand(C);
GameCore->Step();                                           // 기존 호출
```
- `ATetrisPlayerController`는 `BeginPlay`에서 `World->GetSubsystem<UTetrisSessionSubsystem>()`로 세션을 찾아 입력을 기록(`Session->SetMoveHeld()/PushInputEdge()`).

**데이터 흐름**
```
키 입력 → PlayerController(frame) → Session 입력버퍼(held+edge)
                                        │ (매 고정 Step, GameCore->Step() 직전)
                                        ▼
                     FTetrisHandling.AdvanceStep → EGameCommand[] → GameCore.EnqueueCommand → Step()
```

## Formulas

**H1. ms → 스텝 환산 (결정성, Lock Delay와 동일 방식)**
```
DASSteps = round(DASms / 1000 × SimHz)
ARRSteps = round(ARRms / 1000 × SimHz)     // ARR=0은 특수(벽까지) — 아래 H4
DCDSteps = clamp(round(DCDms / 1000 × SimHz), 0, DASSteps)
예) @60Hz: DAS 100ms→6, ARR 17ms→1, DCD 0ms→0
```

**H2. Charging → Repeating 전이**
```
DASCharge가 매 스텝 +1; (DASCharge >= DASSteps)이면 Repeating 진입
```

**H3. ARR 반복 (Repeating)**
```
ARRCounter 매 스텝 +1; (ARRCounter >= ARRSteps)이면 1칸 방출 + ARRCounter=0
```

**H4. ARR=0 — 즉시 벽까지**
```
ARRSteps == 0 → Repeating의 매 스텝마다 ActiveDir로 BoardWidth(=10)개 방출
              (FSM이 TryMove 실패 시 무해 클램프 → 벽에 정렬)
```

**H5. DCD — 방향 전환 시 DAS 부분 보존**
```
방향 전환 시: 새 dir 1칸 즉시 방출 후 DASCharge = DASSteps - DCDSteps  (∈ [0, DASSteps])
- DCD=0     → DASCharge=DASSteps → 즉시 Repeating(풀 DAS 유지, 스냅 전환)
- DCD=DAS   → DASCharge=0       → 풀 재충전(새 press처럼)
```

**변수 정의 / 범위**

| 변수 | 의미 | 기본 | 범위 | 소유 |
|------|------|------|------|------|
| `DASms` | 자동 이동 시작 지연 | 100 | 0~500 | Input |
| `ARRms` | 반복 간격 (0=즉시 벽) | 17 | 0~200 | Input |
| `DCDms` | 방향 전환 DAS 컷 | 0 | 0~DASms | Input |
| `SimHz` | 시뮬 주파수 | 60 | 파생 | FSM/Session |
| `SoftDropFactor` (SDF) | 소프트드롭 중력 배수 | 20(∞ 옵션) | 1~∞ | **GameCore** (참조) |

## Edge Cases

**1. 같은 스텝에 좌+우 동시 press**
- edge 큐 순서상 **나중 것이 ActiveDir**(last-input priority). 둘 다 held면 마지막 누른 방향 유지.

**2. press-release가 한 스텝 사이에 (빠른 탭)**
- edge 큐에 press가 기록되므로 **1칸 이동 보장**(유실 없음). held는 release로 false → 반복 없음.

**3. DAS 충전 중 키 뗐다 재押**
- 새 press edge → `DASCharge=0` 재시작 (재탭은 항상 1칸 + 풀 DAS).

**4. ARR=0인데 이미 벽에 붙음**
- `BoardWidth`개 방출하나 FSM이 전부 무해 클램프 → 실 이동 0. 정상.

**5. 한 스텝에 회전/하드드롭/홀드 다수**
- 모두 순서 보존해 방출. FSM이 처리(HardDrop에서 드레인 break — fsm.md Edge 1). Handling은 순서만 책임.

**6. SoftDrop held 중 피스 교체/스폰**
- GameCore의 `bSoftDropHeld`(`SoftDropOn/Off`로 갱신) 유지 → 다음 피스도 소프트드롭 지속(누르고 있는 한). Handling은 held 추적 안 함. 정상.

**7. Idle/Pause 중 입력**
- Session이 `bRunning=false`면 `AdvanceStep` 미호출. **게임 시작(StartGame) 시 입력 버퍼·핸들링 상태 클리어**(직전 잔여 입력 무시).

**8. 빠른 연타로 edge 큐 누적**
- 다음 스텝에서 전부 드레인 → 결정적. 한 스텝에 회전 다수면 다 방출(과회전 가능하나 재현 가능).

**9. 런타임 `SimHz` 변경**
- `DASSteps` 등 재환산 필요. 진행 중 타이머 해석 꼬임 방지 위해 **변경 시 핸들링 리셋** 권장(Open Question).

**10. 결정성 경계 (리플레이/넷코드)**
- "동일 입력 → 동일 명령"은 **스텝 단위 `FHandlingInput`** 기준으로만 성립한다. raw 프레임 → 고정스텝 그룹핑은 프레임레이트 의존(비결정 경계 — 120fps에선 두 탭이 두 스텝, 30fps에선 한 스텝에 묶일 수 있음). 따라서 리플레이/넷코드는 raw 프레임 입력이 아니라 **스텝 단위 `FHandlingInput`을 기록·재생**한다.

## Dependencies

**Upstream (Input이 의존)**

| 대상 | 유형 | 인터페이스 | 양방향 확인 |
|------|------|-----------|------------|
| FSM (GameCore) | Hard | `EnqueueCommand(EGameCommand)` | ✅ fsm.md가 Input을 upstream으로 명시 |
| Session | Hard | `GetGameCore()` + 고정스텝 루프에서 `Handling.AdvanceStep` 구동 | ✅ ADR-0001이 "후속: Input 명령 큐 연결" 명시 |
| Enhanced Input | Hard | `IMC_Gameplay`, `IA_*` 에셋 + `EnhancedInput` 모듈 | ⚙️ 모듈은 Build.cs에 이미 포함, 플러그인 기본 활성 — 빌드 시 확인 |

**Downstream (Input에 의존)**

| 시스템 | 유형 | 사용 |
|--------|------|------|
| Settings/Options | Soft | `FHandlingConfig`(DAS/ARR/DCD) 런타임 쓰기 (VS, 미설계) |

**호스팅 관계 (composition)**
- `UTetrisSessionSubsystem`가 `FTetrisHandling`(순수)과 `FHandlingInput` 버퍼를 **소유·구동**한다. (Lock Delay가 GameCore에 소유되는 것과 동일 패턴)

**공유 타입 (신규, Input 소유)**
- `FHandlingConfig { int32 DASms, ARRms, DCDms }` (USTRUCT, EditDefaultsOnly)
- `FHandlingInput { bool bLeftHeld, bRightHeld; TArray<EInputEdge> Edges; }` (소프트드롭 held는 GameCore 소유 — edge pass-through)
- `EInputEdge { MoveLeftPress, MoveRightPress, MoveLeftRelease, MoveRightRelease, RotateCW, RotateCCW, HardDrop, Hold, SoftDropOn, SoftDropOff }`
- 기존 재사용: `EGameCommand`(fsm.md)

**양방향 일관성 후속 (TODO)**
- `systems-index.md` Input 행 Depends On `—` → "FSM, Session" 보강.
- (선택) ADR-0001 Related에 input-handling.md 역링크.

## Tuning Knobs

| 변수 | 기본 | 안전 범위 | 영향 / 한계 시 증상 |
|------|------|----------|---------------------|
| `DASms` | 100 | 0~500 | 자동 이동 시작 지연. 낮으면 살짝만 눌러도 주르륵(과민), 높으면 미세 조정은 쉬우나 빠른 이동 둔함 |
| `ARRms` | 17 | 0~200 | 반복 간격. 0=벽까지 즉시(고수용), 높으면 느린 반복. SimHz 해상도(≈1스텝=16.7ms@60)보다 작으면 1스텝으로 라운딩 |
| `DCDms` | 0 | 0~`DASms` | 방향 전환 DAS 컷. 0=스냅 전환(풀 DAS 유지), `DASms`=전환마다 풀 재충전 |
| 키 바인딩 | `IMC_Gameplay` 에셋 | — | IA→키 매핑. 리매핑은 Settings 단계(에디터 에셋 + 런타임 override) |

**외부 소유 — 참조만**

| 변수 | 소유 |
|------|------|
| `SoftDropFactor` (SDF) | GameCore (소프트드롭 중력 배수, ∞=즉시 바닥) |
| `SimHz` | Session/FSM (ms→스텝 환산 해상도 결정) |

**상호작용 주의:**
- `ARRms=0` + 낮은 `DASms` → 벽-투-벽 순간이동이 매우 민감 → 초보자에겐 과조작. 기본값은 보수적으로.
- `DCDms`는 `DASms` 이하로 클램프(H5) — 초과 설정 무의미.
- `SimHz`를 낮추면 DAS/ARR의 ms 해상도가 거칠어짐(30Hz면 ≈33ms 단위) → 핸들링 정밀도 저하.

## Acceptance Criteria

**DAS/ARR 기본 (FTetrisHandling 단독)**
- [ ] 방향 press → 즉시 `MoveLeft/Right` 1개 방출 + `Charging` 진입
- [ ] held, `DASCharge < DASSteps` → 추가 방출 없음
- [ ] `DASSteps` 경과 → `Repeating`, 이후 `ARRSteps`마다 1칸
- [ ] `ARR=0` → Repeating 매 스텝 `BoardWidth`개 방출
- [ ] ms→스텝: DAS 100ms@60Hz → 6스텝째부터 반복

**방향/전환**
- [ ] 반대 방향 press → 새 dir 1칸 + DAS 재설정
- [ ] `DCD=0` → 전환 즉시 Repeating(풀 DAS 유지)
- [ ] `DCD=DAS` → 전환 후 풀 재충전
- [ ] 좌+우 동시 → last-input priority
- [ ] active 해제 & 반대 held → 반대 방향으로 전환(1칸+DAS 시작)

**이산 입력**
- [ ] Rotate/HardDrop/Hold/SoftDropOn/Off → 그대로 순서 보존 방출
- [ ] 한 스텝에 edge 다수 → 전부 순서대로

**탭/유실 방지**
- [ ] 한 스텝 사이 press→release(탭) → 정확히 1칸 (held=false라 반복 없음)

**격리 / 결정성**
- [ ] `FTetrisHandling` 단위 테스트가 PC/Session/Board 없이 통과(`FHandlingInput` 주입만)
- [ ] 동일 (스텝단위) 입력 시퀀스 → 동일 명령 시퀀스 (재현)

**통합**
- [ ] PC 입력 → 세션 고정스텝 → GameCore 피스가 기대대로 이동(통합 테스트 또는 PIE 육안)
- [ ] 회귀 없음: 기존 `Tetris.*` 전부 그린

**성능**
- [ ] `AdvanceStep()` < 2μs (산술 + 소수 명령 생성)

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|------|------|----------|
| 1 | 피스 스폰 시 held 유지 여부(다음 피스로 이동 이어짐) | 플레이테스트 | 구현 후 |
| 2 | 런타임 `SimHz` 변경 시 핸들링 리셋 정책 | Lead Programmer | 구현 단계 |
| 3 | DCD 체감이 Tetr.io와 일치하는지 (우리 정의 H5 검증) | 플레이테스트 | 구현 후 |
| 4 | DAS/ARR/SDF 기본값 최종 (Tetr.io 벤치마크) | 플레이테스트 | 구현 후 |
| 5 | 게임패드 지원(아날로그 → 방향, 입력 힌트 전환) | UX/접근성 | VS 단계 |
| 6 | 키 리매핑 런타임 override 메커니즘 | Settings GDD | VS 단계 |
| 7 | 180° 회전 명령 추가 시 `IA` + `EGameCommand` 확장 (fsm.md OQ7 연계) | Game Designer | VS 단계 |
