#include "Core/CozyRealmEstateGameMode.h"
#include "Core/CozyRealmEstatePlayerController.h"
#include "Camera/CozyRealmCameraPawn.h"
#include "EngineUtils.h"

ACozyRealmEstateGameMode::ACozyRealmEstateGameMode()
{
	DefaultPawnClass = ACozyRealmCameraPawn::StaticClass();
	PlayerControllerClass = ACozyRealmEstatePlayerController::StaticClass();
}

void ACozyRealmEstateGameMode::RestartPlayer(AController* NewPlayer)
{
	// Prefer the camera placed in the level, so its settings asset is the one in use
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		for (TActorIterator<ACozyRealmCameraPawn> It(GetWorld()); It; ++It)
		{
			if (!It->GetController())
			{
				NewPlayer->Possess(*It);
				return;
			}
		}
	}

	Super::RestartPlayer(NewPlayer);
}
