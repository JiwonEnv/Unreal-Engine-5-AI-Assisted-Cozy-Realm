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
	Title,
	/** 일반 문장 */
	Body,
	/** 숫자 (재화 · 수량 · 시간) */
	Number,
	/** 보조 설명 · 작은 글 */
	Small,
	/** 버튼 글자 */
	Button
};

/** 색 역할 · 테마의 색을 고르는 이름 */
UENUM(BlueprintType)
enum class ECozyUiColor : uint8
{
	/** 창 바탕 (종이) */
	Paper,
	/** 조금 짙은 바탕 (묶음 상자) */
	PaperDark,
	/** 글자 (먹) */
	Ink,
	/** 흐린 글자 */
	InkMuted,
	/** 테두리 (목재) */
	Border,
	/** 강조 (주홍 포인트) */
	Point,
	/** 강조 위 글자 */
	OnPoint,
	/** 상태: 진행 중 (오니비) */
	Progress,
	/** 상태: 일시 정지 */
	Paused,
	/** 상태: 충족 · 완료 */
	Ok,
	/** 상태: 잠김 */
	Locked,
	/** 상태: 공간 부족 · 경고 */
	Warning,
	/** 상태: 수령 가능 */
	Claimable,
	/** 색을 바꾸지 않음 (이미지 원래 색) */
	None
};

/** 표시할 게임 값 · 글자와 게이지가 목록에서 고른다 */
UENUM(BlueprintType)
enum class ECozyUiValue : uint8
{
	/** 연결 안 함 (디자이너에 쓴 글자 그대로) */
	None,
	/** 창고 수량 (매개변수: 재료 ID · 최대 = 창고 한도) */
	ResourceAmount,
	/** 신사 레벨 (최대 = 다음 단계가 있으면 +1) */
	ShrineLevel,
	/** 시설 레벨 상한 (= 신사 레벨) */
	FacilityLevelCap,
	/** 게임 시간 (글자 전용 · 낮/밤 · 시:분) */
	GameClock,
	/** 생산 진행률 (매개변수: 시설 정의 ID · 같은 시설이 여럿이면 첫 번째) */
	ProductionProgress,
	/** 미수령량 (매개변수: 시설 정의 ID · 최대 = 미수령 한도) */
	UnclaimedAmount,
	/** 가공 진행률 (매개변수: 시설 정의 ID · 첫 작업) */
	ProcessingProgress,
	/** 업그레이드 진행률 (첫 작업) */
	UpgradeProgress,
	/** 사용 중인 업그레이드 칸 (최대 = 칸 수) */
	UpgradeSlots,
	/** 보관함 시설 수 */
	StoredFacilities
};

/** 버튼 클릭 동작 · 이미 구현된 기능을 목록에서 고른다 */
UENUM(BlueprintType)
enum class ECozyUiAction : uint8
{
	None,
	/** 창고 창 (I) */
	OpenStorage,
	/** 주민(나가야) 창 (Tab) */
	OpenResidents,
	/** 배치 모드 켜기/끄기 (B) */
	TogglePlacement,
	/** 전부 수확 (Space) */
	CollectAll,
	/** 후신소 업그레이드 창 */
	OpenUpgrade,
	/** 디버그 창 (F1) */
	ToggleDebug,
	/** 열린 창 닫기 */
	CloseWindow,
	/** UI 미리보기 켜기/끄기 */
	TogglePreview
};

/** 미리보기 상태 · 실제 값 대신 보여 줄 가짜 상태 */
UENUM(BlueprintType)
enum class ECozyUiPreviewState : uint8
{
	/** 진행 중 (62%) */
	Progress,
	/** 일시 정지 (40%에서 멈춤) */
	Paused,
	/** 가득 참 (100%) */
	Full,
	/** 잠김 (0 · 회색) */
	Locked,
	/** 수령 가능 */
	Claimable,
	/** 비어 있음 (0) */
	Empty
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Font")
	FSlateFontInfo Title;

	/** 본문 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Font")
	FSlateFontInfo Body;

	/** 숫자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Font")
	FSlateFontInfo Number;

	/** 작은 글 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Font")
	FSlateFontInfo Small;

	/** 버튼 글자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Font")
	FSlateFontInfo Button;

	const FSlateFontInfo& Get(ECozyUiTextRole Role) const;
};

/** 테마 색 묶음 */
USTRUCT(BlueprintType)
struct FCozyUiColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Paper = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor PaperDark = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Ink = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor InkMuted = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Border = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Point = FLinearColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor OnPoint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Progress = FLinearColor::Blue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Paused = FLinearColor::Yellow;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Ok = FLinearColor::Green;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Locked = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Warning = FLinearColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color") FLinearColor Claimable = FLinearColor::Yellow;

