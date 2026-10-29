#include "Environment/CozySurfaceNavigation.h"
#include "Estate/CozyEstateSubsystem.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Facilities/CozyFacilityActor.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace CozyWalk
{
	constexpr float Radius = 30.f;
	constexpr float HalfHeight = 70.f;
	constexpr float StepHeight = 35.f;
	float ClearanceCenter(float NormalZ)
	{
		// 경사로 끝의 낮은 턱은 CharacterMovement가 올라설 수 있다.
		// 그 높이까지 올린 몸통 공간을 검사하되 벽·천장은 계속 충돌 검사한다.
		return HalfHeight + Radius * (1.f / FMath::Max(.72f, NormalZ) - 1.f) + 4.f + StepHeight;
	}
	/** 몸통 공간 검사에서 시설·캐릭터는 빼고 지형·벽만 본다 */
	void IgnoreFacilitiesAndCharacters(UWorld* World, FCollisionQueryParams& Params)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
			if (It->IsA<ACozyFacilityActor>() || It->IsA<ACharacter>()) Params.AddIgnoredActor(*It);
	}
}

bool UCozySurfaceNavigation::IsFree(const FVector& Feet, const FGuid& IgnoreFacility) const
{
	const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!Estate) return false;
	const auto& Config = Estate->GetConfig();
	const FIntPoint Cell = Estate->WorldToCell(Feet);
	if (Cell.X < 0 || Cell.Y < 0 || Cell.X >= Config.GridSize.X || Cell.Y >= Config.GridSize.Y) return false;
	for (const FCozyFacilityState& Facility : Estate->GetState().Facilities)
	{
		// 보관한 것 · 무시할 시설 · 걸을 수 있는 장식(돌길)은 길을 막지 않음
		if (Facility.bStored || Facility.InstanceId == IgnoreFacility || Estate->IsWalkableFacility(Facility)) continue;
		const FIntPoint Size = Estate->GetFootprint(Facility.InstanceId, Facility.Rotation);
		const FCozyFacilityRow* Def = Estate->GetFacilityDef(Facility.DefinitionId);
		if (!Def) continue;
		FCozySurfaceHit Ground;
		const FVector Min = Config.GridOrigin + FVector(Facility.GridCoord.X * Config.CellSize, Facility.GridCoord.Y * Config.CellSize, 0);
		const FVector Center = Min + FVector(Size.X * Config.CellSize * .5f, Size.Y * Config.CellSize * .5f, 0);
		const float Z = Facility.bHasSurfacePlacement ? Facility.SurfaceHeight : (CozySurface::GroundAt(GetWorld(), Center, Ground) ? Ground.Position.Z : Config.GridOrigin.Z);
		// 주인공 몸통 높이와 시설 높이가 겹치는 경우만 점유한다.
		if (Feet.Z + 140 < Z || Feet.Z > Z + Def->GreyboxHeight + 15) continue;
		if (Feet.X >= Min.X - 30 && Feet.Y >= Min.Y - 30 && Feet.X <= Min.X + Size.X * Config.CellSize + 30 && Feet.Y <= Min.Y + Size.Y * Config.CellSize + 30) return false;
	}
	return true;
}

bool UCozySurfaceNavigation::Connects(const FCozySurfaceHit& A, const FCozySurfaceHit& B, const FCollisionQueryParams* Clearance) const
{
	const float Length = FVector::Dist2D(A.Position, B.Position);
	if (FMath::Abs(A.Position.Z - B.Position.Z) > Length * .95f + 35.f) return false;
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / 20.f));
	float PreviousZ = A.Position.Z;
	for (int32 I = 1; I <= Steps; ++I)
	{
		const FVector Expected = FMath::Lerp(A.Position, B.Position, float(I) / Steps);
		FCozySurfaceHit First, Second;
		const bool HaveFirst = CozySurface::GroundAt(GetWorld(), Expected, First, &A.SurfaceId);
		const bool HaveSecond = CozySurface::GroundAt(GetWorld(), Expected, Second, &B.SurfaceId);
		if (!HaveFirst && !HaveSecond) return false;
		const FCozySurfaceHit& Sample = !HaveFirst || (HaveSecond && FMath::Abs(Second.Position.Z - Expected.Z) < FMath::Abs(First.Position.Z - Expected.Z)) ? Second : First;
		if (Sample.Normal.Z < .72f || FMath::Abs(Sample.Position.Z - PreviousZ) > 35.f || (Sample.Rules.IsValid() && !Sample.Rules->bAllowWalking)) return false;
		if (Clearance)
		{
			if (GetWorld()->OverlapBlockingTestByChannel(Sample.Position + FVector(0, 0, CozyWalk::ClearanceCenter(Sample.Normal.Z)), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(CozyWalk::Radius, CozyWalk::HalfHeight), *Clearance)) return false;
		}
		PreviousZ = Sample.Position.Z;
	}
	return FMath::Abs(PreviousZ - B.Position.Z) <= 35.f;
}

