#include "Facilities/CozyFacilityActor.h"
#include "Data/CozyRealmDataTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace CozyFacility
{
	/** 엔진 기본 도형은 한 변이 100cm */
	constexpr float BasicShapeSize = 100.f;
	/** 칸 사이에 남길 여백 비율 */
	constexpr float FootprintFill = 0.9f;
	/** 엔진 기본 도형 머티리얼의 색 매개변수 */
	const FName ColorParam(TEXT("Color"));
	/** 상자 모서리 개수 (테두리 외곽선) */
	constexpr int32 EdgeCount = 12;
}

ACozyFacilityActor::ACozyFacilityActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	GreyboxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GreyboxMesh"));
	GreyboxMesh->SetupAttachment(RootComponent);
	GreyboxMesh->SetStaticMesh(CubeMesh.Object);
	GreyboxMesh->SetMaterial(0, ShapeMaterial.Object);
	GreyboxMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	AuraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AuraMesh"));
	AuraMesh->SetupAttachment(RootComponent);
	AuraMesh->SetStaticMesh(PlaneMesh.Object);
	AuraMesh->SetMaterial(0, ShapeMaterial.Object);
	AuraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AuraMesh->SetCastShadow(false);
	AuraMesh->SetVisibility(false);

	for (int32 Index = 0; Index < CozyFacility::EdgeCount; ++Index)
	{
		UStaticMeshComponent* Edge = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("OutlineEdge%d"), Index));
		Edge->SetupAttachment(RootComponent);
		Edge->SetStaticMesh(CubeMesh.Object);
		Edge->SetMaterial(0, ShapeMaterial.Object);
		Edge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Edge->SetCastShadow(false);
		Edge->SetVisibility(false);
		OutlineEdges.Add(Edge);
	}
}

void ACozyFacilityActor::InitFacility(const FGuid& InFacilityId, const FCozyFacilityRow& Def, float CellSize)
{
	FacilityId = InFacilityId;
	BoxHeight = FMath::Max(10.f, Def.GreyboxHeight);
	BaseColor = Def.GreyboxColor;

	const float SizeX = Def.Size.X * CellSize;
	const float SizeY = Def.Size.Y * CellSize;

	// 회색 상자: 칸 크기에 맞추고 지면에서 BoxLift만큼 띄움 (액터 위치 = 지면)
	const float BoxX = SizeX * CozyFacility::FootprintFill;
	const float BoxY = SizeY * CozyFacility::FootprintFill;
	GreyboxMesh->SetRelativeScale3D(FVector(BoxX, BoxY, BoxHeight) / CozyFacility::BasicShapeSize);
	GreyboxMesh->SetRelativeLocation(FVector(0.f, 0.f, BoxLift + BoxHeight * 0.5f));

	GreyboxMaterial = GreyboxMesh->CreateDynamicMaterialInstance(0);
	if (GreyboxMaterial)
	{
		GreyboxMaterial->SetVectorParameterValue(CozyFacility::ColorParam, BaseColor);
	}

	// 바닥 오라: 지면보다 조금 높고 상자 밑면보다 낮게, 상자보다 AuraMargin만큼 넓게 → 둘레가 고르게 보임
	AuraMesh->SetRelativeScale3D(FVector(BoxX + AuraMargin * 2.f, BoxY + AuraMargin * 2.f, CozyFacility::BasicShapeSize) / CozyFacility::BasicShapeSize);
	AuraMesh->SetRelativeLocation(FVector(0.f, 0.f, FMath::Min(AuraLift, BoxLift * 0.5f)));
	if (UMaterialInstanceDynamic* AuraMaterial = AuraMesh->CreateDynamicMaterialInstance(0))
	{
		AuraMaterial->SetVectorParameterValue(CozyFacility::ColorParam, AuraColor);
	}

	// 테두리 외곽선: 상자 모서리 12개를 얇은 막대로 (바닥 4 · 위 4 · 세로 4)
	const float T = OutlineThickness;
	const float HalfX = BoxX * 0.5f;
	const float HalfY = BoxY * 0.5f;
	const float Bottom = BoxLift;
	const float Top = BoxLift + BoxHeight;
	TArray<TPair<FVector, FVector>> Edges; // 위치, 크기(cm)
	for (const float Z : { Bottom, Top })
	{
		Edges.Emplace(FVector(0.f, -HalfY, Z), FVector(BoxX + T, T, T));
		Edges.Emplace(FVector(0.f, HalfY, Z), FVector(BoxX + T, T, T));
		Edges.Emplace(FVector(-HalfX, 0.f, Z), FVector(T, BoxY + T, T));
		Edges.Emplace(FVector(HalfX, 0.f, Z), FVector(T, BoxY + T, T));
	}
	for (const float X : { -HalfX, HalfX })
	{
		for (const float Y : { -HalfY, HalfY })
		{
			Edges.Emplace(FVector(X, Y, (Bottom + Top) * 0.5f), FVector(T, T, BoxHeight + T));
		}
	}
	for (int32 Index = 0; Index < OutlineEdges.Num() && Index < Edges.Num(); ++Index)
	{
		OutlineEdges[Index]->SetRelativeLocation(Edges[Index].Key);
		OutlineEdges[Index]->SetRelativeScale3D(Edges[Index].Value / CozyFacility::BasicShapeSize);
		if (UMaterialInstanceDynamic* EdgeMaterial = OutlineEdges[Index]->CreateDynamicMaterialInstance(0))
		{
			EdgeMaterial->SetVectorParameterValue(CozyFacility::ColorParam, OutlineColor);
		}
	}
}

void ACozyFacilityActor::SetSelected(bool bInSelected)
{
	AuraMesh->SetVisibility(bInSelected);
	for (UStaticMeshComponent* Edge : OutlineEdges)
	{
		Edge->SetVisibility(bInSelected);
	}

	if (GreyboxMaterial)
	{
		GreyboxMaterial->SetVectorParameterValue(CozyFacility::ColorParam, bInSelected ? BaseColor * 1.25f : BaseColor);
	}
}

FVector ACozyFacilityActor::GetIconAnchorLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, BoxLift + BoxHeight + 170.f);
}

FVector ACozyFacilityActor::GetNameAnchorLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, BoxLift + BoxHeight + 10.f);
}
