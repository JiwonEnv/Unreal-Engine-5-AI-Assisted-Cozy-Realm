#include "Environment/CozySurfaceComponent.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h"
#include "Estate/CozyEstateSubsystem.h"

void UCozySurfaceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!PaintMask.IsValid()) { PaintMask.DomainId = PaintDomainId; PaintMask.Initialize(128); }
	if (const UCozyEstateSubsystem* Estate = GetWorld()->GetSubsystem<UCozyEstateSubsystem>())
	{
		for (const FCozyPaintMask& Saved : Estate->GetState().PaintMasks)
			if (Saved.DomainId == PaintDomainId) { RestorePaint(Saved); break; }
	}
	RefreshPaintTexture();
}

void UCozySurfaceComponent::BeginPaintStroke() { History.Begin(PaintMask); }
bool UCozySurfaceComponent::EndPaintStroke() { return History.Commit(PaintMask); }
bool UCozySurfaceComponent::UndoPaint() { const bool Changed = History.Undo(PaintMask); if (Changed) RefreshPaintTexture(); return Changed; }
bool UCozySurfaceComponent::RedoPaint() { const bool Changed = History.Redo(PaintMask); if (Changed) RefreshPaintTexture(); return Changed; }
bool UCozySurfaceComponent::RestorePaint(const FCozyPaintMask& Saved)
{
	if (Saved.DomainId != PaintDomainId) return false;
	if (!Saved.IsValid()) { bPreserveInvalidSavedMask = true; return false; }
	bPreserveInvalidSavedMask = false;
	PaintMask = Saved; History = FCozyPaintHistory(); RefreshPaintTexture(); return true;
}
bool UCozySurfaceComponent::PaintAt(FVector WorldPosition, float Radius, int32 Layer, float Strength)
{
	FVector2D UV;
	if (!bAllowPainting || !WorldToPaintUV(WorldPosition, UV) || Radius <= 0) return false;
	const FVector Scale = GetOwner()->GetActorScale3D().GetAbs();
	const FVector2D Extent = (PaintMax - PaintMin) * FVector2D(Scale.X, Scale.Y);
	if (Extent.X <= 0 || Extent.Y <= 0) return false;
	const bool Changed = PaintMask.Stamp(UV, FVector2D(Radius / Extent.X, Radius / Extent.Y), Layer, Strength);
	if (Changed)
	{
		const FVector2D RadiusUV(Radius / Extent.X, Radius / Extent.Y);
		const int32 N = PaintMask.Resolution;
		const FIntRect Area(
			FMath::Clamp(FMath::FloorToInt((UV.X - RadiusUV.X) * N), 0, N - 1),
			FMath::Clamp(FMath::FloorToInt((UV.Y - RadiusUV.Y) * N), 0, N - 1),
			FMath::Clamp(FMath::CeilToInt((UV.X + RadiusUV.X) * N) + 1, 1, N),
			FMath::Clamp(FMath::CeilToInt((UV.Y + RadiusUV.Y) * N) + 1, 1, N));
		if (!bPaintDirty) DirtyPaintRect = Area;
		else { DirtyPaintRect.Min = DirtyPaintRect.Min.ComponentMin(Area.Min); DirtyPaintRect.Max = DirtyPaintRect.Max.ComponentMax(Area.Max); }
		bPaintDirty = true; bPreserveInvalidSavedMask = false;
	}
	return Changed;
}
void UCozySurfaceComponent::RefreshPaintTexture(bool bWholeMask)
{
	if (!PaintMask.IsValid() || !GetOwner()) return;
	const bool bNewTexture = !PaintTexture || PaintTexture->GetSizeX() != PaintMask.Resolution;
	if (bNewTexture)
	{
		PaintTexture = UTexture2D::CreateTransient(PaintMask.Resolution, PaintMask.Resolution, PF_B8G8R8A8);
		PaintTexture->SRGB = false;
		PaintTexture->Filter = TF_Bilinear;
		PaintTexture->AddressX = TA_Clamp; PaintTexture->AddressY = TA_Clamp;
	}
	const int32 Bytes = PaintMask.Pixels.Num() * sizeof(FColor);
	if (bNewTexture)
	{
		FTexture2DMipMap& Mip = PaintTexture->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, PaintMask.Pixels.GetData(), Bytes);
		Mip.BulkData.Unlock(); PaintTexture->UpdateResource();
	}
	else
	{
		const FIntRect Area = bWholeMask || !bPaintDirty ? FIntRect(0, 0, PaintMask.Resolution, PaintMask.Resolution) : DirtyPaintRect;
		const uint32 Pitch = Area.Width() * sizeof(FColor);
		uint8* Upload = new uint8[Pitch * Area.Height()];
		for (int32 Y = 0; Y < Area.Height(); ++Y)
			FMemory::Memcpy(Upload + Y * Pitch, PaintMask.Pixels.GetData() + (Area.Min.Y + Y) * PaintMask.Resolution + Area.Min.X, Pitch);
		auto* Region = new FUpdateTextureRegion2D(Area.Min.X, Area.Min.Y, 0, 0, Area.Width(), Area.Height());
		PaintTexture->UpdateTextureRegions(0, 1, Region, Pitch, sizeof(FColor), Upload,
			[](uint8* Data, const FUpdateTextureRegion2D* Area) { delete[] Data; delete Area; });
	}
	const FTransform Transform = GetOwner()->GetActorTransform();
	const FVector Origin = Transform.GetLocation();
	const FVector AxisU = Transform.GetUnitAxis(EAxis::X) / FMath::Max(.001, Transform.GetScale3D().X);
	const FVector AxisV = Transform.GetUnitAxis(EAxis::Y) / FMath::Max(.001, Transform.GetScale3D().Y);
	const FLinearColor Bounds(PaintMin.X, PaintMin.Y, PaintMax.X - PaintMin.X, PaintMax.Y - PaintMin.Y);
	if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(GetOwner()))
	{
		Landscape->SetLandscapeMaterialTextureParameterValue(TEXT("CozyPaintMask"), PaintTexture);
		Landscape->SetLandscapeMaterialScalarParameterValue(TEXT("CozyPaintEnabled"), 1.f);
		Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("CozyPaintOrigin"), FLinearColor(Origin));
		Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("CozyPaintAxisU"), FLinearColor(AxisU));
		Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("CozyPaintAxisV"), FLinearColor(AxisV));
		Landscape->SetLandscapeMaterialVectorParameterValue(TEXT("CozyPaintBounds"), Bounds);
	}
	TArray<UMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
	{
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
		if (!Material) Material = Mesh->CreateDynamicMaterialInstance(Slot);
		if (Material)
		{
			Material->SetTextureParameterValue(TEXT("CozyPaintMask"), PaintTexture);
			Material->SetScalarParameterValue(TEXT("CozyPaintEnabled"), 1.f);
			Material->SetVectorParameterValue(TEXT("CozyPaintOrigin"), FLinearColor(Origin));
			Material->SetVectorParameterValue(TEXT("CozyPaintAxisU"), FLinearColor(AxisU));
			Material->SetVectorParameterValue(TEXT("CozyPaintAxisV"), FLinearColor(AxisV));
			Material->SetVectorParameterValue(TEXT("CozyPaintBounds"), Bounds);
		}
	}
}

