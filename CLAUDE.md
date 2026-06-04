# CLAUDE.md - UE5 테트리스 프로젝트 가이드

## 프로젝트 개요

UE5 기반 테트리스 게임. UMG와 Common UI에 대한 깊이 있는 이해를 포트폴리오로 보여주기 위한 프로젝트.
단순 기능 구현이 아니라 **UI 아키텍처 설계 능력**과 **엔진 내부 원리 이해**를 증명하는 것이 핵심 목표.

---

## 개발자 컨텍스트

- 6개월차 주니어 언리얼 게임 클라이언트 개발자
- 기존 포트폴리오: 게임플레이 로직, 보스 AI, 네트워크 동기화 중심 → **UI 경험이 부족**
- 이 프로젝트로 UMG/Common UI 전문성을 입증해야 함
- Rookiss UE5 Framework Insight 강의를 병행 수강 중 (엔진 내부 구조 학습)

---

## 기술 스택 및 규칙

### 엔진 및 언어
- Unreal Engine 5 (최신 안정 버전)
- C++ 중심 개발, Blueprint는 위젯 애니메이션/프로토타이핑에만 제한적 사용
- Common UI Plugin 활성화 필수

### 코딩 컨벤션
- 언리얼 코딩 스탠다드 준수 (Epic Games Coding Standard)
- 클래스 접두사: U (UObject), A (AActor), F (구조체), E (Enum), I (Interface)
- 헤더에 UCLASS, UPROPERTY, UFUNCTION 매크로 적극 활용
- 주석은 "왜(Why)" 중심으로 작성. "무엇(What)"은 코드 자체로 설명되게

### 폴더 구조
```
Source/
├── Tetris/
│   ├── Core/              # GameMode, GameState, GameInstance
│   ├── Board/             # 보드 로직, 블록 데이터, 충돌 판정
│   ├── Block/             # 테트로미노 정의, 회전, 이동
│   ├── Input/             # 입력 처리, Enhanced Input 매핑
│   ├── UI/
│   │   ├── Foundation/    # 기반 위젯 클래스 (프로젝트 공통 Base)
│   │   ├── Views/         # 각 화면별 위젯 (메뉴, 게임, 일시정지, 게임오버)
│   │   ├── Components/    # 재사용 UI 컴포넌트 (블록 미리보기, 점수판 등)
│   │   └── ViewModel/     # UI에 바인딩될 데이터 모델
│   └── System/            # 점수, 레벨, 설정 관리
Content/
├── UI/
│   ├── Widgets/           # Widget Blueprint 에셋
│   ├── Animations/        # Widget Animation 에셋
│   ├── Styles/            # CommonUI 스타일 데이터
│   └── InputActions/      # Common UI Input Action 데이터 에셋
├── Data/                  # DataTable, DataAsset
└── Maps/
```

---

## 아키텍처 설계 원칙

### MVVM 패턴 적용 (핵심)
이 프로젝트의 가장 중요한 설계 원칙. 게임 로직과 UI를 완전히 분리한다.

```
[Model]              [ViewModel]              [View]
보드 상태              UTetrisViewModel         UMG 위젯
블록 데이터     →      게임 상태 래핑       →    화면 표시
점수/레벨              이벤트 발행               애니메이션 재생
```

- **Model**: 순수 게임 로직. UI 코드를 절대 참조하지 않음
- **ViewModel**: Model의 상태를 UI가 소비할 수 있는 형태로 변환. Delegate로 변경 통지
- **View**: ViewModel만 알고 있음. Model을 직접 참조하지 않음

### 게임 로직과 UI의 분리 기준
- 보드의 2D 배열 데이터, 블록 회전/이동/충돌 판정 → Model (UI 무관하게 동작 가능해야 함)
- 점수 텍스트 업데이트, 블록 미리보기 렌더링, 화면 전환 → View
- "줄이 삭제됨" 이벤트 발행, 현재 점수/레벨 상태 노출 → ViewModel

### 왜 이 구조인가 (포트폴리오 설명용)
- 게임 로직 단위 테스트 가능
- UI 교체 시 ViewModel 이하 코드 변경 불필요
- 실무에서 기획 변경에 유연하게 대응 가능