	FLinearColor Get(ECozyUiColor Role) const;
};

/** 여백 · 간격 · 크기 */
USTRUCT(BlueprintType)
struct FCozyUiMetrics
{
	GENERATED_BODY()

	/** 창 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") FMargin WindowPadding = FMargin(24.f);
	/** 패널·칩 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") FMargin PanelPadding = FMargin(12.f, 6.f);
	/** 버튼 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") FMargin ButtonPadding = FMargin(14.f, 6.f);
	/** 자동 정렬 영역의 버튼 사이 간격 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") float Gap = 10.f;
	/** 버튼 기본 크기 (0이면 내용에 맞춤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") FVector2D ButtonSize = FVector2D(0.f, 52.f);
	/** 아이콘 기본 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") FVector2D IconSize = FVector2D(32.f, 32.f);
	/** 게이지 기본 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metrics") float GaugeHeight = 14.f;
};

/** 게이지 모양 (배경 · 채움 이미지와 여백 · 채움 색은 상태색) */
USTRUCT(BlueprintType)
struct FCozyUiGaugeStyle
{
	GENERATED_BODY()

	/** 배경·채움 이미지 (UE 진행 막대 스타일) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauge")
	FProgressBarStyle Style;

	/** 채움 색을 상태에 따라 바꿀지 (끄면 채움 이미지 색 그대로) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauge")
	bool bTintByState = true;
};

/** 화면 구성 요소의 종류 · 종류마다 공통 테마에 모양 틀(Widget Blueprint)이 있다 */
UENUM(BlueprintType)
enum class ECozyUiElementKind : uint8
{
	/** 버튼 (아이콘 · 글자 · 클릭 동작) */
	Button,
	/** 글자 (고정 글 또는 게임 값) */
	Text,
	/** 이미지 · 아이콘 */
	Image,
	/** 게이지 (제목 · 막대 · 숫자) */
	Gauge,
	/** 정보 칩 (배경 + 아이콘 + 글자 · 재화 표시 등) */
	Chip,
	/** 정보 패널 (배경 + 안쪽 영역 · 다른 요소를 담음) */
	Panel
};

/** 이미지를 정해진 칸에 넣는 방법 · 이미지 비율이 바뀌어도 칸 크기는 그대로 */
UENUM(BlueprintType)
enum class ECozyUiImageFit : uint8
{
	/** 비율 유지하며 칸 안에 맞추기 (남는 곳은 비움) */
	KeepRatio,
	/** 칸을 꽉 채우기 (비율 무시) */
	Stretch,
	/** 이미지 원래 크기 (칸 무시) */
	Original
};

/** 요소 크기 정하는 방법 */
UENUM(BlueprintType)
enum class ECozyUiSizeMode : uint8
{
	/** 내용에 맞춤 */
	Content,
	/** 고정 크기 (Size · 한쪽이 0이면 그쪽만 내용에 맞춤) */
	Fixed
};

/** 자동 정렬 방향 */
UENUM(BlueprintType)
enum class ECozyUiFlow : uint8
{
	/** 가로 한 줄 */
	Horizontal,
	/** 세로 한 줄 */
	Vertical,
	/** 가로로 놓다가 공간이 부족하면 줄바꿈 (영역 폭을 디자이너에서 정해야 함) */
	Wrap
};

/** 자동 정렬 간격 방식 */
UENUM(BlueprintType)
enum class ECozyUiSpread : uint8
{
	/** 붙여서 놓기 (간격 Gap) · 묶음 위치는 Pack Align */
	Packed,
	/** 모두 가장 큰 요소와 같은 크기로 (붙여서 · 묶음 위치는 Pack Align · Pack Align이 Fill이면 영역을 나눠 채움) */
	EqualSize,
	/** 양 끝에 붙이고 사이 간격을 균등하게 */
	EqualGap
};

/** 정렬 위치 */
UENUM(BlueprintType)
enum class ECozyUiAlign : uint8
{
	/** 앞 (왼쪽 · 위) */
	Start,
	/** 가운데 */
	Center,
	/** 뒤 (오른쪽 · 아래) */
	End,
	/** 늘려서 채우기 */
	Fill
};

/** 자동 정렬 영역 하나의 설정 · 영역 상자의 위치·크기는 Widget Blueprint 디자이너에서 */
USTRUCT(BlueprintType)
struct FCozyUiAreaLayout
{
	GENERATED_BODY()

