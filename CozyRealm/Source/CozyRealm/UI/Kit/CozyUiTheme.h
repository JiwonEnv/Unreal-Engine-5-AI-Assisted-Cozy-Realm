#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "UI/Kit/CozyUiTypes.h"
#include "UI/Kit/CozyUiScale.h"
#include "CozyUiTheme.generated.h"

class UCozyUiScreen;
class UTexture2D;

/** 한 장의 공유 텍스처 안에서 사용할 아이콘 영역. 좌표와 크기는 픽셀 단위다. */
USTRUCT(BlueprintType)
struct FCozyUiAtlasImage
{
	GENERATED_BODY()

	/** 여러 아이콘이 함께 들어 있는 게임용 텍스처 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Texture (공유 아이콘 시트)"))
	TObjectPtr<UTexture2D> Texture = nullptr;

	/** 시트 원본의 전체 크기. 처음 로드할 때의 임시 크기나 플랫폼별 축소 크기와 무관하게 같은 칸을 표시한다. 0이면 텍스처 크기를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Texture Size (시트 원본 크기)"))
	FVector2D TextureSize = FVector2D::ZeroVector;

	/** 아이콘 영역의 왼쪽 위 좌표 (픽셀). 시트 제작 도구가 자동으로 기록한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Origin (칸 시작 좌표)"))
	FVector2D Origin = FVector2D::ZeroVector;

	/** 표시할 영역 크기 (픽셀). 아이콘 사이의 안전 여백을 포함한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Size (칸 크기)"))
	FVector2D Size = FVector2D(128.f, 128.f);

	/** 기존 이미지의 색상 설정을 유지한다. 기본값은 원본 색상이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Tint (아이콘 색상)"))
	FSlateColor Tint = FLinearColor::White;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyUiAssetChanged, const UObject*);

/**
 *  공통 UI 테마 (모든 화면 공통 모습).
 *  글꼴 · 색 · 이미지 · 버튼 모양 · 게이지 모양 · 여백을 여기서 바꾸면 이 테마를 쓰는 모든 화면에 반영된다.
 *  에디터에서 값을 바꾸면 플레이 중에도 바로 반영된다 (카메라 설정 에셋과 같은 방식).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Cozy UI Theme (UI 테마)"))
class UCozyUiTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 글꼴 (제목 · 본문 · 숫자 · 작은 글 · 버튼) · 각각 글꼴 에셋 · 크기 · 굵기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 글꼴", meta = (DisplayName = "Fonts (글꼴)"))
	FCozyUiFonts Fonts;

	/** 색 (바탕 · 글자 · 테두리 · 강조 · 상태색) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 색", meta = (DisplayName = "Colors (색)"))
	FCozyUiColors Colors;

	/**
	 *  이미지 (창 배경 · 패널 · 칩 · 아이콘 등 하나하나 따로).
	 *  이름 → 이미지(텍스처 · 여백 · 그리기 방식). 화면 부품은 이 이름을 고른다. 이름을 추가하면 목록에 바로 나온다.
	 *  권장 이름: Window · Panel · Chip · Badge · Icon.Gold · Icon.Wheat …
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Images (이미지 이름표)"))
	TMap<FName, FSlateBrush> Images;

	/** 이름 → 공유 시트의 아이콘 칸. 같은 이름의 Images 항목보다 우선한다. 화면에서는 기존 이미지 이름을 그대로 선택한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지", meta = (DisplayName = "Atlas Images (공유 시트 아이콘 이름표)"))
	TMap<FName, FCozyUiAtlasImage> AtlasImages;

	/** 버튼 모양 (이름 → 기본 · 마우스 올림 · 누름 · 비활성 이미지와 여백) · 권장 이름: Default · Main · Danger · Round */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 버튼", meta = (DisplayName = "Button Styles (버튼 모양)"))
	TMap<FName, FButtonStyle> ButtonStyles;

