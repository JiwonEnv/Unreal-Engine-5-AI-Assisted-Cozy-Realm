#include "Environment/CozyEstateScenery.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace CozyScenery
{
	/** 엔진 기본 도형은 한 변(지름)이 100cm · 가운데가 기준점 */
	constexpr float ShapeSize = 100.f;
	/** 흙길·물 두께 (cm) · 바닥 위에 얇게 깐다 */
	constexpr float FlatThickness = 4.f;
	/** 개울 한 토막 길이 (cm) */
	constexpr float StreamStep = 200.f;
	/** 굽이 한 번의 길이 (cm) */
	constexpr float MeanderWavelength = 1700.f;
	/** 나무끼리 최소 간격 (cm) */
	constexpr float TreeSpacing = 190.f;
	/** 길·개울 둘레에 나무를 두지 않는 여유 (cm) */
	constexpr float ClearMargin = 140.f;

	UInstancedStaticMeshComponent* MakeInstances(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh, bool bCastShadow)
	{
		UInstancedStaticMeshComponent* Instances = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Instances->SetupAttachment(Owner->GetRootComponent());
		Instances->SetStaticMesh(Mesh);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCanEverAffectNavigation(false);
		Instances->SetCastShadow(bCastShadow);
		return Instances;
	}
}

ACozyEstateScenery::ACozyEstateScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	PathInstances = CozyScenery::MakeInstances(this, TEXT("PathInstances"), Cube.Object, false);
	HedgeInstances = CozyScenery::MakeInstances(this, TEXT("HedgeInstances"), Cube.Object, true);
	TrunkInstances = CozyScenery::MakeInstances(this, TEXT("TrunkInstances"), Cylinder.Object, true);
	LeafInstances = CozyScenery::MakeInstances(this, TEXT("LeafInstances"), Sphere.Object, true);
	LeafLightInstances = CozyScenery::MakeInstances(this, TEXT("LeafLightInstances"), Sphere.Object, true);
	WaterInstances = CozyScenery::MakeInstances(this, TEXT("WaterInstances"), Cube.Object, false);
	StoneInstances = CozyScenery::MakeInstances(this, TEXT("StoneInstances"), Sphere.Object, true);

	// 기본 색: 환경 머티리얼 인스턴스 (없으면 엔진 기본 회색 · 레벨에서 바꿔도 됨)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dirt(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Dirt.MI_Env_Dirt"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Hedge(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Hedge.MI_Env_Hedge"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Leaf(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Leaf.MI_Env_Leaf"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafLight(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_LeafLight.MI_Env_LeafLight"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Trunk(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Trunk.MI_Env_Trunk"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Water(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Water.MI_Env_Water"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(TEXT("/Game/CozyRealm/Environment/Materials/MI_Env_Stone.MI_Env_Stone"));
	PathMaterial = Dirt.Object;
	HedgeMaterial = Hedge.Object;
	LeafMaterial = Leaf.Object;
	LeafLightMaterial = LeafLight.Object;
	TrunkMaterial = Trunk.Object;
	WaterMaterial = Water.Object;
	StoneMaterial = Stone.Object;
}

void ACozyEstateScenery::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ACozyEstateScenery::Rebuild()
{
	const TPair<UInstancedStaticMeshComponent*, UMaterialInterface*> Slots[] = {
		{ PathInstances, PathMaterial }, { HedgeInstances, HedgeMaterial }, { TrunkInstances, TrunkMaterial },
		{ LeafInstances, LeafMaterial }, { LeafLightInstances, LeafLightMaterial }, { WaterInstances, WaterMaterial },
		{ StoneInstances, StoneMaterial } };
	for (const auto& Slot : Slots)
	{
		Slot.Key->ClearInstances();
		Slot.Key->SetMaterial(0, Slot.Value);
	}

	BuildPaths();
	BuildHedge();

	// 길과 개울 둘레는 나무를 두지 않는 칸으로 모아 숲을 깐다
	TArray<FBox2D> Blocked;
	const FVector2D Out = SideAxis(EntranceSideDegrees);
	const float RoadStart = EstateHalfSize + RingPathWidth;
	const float RoadEnd = RoadStart + HedgeThickness + EntranceRoadLength;
	const FVector2D Side(-Out.Y, Out.X);
	const FVector2D A = Out * RoadStart - Side * (EntranceWidth * 0.5f + CozyScenery::ClearMargin);
	const FVector2D B = Out * RoadEnd + Side * (EntranceWidth * 0.5f + CozyScenery::ClearMargin);
	Blocked.Add(FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y))));
	BuildStream(Blocked);
	BuildForest(Blocked);
}

