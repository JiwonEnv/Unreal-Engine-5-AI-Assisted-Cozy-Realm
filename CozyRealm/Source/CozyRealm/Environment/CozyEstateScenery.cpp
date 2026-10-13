#include "Environment/CozyEstateScenery.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace CozyScenery
{
	/** 엔진 기본 도형은 한 변(지름)이 100cm · 가운데가 기준점 */
	constexpr float ShapeSize = 100.f;
	/** 나무끼리 최소 간격 (cm) */
	constexpr float TreeSpacing = 170.f;
	/** 안개 덩어리 개수 (영지 둘레에 고르게) */
	constexpr int32 FogCount = 16;

	UInstancedStaticMeshComponent* MakeInstances(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Instances = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Instances->SetupAttachment(Owner->GetRootComponent());
		Instances->SetStaticMesh(Mesh);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCanEverAffectNavigation(false);
		return Instances;
	}
}

ACozyEstateScenery::ACozyEstateScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	TrunkInstances = CozyScenery::MakeInstances(this, TEXT("TrunkInstances"), Cylinder.Object);
	LeafInstances = CozyScenery::MakeInstances(this, TEXT("LeafInstances"), Sphere.Object);
	LeafLightInstances = CozyScenery::MakeInstances(this, TEXT("LeafLightInstances"), Sphere.Object);
	BushInstances = CozyScenery::MakeInstances(this, TEXT("BushInstances"), Sphere.Object);
	StoneInstances = CozyScenery::MakeInstances(this, TEXT("StoneInstances"), Sphere.Object);

	for (int32 Index = 0; Index < CozyScenery::FogCount; ++Index)
	{
		ULocalFogVolumeComponent* Fog = CreateDefaultSubobject<ULocalFogVolumeComponent>(*FString::Printf(TEXT("Fog%d"), Index));
		Fog->SetupAttachment(RootComponent);
		FogVolumes.Add(Fog);
	}

	// 기본 색: 환경 머티리얼 인스턴스 (레벨에서 바꿔도 됨)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Leaf(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Leaf.MI_Env_Leaf"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafLight(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_LeafLight.MI_Env_LeafLight"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Trunk(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Trunk.MI_Env_Trunk"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Stone.MI_Env_Stone"));
	LeafMaterial = Leaf.Object;
	LeafLightMaterial = LeafLight.Object;
	TrunkMaterial = Trunk.Object;
	StoneMaterial = Stone.Object;
}

float ACozyEstateScenery::GroundZAt(float X, float Y) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FallbackGroundZ;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CozySceneryGround), false, this);
	if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, 20000.f), FVector(X, Y, -20000.f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return FallbackGroundZ;
}

FVector2D ACozyEstateScenery::RandomOutside(FRandomStream& Random, float ExtraGap) const
{
	// 가장자리에서의 거리: 제곱 분포로 가까운 쪽에 많이 · 네 변 중 하나를 고르고 그 변을 따라 무작위
	const float Start = UnlockedHalfSize + BorderGap + ExtraGap;
	const bool bDense = Random.FRand() < 0.6f;
	const float Depth = bDense ? DenseBandWidth * FMath::Square(Random.FRand()) : Random.FRandRange(0.f, FMath::Max(DenseBandWidth, ForestOuterExtent - Start));
	const float Offset = Start + Depth;
	const float Along = Random.FRandRange(-Offset, Offset);
	switch (Random.RandRange(0, 3))
	{
	case 0: return FVector2D(Offset, Along);
	case 1: return FVector2D(-Offset, Along);
	case 2: return FVector2D(Along, Offset);
	default: return FVector2D(Along, -Offset);
	}
}

#if WITH_EDITOR

void ACozyEstateScenery::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// 끄는 중(드래그)에는 다시 깔지 않고, 값이 정해졌을 때만
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		Rebuild();
	}
}