	/** 버튼 상태별 글자 색 (비활성일 때) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 버튼", meta = (DisplayName = "Disabled Text Color (비활성 버튼 글자 색)"))
	FLinearColor DisabledTextColor = FLinearColor(0.6f, 0.58f, 0.55f);

	/** 게이지 모양 (이름 → 배경 · 채움 이미지) · 권장 이름: Default · Thin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게이지", meta = (DisplayName = "Gauge Styles (게이지 모양)"))
	TMap<FName, FCozyUiGaugeStyle> GaugeStyles;

	/**
	 *  요소 종류별 모양 틀 (Widget Blueprint · 버튼 · 글자 · 이미지 · 게이지 · 칩 · 정보 패널).
	 *  틀 안의 배치(아이콘과 글자 순서 · 여백)를 바꾸면 그 종류의 모든 요소가 바뀐다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7 요소 모양 틀", meta = (DisplayName = "Element Templates (요소 종류별 모양 틀)"))
	TMap<ECozyUiElementKind, TSoftClassPtr<class UCozyUiElementWidget>> ElementTemplates;

	/** 여백 · 간격 · 기본 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6 크기·여백", meta = (DisplayName = "Metrics (크기·여백·간격)"))
	FCozyUiMetrics Metrics;

	const FSlateBrush* FindImage(FName Name) const;
	const FButtonStyle* FindButtonStyle(FName Name) const;
	const FCozyUiGaugeStyle* FindGaugeStyle(FName Name) const;

	/** 에디터 드롭다운용 이름 목록 (모든 테마 에셋의 이름을 모음) */
	UFUNCTION()
	static TArray<FName> GetImageNameOptions();

	FOnCozyUiAssetChanged OnChanged;

private:
	// 화면에 건네는 브러시를 보관한다. 에셋에는 저장하지 않으며, 조회 시 최신 설정으로 갱신한다.
	mutable TMap<FName, FSlateBrush> AtlasBrushCache;

public:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/** 프로젝트 설정 'Cozy UI' · 기본 테마와 화면 블루프린트를 고른다 (코드에 에셋 경로를 두지 않음) */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cozy UI (UI 설정)"))
class UCozyUiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** 기본 테마 (화면 설정에서 따로 고르지 않으면 이것) */
	UPROPERTY(Config, EditAnywhere, Category = "1 테마 (Theme)", meta = (DisplayName = "Default Theme (기본 테마)"))
	TSoftObjectPtr<UCozyUiTheme> DefaultTheme;

	/** 위쪽 HUD 화면 (비우면 HUD가 뜨지 않고 오류 로그·알림으로 알려 줌) */
	UPROPERTY(Config, EditAnywhere, Category = "2 화면 (Screens)", meta = (DisplayName = "HUD Screen (위쪽 HUD 화면)"))
	TSoftClassPtr<UCozyUiScreen> HudScreenClass;

	/**
	 *  창 화면 (창 이름 → Widget Blueprint). 창은 이 화면으로만 뜬다 (틀 · 제목 · 닫기까지 화면이 그림 · 비어 있으면 오류 로그·알림).
	 *  창 이름: Storage · FacilityInfo · Processing · Sales · Upgrade · FieldManagement · Nagaya · OfflineReport · Shrine · Settings
	 *  그 밖의 화면: Placement (배치 모드 왼쪽 위 패널) · PlacementActions (배치 버튼) · FacilityMenu (시설 기능 아이콘)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "2 화면 (Screens)", meta = (DisplayName = "Window Screens (창별 화면)"))
	TMap<FName, TSoftClassPtr<UCozyUiScreen>> WindowScreens;

	/**
	 *  화면 크기별 UI 배율 (글자 · 버튼 · 창이 함께 커지고 작아짐).
	 *  화면의 짧은 쪽(보통 세로) 픽셀 → 배율 · 점 사이는 직선으로 이음 · 목록 밖은 끝 점 비율로 늘이거나 줄임.
	 *  기본: 720 → 0.78 (1280×720에서 덜 작아지게 · 엔진 기본은 0.67 · 0.8은 HUD 위쪽 줄이 겹쳐서 0.78) · 1080 → 1.0 (기준) · 2160 → 2.0
	 *  적용: Config/DefaultEngine.ini [/Script/Engine.UserInterfaceSettings] UIScaleRule=Custom · CustomScalingRuleClass=CozyDpiScalingRule
	 */
	UPROPERTY(Config, EditAnywhere, Category = "3 화면 배율 (UI Scale)", meta = (DisplayName = "Scale Points (화면 크기별 UI 배율)", TitleProperty = "ShortSide"))
	TArray<FCozyUiScalePoint> ScalePoints = { {720, 0.78f}, {1080, 1.f}, {2160, 2.f} };

	static const UCozyUiSettings* Get() { return GetDefault<UCozyUiSettings>(); }
	static UCozyUiTheme* LoadDefaultTheme();
};