FVector2D ACozyEstateScenery::SideAxis(int32 Degrees)
{
	switch (((Degrees % 360) + 360) % 360 / 45)
	{
	case 1: case 2: return FVector2D(0.f, 1.f);
	case 3: case 4: return FVector2D(-1.f, 0.f);
	case 5: case 6: return FVector2D(0.f, -1.f);
	default: return FVector2D(1.f, 0.f);
	}
}

void ACozyEstateScenery::AddBox(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, float YawDegrees)
{
	Target->AddInstance(FTransform(FRotator(0.f, YawDegrees, 0.f), Center, Size / CozyScenery::ShapeSize));
}

void ACozyEstateScenery::BuildPaths()
{
	const float H = EstateHalfSize;
	const float W = RingPathWidth;
	const float Z = OuterGroundZ + CozyScenery::FlatThickness * 0.5f;
	if (W > 0.f)
	{
		// 둘레 길: ±X 쪽은 모서리까지 · ±Y 쪽은 그 사이만 (겹치지 않게)
		for (const float Sign : { 1.f, -1.f })
		{
			AddBox(PathInstances, FVector(Sign * (H + W * 0.5f), 0.f, Z), FVector(W, 2.f * (H + W), CozyScenery::FlatThickness));
			AddBox(PathInstances, FVector(0.f, Sign * (H + W * 0.5f), Z), FVector(2.f * H, W, CozyScenery::FlatThickness));
		}
	}
	// 입구 길: 둘레 길 바깥 → 생울타리 틈 → 바깥으로
	const FVector2D Out = SideAxis(EntranceSideDegrees);
	const float Start = H + W;
	const float Length = HedgeThickness + EntranceRoadLength;
	const FVector2D Mid = Out * (Start + Length * 0.5f);
	const bool bAlongX = FMath::Abs(Out.X) > 0.5f;
	AddBox(PathInstances, FVector(Mid.X, Mid.Y, Z), bAlongX ? FVector(Length, EntranceWidth, CozyScenery::FlatThickness) : FVector(EntranceWidth, Length, CozyScenery::FlatThickness));
}

void ACozyEstateScenery::BuildHedge()
{
	FRandomStream Random(RandomSeed + 101);
	const float Center = EstateHalfSize + RingPathWidth + HedgeThickness * 0.5f;
	const FVector2D Entrance = SideAxis(EntranceSideDegrees);
	const float Gap = EntranceWidth * 0.5f;
	const float Seg = HedgeSegmentLength;

	for (const FVector2D Normal : { FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f) })
	{
		const bool bXSide = FMath::Abs(Normal.X) > 0.5f;
		// X쪽 울타리가 모서리를 덮고, Y쪽은 그 안쪽 길이만
		const float HalfLength = bXSide ? Center + HedgeThickness * 0.5f : Center - HedgeThickness * 0.5f;
		const bool bEntranceSide = Normal.Equals(Entrance);
		const FVector2D Tangent(-Normal.Y, Normal.X);

		for (float From = -HalfLength; From < HalfLength - 1.f; From += Seg)
		{
			float To = FMath::Min(From + Seg, HalfLength);
			// 입구 틈은 비우고 양쪽 남은 조각만 놓는다
			TArray<FVector2D, TInlineAllocator<2>> Pieces;
			if (bEntranceSide && To > -Gap && From < Gap)
			{
				if (From < -Gap) { Pieces.Emplace(From, -Gap); }
				if (To > Gap) { Pieces.Emplace(Gap, To); }
			}
			else
			{
				Pieces.Emplace(From, To);
			}
			for (const FVector2D& Piece : Pieces)
			{
				const float Length = Piece.Y - Piece.X;
				if (Length < 10.f)
				{
					continue;
				}
				const float Height = HedgeHeight * Random.FRandRange(0.88f, 1.12f);
				const FVector2D P = Normal * Center + Tangent * ((Piece.X + Piece.Y) * 0.5f);
				const FVector Size = bXSide ? FVector(HedgeThickness, Length, Height) : FVector(Length, HedgeThickness, Height);
				AddBox(HedgeInstances, FVector(P.X, P.Y, OuterGroundZ + Height * 0.5f), Size);
			}
		}
	}
}

