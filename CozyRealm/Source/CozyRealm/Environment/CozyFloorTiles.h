#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyFloorTiles.generated.h"

USTRUCT()
struct FCozyFloorTile
{
	GENERATED_BODY()
	UPROPERTY() FGuid InstanceId;
	UPROPERTY() FGuid SurfaceId;
	UPROPERTY() FIntPoint Cell = FIntPoint::ZeroValue;
	UPROPERTY() FTransform Transform;
	UPROPERTY() int32 Style = 0;
	UPROPERTY() FName DefinitionId = TEXT("FloorWood");
	UPROPERTY() bool bStored = false;
};

/** 비구조 장식 바닥. 충돌과 길찾기를 바꾸지 않고 종류별 인스턴스로 그린다. */
UCLASS()
class COZYREALM_API ACozyFloorTiles : public AActor
{
	GENERATED_BODY()
public:
	ACozyFloorTiles();
	void Rebuild(const TArray<FCozyFloorTile>& Tiles);
private:
	UPROPERTY() TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Groups;
};
