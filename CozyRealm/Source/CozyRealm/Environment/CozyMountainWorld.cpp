#include "Environment/CozyMountainWorld.h"
#include "Environment/CozySurfaceComponent.h"
#include "Landscape.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

namespace
{
	float SegmentDistance(const FVector2D& P, const FVector& A, const FVector& B, float& T)
	{
		const FVector2D Start(A.X, A.Y), D(B.X-A.X, B.Y-A.Y);
		T = FMath::Clamp(FVector2D::DotProduct(P-Start, D) / FMath::Max(1., D.SizeSquared()), 0., 1.);
		return FVector2D::Distance(P, Start + D*T);
	}
}

ACozyMountainWorld::ACozyMountainWorld()
{
	ComponentsPerSide = 8;
	QuadSize = 100;
	GroundZ = 20;
	TerrainRevision = 1;
	BroadTrees=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("넓은잎나무"));
	Pines=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("소나무"));
	Boulders=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("바위"));
	Cliffs=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("절벽조각"));
	for(UHierarchicalInstancedStaticMeshComponent* C : {BroadTrees.Get(),Pines.Get(),Boulders.Get(),Cliffs.Get()})
	{
		C->SetupAttachment(RootComponent); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCullDistances(0,70000);
	}
	auto Area = [&](const TCHAR* Name, FVector Center, FVector2D Radius)
	{
		FCozyWorldClearing& C = Clearings.AddDefaulted_GetRef();
		C.Name=Name; C.Center=Center; C.Radius=Radius;
	};
	Area(TEXT("시작 영지"), FVector(0,0,0), FVector2D(3100,2700));
	Area(TEXT("서쪽 산중턱 확장 후보"), FVector(-10500,6500,2200), FVector2D(3300,2600));
	Area(TEXT("동쪽 숲 확장 후보"), FVector(10500,4500,1200), FVector2D(3400,2900));
	Area(TEXT("상부 능선 전망지"), FVector(2000,15000,4800), FVector2D(2600,2200));
	Area(TEXT("남쪽 산기슭 후보"), FVector(3500,-13000,-1600), FVector2D(4300,3100));
	auto Route = [&](const TCHAR* Name, std::initializer_list<FVector> Points)
	{
		FCozyWorldRoute& R = Routes.AddDefaulted_GetRef(); R.Name=Name;
		for (const FVector& P : Points) R.Points.Add(P);
	};
	// 평지에 진입하기 전에 목표 높이에 도달하게 해 평지 가장자리에서 급하게 오르지 않는다.
	Route(TEXT("시작→서쪽"), { FVector(-1700,900,0), FVector(-3300,1800,0), FVector(-5300,4000,1100), FVector(-7200,6000,2200), FVector(-9300,6500,2200) });
	Route(TEXT("시작→동쪽"), { FVector(1800,400,0), FVector(3400,700,0), FVector(7100,2600,1200), FVector(9000,4400,1200) });
	Route(TEXT("서쪽→능선"), { FVector(-9800,7600,2200), FVector(-8500,9300,2200), FVector(-5800,12300,3500), FVector(-1000,14000,4800), FVector(1400,15000,4800) });
	Route(TEXT("동쪽→능선"), { FVector(10500,5900,1200), FVector(11000,8000,1200), FVector(10500,10800,2300), FVector(7500,13000,3500), FVector(4700,14800,4800), FVector(2800,15000,4800) });
	Route(TEXT("시작→산기슭"), { FVector(300,-1900,0), FVector(100,-3200,0), FVector(-700,-6500,-600), FVector(500,-9500,-1600), FVector(3500,-12300,-1600) });
}