void ACozyEstateScenery::BuildStream(TArray<FBox2D>& OutBlocked)
{
	if (!bStreamEnabled)
	{
		return;
	}
	FRandomStream Random(RandomSeed + 202);
	const FVector2D Normal = SideAxis(StreamSideDegrees);
	const FVector2D Tangent(-Normal.Y, Normal.X);
	const float Base = EstateHalfSize + RingPathWidth + HedgeThickness + StreamDistance;
	const float Extent = ForestOuterExtent + 200.f;
	const float Z = OuterGroundZ + CozyScenery::FlatThickness * 0.5f + 0.5f;

	auto PointAt = [&](float S)
	{
		const float Offset = StreamMeander * FMath::Sin(S / CozyScenery::MeanderWavelength * UE_TWO_PI);
		return Normal * (Base + Offset) + Tangent * S;
	};

	for (float S = -Extent; S < Extent; S += CozyScenery::StreamStep)
	{
		const FVector2D P0 = PointAt(S);
		const FVector2D P1 = PointAt(S + CozyScenery::StreamStep);
		const FVector2D Mid = (P0 + P1) * 0.5f;
		const FVector2D Dir = P1 - P0;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		// 토막끼리 이음매가 비지 않게 조금 겹친다
		AddBox(WaterInstances, FVector(Mid.X, Mid.Y, Z), FVector(Dir.Size() + 60.f, StreamWidth, CozyScenery::FlatThickness), Yaw);

		// 물가 돌 (토막마다 0~2개)
		const int32 Stones = Random.RandRange(0, 2);
		for (int32 Index = 0; Index < Stones; ++Index)
		{
			const float Bank = (Random.FRand() < 0.5f ? -1.f : 1.f) * (StreamWidth * 0.5f + Random.FRandRange(5.f, 35.f));
			const FVector2D Across = FVector2D(-Dir.Y, Dir.X).GetSafeNormal();
			const FVector2D StoneAt = Mid + Across * Bank + Dir * Random.FRandRange(-0.4f, 0.4f);
			const float Size = Random.FRandRange(35.f, 70.f);
			StoneInstances->AddInstance(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(StoneAt.X, StoneAt.Y, OuterGroundZ + Size * 0.15f),
				FVector(Size, Size * 0.8f, Size * 0.45f) / CozyScenery::ShapeSize));
		}

		const float Clear = StreamWidth * 0.5f + CozyScenery::ClearMargin;
		OutBlocked.Add(FBox2D(FVector2D(FMath::Min(P0.X, P1.X), FMath::Min(P0.Y, P1.Y)) - FVector2D(Clear), FVector2D(FMath::Max(P0.X, P1.X), FMath::Max(P0.Y, P1.Y)) + FVector2D(Clear)));
	}
}

void ACozyEstateScenery::BuildForest(const TArray<FBox2D>& Blocked)
{
	FRandomStream Random(RandomSeed);
	const float Inner = EstateHalfSize + RingPathWidth + HedgeThickness + ForestInnerGap;
	const float Outer = FMath::Max(Inner + 100.f, ForestOuterExtent);
	TArray<FVector2D> Placed;
	Placed.Reserve(TreeCount);

	for (int32 Attempt = 0; Attempt < TreeCount * 30 && Placed.Num() < TreeCount; ++Attempt)
	{
		const FVector2D P(Random.FRandRange(-Outer, Outer), Random.FRandRange(-Outer, Outer));
		if (FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y)) < Inner)
		{
			continue;
		}
		if (Blocked.ContainsByPredicate([&P](const FBox2D& Box) { return Box.IsInside(P); }))
		{
			continue;
		}
		if (Placed.ContainsByPredicate([&P](const FVector2D& Other) { return FVector2D::DistSquared(P, Other) < FMath::Square(CozyScenery::TreeSpacing); }))
		{
			continue;
		}
		Placed.Add(P);

		const float Scale = Random.FRandRange(TreeScaleRange.X, FMath::Max(TreeScaleRange.X, TreeScaleRange.Y));
		const float TrunkHeight = 90.f * Scale;
		const float TrunkWidth = 34.f * Scale;
		TrunkInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P.X, P.Y, OuterGroundZ + TrunkHeight * 0.5f),
			FVector(TrunkWidth, TrunkWidth, TrunkHeight) / CozyScenery::ShapeSize));

		// 둥근 잎 덩어리 1~2개 (밝은 잎과 짙은 잎을 섞음)
		UInstancedStaticMeshComponent* Leaves = Random.FRand() < 0.4f ? LeafLightInstances : LeafInstances;
		const float Crown = 175.f * Scale;
		const float CrownZ = OuterGroundZ + TrunkHeight + Crown * 0.38f;
		Leaves->AddInstance(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, CrownZ), FVector(Crown, Crown, Crown * 0.9f) / CozyScenery::ShapeSize));
		if (Random.FRand() < 0.45f)
		{
			const float Small = Crown * 0.62f;
			const FVector2D Offset = FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)).GetSafeNormal() * Crown * 0.28f;
			Leaves->AddInstance(FTransform(FRotator::ZeroRotator, FVector(P.X + Offset.X, P.Y + Offset.Y, CrownZ + Crown * 0.36f), FVector(Small) / CozyScenery::ShapeSize));
		}
	}
}
