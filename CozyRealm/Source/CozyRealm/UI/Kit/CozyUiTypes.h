#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Components/ProgressBar.h"
#include "CozyUiTypes.generated.h"

/** 글자 역할 · 테마의 글꼴을 고르는 이름 */
UENUM(BlueprintType)
enum class ECozyUiTextRole : uint8
{
	/** 창 제목 · 영지 이름 */
	Title UMETA(DisplayName = "Title (제목)"),
	/** 일반 문장 */
	Body UMETA(DisplayName = "Body (본문)"),
	/** 숫자 (재화 · 수량 · 시간) */
	Number UMETA(DisplayName = "Number (숫자)"),
	/** 보조 설명 · 작은 글 */
	Small UMETA(DisplayName = "Small (작은 글)"),
	/** 버튼 글자 */
	Button UMETA(DisplayName = "Button (버튼 글자)")
};

/** 색 역할 · 테마의 색을 고르는 이름 */
UENUM(BlueprintType)
enum class ECozyUiColor : uint8
{
	/** 창 바탕 (종이) */
	Paper UMETA(DisplayName = "Paper (종이 바탕)"),
	/** 조금 짙은 바탕 (묶음 상자) */
	PaperDark UMETA(DisplayName = "Paper Dark (짙은 바탕)"),
	/** 글자 (먹) */
	Ink UMETA(DisplayName = "Ink (글자)"),
	/** 흐린 글자 */
	InkMuted UMETA(DisplayName = "Ink Muted (흐린 글자)"),
	/** 테두리 (목재) */
	Border UMETA(DisplayName = "Border (테두리)"),
	/** 강조 (주홍 포인트) */
	Point UMETA(DisplayName = "Point (강조)"),
	/** 강조 위 글자 */
	OnPoint UMETA(DisplayName = "On Point (강조 위 글자)"),
	/** 상태: 진행 중 (오니비) */
	Progress UMETA(DisplayName = "Progress (진행 중)"),
	/** 상태: 일시 정지 */
	Paused UMETA(DisplayName = "Paused (일시 정지)"),
	/** 상태: 충족 · 완료 */
	Ok UMETA(DisplayName = "Ok (충족·완료)"),
	/** 상태: 잠김 */
	Locked UMETA(DisplayName = "Locked (잠김)"),
	/** 상태: 공간 부족 · 경고 */
	Warning UMETA(DisplayName = "Warning (가득 참·경고)"),
	/** 상태: 수령 가능 */
	Claimable UMETA(DisplayName = "Claimable (수령 가능)"),
	/** 색을 바꾸지 않음 (이미지 원래 색) */
	None UMETA(DisplayName = "None (바꾸지 않음·상태색)")
};