void UCozySurfaceNavigation::Build()
{
	if (!Nodes.IsEmpty()) return;
	const auto* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>();
	if (!Estate) return;
	const auto& Config = Estate->GetConfig();
	TArray<FGuid> Surfaces;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (const auto* Rules = It->FindComponentByClass<UCozySurfaceComponent>()) Surfaces.AddUnique(Rules->SurfaceId);
		else if (It->IsA<ALandscapeProxy>() || It->ActorHasTag(TEXT("CozyGround"))) Surfaces.AddUnique(FGuid());
	}
	const float Step = Config.CellSize * .5f;
	FCollisionQueryParams Clearance(SCENE_QUERY_STAT(CozySurfaceWalkClearance), false);
	CozyWalk::IgnoreFacilitiesAndCharacters(GetWorld(), Clearance);
	for (int32 Y = 0; Y < Config.GridSize.Y * 2; ++Y)
	for (int32 X = 0; X < Config.GridSize.X * 2; ++X)
	for (const FGuid& Id : Surfaces)
	{
		FCozySurfaceHit Hit;
		const FVector Position = Config.GridOrigin + FVector((X + .5f) * Step, (Y + .5f) * Step, 0);
		if (!CozySurface::GroundAt(GetWorld(), Position, Hit, &Id) || Hit.Normal.Z < .72f || (Hit.Rules.IsValid() && !Hit.Rules->bAllowWalking)) continue;
		// 경사면에서는 캡슐의 둥근 아랫부분이 수직 최저점보다 먼저 바닥에 닿는다.
		// 평지와 같은 높이에 두면 정상 경사로도 미세한 바닥 겹침으로 전부 제외된다.
		if (GetWorld()->OverlapBlockingTestByChannel(Hit.Position + FVector(0, 0, CozyWalk::ClearanceCenter(Hit.Normal.Z)), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(CozyWalk::Radius, CozyWalk::HalfHeight), Clearance)) continue;
		FNode Node; Node.Surface = Hit; Node.Cell = FIntPoint(X, Y);
		Columns.FindOrAdd(Node.Cell).Add(Nodes.Add(MoveTemp(Node)));
	}
	const FIntPoint Directions[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	for (int32 I = 0; I < Nodes.Num(); ++I)
	for (const FIntPoint& Direction : Directions)
	{
		const TArray<int32>* Adjacent = Columns.Find(Nodes[I].Cell + Direction);
		if (!Adjacent) continue;
		for (int32 J : *Adjacent) if (Connects(Nodes[I].Surface, Nodes[J].Surface, &Clearance)) Nodes[I].Neighbors.Add(J);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("CozyTerrainTest")))
		for (const FNode& Node : Nodes)
			if (Node.Cell.Y == 13 && Node.Cell.X >= 6 && Node.Cell.X <= 17)
				UE_LOG(LogTemp, Display, TEXT("[지형 경로] 위치=%s 표면=%s 연결=%d 비어있음=%d"), *Node.Surface.Position.ToString(), *Node.Surface.SurfaceId.ToString(), Node.Neighbors.Num(), IsFree(Node.Surface.Position));
}