	/** 영역 이름 · 디자이너의 영역 상자(WBP_UiArea)의 Area Id와 같게 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FName Id;

	/** 용도 설명 (알아보기 쉽게 · 동작에는 영향 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FText Purpose;

	/** 정렬 방향 (가로 · 세로 · 줄바꿈) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	ECozyUiFlow Flow = ECozyUiFlow::Horizontal;

	/** 간격 방식 (붙여서 · 같은 크기 · 균등 간격) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	ECozyUiSpread Spread = ECozyUiSpread::Packed;

	/** 요소 사이 간격 (-1이면 테마 Gap) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	float Gap = -1.f;

	/** 붙여서 놓을 때 묶음 위치 (가로면 왼쪽·가운데·오른쪽 · 세로면 위·가운데·아래) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	ECozyUiAlign PackAlign = ECozyUiAlign::Start;

	/** 줄 반대 방향 맞춤 (가로 줄이면 위·가운데·아래 · 세로 줄이면 왼쪽·가운데·오른쪽 · Fill이면 늘림) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	ECozyUiAlign ItemAlign = ECozyUiAlign::Center;

	/** 영역 안쪽 여백 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	FName Id;

	/** 용도 설명 (알아보기 쉽게 · 예: 골드 보유량) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	FText Purpose;

	/** 종류 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	ECozyUiElementKind Kind = ECozyUiElementKind::Button;

	/** 들어갈 자동 정렬 영역 (비우면 자유 배치 · 디자이너에 놓은 요소용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	FName Area;

	/** 영역 안 순서 (작을수록 앞) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	int32 Order = 0;

	/** 표시 여부 (끄면 자리도 차지하지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 기본")
	bool bVisible = true;

	/** 글자 · 형식 ({0} 현재 · {1} 최대 · {2} 퍼센트 · {3} 남은 시간 · {4} 상태 글) · 게이지는 제목 줄 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자")
	FText Label;

	/** 글꼴 역할 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자")
	ECozyUiTextRole TextRole = ECozyUiTextRole::Body;

	/** 글자 색 역할 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자")
	ECozyUiColor TextColor = ECozyUiColor::Ink;

	/** 긴 글을 자르지 않고 줄바꿈 (요소 폭이 정해져 있어야 함) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 글자")
	bool bWrapText = false;

	/** 아이콘·이미지 (테마 이미지 이름 · 비우면 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName Image;

	/** 이미지를 칸에 넣는 방법 (비율 유지 맞추기 · 꽉 채우기 · 원래 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지")
	ECozyUiImageFit ImageFit = ECozyUiImageFit::KeepRatio;

	/** 이미지 칸 크기 (0이면 테마 아이콘 크기) · 이미지 비율이 달라도 이 칸은 그대로 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지")
	FVector2D ImageBox = FVector2D::ZeroVector;

	/** 이미지 색 입히기 (None이면 원래 색) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지")
	ECozyUiColor ImageTint = ECozyUiColor::None;

	/** 배경 이미지 (칩·패널 · 테마 이미지 이름 · 예: Chip · Window · Panel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName Background;

	/** 버튼·게이지 모양 이름 (테마의 버튼 모양 · 게이지 모양 · 비우면 Default) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지")
	FName Style;

	/** 크기 방법 (내용에 맞춤 · 고정) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 크기")
	ECozyUiSizeMode SizeMode = ECozyUiSizeMode::Content;

	/** 고정 크기 (한쪽이 0이면 그쪽만 내용에 맞춤) · 자유 배치 요소는 디자이너 크기가 우선 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 크기")
	FVector2D Size = FVector2D::ZeroVector;

	/** 연결할 게임 값 (글자·칩·게이지) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	ECozyUiValue Value = ECozyUiValue::None;

	/** 값 매개변수 (재료 ID 예: Wheat · 시설 정의 ID 예: Field) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	FName ValueParam;

	/** 게이지 채움 방향 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	TEnumAsByte<EProgressBarFillType::Type> FillType = EProgressBarFillType::LeftToRight;

	/** 게이지 안에 숫자 (현재/최대) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	bool bShowNumber = false;

	/** 게이지 안에 퍼센트 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	bool bShowPercent = false;

	/** 게이지 안에 남은 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	bool bShowRemaining = false;

	/** 게이지 채움 색 (None이면 상태색: 진행 · 일시 정지 · 가득 참) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게임 값")
	ECozyUiColor FillColor = ECozyUiColor::None;

	/** 클릭 동작 (버튼) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6 동작")
	ECozyUiAction Action = ECozyUiAction::None;

	/** 동작 매개변수 (지금은 쓰지 않음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6 동작")
	FName ActionParam;

	/** 정보 패널 안쪽 영역 이름 (이 영역에 넣은 요소들이 패널 안에 놓임) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7 패널")
	FName ChildArea;

	/** 이 요소만 다른 모양 틀 (비우면 테마의 종류별 틀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "8 고급")
	TSoftClassPtr<class UCozyUiElementWidget> TemplateOverride;
};