/** 표시할 게임 값 · 글자와 게이지가 목록에서 고른다 */
UENUM(BlueprintType)
enum class ECozyUiValue : uint8
{
	/** 연결 안 함 (디자이너에 쓴 글자 그대로) */
	None UMETA(DisplayName = "None (연결 안 함)"),
	/** 창고 수량 (매개변수: 재료 ID · 최대 = 창고 한도) */
	ResourceAmount UMETA(DisplayName = "Resource Amount (창고 수량)"),
	/** 신사 레벨 (최대 = 다음 단계가 있으면 +1) */
	ShrineLevel UMETA(DisplayName = "Shrine Level (신사 레벨)"),
	/** 시설 레벨 상한 (= 신사 레벨) */
	FacilityLevelCap UMETA(DisplayName = "Facility Level Cap (시설 레벨 상한)"),
	/** 게임 시간 (글자 전용 · 낮/밤 · 시:분) */
	GameClock UMETA(DisplayName = "Game Clock (게임 시간)"),
	/** 생산 진행률 (매개변수: 시설 정의 ID · 같은 시설이 여럿이면 첫 번째) */
	ProductionProgress UMETA(DisplayName = "Production Progress (생산 진행률)"),
	/** 미수령량 (매개변수: 시설 정의 ID · 최대 = 미수령 한도) */
	UnclaimedAmount UMETA(DisplayName = "Unclaimed Amount (미수령량)"),
	/** 가공 진행률 (매개변수: 시설 정의 ID · 첫 작업) */
	ProcessingProgress UMETA(DisplayName = "Processing Progress (가공 진행률)"),
	/** 업그레이드 진행률 (첫 작업) */
	UpgradeProgress UMETA(DisplayName = "Upgrade Progress (업그레이드 진행률)"),
	/** 사용 중인 업그레이드 칸 (최대 = 칸 수) */
	UpgradeSlots UMETA(DisplayName = "Upgrade Slots (업그레이드 칸)"),
	/** 보관함 시설 수 */
	StoredFacilities UMETA(DisplayName = "Stored Facilities (보관 시설 수)"),
	/** 창고에 더 받을 수 있는 수량 (매개변수: 재료 ID · {4} = '더 받을 수 있음 N개' 또는 '가득 참' · 가득 차면 경고색) */
	StorageSpace UMETA(DisplayName = "Storage Space (창고 남은 공간)"),
	/** 시설 이름 (매개변수: 시설 · {0} 레벨 · {4} 이름 · 밭처럼 개별 레벨이 없는 시설은 {4}만) */
	FacilityName UMETA(DisplayName = "Facility Name (시설 이름·레벨)"),
	/** 배치 주민 (매개변수: 시설 · {0} 인원 · {1} 최대 · {4} 이름들) */
	Residents UMETA(DisplayName = "Residents (배치 주민)"),
	/** 미수령 품목의 창고 상태 (매개변수: 시설 · {0} 창고 보유 · {1} 창고 한도 · {4} '지금 수령 가능 N개' 안내) */
	UnclaimedStorage UMETA(DisplayName = "Unclaimed Storage (미수령 품목 창고 상태)"),
	/** 공통 관리 효과 (매개변수: 시설 · {4} '생산 속도 ×1.2 (밭 관리 시설 Lv2)') */
	GrowthEffect UMETA(DisplayName = "Growth Effect (공통 관리 효과)"),
	/** 키우는 작물 (매개변수: 시설 · {4} 작물 이름) */
	CurrentCrop UMETA(DisplayName = "Current Crop (키우는 작물)"),
	/** 작물 선택지 하나 (매개변수: 작물 ID · 반복 목록 안에서 @Row · {4} '밀 (키우는 중)' · 고를 수 없으면 잠김색) */
	CropOption UMETA(DisplayName = "Crop Option (작물 선택지)"),
	/** 밭 관리 상태 (매개변수: 시설 · {0} 단계 · {4} 속도·작물·다음 단계 안내 여러 줄) */
	FieldManagement UMETA(DisplayName = "Field Management (밭 관리 상태)"),
	/** 방치 보상 요약 ({4} '자리를 비운 동안: 3시간' 등) */
	OfflineSummary UMETA(DisplayName = "Offline Summary (방치 보상 요약)"),
	/** 주민의 배치 상태 (매개변수: 주민 · 반복 목록 안에서 @Row · {4} '배치: 밭' / '미배치 (나가야)') */
	ResidentPlacement UMETA(DisplayName = "Resident Placement (주민 배치 상태)"),
	/** 나가야 창에서 배치할 시설 ({4} '배치할 시설: 제분소' · 바로가기로 열지 않았으면 빈칸) */
	WindowTarget UMETA(DisplayName = "Window Target (배치할 시설)"),
	/** 방금 한 일 (버튼 결과 안내 · {4}) */
	Feedback UMETA(DisplayName = "Feedback (방금 한 일)")
};