UCozySurfaceComponent::UCozySurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCozySurfaceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bPaintDirty) { RefreshPaintTexture(false); bPaintDirty = false; }
}

void UCozySurfaceComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	if (!IsTemplate() && !SurfaceId.IsValid()) SurfaceId = FGuid::NewGuid();
	if (!IsTemplate() && !PaintDomainId.IsValid()) PaintDomainId = FGuid::NewGuid();
}

void UCozySurfaceComponent::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal && !IsTemplate())
	{
		SurfaceId = FGuid::NewGuid(); PaintDomainId = FGuid::NewGuid();
	}
}

bool UCozySurfaceComponent::WorldToPaintUV(const FVector& WorldPosition, FVector2D& OutUV) const
{
	if (!GetOwner()) return false;
	const FVector Local = GetOwner()->GetActorTransform().InverseTransformPosition(WorldPosition);
	const FVector2D Size = PaintMax - PaintMin;
	if (Size.X <= 0 || Size.Y <= 0) return false;
	OutUV = (FVector2D(Local.X, Local.Y) - PaintMin) / Size;
	return OutUV.X >= 0 && OutUV.X <= 1 && OutUV.Y >= 0 && OutUV.Y <= 1;
}

bool CozySurface::Trace(const UWorld* World, const FVector& Start, const FVector& End, FCozySurfaceHit& Out, const FGuid* RequiredSurface)
{
	if (!World) return false;
	TArray<FHitResult> Hits;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	World->LineTraceMultiByObjectType(Hits, Start, End, Objects, FCollisionQueryParams(SCENE_QUERY_STAT(CozySurface), true));
	for (const FHitResult& Hit : Hits)
	{
		AActor* Actor = Hit.GetActor();
		if (!Actor) continue;
		UCozySurfaceComponent* Rules = Actor->FindComponentByClass<UCozySurfaceComponent>();
		// 기존 Landscape는 별도 설정 없이 기본 표면으로 사용한다.
		if (!Rules && !Actor->IsA<ALandscapeProxy>() && !Actor->ActorHasTag(TEXT("CozyGround"))) continue;
		const FGuid Id = Rules ? Rules->SurfaceId : FGuid();
		if (RequiredSurface && Id != *RequiredSurface) continue;
		Out.Position = Hit.ImpactPoint;
		Out.Normal = Hit.ImpactNormal;
		Out.SurfaceId = Rules ? Rules->SurfaceId : FGuid();
		Out.Rules = Rules;
		return true;
	}
	return false;
}