---

## Common UI 구현 가이드

### 반드시 사용해야 할 Common UI 클래스들
- `UCommonActivatableWidget`: 모든 주요 화면의 베이스. 화면 스택 관리의 핵심
- `UCommonButtonBase`: 키보드/게임패드 자동 전환을 위한 버튼 베이스
- `UCommonUIActionRouterBase`: 입력 라우팅 시스템
- `FUIInputConfig`: 화면별 입력 설정 (게임 입력 vs UI 입력)

### 화면 스택 구조
```
[PrimaryGameLayout]
├── GameLayer_Menu        # 메인 메뉴, 설정
├── GameLayer_Game        # 게임 HUD
├── GameLayer_GameMenu    # 일시정지 메뉴 (게임 위에 오버레이)
└── GameLayer_Modal       # 팝업, 확인 다이얼로그
```

### 입력 관리 (차별화 포인트)
- 게임패드 네비게이션 완벽 지원 필수
- 키보드 ↔ 게임패드 전환 시 UI 힌트 자동 변경 (CommonUI InputAction 위젯 활용)
- 게임 플레이 중: 게임 입력 활성, UI 입력 비활성
- 일시정지/메뉴: UI 입력 활성, 게임 입력 비활성
- `GetDesiredInputConfig()` 오버라이드로 화면별 입력 모드 제어

### Common UI 도입 시 주의사항
- CommonActivatableWidget의 `NativeOnActivated` / `NativeOnDeactivated` 생명주기를 정확히 이해할 것
- Widget Pool에서 재사용되는 위젯은 상태 초기화를 반드시 처리
- Back 핸들링: `SetIsBackHandler(true)` + `OnHandleBackAction()` 오버라이드

---

## 테트리스 게임 로직 설계

### 보드 시스템
- 10x20 2D 배열 (표준 테트리스 규격)
- 셀 상태: Empty, Filled(블록 색상 정보 포함)
- 줄 완성 판정, 줄 삭제, 위 블록 낙하 로직은 순수 C++ 함수로 구현
- UI와 무관하게 콘솔에서도 동작 가능한 수준으로 분리

### 블록 (테트로미노) 시스템
- 7종 테트로미노를 DataAsset 또는 DataTable로 정의
- 각 블록의 회전 상태를 4x4 배열로 미리 정의 (SRS 회전 시스템 권장)
- Wall Kick 데이터도 데이터 주도 방식으로 관리
- 하드코딩 금지. 새로운 블록 추가 시 데이터만 추가하면 되는 구조

### 게임 플로우
```
시작 화면 → 게임 플레이 → 게임 오버 → 결과 → 시작 화면
                ↕
            일시정지 메뉴
                ↕
              설정 화면
```

### 점수 및 레벨 시스템
- 동시 줄 삭제 보너스 (1줄: 100, 2줄: 300, 3줄: 500, 4줄: 800)
- 레벨별 낙하 속도 증가
- 콤보 시스템 (연속 줄 삭제 시 배율)

---

## UI 구현 상세

### 게임 HUD 구성요소
- 보드 렌더링 (UMG 기반, Grid 또는 개별 블록 위젯)
- 다음 블록 미리보기 (Next Queue, 최소 3개)
- Hold 블록 표시
- 점수 / 레벨 / 삭제 줄 수 표시
- 콤보 연출 텍스트

### Widget Animation 적용 대상 (필수)
- 블록 착지 시 시각 피드백 (흔들림 또는 번쩍임)
- 줄 삭제 시 삭제 연출 (페이드아웃 + 위에서 떨어지는 효과)
- 콤보/보너스 텍스트 팝업 애니메이션
- 화면 전환 애니메이션 (메뉴 ↔ 게임, 게임 ↔ 게임오버)
- 레벨업 시 UI 연출