/** 반복 목록의 출처 · 줄 수가 게임 상태에 따라 바뀌는 목록 */
UENUM(BlueprintType)
enum class ECozyUiListSource : uint8
{
	None UMETA(DisplayName = "None (없음)"),
	/** 창고의 모든 아이템 (재료 + 재화) */
	StorageItems UMETA(DisplayName = "Storage Items (창고 전체)"),
	/** 창고의 재료만 (한도가 있는 것) */
	StorageMaterials UMETA(DisplayName = "Storage Materials (창고 재료)"),
	/** 창고의 재화만 (골드 · 부적 등) */
	StorageCurrencies UMETA(DisplayName = "Storage Currencies (창고 재화)"),
	/** 판매할 수 있는 재료 */
	SaleItems UMETA(DisplayName = "Sale Items (판매 가능 재료)"),
	/** 보관함에 들어간 시설 */
	StoredFacilities UMETA(DisplayName = "Stored Facilities (보관 시설)"),
	/** 방치 보상 결과 줄 (줄 이름 = 결과 글) */
	OfflineReportLines UMETA(DisplayName = "Offline Report Lines (방치 보상 결과)"),
	/** 모든 주민 (줄 ID = 주민 ID · 이름) */
	Residents UMETA(DisplayName = "Residents (주민)"),
	/** 창 시설이 고를 수 있는 작물 (줄 ID = 작물 ID) */
	WindowCrops UMETA(DisplayName = "Window Crops (창 시설의 작물)"),
	/** 주민 한 줄 안에서: 그 주민을 보낼 수 있는 시설 (줄 ID = '주민|시설' · 이름 = '제분소로 옮기기' 등 버튼 글자) */
	AssignTargets UMETA(DisplayName = "Assign Targets (주민을 보낼 시설)")
};

/** 배경 안쪽 여백 */
UENUM(BlueprintType)
enum class ECozyUiPadding : uint8
{
	/** 자동 (배경이 없으면 0 · 정보 패널은 창 여백 · 그 밖은 패널 여백) */
	Auto UMETA(DisplayName = "Auto (자동)"),
	/** 없음 */
	None UMETA(DisplayName = "None (없음)"),
	/** 테마의 창 여백 */
	Window UMETA(DisplayName = "Window (창 여백)"),
	/** 테마의 패널·칩 여백 */
	Panel UMETA(DisplayName = "Panel (패널 여백)"),
	/** 테마의 버튼 여백 */
	Button UMETA(DisplayName = "Button (버튼 여백)")
};

/** 버튼 클릭 동작 · 이미 구현된 기능을 목록에서 고른다 */
UENUM(BlueprintType)
enum class ECozyUiAction : uint8
{
	None UMETA(DisplayName = "None (동작 없음)"),
	/** 창고 창 (I) */
	OpenStorage UMETA(DisplayName = "Open Storage (창고 창)"),
	/** 주민(나가야) 창 (Tab) */
	OpenResidents UMETA(DisplayName = "Open Residents (주민 창)"),
	/** 배치 모드 켜기/끄기 (B) */
	TogglePlacement UMETA(DisplayName = "Toggle Placement (배치 모드)"),
	/** 전부 수확 (Space) */
	CollectAll UMETA(DisplayName = "Collect All (전부 수확)"),
	/** 후신소 업그레이드 창 */
	OpenUpgrade UMETA(DisplayName = "Open Upgrade (후신소 업그레이드 창)"),
	/** 디버그 창 (F1) */
	ToggleDebug UMETA(DisplayName = "Toggle Debug (디버그 창)"),
	/** 열린 창 닫기 */
	CloseWindow UMETA(DisplayName = "Close Window (창 닫기)"),
	/** UI 미리보기 켜기/끄기 */
	TogglePreview UMETA(DisplayName = "Toggle Preview (UI 미리보기)"),
	/** 창 시설의 미수령분 수령 */
	CollectWindow UMETA(DisplayName = "Collect Window (창 시설 수령)"),
	/** 창 시설에 주민 배치하러 나가야로 */
	OpenNagayaForWindow UMETA(DisplayName = "Open Nagaya For Window (창 시설 주민 배치)"),
	/** 방치 보상 확인 (닫기) */
	ConfirmOfflineReport UMETA(DisplayName = "Confirm Offline Report (방치 보상 확인)"),
	/** 작물 바꾸기 (Action Param: 작물 ID · 목록 안에서 @Row) */
	SelectCrop UMETA(DisplayName = "Select Crop (작물 바꾸기)"),
	/** 주민 배치 (Action Param: '주민|시설' · AssignTargets 목록 안에서 @Row) */
	AssignResident UMETA(DisplayName = "Assign Resident (주민 배치)"),
	/** 주민 배치 해제 (Action Param: 주민 ID · 목록 안에서 @Row) */
	UnassignResident UMETA(DisplayName = "Unassign Resident (주민 배치 해제)")
};