bool UCozySurfaceNavigation::FindPath(const FVector& StartFeet, const FVector& GoalFeet, TArray<FVector>& OutPoints, const FGuid& IgnoreFacility, const FBox* ExtraBlock)
{
	Build(); OutPoints.Reset();
	FCozySurfaceHit StartSurface, GoalSurface;
	if (!FindSupportingSurface(StartFeet, StartSurface) || !FindSupportingSurface(GoalFeet, GoalSurface)
		|| !IsFree(StartSurface.Position, IgnoreFacility) || !IsFree(GoalSurface.Position, IgnoreFacility)) return false;
	FCollisionQueryParams EndpointClearance(SCENE_QUERY_STAT(CozyPathEndpoints), false);
	CozyWalk::IgnoreFacilitiesAndCharacters(GetWorld(), EndpointClearance);
	int32 Start = INDEX_NONE, Goal = INDEX_NONE;
	double StartDistance = TNumericLimits<double>::Max(), GoalDistance = StartDistance;
	TArray<bool> Free; Free.Init(false, Nodes.Num());
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		Free[I] = IsFree(Nodes[I].Surface.Position, IgnoreFacility);
		if (ExtraBlock && ExtraBlock->IsInsideOrOn(Nodes[I].Surface.Position)) Free[I] = false;
		if (!Free[I]) continue;
		const FVector P = Nodes[I].Surface.Position;
		const double SD = FVector::DistSquared(P, StartFeet), GD = FVector::DistSquared(P, GoalFeet);
		if (FMath::Abs(P.Z - StartFeet.Z) <= 100 && SD < StartDistance && Connects(StartSurface, Nodes[I].Surface, &EndpointClearance)) { StartDistance = SD; Start = I; }
		if (FMath::Abs(P.Z - GoalFeet.Z) <= 100 && GD < GoalDistance && Connects(Nodes[I].Surface, GoalSurface, &EndpointClearance)) { GoalDistance = GD; Goal = I; }
	}
	if (Start == INDEX_NONE || Goal == INDEX_NONE || StartDistance > FMath::Square(250.) || GoalDistance > FMath::Square(150.)) return false;
	TArray<float> Cost; Cost.Init(TNumericLimits<float>::Max(), Nodes.Num());
	TArray<int32> Parent; Parent.Init(INDEX_NONE, Nodes.Num());
	TArray<int32> Open; Open.Add(Start); Cost[Start] = 0;
	TArray<bool> Closed; Closed.Init(false, Nodes.Num());
	while (!Open.IsEmpty())
	{
		int32 Best = 0;
		for (int32 I = 1; I < Open.Num(); ++I)
			if (Cost[Open[I]] + FVector::Dist(Nodes[Open[I]].Surface.Position, Nodes[Goal].Surface.Position) < Cost[Open[Best]] + FVector::Dist(Nodes[Open[Best]].Surface.Position, Nodes[Goal].Surface.Position)) Best = I;
		const int32 Current = Open[Best]; Open.RemoveAtSwap(Best);
		if (Closed[Current]) continue;
		Closed[Current] = true;
		if (Current == Goal) break;
		for (int32 Next : Nodes[Current].Neighbors)
		{
			if (!Free[Next] || Closed[Next]) continue;
			const float NewCost = Cost[Current] + FVector::Dist(Nodes[Current].Surface.Position, Nodes[Next].Surface.Position);
			if (NewCost < Cost[Next]) { Cost[Next] = NewCost; Parent[Next] = Current; Open.Add(Next); }
		}
	}
	if (!Closed[Goal]) return false;
	for (int32 I = Goal; I != INDEX_NONE; I = Parent[I])
	{
		OutPoints.Insert(Nodes[I].Surface.Position, 0);
		if (I == Start) break;
	}
	if (OutPoints.IsEmpty() || !OutPoints.Last().Equals(GoalSurface.Position, 1.f)) OutPoints.Add(GoalSurface.Position);
	return true;
}

bool UCozySurfaceNavigation::FindSupportingSurface(const FVector& Feet, FCozySurfaceHit& Out) const
{
	TSet<FGuid> Checked;
	double Difference = 50.;
	bool Found = false;
	for (const FNode& Node : Nodes)
	{
		if (Checked.Contains(Node.Surface.SurfaceId)) continue;
		Checked.Add(Node.Surface.SurfaceId);
		FCozySurfaceHit Hit;
		if (CozySurface::GroundAt(GetWorld(), Feet, Hit, &Node.Surface.SurfaceId) && Hit.Normal.Z >= .72f
			&& (!Hit.Rules.IsValid() || Hit.Rules->bAllowWalking) && FMath::Abs(Hit.Position.Z - Feet.Z) < Difference)
		{ Difference = FMath::Abs(Hit.Position.Z - Feet.Z); Out = Hit; Found = true; }
	}
	return Found;
}

bool UCozySurfaceNavigation::FindNearestFree(const FVector& Feet, FVector& OutFeet)
{
	Build();
	double Distance = TNumericLimits<double>::Max();
	bool Found = false;
	for (const FNode& Node : Nodes)
	{
		const FVector Position = Node.Surface.Position;
		// 시설 이동으로 밀어낼 때 다른 층으로 순간 이동시키지 않는다.
		if (FMath::Abs(Position.Z - Feet.Z) > 50 || !IsFree(Position)) continue;
		const double Candidate = FVector::DistSquared(Position, Feet);
		if (Candidate < Distance) { Distance = Candidate; OutFeet = Position; Found = true; }
	}
	return Found;
}

bool UCozySurfaceNavigation::HasAccessibleEdge(const FVector& Min, const FVector2D& Size, float Height, const FGuid& IgnoreFacility)
{
	Build();
	const FBox Block(Min - FVector(30, 30, 10), Min + FVector(Size.X + 30, Size.Y + 30, Height + 150));
	int32 Anchor = INDEX_NONE;
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const FVector P = Nodes[I].Surface.Position;
		if (Block.IsInsideOrOn(P) || !IsFree(P, IgnoreFacility)) continue;
		if (Anchor == INDEX_NONE || P.Z < Nodes[Anchor].Surface.Position.Z) Anchor = I;
	}
	if (Anchor == INDEX_NONE) return false;
	const FVector Entrances[] = {
		Min + FVector(-75, Size.Y * .5, 0), Min + FVector(Size.X + 75, Size.Y * .5, 0),
		Min + FVector(Size.X * .5, -75, 0), Min + FVector(Size.X * .5, Size.Y + 75, 0) };
	for (const FVector& Entrance : Entrances)
	{
		TArray<FVector> Path;
		if (FindPath(Nodes[Anchor].Surface.Position, Entrance, Path, IgnoreFacility, &Block)) return true;
	}
	return false;
}