float ACozyMountainWorld::RouteBlend(float X, float Y, float Base) const
{
	float BestWeight=0, BestHeight=Base, BestDistance=MAX_flt;
	for (const FCozyWorldRoute& R : Routes)
	for (int32 I=1; I<R.Points.Num(); ++I)
	{
		float T; const float Distance=SegmentDistance(FVector2D(X,Y), R.Points[I-1], R.Points[I], T);
		const float W=1-FMath::SmoothStep(R.HalfWidth, R.HalfWidth+R.Falloff, Distance);
		// 폭 안의 여러 구간이 모두 가중치 1이어도 실제로 가장 가까운 구간을 선택한다.
		// 이전 구간을 계속 선택하면 꺾임 뒤에서 갑자기 다음 높이로 뛰어 단차가 생긴다.
		if (Distance<BestDistance) { BestDistance=Distance; BestWeight=W; BestHeight=FMath::Lerp(R.Points[I-1].Z,R.Points[I].Z,T); }
	}
	return FMath::Lerp(Base, BestHeight, BestWeight);
}

float ACozyMountainWorld::GetLayoutHeight(float X, float Y) const
{
	auto Peak = [&](float CX, float CY, float RX, float RY, float H)
	{
		return H*FMath::Exp(-FMath::Square((X-CX)/RX)-FMath::Square((Y-CY)/RY));
	};
	float H=-2200+Peak(-11000,18000,12000,12000,10800)+Peak(13000,19000,11000,12000,10000)
		+Peak(-19000,-1000,6500,13000,5600)+Peak(18000,-1000,7500,14000,5300)
		+Peak(0,4000,15000,12000,2600);
	const float Noise=FMath::PerlinNoise2D(FVector2D(X,Y)/2400.+FVector2D(7,19));
	H+=Noise*300+FMath::PerlinNoise2D(FVector2D(X,Y)/900.)*70;
	for (const FCozyWorldClearing& C : Clearings)
	{
		const float D=FMath::Sqrt(FMath::Square((X-C.Center.X)/C.Radius.X)+FMath::Square((Y-C.Center.Y)/C.Radius.Y));
		const float Outer=1.f+C.Falloff/float(FMath::Min(C.Radius.X,C.Radius.Y));
		const float W=1-FMath::SmoothStep(1.f,Outer,D);
		H=FMath::Lerp(H,float(C.Center.Z),W);
	}
	// 길은 지정한 고도점을 우선한다. 평지 외곽의 완충 경사를 길에 다시 더하지 않는다.
	return RouteBlend(X,Y,H);
}

float ACozyMountainWorld::GetPathDistance(float X, float Y) const
{
	float Best=MAX_flt;
	for (const FCozyWorldRoute& R : Routes)
	for (int32 I=1; I<R.Points.Num(); ++I)
	{
		float T; Best=FMath::Min(Best,SegmentDistance(FVector2D(X,Y),R.Points[I-1],R.Points[I],T));
	}
	return Best;
}

void ACozyMountainWorld::BeginPlay()
{
	Super::BeginPlay();
	if (Landscape)
	if (UCozySurfaceComponent* Surface=Landscape->FindComponentByClass<UCozySurfaceComponent>())
	{
		// 새 맵은 레벨 검토용이다. 기존 24m 영지의 저장 도메인과 혼용하지 않는다.
		Surface->SurfaceId=FGuid(0xC02E0023,0,0,1);
		Surface->PaintDomainId=FGuid(0xC02E0023,0,0,2);
		Surface->bAllowBuilding=false; Surface->bAllowPainting=false;
	}
}

#if WITH_EDITOR
float ACozyMountainWorld::HeightAt(float X, float Y) const { return GetLayoutHeight(X,Y); }
float ACozyMountainWorld::LayerWeightAt(float X,float Y,float Height,float Slope,FName Layer) const
{
	const float Path=1-FMath::SmoothStep(210.f,370.f,GetPathDistance(X,Y));
	const float Rock=FMath::SmoothStep(.55f,1.05f,Slope)*(1-Path);
	const float Dark=FMath::Clamp(.3f+FMath::PerlinNoise2D(FVector2D(X,Y)/1700.)*.6f,0.f,1.f);
	if(Layer==TEXT("Path")) return Path;
	if(Layer==TEXT("Rock")) return Rock;
	if(Layer==TEXT("GrassB")) return Dark*(1-Rock)*(1-Path);
	if(Layer==TEXT("GrassA")) return (1-Dark)*(1-Rock)*(1-Path);
	return 0;
}
#endif
