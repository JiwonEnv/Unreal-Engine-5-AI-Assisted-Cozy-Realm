#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "CozyGroundMesh.generated.h"

/** 언덕 상판·고정 경사로용 메시. 절벽 옆면은 건설 기울기 검사에서 제외된다. */
UCLASS()
class COZYREALM_API ACozyGroundMesh : public AStaticMeshActor
{
	GENERATED_BODY()
public:
	ACozyGroundMesh();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="표면") TObjectPtr<class UCozySurfaceComponent> Surface;
};
