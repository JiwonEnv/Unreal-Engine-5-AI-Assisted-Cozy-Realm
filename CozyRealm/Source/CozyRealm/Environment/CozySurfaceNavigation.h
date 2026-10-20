#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Environment/CozySurfaceComponent.h"
#include "CozySurfaceNavigation.generated.h"

/** 기존 칸 길찾기의 확장. 같은 XY의 각 표면을 별도 노드로 보관한다. */
UCLASS()
class COZYREALM_API UCozySurfaceNavigation : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	bool FindPath(const FVector& StartFeet, const FVector& GoalFeet, TArray<FVector>& OutPoints, const FGuid& IgnoreFacility = FGuid(), const FBox* ExtraBlock = nullptr);
	bool HasAccessibleEdge(const FVector& Min, const FVector2D& Size, float Height, const FGuid& IgnoreFacility);
	bool IsFree(const FVector& Feet, const FGuid& IgnoreFacility = FGuid()) const;
	bool FindNearestFree(const FVector& Feet, FVector& OutFeet);
	void Invalidate() { Nodes.Reset(); Columns.Reset(); }
	int32 GetNodeCount() const { return Nodes.Num(); }
private:
	struct FNode { FCozySurfaceHit Surface; FIntPoint Cell; TArray<int32> Neighbors; };
	TArray<FNode> Nodes;
	TMap<FIntPoint, TArray<int32>> Columns;
	void Build();
	bool Connects(const FCozySurfaceHit& A, const FCozySurfaceHit& B, const struct FCollisionQueryParams* Clearance = nullptr) const;
	bool FindSupportingSurface(const FVector& Feet, FCozySurfaceHit& Out) const;
};
