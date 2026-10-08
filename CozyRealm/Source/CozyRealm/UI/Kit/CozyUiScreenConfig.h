#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Kit/CozyUiTypes.h"
#include "UI/Kit/CozyUiTheme.h"
#include "CozyUiScreenConfig.generated.h"

/**
 *  화면 하나의 설정 (그 화면만 다른 것 · 화면 구성).
 *  - 테마를 통째로 바꾸거나, 색·글꼴 일부만 덮어쓴다.
 *  - 영역 목록: 자동 정렬 영역마다 방향(가로·세로·줄바꿈) · 간격 방식 · 맞춤.
 *  - 요소 목록: 버튼 · 글자 · 이미지 · 게이지 · 칩 · 정보 패널을 추가·삭제·숨김·순서 변경 · 게임 값·동작 연결.
 *  영역 상자와 자유 배치 요소의 위치·크기·화면 기준점은 이 에셋이 아니라 Widget Blueprint 디자이너에서 정한다 (서로 덮어쓰지 않음).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Cozy UI Screen Config (화면 설정)"))
class UCozyUiScreenConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 이 화면만 다른 테마 (비우면 프로젝트 설정의 기본 테마) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 테마", meta = (DisplayName = "Theme Override (이 화면만 다른 테마)"))
	TObjectPtr<UCozyUiTheme> ThemeOverride;

	/** 이 화면만 다른 색 (넣은 색만 덮어씀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 테마", meta = (DisplayName = "Color Overrides (이 화면만 다른 색)"))
	TMap<ECozyUiColor, FLinearColor> ColorOverrides;

	/** 이 화면만 다른 글꼴 (넣은 역할만 덮어씀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 테마", meta = (DisplayName = "Font Overrides (이 화면만 다른 글꼴)"))
	TMap<ECozyUiTextRole, FSlateFontInfo> FontOverrides;

	/** 자동 정렬 영역 목록 (영역 상자 WBP_UiArea의 Area Id와 이름을 맞춤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 영역", meta = (DisplayName = "Areas (자동 정렬 영역 목록)", TitleProperty = "{Id} · {Purpose} · {Flow} · {Spread}"))
	TArray<FCozyUiAreaLayout> Areas;

	/** 화면 구성 요소 목록 (줄 추가 = 요소 추가 · 줄 삭제 = 요소 삭제 · Visible 끄기 = 숨김 · Order = 순서) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3 요소", meta = (DisplayName = "Elements (화면 요소 목록)", TitleProperty = "{Id} · {Purpose} · {Kind} · {Area} {Order}"))
	TArray<FCozyUiElementEntry> Elements;

	const FCozyUiAreaLayout* FindArea(FName AreaId) const
	{
		return AreaId.IsNone() ? nullptr : Areas.FindByPredicate([AreaId](const FCozyUiAreaLayout& Each) { return Each.Id == AreaId; });
	}

	const FCozyUiElementEntry* FindElement(FName ElementId) const
	{
		return ElementId.IsNone() ? nullptr : Elements.FindByPredicate([ElementId](const FCozyUiElementEntry& Each) { return Each.Id == ElementId; });
	}

	FOnCozyUiAssetChanged OnChanged;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		OnChanged.Broadcast(this);
	}
#endif
};
