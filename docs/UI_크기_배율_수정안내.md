# UI 창 크기 · 화면 배율 수정 안내

2026-10-18 · 그래픽·UI 1 (UI 기능 · 배치 · 크기 확정) · 결정: 사용자 · 구현: Claude

코드나 노드를 고치지 않고 **에셋·설정 칸의 숫자만** 바꿔서 창 크기와 UI 배율을 고칠 수 있게 했다.

---

## 1. 창 크기 3종 — 테마 에셋에서

`Content/CozyRealm/UI/Theme/DA_UiTheme_CozyVillage` → **6 크기·여백 → Metrics → 창 크기 (Window Sizes)**

| 칸 | 지금 값 | 쓰는 창 |
|---|---|---|
| Window Small Width (작은 창 폭) | 520 | 방치 보상 |
| Window Medium Width (기본 창 폭) | 660 | 창고 · 주민 관리 · 시설 정보(밭) · 밭 관리 · 가공 · 판매 · 신사 |
| Window Large Width (큰 창 폭) | 720 | 업그레이드(후신소) · 설정 |

- 숫자를 바꾸면 같은 종류의 창이 모두 바뀐다. 플레이 중에도 바로 반영된다.
- 숫자는 1920×1080 기준이다. 다른 해상도에서는 아래 화면 배율만큼 함께 커지거나 작아진다.
- 창 높이는 내용만큼이다. 화면보다 길면 본문만 스크롤되고 아래 버튼 줄은 고정된다(10/16 수정).

### 어떤 창이 어떤 크기를 쓰는지 바꾸기

`Content/CozyRealm/UI/Screens/DA_UiScreen_<창 이름>` → **Elements → `Window`(Area = Root) → Size Mode**

- `Window Small (테마 작은 창 폭)` · `Window Medium (테마 기본 창 폭)` · `Window Large (테마 큰 창 폭)` 중에서 고른다.
- 이 창만 따로 폭을 정하려면 `Fixed (고정)`을 고르고 Size의 X에 폭을 넣는다.
- Size의 Y는 창 높이다. 0이면 내용에 맞춘다.

## 2. 화면 크기별 UI 배율 — 프로젝트 설정에서

**편집 → 프로젝트 설정 → Cozy UI (UI 설정) → 3 화면 배율 (UI Scale) → Scale Points**

| 화면 짧은 쪽 픽셀 | UI 배율 | 뜻 |
|---|---|---|
| 720 | 0.78 | 1280×720에서 글자·버튼·창이 기준의 0.78배 (엔진 기본 규칙은 0.67배였음) |
| 1080 | 1.0 | 1920×1080 = 기준 크기 |
| 2160 | 2.0 | 3840×2160(4K)에서 2배 |

- 결정은 '0.8배 정도'였는데, 0.8에서는 1280×720 HUD 위쪽 줄(왼쪽 칩 6개 + 오른쪽 버튼 5개)이 몇 픽셀 겹쳐서 0.78로 했다(2026-10-18 PIE 확인). HUD에 칩이나 버튼을 더 넣으면 이 값을 조금 낮추거나 HUD 배치를 바꿔야 한다.
- 화면의 짧은 쪽(보통 세로) 길이로 배율을 정한다. 점과 점 사이는 직선으로 잇는다. 예를 들어 900(1600×900)이면 약 0.89다.
- 목록보다 작거나 큰 화면은 끝 점의 비율대로 늘이거나 줄인다.
- 점을 추가하거나 숫자를 바꾸면 저장 위치는 `Config/DefaultGame.ini` → `[/Script/CozyRealm.CozyUiSettings]` → `ScalePoints`다.
- 이 규칙은 엔진 설정 `Config/DefaultEngine.ini` → `[/Script/Engine.UserInterfaceSettings]`에 연결돼 있다: `UIScaleRule=Custom` · `CustomScalingRuleClass=/Script/CozyRealm.CozyDpiScalingRule`. 이 두 줄을 지우면 엔진 기본 규칙(DPI 곡선)으로 돌아간다.

## 3. 글자 크기 — 테마 에셋에서 (원래 있던 칸)

`DA_UiTheme_CozyVillage` → **1 글꼴 → Fonts**: 제목 30 · 본문 22 · 숫자 28 · 작은 글 18 · 버튼 20 (1920×1080 기준 · 화면 배율이 함께 곱해짐)

- 글꼴 모양(주아)은 나중에 '그래픽 방향 확정' 카드에서 바꿀 수 있다. 같은 칸에서 글꼴 에셋만 바꾸면 된다.

## 4. HUD 왼쪽 아래 상태 판

- 사용자 결정으로 숨겼다. 시설 머리 위 상태 표시(그래픽·UI 3)가 이 역할을 대신한다.
- 되살리려면 `DA_UiScreen_HUD` → Elements → `StatusPanel` → **Visible**을 체크한다.

## 5. 코드 위치 (참고)

| 무엇 | 파일 |
|---|---|
| 창 크기 종류 (Size Mode 3개 추가) | `Source/CozyRealm/UI/Kit/CozyUiTypes.h` (`ECozyUiSizeMode` · `FCozyUiMetrics`) · 적용 `CozyUiWidgets.cpp` |
| 화면 배율 규칙 | `Source/CozyRealm/UI/Kit/CozyUiScale.h/.cpp` (`UCozyDpiScalingRule`) · 설정 칸 `CozyUiTheme.h` (`UCozyUiSettings::ScalePoints`) |
