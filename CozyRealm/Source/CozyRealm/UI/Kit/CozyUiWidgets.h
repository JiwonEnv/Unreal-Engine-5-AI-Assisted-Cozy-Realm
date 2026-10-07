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

	/** 크기를 테마 아이콘 크기로 맞출지 (끄면 이미지 원래 크기 · 슬롯 크기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	bool bUseThemeIconSize = false;

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
 *  버튼 (Widget Blueprint WBP_UiButton의 부모).
 *  디자이너에 놓으면 '자유 배치 버튼': 위치·크기는 디자이너, ButtonId가 화면 설정에 있으면 글자·아이콘·표시·동작은 설정 값.
 *  버튼 영역이 만들면 '자동 정렬 버튼': 모든 값이 화면 설정 한 줄에서 옴.
 */
UCLASS(Abstract, meta = (DisplayName = "Cozy 버튼"))
class UCozyUiButton : public UUserWidget, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 화면 설정의 버튼 줄과 연결하는 ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	FName ButtonId;

	/** 설정에 없을 때 쓰는 값 (설정에 같은 ID가 있으면 설정 값이 우선) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	FCozyUiButtonEntry Defaults;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	/** 자동 정렬 영역이 만든 버튼이면 그 줄 · 아니면 설정에서 ButtonId로 찾음 */
	void SetEntry(const FCozyUiButtonEntry& InEntry) { Entry = InEntry; bHasEntry = true; }
	const FCozyUiButtonEntry& GetEffectiveEntry() const { return Entry; }

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleClicked();

private:
	FCozyUiButtonEntry Entry;
	bool bHasEntry = false;
	bool bFromArea = false;
	friend class UCozyUiButtonArea;
};

/**
 *  자동 정렬 버튼 영역 (WBP_UiButtonArea의 부모 · 안에 Box 패널 하나).
 *  영역의 위치·크기·앵커는 화면 디자이너에서, 안의 버튼 목록·순서는 화면 설정 Data Asset에서 정한다.
 *  Box를 가로 상자·세로 상자·줄바꿈 상자 중 무엇으로 두느냐로 정렬 방향이 정해진다.
 */
UCLASS(Abstract, meta = (DisplayName = "Cozy 버튼 영역"))
class UCozyUiButtonArea : public UUserWidget, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 이 영역 이름 · 화면 설정 버튼 줄의 Area와 같으면 여기 들어옴 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	FName AreaId;

	/** 만들 버튼 블루프린트 (WBP_UiButton 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	TSubclassOf<UCozyUiButton> ButtonClass;

	/** 버튼 사이 간격 (음수면 테마 간격) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	float Spacing = -1.f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box;

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCozyUiButton>> Spawned;
	FString BuiltSignature;
};

/**
 *  게이지 (WBP_UiGauge의 부모 · 안에 Bar 진행 막대 + 선택 ValueText 글자).
 *  모양(배경·채움 이미지)은 테마 게이지 모양, 크기·위치는 디자이너, 표시할 값은 목록에서 고른다.
 */
UCLASS(Abstract, meta = (DisplayName = "Cozy 게이지"))
class UCozyUiGauge : public UUserWidget, public ICozyUiElement
{
	GENERATED_BODY()

public:

	/** 테마 게이지 모양 이름 (비우면 Default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI", meta = (GetOptions = "CozyUiTheme.GetGaugeStyleOptions"))
	FName Style;

	/** 채움 색 (None이면 상태에 따라 · 진행 · 일시 정지 · 가득 참 · 잠김 · 수령 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	ECozyUiColor FillColor = ECozyUiColor::None;

	/** 채움 방향 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI")
	TEnumAsByte<EProgressBarFillType::Type> FillType = EProgressBarFillType::LeftToRight;

	/** 표시할 게임 값 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	ECozyUiValue Value = ECozyUiValue::ResourceAmount;

	/** 값 매개변수 (재료 ID · 시설 정의 ID) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	FName ValueParam;

	/** 숫자 표시 (현재/최대) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	bool bShowNumber = true;

	/** 퍼센트 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	bool bShowPercent = false;

	/** 남은 시간 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값")
	bool bShowRemaining = false;

	/** 디자이너에서 보일 채움 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cozy UI|값", meta = (ClampMin = "0", ClampMax = "1"))
	float DesignPercent = 0.6f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	virtual void ApplyCozyTheme(const UCozyUiScreen& Screen) override;
	virtual void UpdateCozyValue(const UCozyUiScreen& Screen) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("Cozy UI")); }
#endif

private:
	bool bTintByState = true;
};

/** 시간 · 수량 글자 도우미 */
namespace CozyUiFormat
{
	FText Duration(float Seconds);
}
