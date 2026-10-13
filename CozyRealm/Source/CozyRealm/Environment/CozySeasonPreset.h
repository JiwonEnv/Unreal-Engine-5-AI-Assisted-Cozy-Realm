#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CozySeasonPreset.generated.h"

class UMaterialInstance;

/**
 *  계절 하나 (봄 · 여름 · 가을 · 겨울 …).
 *  지형 모습은 같은 기본 틀(M_CozyLandscape)에서 만든 계절별 머티리얼 인스턴스로 정한다 → 칠해 둔 재질 칸 위치는 그대로, 모습만 바뀐다.
 *  새 계절은 이 에셋을 하나 더 만들고 '영지 지형' 액터의 Seasons 목록에 넣으면 된다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Cozy Season Preset (계절)"))
class UCozySeasonPreset : public UDataAsset
{
	GENERATED_BODY()

public:

	/** 화면·디버그에 보일 이름 (예: 봄) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "계절", meta = (DisplayName = "Display Name (이름)"))
	FText DisplayName;

	/** 이 계절의 지형 모습 · 기본 틀 M_CozyLandscape의 인스턴스 (MI_CozyLandscape_봄 등) · 칸별 색·그림·눈 덮임은 이 인스턴스에서 바꾼다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "계절", meta = (DisplayName = "Landscape Material (지형 머티리얼)"))
	TObjectPtr<UMaterialInstance> LandscapeMaterial;
};