/** 미리보기 상태 · 실제 값 대신 보여 줄 가짜 상태 */
UENUM(BlueprintType)
enum class ECozyUiPreviewState : uint8
{
	/** 진행 중 (62%) */
	Progress UMETA(DisplayName = "Progress (진행 중)"),
	/** 일시 정지 (40%에서 멈춤) */
	Paused UMETA(DisplayName = "Paused (일시 정지)"),
	/** 가득 참 (100%) */
	Full UMETA(DisplayName = "Full (가득 참)"),
	/** 잠김 (0 · 회색) */
	Locked UMETA(DisplayName = "Locked (잠김)"),
	/** 수령 가능 */
	Claimable UMETA(DisplayName = "Claimable (수령 가능)"),
	/** 비어 있음 (0) */
	Empty UMETA(DisplayName = "Empty (비어 있음)")
};

/** 반복 목록의 한 줄 (ID · 표시 이름) */
struct FCozyUiListRow
{
	FName Id;
	FText Name;
};

/** 값 하나 (현재 · 최대 · 남은 시간 · 상태) */
USTRUCT(BlueprintType)
struct FCozyUiValueResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Value")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Value")
	float Current = 0.f;

	/** 0이면 최대가 없음 (재화 등) */
	UPROPERTY(BlueprintReadOnly, Category = "Value")
	float Max = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Value")
	float RemainingSeconds = 0.f;

	/** 값을 대신하는 글자 (게임 시간 · 잠김 등) */
	UPROPERTY(BlueprintReadOnly, Category = "Value")
	FText Text;

	/** 표시 상태 → 게이지 채움 색 (진행 · 일시 정지 · 가득 참 · 잠김 · 수령 가능) */
	UPROPERTY(BlueprintReadOnly, Category = "Value")
	ECozyUiColor StateColor = ECozyUiColor::Progress;

	float Ratio() const { return Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f; }
};

/** 테마 글꼴 묶음 */
USTRUCT(BlueprintType)
struct FCozyUiFonts
{
	GENERATED_BODY()

	/** 제목 (글꼴 에셋 · 크기 · 굵기) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "글꼴 (Font)", meta = (DisplayName = "Title (제목)"))
	FSlateFontInfo Title;

	/** 본문 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "글꼴 (Font)", meta = (DisplayName = "Body (본문)"))
	FSlateFontInfo Body;

	/** 숫자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "글꼴 (Font)", meta = (DisplayName = "Number (숫자)"))
	FSlateFontInfo Number;

	/** 작은 글 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "글꼴 (Font)", meta = (DisplayName = "Small (작은 글)"))
	FSlateFontInfo Small;

	/** 버튼 글자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "글꼴 (Font)", meta = (DisplayName = "Button (버튼 글자)"))
	FSlateFontInfo Button;

	const FSlateFontInfo& Get(ECozyUiTextRole Role) const;
};

/** 테마 색 묶음 */
USTRUCT(BlueprintType)
struct FCozyUiColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Paper (창 바탕 · 종이)", ToolTip = "창 바탕색")) FLinearColor Paper = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Paper Dark (짙은 바탕)", ToolTip = "묶음 상자 등 조금 짙은 바탕색")) FLinearColor PaperDark = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Ink (글자 · 먹)", ToolTip = "기본 글자색")) FLinearColor Ink = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Ink Muted (흐린 글자)", ToolTip = "보조 설명 등 흐린 글자색")) FLinearColor InkMuted = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Border (테두리 · 목재)", ToolTip = "테두리색")) FLinearColor Border = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Point (강조 · 주홍)", ToolTip = "제목 등 강조색")) FLinearColor Point = FLinearColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "On Point (강조 위 글자)", ToolTip = "강조색 위에 올리는 글자색")) FLinearColor OnPoint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Progress (상태: 진행 중)", ToolTip = "게이지 상태색: 진행 중")) FLinearColor Progress = FLinearColor::Blue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Paused (상태: 일시 정지)", ToolTip = "게이지 상태색: 일시 정지")) FLinearColor Paused = FLinearColor::Yellow;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Ok (상태: 충족·완료)", ToolTip = "상태색: 충족 · 완료")) FLinearColor Ok = FLinearColor::Green;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Locked (상태: 잠김)", ToolTip = "상태색: 잠김")) FLinearColor Locked = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Warning (상태: 가득 참·경고)", ToolTip = "상태색: 공간 부족 · 경고")) FLinearColor Warning = FLinearColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "색 (Color)", meta = (DisplayName = "Claimable (상태: 수령 가능)", ToolTip = "상태색: 수령 가능")) FLinearColor Claimable = FLinearColor::Yellow;

	FLinearColor Get(ECozyUiColor Role) const;
};

