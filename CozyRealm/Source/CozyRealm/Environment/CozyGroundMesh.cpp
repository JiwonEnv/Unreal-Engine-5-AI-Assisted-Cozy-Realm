#include "Environment/CozyGroundMesh.h"
#include "Environment/CozySurfaceComponent.h"

ACozyGroundMesh::ACozyGroundMesh()
{
	Surface = CreateDefaultSubobject<UCozySurfaceComponent>(TEXT("Surface"));
}
