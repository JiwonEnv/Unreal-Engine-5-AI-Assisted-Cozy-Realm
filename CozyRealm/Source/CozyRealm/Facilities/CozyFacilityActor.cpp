#include "Facilities/CozyFacilityActor.h"
#include "Data/CozyRealmDataTypes.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
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
	/** 낮/밤 조명이 밤에 켜는 빛의 이름표 */
	const FName NightLightTag(TEXT("CozyNightLight"));
	const FName OnibiLightTag(TEXT("CozyOnibiLight"));

	UPointLightComponent* MakeNightLight(AActor* Owner, const TCHAR* Name, const FColor& Color, float Candela, float Radius)
	{
		UPointLightComponent* Light = Owner->CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(Owner->GetRootComponent());
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Candela);
		Light->SetLightColor(FLinearColor(Color));
		Light->SetAttenuationRadius(Radius);
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		Light->ComponentTags.Add(NightLightTag);
		return Light;
	}

	/**
	 *  박공 지붕 삼각기둥 (용마루 = X축 · 바닥 가운데가 원점 · 밑면은 벽 위에 가려져 생략).
	 *  면마다 삼각형을 양쪽 감김으로 넣고 법선은 바깥쪽 하나로 → 감김 방향과 관계없이 밖에서 바르게 보임.
	 */
	void BuildGableRoof(UProceduralMeshComponent* Mesh, float Ridge, float Span, float Height)
	{
		const float X = Ridge * 0.5f, Y = Span * 0.5f;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		auto AddFace = [&](std::initializer_list<FVector> Points, const FVector& Normal)
		{
			const int32 Base = Vertices.Num();
			for (const FVector& P : Points)
			{
				Vertices.Add(P);
				Normals.Add(Normal);
			}
			const int32 Count = static_cast<int32>(Points.size());
			for (int32 I = 1; I + 1 < Count; ++I)
			{
				Triangles.Append({ Base, Base + I, Base + I + 1, Base, Base + I + 1, Base + I });
			}
		};
		const FVector LeftNormal = FVector(0.f, -Height, Y).GetSafeNormal();
		const FVector RightNormal = FVector(0.f, Height, Y).GetSafeNormal();
		AddFace({ FVector(-X, -Y, 0.f), FVector(X, -Y, 0.f), FVector(X, 0.f, Height), FVector(-X, 0.f, Height) }, LeftNormal);
		AddFace({ FVector(-X, Y, 0.f), FVector(-X, 0.f, Height), FVector(X, 0.f, Height), FVector(X, Y, 0.f) }, RightNormal);
		AddFace({ FVector(-X, -Y, 0.f), FVector(-X, 0.f, Height), FVector(-X, Y, 0.f) }, FVector(-1.f, 0.f, 0.f));
		AddFace({ FVector(X, -Y, 0.f), FVector(X, Y, 0.f), FVector(X, 0.f, Height) }, FVector(1.f, 0.f, 0.f));
		Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, {}, {}, {}, true);
	}
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

	// 지붕: InitFacility에서 삼각기둥 메시를 만듦 (클릭 선택이 지붕에도 걸리게 충돌 포함)
	RoofMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoofMesh"));
	RoofMesh->SetupAttachment(RootComponent);
	RoofMesh->SetMaterial(0, ShapeMaterial.Object);
	RoofMesh->bUseComplexAsSimpleCollision = true;
	RoofMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RoofMesh->SetVisibility(false);

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

	// 밤 빛 (낮에는 꺼져 있고 낮/밤 조명이 밤 정도만큼 켬)
	LanternLight = CozyFacility::MakeNightLight(this, TEXT("LanternLight"), FColor(255, 176, 97), 10.f, 520.f);
	OnibiLight = CozyFacility::MakeNightLight(this, TEXT("OnibiLight"), FColor(127, 176, 255), 6.f, 380.f);
	OnibiLight->ComponentTags.Add(CozyFacility::OnibiLightTag);
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
	// 처마보다 높은 시설: 벽은 처마까지, 나머지는 박공 지붕 (용마루는 긴 변 방향)
	const float WallHeight = BoxHeight > EaveHeight ? EaveHeight : BoxHeight;
	const float RoofHeight = BoxHeight - WallHeight;
	GreyboxMesh->SetRelativeScale3D(FVector(BoxX, BoxY, WallHeight) / CozyFacility::BasicShapeSize);
	GreyboxMesh->SetRelativeLocation(FVector(0.f, 0.f, BoxLift + WallHeight * 0.5f));
	RoofMesh->SetVisibility(RoofHeight > 0.f);
	if (RoofHeight > 0.f)
	{
		const bool bRidgeAlongX = BoxX >= BoxY;
		const float Ridge = (bRidgeAlongX ? BoxX : BoxY) + RoofOverhang * 2.f;
		const float Span = (bRidgeAlongX ? BoxY : BoxX) + RoofOverhang * 2.f;
		CozyFacility::BuildGableRoof(RoofMesh, Ridge, Span, RoofHeight);
		RoofMesh->SetRelativeRotation(FRotator(0.f, bRidgeAlongX ? 0.f : 90.f, 0.f));
		RoofMesh->SetRelativeLocation(FVector(0.f, 0.f, BoxLift + WallHeight));
		RoofMaterial = RoofMesh->CreateDynamicMaterialInstance(0);
		if (RoofMaterial)
		{
			RoofMaterial->SetVectorParameterValue(CozyFacility::ColorParam, BaseColor * 0.6f);
		}
	}

	GreyboxMaterial = GreyboxMesh->CreateDynamicMaterialInstance(0);
	if (GreyboxMaterial)
	{
		GreyboxMaterial->SetVectorParameterValue(CozyFacility::ColorParam, BaseColor);
	}

	// 바닥 오라: 지면보다 조금 높고 상자 밑면보다 낮게, 상자보다 AuraMargin만큼 넓게 → 둘레가 고르게 보임
	AuraMesh->SetRelativeScale3D(FVector(BoxX + AuraMargin * 2.f, BoxY + AuraMargin * 2.f, CozyFacility::BasicShapeSize) / CozyFacility::BasicShapeSize);
	AuraMesh->SetRelativeLocation(FVector(0.f, 0.f, FMath::Min(AuraLift, BoxLift * 0.5f)));
	AuraMaterial = AuraMesh->CreateDynamicMaterialInstance(0);
	if (AuraMaterial)
	{
		AuraMaterial->SetVectorParameterValue(CozyFacility::ColorParam, AuraColor);
	}

	// 테두리 외곽선: 상자 모서리 12개를 얇은 막대로 (바닥 4 · 위 4 · 세로 4)
	const float T = OutlineThickness;
	const float HalfX = BoxX * 0.5f;
	const float HalfY = BoxY * 0.5f;

	// 밤 빛 위치: 초롱은 카메라 쪽(-X) 모서리 처마 높이 · 오니비는 반대쪽 모서리에 낮게 떠 있음
	LanternLight->SetRelativeLocation(FVector(-HalfX - 30.f, -HalfY * 0.6f, BoxLift + FMath::Min(BoxHeight, 160.f)));
	OnibiLight->SetRelativeLocation(FVector(-HalfX - 25.f, HalfY * 0.6f, BoxLift + 70.f));
	const float Bottom = BoxLift;
	const float Top = BoxLift + WallHeight;
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
			Edges.Emplace(FVector(X, Y, (Bottom + Top) * 0.5f), FVector(T, T, WallHeight + T));
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
	if (RoofMaterial)
	{
		RoofMaterial->SetVectorParameterValue(CozyFacility::ColorParam, (bInSelected ? BaseColor * 1.25f : BaseColor) * 0.6f);
	}
}

void ACozyFacilityActor::SetPlacementPreview(bool bValid)
{
	if (AuraMaterial)
	{
		const FLinearColor Color = bValid ? FLinearColor(0.15f, 0.85f, 0.3f) : FLinearColor(0.95f, 0.15f, 0.12f);
		AuraMaterial->SetVectorParameterValue(CozyFacility::ColorParam, Color);
	}
	AuraMesh->SetVisibility(true);
	for (UStaticMeshComponent* Edge : OutlineEdges)
	{
		Edge->SetVisibility(false);
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