/** 여백 · 간격 · 크기 */
USTRUCT(BlueprintType)
struct FCozyUiMetrics
{
	GENERATED_BODY()

	/** 창 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Window Padding (창 안쪽 여백)")) FMargin WindowPadding = FMargin(24.f);
	/** 패널·칩 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Panel Padding (패널·칩 안쪽 여백)")) FMargin PanelPadding = FMargin(12.f, 6.f);
	/** 버튼 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Button Padding (버튼 안쪽 여백)")) FMargin ButtonPadding = FMargin(14.f, 6.f);
	/** 자동 정렬 영역의 버튼 사이 간격 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Gap (간격)")) float Gap = 10.f;
	/** 버튼 기본 크기 (0이면 내용에 맞춤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Button Size (버튼 기본 크기)")) FVector2D ButtonSize = FVector2D(0.f, 52.f);
	/** 아이콘 기본 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Icon Size (아이콘 칸 기본 크기)")) FVector2D IconSize = FVector2D(32.f, 32.f);
	/** 게이지 기본 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "크기·여백 (Metrics)", meta = (DisplayName = "Gauge Height (게이지 기본 높이)")) float GaugeHeight = 14.f;
};

/** 게이지 모양 (배경 · 채움 이미지와 여백 · 채움 색은 상태색) */
USTRUCT(BlueprintType)
struct FCozyUiGaugeStyle
{
	GENERATED_BODY()