void ACozyEstateScenery::Rebuild()
{
	Modify();
	const TPair<UInstancedStaticMeshComponent*, UMaterialInterface*> Slots[] = {
		{ TrunkInstances, TrunkMaterial }, { LeafInstances, LeafMaterial }, { LeafLightInstances, LeafLightMaterial },
		{ BushInstances, LeafLightMaterial }, { StoneInstances, StoneMaterial } };
	for (const auto& Slot : Slots)
	{
		Slot.Key->Modify();
		Slot.Key->ClearInstances();
		Slot.Key->SetMaterial(0, Slot.Value);
	}

	// 나무 (줄기 + 둥근 잎 1~2덩어리)
	FRandomStream Random(RandomSeed);
	TArray<FVector2D> Placed;
	for (int32 Attempt = 0; Attempt < TreeCount * 30 && Placed.Num() < TreeCount; ++Attempt)
	{
		const FVector2D P = RandomOutside(Random, 60.f);
		if (Placed.ContainsByPredicate([&P](const FVector2D& Other) { return FVector2D::DistSquared(P, Other) < FMath::Square(CozyScenery::TreeSpacing); }))
		{
			continue;
		}
		Placed.Add(P);
		const float Z = GroundZAt(P.X, P.Y);
		const float Scale = Random.FRandRange(TreeScaleRange.X, FMath::Max(TreeScaleRange.X, TreeScaleRange.Y));
		const float TrunkHeight = 95.f * Scale;
		const float TrunkWidth = 34.f * Scale;
		// 줄기는 땅에 조금 묻어 비탈에서도 뜨지 않게
		TrunkInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P.X, P.Y, Z + TrunkHeight * 0.5f - 15.f), FVector(TrunkWidth, TrunkWidth, TrunkHeight + 30.f) / CozyScenery::ShapeSize));
		UInstancedStaticMeshComponent* Leaves = Random.FRand() < 0.4f ? LeafLightInstances : LeafInstances;
		const float Crown = 180.f * Scale;
		const float CrownZ = Z + TrunkHeight + Crown * 0.36f;
		Leaves->AddInstance(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, CrownZ), FVector(Crown, Crown, Crown * 0.9f) / CozyScenery::ShapeSize));
		if (Random.FRand() < 0.45f)
		{
			const float Small = Crown * 0.62f;
			const FVector2D Shift = FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)).GetSafeNormal() * Crown * 0.28f;
			Leaves->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P.X + Shift.X, P.Y + Shift.Y, CrownZ + Crown * 0.36f), FVector(Small) / CozyScenery::ShapeSize));
		}
	}

	// 덤불 (낮고 넓은 잎 덩어리 · 가장자리 가까이에 몰림 → 길이 막힌 느낌)
	FRandomStream BushRandom(RandomSeed + 11);
	for (int32 Index = 0; Index < BushCount; ++Index)
	{
		const FVector2D P = RandomOutside(BushRandom, 0.f);
		const float Size = BushRandom.FRandRange(90.f, 170.f);
		BushInstances->AddInstance(FTransform(FRotator(0.f, BushRandom.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundZAt(P.X, P.Y) + Size * 0.22f),
			FVector(Size * 1.3f, Size, Size * 0.65f) / CozyScenery::ShapeSize));
	}

	// 바위 (납작하게 · 가장자리와 산비탈에 섞어서)
	FRandomStream RockRandom(RandomSeed + 23);
	for (int32 Index = 0; Index < RockCount; ++Index)
	{
		const FVector2D P = RandomOutside(RockRandom, 20.f);
		const float Size = RockRandom.FRandRange(60.f, 190.f);
		StoneInstances->AddInstance(FTransform(FRotator(RockRandom.FRandRange(-8.f, 8.f), RockRandom.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, GroundZAt(P.X, P.Y) + Size * 0.12f),
			FVector(Size, Size * 0.8f, Size * 0.5f) / CozyScenery::ShapeSize));
	}

	// 안개: 영지 둘레에 고르게 (네 변 · 모서리 포함)
	const float Ring = UnlockedHalfSize + BorderGap + FogDistance;
	for (int32 Index = 0; Index < FogVolumes.Num(); ++Index)
	{
		ULocalFogVolumeComponent* Fog = FogVolumes[Index];
		Fog->Modify();
		const float Angle = UE_TWO_PI * Index / FogVolumes.Num();
		// 원 둘레의 점을 네모 둘레로 펴서 놓음
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D P = Dir / FMath::Max(FMath::Abs(Dir.X), FMath::Abs(Dir.Y)) * Ring;
		Fog->SetVisibility(bFogEnabled);
		Fog->SetRelativeLocation(FVector(P.X, P.Y, GroundZAt(P.X, P.Y) + 150.f));
		Fog->SetRelativeScale3D(FVector(FogRadius / ULocalFogVolumeComponent::GetBaseVolumeSize()));
		Fog->SetRadialFogExtinction(FogDensity);
		Fog->SetHeightFogExtinction(FogDensity);
		Fog->SetHeightFogFalloff(450.f);
		Fog->SetFogAlbedo(FogColor);
	}
	MarkPackageDirty();
}

#endif
