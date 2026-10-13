#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyEstateScenery.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 *  영지 둘레 풍경 (임시 모형 · 실제 에셋이 생기기 전 단계).
 *  영지 칸 바깥에 흙길 둘레 · 생울타리(입구 하나) · 입구 길 · 바깥 숲 · 개울을 엔진 기본 도형으로 깐다.
 *  값을 바꾸면 에디터에서 바로 다시 깔린다 (OnConstruction). 충돌은 모두 꺼서 시설 클릭·배치에 영향이 없다.
 *  영지 칸 안쪽(시설 배치 칸)에는 아무것도 놓지 않는다.
 */
UCLASS(meta = (DisplayName = "Cozy Estate Scenery (영지 둘레 풍경)"))
class ACozyEstateScenery : public AActor
{
	GENERATED_BODY()

public:

	ACozyEstateScenery();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 영지 칸 영역 절반 크기 (cm) · 영지 설정의 칸 수 × 칸 크기 ÷ 2 (지금 12칸 × 200cm → 1200) */
	UPROPERTY(EditAnywhere, Category = "1 영지", meta = (DisplayName = "Estate Half Size (영지 절반 크기)", ClampMin = "100"))
	float EstateHalfSize = 1200.f;

	/** 영지 바닥 윗면 높이 (cm) · 흙길·개울을 이 높이에 맞춰 깐다 */
	UPROPERTY(EditAnywhere, Category = "1 영지", meta = (DisplayName = "Ground Top Z (바닥 윗면 높이)"))
	float GroundTopZ = 20.f;

	/** 바깥 바닥 윗면 높이 (cm) · 숲·개울·입구 길이 놓이는 높이 */
	UPROPERTY(EditAnywhere, Category = "1 영지", meta = (DisplayName = "Outer Ground Z (바깥 바닥 높이)"))
	float OuterGroundZ = -0.5f;

	/** 영지 둘레 흙길 폭 (cm) · 0이면 둘레 길 없음 */
	UPROPERTY(EditAnywhere, Category = "2 흙길", meta = (DisplayName = "Ring Path Width (둘레 길 폭)", ClampMin = "0"))
	float RingPathWidth = 140.f;

	/** 입구 방향 (0 = +X · 90 = +Y · 180 = -X · 270 = -Y) · 기본은 카메라 쪽 (-X) */
	UPROPERTY(EditAnywhere, Category = "2 흙길", meta = (DisplayName = "Entrance Side (입구 방향, 도)"))
	int32 EntranceSideDegrees = 180;

	/** 입구 폭 (cm) · 생울타리가 끊기는 폭이자 입구 길 폭 */
	UPROPERTY(EditAnywhere, Category = "2 흙길", meta = (DisplayName = "Entrance Width (입구 폭)", ClampMin = "50"))
	float EntranceWidth = 300.f;

	/** 입구에서 바깥으로 이어지는 길 길이 (cm) */
	UPROPERTY(EditAnywhere, Category = "2 흙길", meta = (DisplayName = "Entrance Road Length (입구 길 길이)", ClampMin = "0"))
	float EntranceRoadLength = 2600.f;

	/** 생울타리 두께 (cm) */
	UPROPERTY(EditAnywhere, Category = "3 생울타리", meta = (DisplayName = "Hedge Thickness (두께)", ClampMin = "10"))
	float HedgeThickness = 70.f;

	/** 생울타리 높이 (cm) */
	UPROPERTY(EditAnywhere, Category = "3 생울타리", meta = (DisplayName = "Hedge Height (높이)", ClampMin = "10"))
	float HedgeHeight = 80.f;

	/** 생울타리 한 덩어리 길이 (cm) · 덩어리마다 높이가 조금씩 달라 손질한 울타리처럼 보임 */
	UPROPERTY(EditAnywhere, Category = "3 생울타리", meta = (DisplayName = "Hedge Segment Length (덩어리 길이)", ClampMin = "50"))
	float HedgeSegmentLength = 200.f;

	/** 나무 개수 */
	UPROPERTY(EditAnywhere, Category = "4 숲", meta = (DisplayName = "Tree Count (나무 개수)", ClampMin = "0", ClampMax = "400"))
	int32 TreeCount = 90;