bool CozySurface::GroundAt(const UWorld* World, const FVector& Position, FCozySurfaceHit& Out, const FGuid* RequiredSurface)
{
	return Trace(World, Position + FVector(0, 0, 10000), Position - FVector(0, 0, 10000), Out, RequiredSurface);
}

bool CozySurface::CheckFootprint(const UWorld* World, const FVector& Min, const FVector2D& Size, float Spacing, FCozySurfaceHit& Out, FText& Reason, const FGuid* RequiredSurface)
{
	if (Size.X <= 0 || Size.Y <= 0 || !GroundAt(World, Min + FVector(Size.X * .5, Size.Y * .5, 0), Out, RequiredSurface))
	{
		Reason = FText::FromString(TEXT("시설을 지지할 지면이 없습니다"));
		return false;
	}
	const UCozySurfaceComponent* Rules = Out.Rules.Get();
	if (Rules && !Rules->bAllowBuilding)
	{
		Reason = FText::FromString(TEXT("건설할 수 없는 표면입니다"));
		return false;
	}
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(Rules ? Rules->MaxBuildSlope : 8.f));
	const float HeightTolerance = Rules ? Rules->MaxSupportHeightDifference : 12.f;
	const int32 NX = FMath::Clamp(FMath::CeilToInt(Size.X / FMath::Max(25.f, Spacing)), 1, 128);
	const int32 NY = FMath::Clamp(FMath::CeilToInt(Size.Y / FMath::Max(25.f, Spacing)), 1, 128);
	float Low = Out.Position.Z, High = Low;
	for (int32 Y = 0; Y <= NY; ++Y)
	for (int32 X = 0; X <= NX; ++X)
	{
		// 시설 외곽까지 검사하되 경계 수치 오차를 피하도록 1cm 안쪽을 쓴다.
		const FVector P = Min + FVector(FMath::Lerp(1., FMath::Max(1., Size.X - 1.), double(X) / NX), FMath::Lerp(1., FMath::Max(1., Size.Y - 1.), double(Y) / NY), 0);
		FCozySurfaceHit Sample;
		FVector2D UV;
		if (!GroundAt(World, P, Sample, &Out.SurfaceId) || Sample.Normal.Z < MinNormalZ || (Rules && !Rules->WorldToPaintUV(Sample.Position, UV)))
		{
			Reason = FText::FromString(TEXT("시설 바닥 전체가 같은 평평한 표면 위에 있어야 합니다"));
			return false;
		}
		Low = FMath::Min(Low, float(Sample.Position.Z));
		High = FMath::Max(High, float(Sample.Position.Z));
	}
	if (High - Low > HeightTolerance)
	{
		Reason = FText::FromString(TEXT("지면 높이 차이가 커서 시설이 뜨거나 묻힙니다"));
		return false;
	}
	Out.Position.Z = High;
	Reason = FText::GetEmpty();
	return true;
}
