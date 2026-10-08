#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "UI/Kit/CozyUiTypes.h"
#include "CozyUiWidgets.generated.h"

class UCozyUiScreen;
class UCozyUiTheme;
class UCozyUiScreenConfig;
class UButton;
class UProgressBar;
class UPanelWidget;

/**
 *  편집 부품 공통 · 화면(UCozyUiScreen)이 테마를 적용하고 값을 갱신할 때 부른다.
 *  부품은 테마의 '역할 이름'만 고르고, 실제 글꼴·색·이미지는 테마 에셋에서 온다.
 */
UINTERFACE(MinimalAPI)
class UCozyUiElement : public UInterface
{
	GENERATED_BODY()
};

class ICozyUiElement
{
	GENERATED_BODY()

public:
	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) = 0;
	virtual void UpdateCozyValue(const UCozyUiScreen& Screen) {}
};

/** 글자 · 글꼴 역할과 색 역할을 고르고, 원하면 게임 값과 연결 */
UCLASS(meta = (DisplayName = "Cozy 글자"))
class UCozyUiText : public UTextBlock, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 글꼴 역할 (테마의 제목 · 본문 · 숫자 · 작은 글 · 버튼) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	ECozyUiTextRole Role = ECozyUiTextRole::Body;

	/** 글자 크기만 따로 (0이면 테마 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	int32 SizeOverride = 0;

	/** 색 역할 (None이면 디자이너에서 고른 색 그대로) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	ECozyUiColor ColorRole = ECozyUiColor::Ink;

	/** 표시할 게임 값 (None이면 디자이너에 쓴 글자 그대로) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	ECozyUiValue Value = ECozyUiValue::None;

	/** 값 매개변수 (재료 ID · 시설 정의 ID) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	FName ValueParam;

	/** 표시 형식 · {0} 현재 · {1} 최대 · {2} 퍼센트 · {3} 남은 시간 · {4} 값 글자 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	FText Format = FText::FromString(TEXT("{0}"));

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;
	virtual void UpdateCozyValue(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif
};

/** 이미지 · 테마 이미지 이름을 고름 (아이콘 · 장식) */
UCLASS(meta = (DisplayName = "Cozy 이미지"))
class UCozyUiImage : public UImage, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 테마 이미지 이름 (예: Icon.Gold) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI", meta = (GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName ImageName;

	/** 색 입히기 (None이면 이미지 원래 색) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	ECozyUiColor TintRole = ECozyUiColor::None;

	/** 크기를 테마 아이콘 크기 칸에 맞출지 (끄면 Fit Box 칸 · 둘 다 없으면 이미지 원래 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	bool bUseThemeIconSize = false;

	/** 칸 크기 (0이면 칸 없음 = 원래 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	FVector2D FitBox = FVector2D::ZeroVector;

	/** 칸에 넣는 방법 (비율 유지 맞추기 · 꽉 채우기 · 원래 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	ECozyUiImageFit Fit = ECozyUiImageFit::KeepRatio;

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif
};

/** 창 · 패널 · 칩 배경 (테마 이미지 + 테마 여백) · 안에 다른 부품을 하나 담음 */
UCLASS(meta = (DisplayName = "Cozy 배경 상자"))
class UCozyUiBorder : public UBorder, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 배경 이미지 이름 (예: Window · Panel · Chip) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI", meta = (GetOptions = "CozyUiTheme.GetImageNameOptions"))
	FName ImageName;

	/** 색 입히기 (None이면 이미지 원래 색) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	ECozyUiColor TintRole = ECozyUiColor::None;

	/** 안쪽 여백: 0 창 · 1 패널·칩 · 2 버튼 · 3 디자이너 값 그대로 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI", meta = (ClampMin = "0", ClampMax = "3"))
	int32 PaddingRole = 1;

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif
};

/**
 *  화면 구성 요소 (버튼 · 글자 · 이미지 · 게이지 · 칩 · 정보 패널의 모양 틀 WBP_Ui*의 부모).
 *  - 영역이 만든 요소: 화면 설정의 요소 한 줄이 모든 내용을 정하고, 위치는 영역 설정이 정한다.
 *  - 디자이너에 놓은 요소(자유 배치): Element Id로 화면 설정의 한 줄을 찾아 내용만 받고, 위치·크기·기준점은 디자이너가 정한다.
 *  틀 안에 있는 부품만 쓴다 (이름으로 연결 · 모두 선택 사항):
 *  SizeBox(전체 크기) · Button · Background(배경) · IconBox(이미지 칸) · Icon · Label(글자) · Bar(게이지 막대) · ValueText(막대 안 글자) · ChildArea(패널 안 영역)
 */