	/** 배경·채움 이미지 (UE 진행 막대 스타일) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "게이지 (Gauge)", meta = (DisplayName = "Style (버튼·게이지 모양 이름)"))
	FProgressBarStyle Style;

	/** 채움 색을 상태에 따라 바꿀지 (끄면 채움 이미지 색 그대로) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "게이지 (Gauge)", meta = (DisplayName = "Tint By State (상태색으로 칠하기)"))
	bool bTintByState = true;
};

/** 화면 구성 요소의 종류 · 종류마다 공통 테마에 모양 틀(Widget Blueprint)이 있다 */
UENUM(BlueprintType)
enum class ECozyUiElementKind : uint8
{
	/** 버튼 (아이콘 · 글자 · 클릭 동작) */
	Button UMETA(DisplayName = "Button (버튼)"),
	/** 글자 (고정 글 또는 게임 값) */
	Text UMETA(DisplayName = "Text (글자)"),
	/** 이미지 · 아이콘 */
	Image UMETA(DisplayName = "Image (이미지)"),
	/** 게이지 (제목 · 막대 · 숫자) */
	Gauge UMETA(DisplayName = "Gauge (게이지)"),
	/** 정보 칩 (배경 + 아이콘 + 글자 · 재화 표시 등) */
	Chip UMETA(DisplayName = "Chip (정보 칩)"),
	/** 정보 패널 (배경 + 안쪽 영역 · 다른 요소를 담음) */
	Panel UMETA(DisplayName = "Panel (정보 패널)"),
	/** 반복 목록 (목록 출처의 줄마다 '한 줄 영역'의 요소들을 만듦 · 예: 창고 재료 목록) */
	List UMETA(DisplayName = "List (반복 목록)")
};

/** 이미지를 정해진 칸에 넣는 방법 · 이미지 비율이 바뀌어도 칸 크기는 그대로 */
UENUM(BlueprintType)
enum class ECozyUiImageFit : uint8
{
	/** 비율 유지하며 칸 안에 맞추기 (남는 곳은 비움) */
	KeepRatio UMETA(DisplayName = "Keep Ratio (비율 유지 맞추기)"),
	/** 칸을 꽉 채우기 (비율 무시) */
	Stretch UMETA(DisplayName = "Stretch (꽉 채우기)"),
	/** 이미지 원래 크기 (칸 무시) */
	Original UMETA(DisplayName = "Original (원래 크기)")
};

/** 요소 크기 정하는 방법 */
UENUM(BlueprintType)
enum class ECozyUiSizeMode : uint8
{
	/** 내용에 맞춤 */
	Content UMETA(DisplayName = "Content (내용에 맞춤)"),
	/** 고정 크기 (Size · 한쪽이 0이면 그쪽만 내용에 맞춤) */
	Fixed UMETA(DisplayName = "Fixed (고정)"),
	/** 줄의 남은 공간 채우기 (가로 줄이면 남은 폭 · 긴 글은 Wrap Text와 함께 쓰면 줄 안에서 줄바꿈) */
	Fill UMETA(DisplayName = "Fill (남은 공간 채우기)")
};

/** 자동 정렬 방향 */
UENUM(BlueprintType)
enum class ECozyUiFlow : uint8
{
	/** 가로 한 줄 */
	Horizontal UMETA(DisplayName = "Horizontal (가로)"),
	/** 세로 한 줄 */
	Vertical UMETA(DisplayName = "Vertical (세로)"),
	/** 가로로 놓다가 공간이 부족하면 줄바꿈 (영역 폭을 디자이너에서 정해야 함) */
	Wrap UMETA(DisplayName = "Wrap (줄바꿈)")
};

/** 자동 정렬 간격 방식 */
UENUM(BlueprintType)
enum class ECozyUiSpread : uint8
{
	/** 붙여서 놓기 (간격 Gap) · 묶음 위치는 Pack Align */
	Packed UMETA(DisplayName = "Packed (붙여서)"),
	/** 모두 가장 큰 요소와 같은 크기로 (붙여서 · 묶음 위치는 Pack Align · Pack Align이 Fill이면 영역을 나눠 채움) */
	EqualSize UMETA(DisplayName = "Equal Size (같은 크기)"),
	/** 양 끝에 붙이고 사이 간격을 균등하게 */
	EqualGap UMETA(DisplayName = "Equal Gap (균등 간격)")
};

/** 정렬 위치 */
UENUM(BlueprintType)
enum class ECozyUiAlign : uint8
{
	/** 앞 (왼쪽 · 위) */
	Start UMETA(DisplayName = "Start (앞)"),
	/** 가운데 */
	Center UMETA(DisplayName = "Center (가운데)"),
	/** 뒤 (오른쪽 · 아래) */
	End UMETA(DisplayName = "End (뒤)"),
	/** 늘려서 채우기 */
	Fill UMETA(DisplayName = "Fill (채우기)")
};

/** 자동 정렬 영역 하나의 설정 · 영역 상자의 위치·크기는 Widget Blueprint 디자이너에서 */
USTRUCT(BlueprintType)
struct FCozyUiAreaLayout
{
	GENERATED_BODY()

