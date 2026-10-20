#include "Environment/CozyFloorTiles.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACozyFloorTiles::ACozyFloorTiles()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	for (int32 I = 0; I < 3; ++I)
	{
		UInstancedStaticMeshComponent* Group = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Tiles%d"), I));
		Group->SetupAttachment(RootComponent);
		Group->SetStaticMesh(Cube.Object);
		Group->SetMaterial(0, Material.Object);
		Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Group->SetCanEverAffectNavigation(false);
		Group->SetCastShadow(false);
		Groups.Add(Group);
	}
}

void ACozyFloorTiles::Rebuild(const TArray<FCozyFloorTile>& Tiles)
{
	const FLinearColor Colors[] = { FLinearColor(.38f, .24f, .12f), FLinearColor(.55f, .53f, .48f), FLinearColor(.25f, .38f, .22f) };
	for (int32 I = 0; I < Groups.Num(); ++I)
	{
		Groups[I]->ClearInstances();
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Groups[I]->GetMaterial(0));
		if (!Material) Material = Groups[I]->CreateDynamicMaterialInstance(0);
		if (Material) Material->SetVectorParameterValue(TEXT("Color"), Colors[I]);
	}
	for (const FCozyFloorTile& Tile : Tiles)
	{
		if (!Tile.bStored && Groups.IsValidIndex(Tile.Style)) Groups[Tile.Style]->AddInstance(Tile.Transform, true);
	}
}
