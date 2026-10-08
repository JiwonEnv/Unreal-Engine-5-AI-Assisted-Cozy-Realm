#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "UI/Kit/CozyUiTypes.h"
#include "CozyUiTheme.generated.h"

class UCozyUiTheme;
class UCozyUiScreenConfig;
class UCozyUiScreen;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCozyUiAssetChanged, const UObject*);

/**
 *  공통 UI 테마 (모든 화면 공통 모습).
 *  글꼴 · 색 · 이미지 · 버튼 모양 · 게이지 모양 · 여백을 여기서 바꾸면 이 테마를 쓰는 모든 화면에 반영된다.
 *  에디터에서 값을 바꾸면 플레이 중에도 바로 반영된다 (카메라 설정 에셋과 같은 방식).
 */
UCLASS(BlueprintType)
class UCozyUiTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 글꼴 (제목 · 본문 · 숫자 · 작은 글 · 버튼) · 각각 글꼴 에셋 · 크기 · 굵기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 글꼴")
	FCozyUiFonts Fonts;

	/** 색 (바탕 · 글자 · 테두리 · 강조 · 상태색) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 색")
	FCozyUiColors Colors;

	/**
	 *  이미지 (창 배경 · 패널 · 칩 · 아이콘 등 하나하나 따로).
	 *  이름 → 이미지(텍스처 · 여백 · 그리기 방식). 화면 부품은 이 이름을 고른다. 이름을 추가하면 목록에 바로 나온다.
	 *  권장 이름: Window · Panel · Chip · Badge · Icon.Gold · Icon.Wheat …
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 이미지")
	TMap<FName, FSlateBrush> Images;

	/** 버튼 모양 (이름 → 기본 · 마우스 올림 · 누름 · 비활성 이미지와 여백) · 권장 이름: Default · Main · Danger · Round */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 버튼")
	TMap<FName, FButtonStyle> ButtonStyles;

	/** 버튼 상태별 글자 색 (비활성일 때) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4 버튼")
	FLinearColor DisabledTextColor = FLinearColor(0.6f, 0.58f, 0.55f);

	/** 게이지 모양 (이름 → 배경 · 채움 이미지) · 권장 이름: Default · Thin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5 게이지")
	TMap<FName, FCozyUiGaugeStyle> GaugeStyles;

	/**
	 *  요소 종류별 모양 틀 (Widget Blueprint · 버튼 · 글자 · 이미지 · 게이지 · 칩 · 정보 패널).
	 *  틀 안의 배치(아이콘과 글자 순서 · 여백)를 바꾸면 그 종류의 모든 요소가 바뀐다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "7 요소 모양 틀")
	TMap<ECozyUiElementKind, TSoftClassPtr<class UCozyUiElementWidget>> ElementTemplates;

	/** 여백 · 간격 · 기본 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "6 크기·여백")
	FCozyUiMetrics Metrics;

	const FSlateBrush* FindImage(FName Name) const { return Name.IsNone() ? nullptr : Images.Find(Name); }
	const FButtonStyle* FindButtonStyle(FName Name) const;
	const FCozyUiGaugeStyle* FindGaugeStyle(FName Name) const;

	/** 에디터 드롭다운용 이름 목록 (모든 테마 에셋의 이름을 모음) */
	UFUNCTION()
	static TArray<FName> GetImageNameOptions();
	UFUNCTION()
	static TArray<FName> GetButtonStyleOptions();
	UFUNCTION()
	static TArray<FName> GetGaugeStyleOptions();

	FOnCozyUiAssetChanged OnChanged;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/** 프로젝트 설정 'Cozy UI' · 기본 테마와 화면 블루프린트를 고른다 (코드에 에셋 경로를 두지 않음) */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Cozy UI"))
class UCozyUiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** 기본 테마 (화면 설정에서 따로 고르지 않으면 이것) */
	UPROPERTY(Config, EditAnywhere, Category = "Theme")
	TSoftObjectPtr<UCozyUiTheme> DefaultTheme;

	/** 위쪽 HUD 화면 (비우면 기존 임시 HUD) */
	UPROPERTY(Config, EditAnywhere, Category = "Screens")
	TSoftClassPtr<UCozyUiScreen> HudScreenClass;

	/**
	 *  창 화면 (창 이름 → Widget Blueprint). 지정한 창은 예전 창 대신 이 화면을 띄운다 (틀 · 제목 · 닫기까지 화면이 그림).
	 *  창 이름: Storage · FacilityInfo · Processing · Sales · Upgrade · FieldManagement · Nagaya · OfflineReport · Placeholder
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Screens")
	TMap<FName, TSoftClassPtr<UCozyUiScreen>> WindowScreens;

	static const UCozyUiSettings* Get() { return GetDefault<UCozyUiSettings>(); }
	static UCozyUiTheme* LoadDefaultTheme();
};
