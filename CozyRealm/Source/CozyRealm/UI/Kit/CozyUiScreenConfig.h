#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Kit/CozyUiTypes.h"
#include "UI/Kit/CozyUiTheme.h"
#include "CozyUiScreenConfig.generated.h"

/**
 *  화면 하나의 설정 (그 화면만 다른 것).
 *  - 테마를 통째로 바꾸거나, 색·글꼴 일부만 덮어쓴다.
 *  - 버튼 목록: 추가·삭제·순서·글자·아이콘·크기·표시 여부·클릭 동작.
 *  위치·크기 배치는 이 에셋이 아니라 화면 Widget Blueprint 디자이너에서 정한다 (서로 덮어쓰지 않음).
 */
UCLASS(BlueprintType)
class UCozyUiScreenConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 이 화면만 다른 테마 (비우면 프로젝트 설정의 기본 테마) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 테마")
	TObjectPtr<UCozyUiTheme> ThemeOverride;

	/** 이 화면만 다른 색 (넣은 색만 덮어씀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 테마")
	TMap<ECozyUiColor, FLinearColor> ColorOverrides;

	/** 이 화면만 다른 글꼴 (넣은 역할만 덮어씀) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1 테마")
	TMap<ECozyUiTextRole, FSlateFontInfo> FontOverrides;

	/**
	 *  버튼 목록.
	 *  Area를 고른 버튼은 그 이름의 자동 정렬 영역에 Order 순서로 들어간다 (줄을 추가·삭제하면 버튼 수가 바뀜).
	 *  Area를 비운 버튼은 디자이너에 놓은 같은 ID의 자유 배치 버튼에 글자·아이콘·표시 여부·동작만 준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2 버튼", meta = (TitleProperty = "{Id} · {Area} · {Order} · {Label}"))
	TArray<FCozyUiButtonEntry> Buttons;

	FOnCozyUiAssetChanged OnChanged;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		OnChanged.Broadcast(this);
	}
#endif
};