### 데이터 바인딩 방식
- ViewModel에서 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`로 변경 이벤트 선언
- View에서 `BindWidget`이 아닌 이벤트 기반으로 UI 업데이트
- 직접 참조(GetGameState→GetScore) 형태 금지

---

## Enhanced Input 설정

### Input Mapping Context 구성
- `IMC_Gameplay`: 블록 이동(좌/우), 회전(시계/반시계), 소프트드롭, 하드드롭, Hold
- `IMC_Menu`: UI 네비게이션, 확인, 취소, 일시정지
- 게임 상태에 따라 IMC 전환

### Input Action 정의
```
IA_MoveLeft      - 좌로 이동 (DAS/ARR 적용)
IA_MoveRight     - 우로 이동 (DAS/ARR 적용)
IA_RotateCW      - 시계방향 회전
IA_RotateCCW     - 반시계방향 회전
IA_SoftDrop      - 소프트 드롭 (누르고 있으면 가속)
IA_HardDrop      - 하드 드롭 (즉시 착지)
IA_Hold          - Hold 전환
IA_Pause         - 일시정지
```

### DAS/ARR 구현 (Delayed Auto Shift / Auto Repeat Rate)
- 키를 누르면 즉시 1칸 이동
- 일정 시간(DAS, 기본 170ms) 후 자동 반복 시작
- 반복 간격(ARR, 기본 50ms)으로 연속 이동
- 설정 화면에서 DAS/ARR 값 조정 가능하게 구현

---

## 포트폴리오 차별화 요소 체크리스트

### 필수 구현 (이것들이 없으면 프로젝트 의미가 반감)
- [ ] MVVM 패턴으로 게임 로직/UI 완전 분리
- [ ] Common UI 기반 화면 스택 관리 (CommonActivatableWidget)
- [ ] CommonButtonBase 기반 버튼 (키보드/게임패드 자동 전환)
- [ ] 게임패드 완벽 지원 (네비게이션, 입력 힌트 자동 전환)
- [ ] Widget Animation을 활용한 UI 연출 (최소 5가지 이상)
- [ ] 화면별 Input Routing (게임 입력 ↔ UI 입력 전환)
- [ ] 데이터 주도 설계 (블록 정의, 점수 테이블 등을 DataAsset으로 관리)

### 권장 구현 (있으면 깊이감 상승)
- [ ] Ghost Piece (착지 예측 표시)
- [ ] DAS/ARR 설정 가능
- [ ] 접근성 옵션 (색맹 모드, 키 리매핑)
- [ ] 사운드 매니저 (BGM, 효과음, 볼륨 설정)
- [ ] 로컬 랭킹 시스템 (SaveGame 활용)
- [ ] Lyra의 Common UI 사용 패턴 참고 및 문서화

### 금지 사항
- [ ] 게임 로직에서 UMG 위젯 직접 참조
- [ ] BeginPlay에 모든 초기화 코드 몰아넣기
- [ ] 하드코딩된 매직 넘버 (블록 데이터, 점수 값 등)
- [ ] Blueprint만으로 핵심 로직 구현
- [ ] Tick에서 매 프레임 UI 업데이트 (이벤트 기반으로 전환)

---

## 코드 리뷰 시 확인 포인트

코드를 작성하거나 리뷰할 때 항상 다음을 확인:

1. **이 코드가 UI와 게임 로직을 분리하고 있는가?**
2. **Common UI의 기능을 활용하고 있는가, 아니면 우회하고 있는가?**
3. **이 구현을 면접에서 "왜 이렇게 했는지" 설명할 수 있는가?**
4. **데이터 주도 방식인가, 하드코딩인가?**
5. **게임패드에서도 정상 동작하는가?**

---

## 참고 자료 우선순위

1. Lyra 프로젝트의 UI 파트 (Common UI 활용 패턴 참고)
2. Epic Games 공식 Common UI 문서
3. Rookiss UE5 Framework Insight 강의 (프레임워크 이해)
4. UE5 소스 코드 - Slate/UMG 레이어

---

## 기술 블로그 작성 주제 (면접 대비)

프로젝트 진행 중 다음 주제들을 기술 블로그로 정리할 것:

1. 왜 MVVM을 선택했는가 (MVC와의 비교)
2. Common UI 도입 전후 비교 (기존 UMG 방식 vs Common UI)
3. 입력 시스템 설계 (Enhanced Input + Common UI Input Routing)
4. Widget Animation 활용 사례와 구현 방법
5. 트러블슈팅 기록 (문제 → 원인 분석 → 해결 과정)