	/** 영역 이름 · 디자이너의 영역 상자(WBP_UiArea)의 Area Id와 같게 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Id (이름)"))
	FName Id;

	/** 용도 설명 (알아보기 쉽게 · 동작에는 영향 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Purpose (용도)"))
	FText Purpose;

	/** 정렬 방향 (가로 · 세로 · 줄바꿈) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Flow (정렬 방향)"))
	ECozyUiFlow Flow = ECozyUiFlow::Horizontal;

	/** 간격 방식 (붙여서 · 같은 크기 · 균등 간격) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Spread (간격 방식)"))
	ECozyUiSpread Spread = ECozyUiSpread::Packed;

	/** 요소 사이 간격 (-1이면 테마 Gap) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Gap (간격)"))
	float Gap = -1.f;

	/** 붙여서 놓을 때 묶음 위치 (가로면 왼쪽·가운데·오른쪽 · 세로면 위·가운데·아래) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Pack Align (묶음 위치)"))
	ECozyUiAlign PackAlign = ECozyUiAlign::Start;

	/** 줄 반대 방향 맞춤 (가로 줄이면 위·가운데·아래 · 세로 줄이면 왼쪽·가운데·오른쪽 · Fill이면 늘림) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Item Align (줄 반대 방향 맞춤)"))
	ECozyUiAlign ItemAlign = ECozyUiAlign::Center;

	/** 영역 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "영역 (Area)", meta = (DisplayName = "Padding (안쪽 여백)"))
	FMargin Padding = FMargin(0.f);
};

/**
 *  화면 구성 요소 한 줄 (버튼 · 글자 · 이미지 · 게이지 · 칩 · 정보 패널).
 *  Area를 고르면 그 자동 정렬 영역에 Order 순서로 만들어진다 (줄 추가·삭제 = 요소 추가·삭제).
 *  Area를 비우면 디자이너에 놓은 같은 Id의 자유 배치 요소에 내용만 준다 (위치·크기는 디자이너).
 */
USTRUCT(BlueprintType)
struct FCozyUiElementEntry
{
	GENERATED_BODY()