	/** 생울타리 바깥에서 숲이 시작하는 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "4 숲", meta = (DisplayName = "Forest Inner Gap (숲 시작 거리)", ClampMin = "0"))
	float ForestInnerGap = 350.f;

	/** 숲이 끝나는 거리 (영지 중심에서 · cm) · 바깥 바닥보다 작게 */
	UPROPERTY(EditAnywhere, Category = "4 숲", meta = (DisplayName = "Forest Outer Extent (숲 끝 거리)", ClampMin = "500"))
	float ForestOuterExtent = 3800.f;

	/** 나무 크기 범위 (기본 크기 대비 배율) */
	UPROPERTY(EditAnywhere, Category = "4 숲", meta = (DisplayName = "Tree Scale Range (나무 크기 범위)"))
	FVector2D TreeScaleRange = FVector2D(0.8f, 1.35f);

	/** 배치 무작위 씨앗 · 바꾸면 나무 배치가 바뀜 (같은 값이면 항상 같은 배치) */
	UPROPERTY(EditAnywhere, Category = "4 숲", meta = (DisplayName = "Random Seed (배치 씨앗)"))
	int32 RandomSeed = 7;

	/** 개울을 놓을지 */
	UPROPERTY(EditAnywhere, Category = "5 개울", meta = (DisplayName = "Stream Enabled (개울 사용)"))
	bool bStreamEnabled = true;

	/** 개울 방향 (0 = +X · 90 = +Y · 180 = -X · 270 = -Y) · 영지의 그 쪽 바깥을 가로지름 */
	UPROPERTY(EditAnywhere, Category = "5 개울", meta = (DisplayName = "Stream Side (개울 방향, 도)"))
	int32 StreamSideDegrees = 90;

	/** 생울타리 바깥에서 개울 중심까지 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "5 개울", meta = (DisplayName = "Stream Distance (개울 거리)", ClampMin = "100"))
	float StreamDistance = 900.f;

	/** 개울 폭 (cm) */
	UPROPERTY(EditAnywhere, Category = "5 개울", meta = (DisplayName = "Stream Width (개울 폭)", ClampMin = "50"))
	float StreamWidth = 240.f;

	/** 개울이 굽이치는 폭 (cm) */
	UPROPERTY(EditAnywhere, Category = "5 개울", meta = (DisplayName = "Stream Meander (굽이 폭)", ClampMin = "0"))
	float StreamMeander = 180.f;

	/** 색 (엔진 기본 도형 머티리얼에 Color만 바꾼 인스턴스 · Content/CozyRealm/Environment/Materials) */
	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Path Material (흙길)"))
	TObjectPtr<UMaterialInterface> PathMaterial;

	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Hedge Material (생울타리)"))
	TObjectPtr<UMaterialInterface> HedgeMaterial;

	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Leaf Material (나뭇잎)"))
	TObjectPtr<UMaterialInterface> LeafMaterial;

	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Leaf Light Material (밝은 나뭇잎)"))
	TObjectPtr<UMaterialInterface> LeafLightMaterial;

	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Trunk Material (나무 줄기)"))
	TObjectPtr<UMaterialInterface> TrunkMaterial;

	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Water Material (개울 물)"))
	TObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(EditAnywhere, Category = "6 색", meta = (DisplayName = "Stone Material (개울 돌)"))
	TObjectPtr<UMaterialInterface> StoneMaterial;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> PathInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> HedgeInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> TrunkInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeafInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeafLightInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> WaterInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstances;

private:

	void Rebuild();
	void BuildPaths();
	void BuildHedge();
	void BuildStream(TArray<FBox2D>& OutBlocked);
	void BuildForest(const TArray<FBox2D>& Blocked);

	/** 방향(도) → 바깥쪽 단위 벡터 (가장 가까운 축) */
	static FVector2D SideAxis(int32 Degrees);

	/** 영지 중심 기준 x·y·z(윗면) · 크기(cm)로 상자 하나 추가 */
	static void AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, float YawDegrees = 0.f);
};
