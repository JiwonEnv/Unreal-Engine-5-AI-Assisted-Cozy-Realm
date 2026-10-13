#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CozyEstateScenery.generated.h"

class UInstancedStaticMeshComponent;
class ULocalFogVolumeComponent;
class UMaterialInterface;

/**
 *  영지 둘레 자연 경계 (임시 모형 · 실제 에셋이 생기기 전 단계).
 *  울타리 같은 인공물 없이, 지금 열린 영지 바깥을 나무 · 덤불 · 바위 · 안개로 채워 '아직 갈 수 없는 곳'으로 보이게 한다.
 *  영지가 넓어지면 Unlocked Half Size를 키우고 '자연 경계 다시 깔기'를 누르면 그 안쪽 나무·덤불·바위가 빠진다.
 *  나무 위치는 지형 높이에 맞춰 저장된다 → 지형을 다시 만들거나 손질한 뒤에는 '자연 경계 다시 깔기'를 한 번 누른다.
 *  충돌은 모두 꺼서 시설 클릭·배치에 영향이 없다.
 */
UCLASS(meta = (DisplayName = "Cozy Estate Scenery (영지 둘레 자연 경계)"))
class ACozyEstateScenery : public AActor
{
	GENERATED_BODY()

public:

	ACozyEstateScenery();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** 지금 값으로 나무 · 덤불 · 바위 · 안개를 다시 깐다 (지형 높이에 맞춤) */
	UFUNCTION(CallInEditor, Category = "6 도구", meta = (DisplayName = "자연 경계 다시 깔기"))
	void Rebuild();
#endif

	/** 지금 열린 영지 절반 크기 (cm) · 영지 칸 수 × 칸 크기 ÷ 2 (지금 12칸 × 200cm → 1200) · 이 안에는 아무것도 두지 않음 */
	UPROPERTY(EditAnywhere, Category = "1 영지", meta = (DisplayName = "Unlocked Half Size (열린 영지 절반 크기)", ClampMin = "100"))
	float UnlockedHalfSize = 1200.f;

	/** 열린 영지 가장자리에서 나무까지 띄우는 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "1 영지", meta = (DisplayName = "Border Gap (가장자리 여유)", ClampMin = "0"))
	float BorderGap = 120.f;

	/** 지형을 찾지 못할 때 쓰는 바닥 높이 (cm) */
	UPROPERTY(EditAnywhere, Category = "1 영지", meta = (DisplayName = "Fallback Ground Z (기본 바닥 높이)"))
	float FallbackGroundZ = 20.f;

	/** 가장자리 빽빽한 띠 폭 (cm) · 이 안에 나무·덤불의 절반 이상이 모여 막힌 느낌을 줌 */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Dense Band Width (빽빽한 띠 폭)", ClampMin = "200"))
	float DenseBandWidth = 1500.f;

	/** 숲이 끝나는 거리 (영지 중심에서 · cm) · 지형 크기보다 작게 */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Forest Outer Extent (숲 끝 거리)", ClampMin = "500"))
	float ForestOuterExtent = 7400.f;

	/** 나무 개수 */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Tree Count (나무 개수)", ClampMin = "0", ClampMax = "800"))
	int32 TreeCount = 190;

	/** 덤불 개수 */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Bush Count (덤불 개수)", ClampMin = "0", ClampMax = "800"))
	int32 BushCount = 170;

	/** 바위 개수 */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Rock Count (바위 개수)", ClampMin = "0", ClampMax = "400"))
	int32 RockCount = 70;

	/** 나무 크기 범위 (기본 크기 대비 배율) */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Tree Scale Range (나무 크기 범위)"))
	FVector2D TreeScaleRange = FVector2D(0.85f, 1.45f);

	/** 배치 무작위 씨앗 · 같은 값이면 항상 같은 배치 */
	UPROPERTY(EditAnywhere, Category = "2 나무", meta = (DisplayName = "Random Seed (배치 씨앗)"))
	int32 RandomSeed = 7;

	/** 영지 둘레에 안개를 깔지 */
	UPROPERTY(EditAnywhere, Category = "3 안개", meta = (DisplayName = "Fog Enabled (안개 사용)"))
	bool bFogEnabled = true;

	/** 열린 영지 가장자리에서 안개 중심까지 거리 (cm) */
	UPROPERTY(EditAnywhere, Category = "3 안개", meta = (DisplayName = "Fog Distance (안개 거리)", ClampMin = "0"))
	float FogDistance = 900.f;

	/** 안개 덩어리 하나의 반지름 (cm) */
	UPROPERTY(EditAnywhere, Category = "3 안개", meta = (DisplayName = "Fog Radius (안개 크기)", ClampMin = "100"))
	float FogRadius = 1100.f;

	/** 안개 짙기 */
	UPROPERTY(EditAnywhere, Category = "3 안개", meta = (DisplayName = "Fog Density (안개 짙기)", ClampMin = "0", ClampMax = "4"))
	float FogDensity = 0.55f;

	/** 안개 색 */
	UPROPERTY(EditAnywhere, Category = "3 안개", meta = (DisplayName = "Fog Color (안개 색)"))
	FLinearColor FogColor = FLinearColor(0.93f, 0.95f, 1.f);

	/** 색 (엔진 기본 도형 머티리얼에 Color만 바꾼 인스턴스 · Content/CozyRealm/Environment/Materials) */
	UPROPERTY(EditAnywhere, Category = "4 색", meta = (DisplayName = "Leaf Material (나뭇잎)"))
	TObjectPtr<UMaterialInterface> LeafMaterial;

	UPROPERTY(EditAnywhere, Category = "4 색", meta = (DisplayName = "Leaf Light Material (밝은 나뭇잎 · 덤불)"))
	TObjectPtr<UMaterialInterface> LeafLightMaterial;

	UPROPERTY(EditAnywhere, Category = "4 색", meta = (DisplayName = "Trunk Material (나무 줄기)"))
	TObjectPtr<UMaterialInterface> TrunkMaterial;

	UPROPERTY(EditAnywhere, Category = "4 색", meta = (DisplayName = "Stone Material (바위)"))
	TObjectPtr<UMaterialInterface> StoneMaterial;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> TrunkInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeafInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeafLightInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> BushInstances;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> StoneInstances;

	/** 영지 둘레 안개 (고정 개수 · 위치는 다시 깔 때 정함) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<ULocalFogVolumeComponent>> FogVolumes;

private:

	/** 그 위치의 지형 높이 (지형이 없으면 기본 바닥 높이) */
	float GroundZAt(float X, float Y) const;

	/** 열린 영지 바깥에서 무작위 위치 하나 (가장자리 가까이에 더 많이) */
	FVector2D RandomOutside(FRandomStream& Random, float ExtraGap) const;
};