	/** 요소 이름 (영어 · 자유 배치 요소는 디자이너의 Element Id와 같게) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본", meta = (DisplayName = "Id (이름)"))
	FName Id;

	/** 용도 설명 (알아보기 쉽게 · 예: 골드 보유량) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본", meta = (DisplayName = "Purpose (용도)"))
	FText Purpose;

	/** 종류 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본", meta = (DisplayName = "Kind (종류)"))
	ECozyUiElementKind Kind = ECozyUiElementKind::Button;

	/** 들어갈 자동 정렬 영역 (비우면 자유 배치 · 디자이너에 놓은 요소용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본", meta = (DisplayName = "Area (들어갈 영역)"))
	FName Area;

	/** 영역 안 순서 (작을수록 앞) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본", meta = (DisplayName = "Order (순서)"))
	int32 Order = 0;

	/** 표시 여부 (끄면 자리도 차지하지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본", meta = (DisplayName = "Visible (표시)"))
	bool bVisible = true;

	/** 글자 · 형식 ({0} 현재 · {1} 최대 · {2} 퍼센트 · {3} 남은 시간 · {4} 상태 글) · 게이지는 제목 줄 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자", meta = (DisplayName = "Label (글자·형식)"))
	FText Label;

	/** 글꼴 역할 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자", meta = (DisplayName = "Text Role (글꼴 역할)"))
	ECozyUiTextRole TextRole = ECozyUiTextRole::Body;

	/** 글자 색 역할 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자", meta = (DisplayName = "Text Color (글자 색)"))
	ECozyUiColor TextColor = ECozyUiColor::Ink;

	/** 긴 글을 자르지 않고 줄바꿈 (요소 폭이 정해져 있어야 함) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자", meta = (DisplayName = "Wrap Text (줄바꿈)"))
	bool bWrapText = false;

	/** 아이콘·이미지 (테마 이미지 이름 · 비우면 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Image (이미지 이름표)", GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName Image;

	/** 이미지를 칸에 넣는 방법 (비율 유지 맞추기 · 꽉 채우기 · 원래 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Image Fit (이미지 맞춤)"))
	ECozyUiImageFit ImageFit = ECozyUiImageFit::KeepRatio;

	/** 이미지 칸 크기 (0이면 테마 아이콘 크기) · 이미지 비율이 달라도 이 칸은 그대로 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Image Box (이미지 칸 크기)"))
	FVector2D ImageBox = FVector2D::ZeroVector;

	/** 이미지 색 입히기 (None이면 원래 색) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Image Tint (이미지 색 입히기)"))
	ECozyUiColor ImageTint = ECozyUiColor::None;

	/** 배경 이미지 (칩·패널 · 테마 이미지 이름 · 예: Chip · Window · Panel) · 반복 목록이면 각 줄의 배경 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Background (배경 이미지)", GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName Background;

	/** 버튼·게이지 모양 이름 (테마의 버튼 모양 · 게이지 모양 · 비우면 Default) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Style (버튼·게이지 모양 이름)"))
	FName Style;

	/** 크기 방법 (내용에 맞춤 · 고정) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 크기", meta = (DisplayName = "Size Mode (크기 방법)"))
	ECozyUiSizeMode SizeMode = ECozyUiSizeMode::Content;

	/** 고정 크기 (한쪽이 0이면 그쪽만 내용에 맞춤) · 자유 배치 요소는 디자이너 크기가 우선 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 크기", meta = (DisplayName = "Size (크기)"))
	FVector2D Size = FVector2D::ZeroVector;

	/** 연결할 게임 값 (글자·칩·게이지) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Value (연결할 게임 값)"))
	ECozyUiValue Value = ECozyUiValue::None;

	/** 값 매개변수 (재료 ID 예: Wheat · 시설 정의 ID 예: Field) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Value Param (값 매개변수)"))
	FName ValueParam;

	/** 게이지 채움 방향 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Fill Type (채움 방향)"))
	TEnumAsByte<EProgressBarFillType::Type> FillType = EProgressBarFillType::LeftToRight;

	/** 게이지 안에 숫자 (현재/최대) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Show Number (숫자 표시)"))
	bool bShowNumber = false;

	/** 게이지 안에 퍼센트 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Show Percent (퍼센트 표시)"))
	bool bShowPercent = false;

	/** 게이지 안에 남은 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Show Remaining (남은 시간 표시)"))
	bool bShowRemaining = false;

	/** 게이지 채움 색 (None이면 상태색: 진행 · 일시 정지 · 가득 참) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값", meta = (DisplayName = "Fill Color (채움 색)"))
	ECozyUiColor FillColor = ECozyUiColor::None;

	/** 클릭 동작 (버튼) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6 동작", meta = (DisplayName = "Action (클릭 동작)"))
	ECozyUiAction Action = ECozyUiAction::None;

	/** 동작 매개변수 (지금은 쓰지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6 동작", meta = (DisplayName = "Action Param (동작 매개변수)"))
	FName ActionParam;

	/** 배경 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Padding Role (배경 안쪽 여백)"))
	ECozyUiPadding PaddingRole = ECozyUiPadding::Auto;

	/** 정보 패널: 안쪽 영역 이름 (이 영역에 넣은 요소들이 패널 안에 놓임) · 반복 목록: 줄들을 늘어놓는 영역 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7 패널·목록", meta = (DisplayName = "Child Area (안쪽 영역)"))
	FName ChildArea;

	/** 반복 목록: 줄 출처 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7 패널·목록", meta = (DisplayName = "List Source (목록 출처)"))
	ECozyUiListSource ListSource = ECozyUiListSource::None;

	/**
	 *  반복 목록: 한 줄 영역 이름. 이 영역에 넣은 요소들이 줄마다 만들어진다.
	 *  그 요소들의 글자에 {Row}를 쓰면 줄 이름(예: 밀), Value Param·Image에 @Row를 쓰면 줄 ID(예: Wheat → Icon.@Row = Icon.Wheat)로 바뀐다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7 패널·목록", meta = (DisplayName = "Row Area (한 줄 영역)"))
	FName RowArea;

	/** 이 요소만 다른 모양 틀 (비우면 테마의 종류별 틀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "8 고급", meta = (DisplayName = "Template Override (이 요소만 다른 모양 틀)"))
	TSoftClassPtr<class UCozyUiElementWidget> TemplateOverride;
};