UCLASS(Abstract, meta = (DisplayName = "Cozy 화면 요소"))
class UCozyUiElementWidget : public UUserWidget, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 자유 배치용 · 화면 설정 요소 목록의 Id (영역이 만든 요소는 자동으로 채워짐) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy UI")
	FName ElementId;

	/** 화면 설정에 같은 Id가 없을 때 쓸 내용 (디자이너 미리보기용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cozy UI")
	FCozyUiElementEntry Defaults;

	/** 영역이 만든 요소의 내용 지정 */
	void SetEntry(const FCozyUiElementEntry& InEntry, bool bInFromArea) { Entry = InEntry; bFromArea = bInFromArea; }

	/** 반복 목록의 줄 (안쪽 영역에 {Row} · @Row로 전달) */
	void SetRow(FName InRowId, const FText& InRowName) { RowId = InRowId; RowName = InRowName; }
	const FCozyUiElementEntry& GetEntry() const { return Entry; }

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;
	virtual void UpdateCozyValue(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class USizeBox> SizeBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Background;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class USizeBox> IconBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UCozyUiArea> ChildArea;

private:
	UFUNCTION()
	void HandleClicked();

	FCozyUiElementEntry Entry;
	bool bFromArea = false;
	bool bTintByState = true;
	FName RowId;
	FText RowName;
};

/**
 *  자동 정렬 영역 (WBP_UiArea의 부모).
 *  디자이너에서는 이 상자의 위치·크기·화면 기준점만 정한다.
 *  안에 들어갈 요소와 정렬 방식(가로·세로·줄바꿈 · 같은 크기 · 균등 간격)은 화면 설정의 영역·요소 목록이 정한다.
 */
UCLASS(Abstract, meta = (DisplayName = "Cozy 자동 정렬 영역"))
class UCozyUiArea : public UUserWidget, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 영역 이름 (화면 설정 영역 목록의 Id) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	FName AreaId;

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;

	/** 이 영역이 놓인 줄 (반복 목록 안 · 안쪽 요소의 {Row} · @Row를 바꿈) */
	void SetRowContext(FName InRowId, const FText& InRowName) { RowId = InRowId; RowName = InRowName; }

	/** 반복 목록 모드: 출처의 줄마다 한 줄 영역(RowArea)을 담은 줄 상자를 만든다 · None이면 보통 영역 */
	void SetList(ECozyUiListSource InSource, FName InRowArea, FName InRowBackground) { ListSource = InSource; ListRowArea = InRowArea; ListRowBackground = InRowBackground; }

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif

protected:
	/** 요소를 담는 상자 (틀에 'Host'라는 이름의 Border) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Host;

private:
	FString BuiltSignature;
	FName RowId;
	FText RowName;
	ECozyUiListSource ListSource = ECozyUiListSource::None;
	FName ListRowArea;
	FName ListRowBackground;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiElementWidget>> Spawned;
};

/** 이미지 맞춤 · 칸 크기와 방법으로 그릴 크기를 정한다 */
namespace CozyUiFit
{
	FVector2D Resolve(const FSlateBrush& Brush, const FVector2D& Box, ECozyUiImageFit Fit);
}

/** 반복 목록 줄 바꿔 넣기 · 글자의 {Row} → 줄 이름, Value Param·Image·Background의 @Row → 줄 ID */
namespace CozyUiRow
{
	FCozyUiElementEntry Resolve(const FCozyUiElementEntry& In, FName RowId, const FText& RowName);
}

/** 시간 · 수량 글자 도우미 */
namespace CozyUiFormat
{
	FText Duration(float Seconds);
}
