#pragma once

#include "CoreMinimal.h"
#include "Environment/CozyEstateTerrain.h"
#include "CozyMountainWorld.generated.h"

USTRUCT(BlueprintType)
struct FCozyWorldClearing
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Center = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector2D Radius = FVector2D(2000, 2000);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Falloff = 1600;
};

USTRUCT(BlueprintType)
struct FCozyWorldRoute
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> Points;
	/** 평평한 길의 절반 폭. 비탈과 이어지는 여유 폭은 별도다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="150")) float HalfWidth = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100")) float Falloff = 900;
};

/** 평지와 길을 배열로 관리하며, 생성된 실체는 편집 가능한 일반 Landscape다. */
UCLASS(meta=(DisplayName="산 월드 레이아웃 (시작·확장·길)"))
class COZYREALM_API ACozyMountainWorld : public ACozyEstateTerrain
{
	GENERATED_BODY()
public:
	ACozyMountainWorld();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="월드 구역") TArray<FCozyWorldClearing> Clearings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="월드 동선") TArray<FCozyWorldRoute> Routes;
	UPROPERTY(VisibleAnywhere, Category="월드 장식") TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> BroadTrees;
	UPROPERTY(VisibleAnywhere, Category="월드 장식") TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Pines;
	UPROPERTY(VisibleAnywhere, Category="월드 장식") TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Boulders;
	UPROPERTY(VisibleAnywhere, Category="월드 장식") TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Cliffs;
	/** 레이아웃의 높이는 GroundZ에 대한 상대 높이(cm). */
	UFUNCTION(BlueprintPure, Category="월드") float GetLayoutHeight(float X, float Y) const;
	UFUNCTION(BlueprintPure, Category="월드") float GetPathDistance(float X, float Y) const;
	virtual void BeginPlay() override;
protected:
#if WITH_EDITOR
	virtual float HeightAt(float X, float Y) const override;
	virtual float LayerWeightAt(float X, float Y, float Height, float Slope, FName Layer) const override;
#endif
private:
	float RouteBlend(float X, float Y, float Base) const;
};
