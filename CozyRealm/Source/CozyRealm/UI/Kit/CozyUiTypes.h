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

/** 화면 설정의 버튼 한 줄 · 자동 정렬 영역에 들어가거나 자유 배치 버튼의 내용을 정한다 */
USTRUCT(BlueprintType)
struct FCozyUiButtonEntry
{
	GENERATED_BODY()

	/** 버튼 ID · 자유 배치 버튼은 같은 ID로 연결 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FName Id;

	/** 들어갈 자동 정렬 영역 이름 · 비우면 자유 배치 버튼의 내용으로만 씀 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FName Area;

	/** 영역 안 순서 (작을수록 앞) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	int32 Order = 0;

	/** 표시 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	bool bVisible = true;

	/** 버튼 글자 (비우면 글자 없음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FText Label;

	/** 아이콘 · 테마 이미지 이름 (예: Icon.Storage) · 비우면 아이콘 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button", meta = (GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName Icon;

	/** 버튼 모양 · 테마 버튼 모양 이름 (비우면 Default) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button", meta = (GetOptions = "CozyUiTheme.GetButtonStyleOptions"))
	FName Style;

	/** 크기 (0이면 테마 기본) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FVector2D Size = FVector2D::ZeroVector;

	/** 클릭 동작 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	ECozyUiAction Action = ECozyUiAction::None;

	/** 동작 매개변수 (지금은 쓰지 않음 · 나중 동작용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FName ActionParam;
};
